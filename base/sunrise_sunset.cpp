#include "base/sunrise_sunset.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"
#include "base/math.hpp"
#include "base/timegm.hpp"

namespace
{

// Sun's zenith for sunrise/sunset
//   offical      = 90 degrees 50'
//   civil        = 96 degrees
//   nautical     = 102 degrees
//   astronomical = 108 degrees
double constexpr kZenith = 90 + 50. / 60.; // 90 degrees 50'

time_t constexpr kOneDaySeconds = 24 * 60 * 60;

double NormalizeAngle(double a)
{
  double res = fmod(a, 360.);
  if (res < 0)
    res += 360.;
  return res;
}

void NextDay(int & year, int & month, int & day)
{
  ASSERT_GREATER_OR_EQUAL(month, 1, ());
  ASSERT_LESS_OR_EQUAL(month, 12, ());
  ASSERT_GREATER_OR_EQUAL(day, 1, ());
  ASSERT_LESS_OR_EQUAL(day, base::DaysOfMonth(year, month), ());

  if (day < base::DaysOfMonth(year, month))
  {
    ++day;
    return;
  }
  if (month < 12)
  {
    day = 1;
    ++month;
    return;
  }
  day = 1;
  month = 1;
  ++year;
}

void PrevDay(int & year, int & month, int & day)
{
  ASSERT_GREATER_OR_EQUAL(month, 1, ());
  ASSERT_LESS_OR_EQUAL(month, 12, ());
  ASSERT_GREATER_OR_EQUAL(day, 1, ());
  ASSERT_LESS_OR_EQUAL(day, base::DaysOfMonth(year, month), ());

  if (day > 1)
  {
    --day;
    return;
  }
  if (month > 1)
  {
    --month;
    day = base::DaysOfMonth(year, month);
    return;
  }
  --year;
  month = 12;
  day = 31;
}

enum class DayEventType
{
  Sunrise,
  Sunset,
  PolarDay,
  PolarNight
};

// Main work-horse function which calculates sunrise/sunset in a specified date in a specified location.
// This function was taken from source http://williams.best.vwh.net/sunrise_sunset_algorithm.htm.
// Notation is kept to have source close to source.
// Original article is // http://babel.hathitrust.org/cgi/pt?id=uiug.30112059294311;view=1up;seq=25
std::pair<DayEventType, time_t> CalculateDayEventTime(time_t timeUtc,
                                                 double latitude, double longitude,
                                                 bool sunrise)
{
  tm const * const gmt = gmtime(&timeUtc);
  if (nullptr == gmt)
    MYTHROW(RootException, ("gmtime failed, time =", timeUtc));

  int year = gmt->tm_year + 1900;
  int month = gmt->tm_mon + 1;
  int day = gmt->tm_mday;

  // 1. first calculate the day of the year

  double const N1 = floor(275. * month / 9.);
  double const N2 = floor((month + 9.) / 12.);
  double const N3 = (1. + floor((year - 4. * floor(year / 4.) + 2.) / 3.));
  double const N = N1 - (N2 * N3) + day - 30.;

  // 2. convert the longitude to hour value and calculate an approximate time

  double const lngHour = longitude / 15;

  double t = 0;
  if (sunrise)
    t = N + ((6 - lngHour) / 24);
  else
    t = N + ((18 - lngHour) / 24);

  // 3. calculate the Sun's mean anomaly

  double const M = (0.9856 * t) - 3.289;

  // 4. calculate the Sun's true longitude

  double L = M + (1.916 * sin(math::DegToRad(M))) + (0.020 * sin(2 * math::DegToRad(M))) + 282.634;
  // NOTE: L potentially needs to be adjusted into the range [0,360) by adding/subtracting 360
  L = NormalizeAngle(L);

  // 5a. calculate the Sun's right ascension

  double RA = math::RadToDeg( atan(0.91764 * tan(math::DegToRad(L))) );
  // NOTE: RA potentially needs to be adjusted into the range [0,360) by adding/subtracting 360
  RA = NormalizeAngle(RA);

  // 5b. right ascension value needs to be in the same quadrant as L

  double const Lquadrant = (floor( L / 90)) * 90;
  double const RAquadrant = (floor(RA / 90)) * 90;
  RA = RA + (Lquadrant - RAquadrant);

  // 5c. right ascension value needs to be converted into hours

  RA = RA / 15;

  // 6. calculate the Sun's declination

  double sinDec = 0.39782 * sin(math::DegToRad(L));
  double cosDec = cos(asin(sinDec));

  // 7a. calculate the Sun's local hour angle

  double cosH = (cos(math::DegToRad(kZenith)) - (sinDec * sin(math::DegToRad(latitude)))) / (cosDec * cos(math::DegToRad(latitude)));

  // if cosH > 1 then sun is never rises on this location on specified date (polar night)
  // if cosH < -1 then sun is never sets on this location on specified date (polar day)
  if (cosH < -1 || cosH > 1)
  {
    int const h = sunrise ? 0 : 23;
    int const m = sunrise ? 0 : 59;
    int const s = sunrise ? 0 : 59;

    return std::make_pair((cosH < -1) ? DayEventType::PolarDay : DayEventType::PolarNight,
                     base::TimeGM(year, month, day, h, m, s));
  }

  // 7b. finish calculating H and convert into hours

  double H = 0;
  if (sunrise)
    H = 360 - math::RadToDeg(acos(cosH));
  else
    H = math::RadToDeg(acos(cosH));

  H = H / 15;

  // 8. calculate local mean time of rising/setting

  double T = H + RA - (0.06571 * t) - 6.622;

  if (T > 24.)
    T = fmod(T, 24.);
  else if (T < 0)
    T += 24.;

  // 9. adjust back to UTC

  double UT = T - lngHour;

  if (UT > 24.)
  {
    NextDay(year, month, day);
    UT = fmod(UT, 24.0);
  }
  else if (UT < 0)
  {
    PrevDay(year, month, day);
    UT += 24.;
  }

  // UT - is a hour with fractional part of date year/month/day, in range of [0;24)

  int const h = floor(UT); // [0;24)
  int const m = floor((UT - h) * 60); // [0;60)
  int const s = fmod(floor(UT * 60 * 60) /* number of seconds from 0:0 to UT */, 60); // [0;60)

  return std::make_pair(sunrise ? DayEventType::Sunrise : DayEventType::Sunset,
                   base::TimeGM(year, month, day, h, m, s));
}

} // namespace

DayTimeType GetDayTime(time_t timeUtc, double latitude, double longitude)
{
  auto const sunrise = CalculateDayEventTime(timeUtc, latitude, longitude, true /* sunrise */);
  auto const sunset = CalculateDayEventTime(timeUtc, latitude, longitude, false /* sunrise */);

  // Edge cases: polar day and polar night
  if (sunrise.first == DayEventType::PolarDay || sunset.first == DayEventType::PolarDay)
    return DayTimeType::PolarDay;
  if (sunrise.first == DayEventType::PolarNight || sunset.first == DayEventType::PolarNight)
    return DayTimeType::PolarNight;

  if (timeUtc < sunrise.second)
  {
    auto const prevSunrise = CalculateDayEventTime(timeUtc - kOneDaySeconds, latitude, longitude, true /* sunrise */);
    auto const prevSunset = CalculateDayEventTime(timeUtc - kOneDaySeconds, latitude, longitude, false /* sunrise */);

    if (timeUtc >= prevSunset.second)
      return DayTimeType::Night;
    if (timeUtc < prevSunrise.second)
      return DayTimeType::Night;
    return DayTimeType::Day;
  }
  if (timeUtc > sunset.second)
  {
    auto const nextSunrise = CalculateDayEventTime(timeUtc + kOneDaySeconds, latitude, longitude, true /* sunrise */);
    auto const nextSunset = CalculateDayEventTime(timeUtc + kOneDaySeconds, latitude, longitude, false /* sunrise */);

    if (timeUtc < nextSunrise.second)
      return DayTimeType::Night;
    if (timeUtc > nextSunset.second)
      return DayTimeType::Night;
    return DayTimeType::Day;
  }

  return DayTimeType::Day;
}

std::string DebugPrint(DayTimeType type)
{
  switch (type)
  {
  case DayTimeType::Day: return "Day";
  case DayTimeType::Night: return "Night";
  case DayTimeType::PolarDay: return "PolarDay";
  case DayTimeType::PolarNight: return "PolarNight";
  }
  return {};
}
