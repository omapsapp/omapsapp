#include "platform/locale.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/settings.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/bits.hpp"
#include "base/logging.hpp"
#include "base/macros.hpp"
#include "base/math.hpp"
#include "base/string_utils.hpp"

#include <cmath>
#include <cstring>    // strstr
#include <iomanip>
#include <sstream>

namespace measurement_utils
{
std::string ToStringPrecision(double d, int pr)
{
  // We assume that the app will be restarted if a user changes device's locale.
  static auto const locale = platform::GetCurrentLocale();

  return ToStringPrecisionLocale(locale, d, pr);
}

std::string ToStringPrecisionLocale(platform::Locale const & loc, double d, int pr)
{
  std::string formatBuf("%0.0f");
  if (pr > 0)
  {
    if (pr < 10)
      formatBuf[3] = '0' + pr;  // E.g. replace %0.0f with %0.1f
    else
      LOG(LERROR, ("Too big unsupported precision", pr));
  }

  char textBuf[50];
  int const n = std::snprintf(textBuf, sizeof(textBuf), formatBuf.c_str(), d);
  if (n < 0 || n >= static_cast<int>(sizeof(textBuf)))
  {
    LOG(LERROR, ("snprintf", pr, d, "failed with", n));
    return std::to_string(static_cast<int64_t>(d));
  }

  std::string out(textBuf);

  // std::locale does not work on Android NDK, so decimal and grouping (thousands) separator
  // shall be customized manually here.

  if (pr)
  {
    // Value with decimals. Set locale decimal separator.
    if (loc.m_decimalSeparator != ".")
      out.replace(out.size() - pr - 1, 1, loc.m_decimalSeparator);
  }
  else
  {
    // Value with no decimals. Check if it's equal or bigger than 10000 to
    // insert the grouping (thousands) separator characters.
    if (out.size() > 4 && !loc.m_groupingSeparator.empty())
      for (long pos = static_cast<long>(out.size()) - 3; pos > 0; pos -= 3)
        out.insert(pos, loc.m_groupingSeparator);
  }

  return out;
}

std::string_view DebugPrint(Units units)
{
  switch (units)
  {
  case Units::Imperial: return "Units::Imperial";
  case Units::Metric: return "Units::Metric";
  }
  UNREACHABLE();
}

Units GetMeasurementUnits()
{
  Units units = Units::Metric;
  settings::TryGet(settings::kMeasurementUnits, units);
  return units;
}

double ToSpeedKmPH(double speed, Units units)
{
  switch (units)
  {
  case Units::Imperial: return MiphToKmph(speed);
  case Units::Metric: return speed;
  }
  UNREACHABLE();
}

std::string FormatLatLonAsDMSImpl(double value, char positive, char negative, int dac)
{
  using namespace base;

  std::ostringstream sstream;
  sstream << std::setfill('0');

  // Degrees
  double i;
  double d = std::modf(std::fabs(value), &i);
  sstream << std::setw(2) << i << "°";

  // Minutes
  d = std::modf(d * 60.0, &i);
  sstream << std::setw(2) << i << "′";

  // Seconds
  d = d * 60.0;
  if (dac == 0)
    d = SignedRound(d);

  d = std::modf(d, &i);
  sstream << std::setw(2) << i;

  if (dac > 0)
    sstream << strings::to_string_dac(d, dac).substr(1);

  sstream << "″";

  // This condition is too heavy for production purposes (but more correct).
  //if (base::SignedRound(value * 3600.0 * pow(10, dac)) != 0)
  if (!AlmostEqualULPs(value, 0.0))
  {
    char postfix = positive;
    if (value < 0.0)
      postfix = negative;

    sstream << postfix;
  }

  return sstream.str();
}

std::string FormatLatLonAsDMS(double lat, double lon, bool withComma, int dac)
{
  return (FormatLatLonAsDMSImpl(lat, 'N', 'S', dac) + (withComma ? ", " : " ") +
          FormatLatLonAsDMSImpl(lon, 'E', 'W', dac));
}

void FormatLatLonAsDMS(double lat, double lon, std::string & latText, std::string & lonText, int dac)
{
  latText = FormatLatLonAsDMSImpl(lat, 'N', 'S', dac);
  lonText = FormatLatLonAsDMSImpl(lon, 'E', 'W', dac);
}

void FormatMercatorAsDMS(m2::PointD const & mercator, std::string & lat, std::string & lon, int dac)
{
  lat = FormatLatLonAsDMSImpl(mercator::YToLat(mercator.y), 'N', 'S', dac);
  lon = FormatLatLonAsDMSImpl(mercator::XToLon(mercator.x), 'E', 'W', dac);
}

std::string FormatMercatorAsDMS(m2::PointD const & mercator, int dac)
{
  return FormatLatLonAsDMS(mercator::YToLat(mercator.y), mercator::XToLon(mercator.x), dac);
}

// @TODO take into account decimal points or commas as separators in different locales
std::string FormatLatLon(double lat, double lon, int dac)
{
  return strings::to_string_dac(lat, dac) + " " + strings::to_string_dac(lon, dac);
}

std::string FormatLatLon(double lat, double lon, bool withComma, int dac)
{
  return strings::to_string_dac(lat, dac) + (withComma ? ", " : " ") + strings::to_string_dac(lon, dac);
}

void FormatLatLon(double lat, double lon, std::string & latText, std::string & lonText, int dac)
{
  latText = strings::to_string_dac(lat, dac);
  lonText = strings::to_string_dac(lon, dac);
}

std::string FormatMercator(m2::PointD const & mercator, int dac)
{
  return FormatLatLon(mercator::YToLat(mercator.y), mercator::XToLon(mercator.x), dac);
}

void FormatMercator(m2::PointD const & mercator, std::string & lat, std::string & lon, int dac)
{
  lat = strings::to_string_dac(mercator::YToLat(mercator.y), dac);
  lon = strings::to_string_dac(mercator::XToLon(mercator.x), dac);
}

double MpsToUnits(double metersPerSecond, Units units)
{
  switch (units)
  {
  case Units::Imperial: return KmphToMiph(MpsToKmph(metersPerSecond)); break;
  case Units::Metric: return MpsToKmph(metersPerSecond); break;
  }
  UNREACHABLE();
}

std::string FormatSpeedNumeric(double metersPerSecond, Units units)
{
  double const unitsPerHour = MpsToUnits(metersPerSecond, units);
  double roundedValue = std::round(unitsPerHour);
  return std::to_string(static_cast<int>(roundedValue));
}

std::string FormatOsmLink(double lat, double lon, int zoom)
{
  static constexpr char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_~";

  // Same as (lon + 180) / 360 * 1UL << 32, but without warnings.
  double constexpr factor = (1 << 30) / 90.0;
  uint32_t const x = round((lon + 180.0) * factor);
  uint32_t const y = round((lat + 90.0) * factor * 2.0);
  uint64_t const code = bits::BitwiseMerge(y, x);
  std::string osmUrl = "https://osm.org/go/";

  for (int i = 0; i < (zoom + 10) / 3; ++i)
  {
    const uint64_t digit = (code >> (58 - 6 * i)) & 0x3f;
    ASSERT_LESS(digit, ARRAY_SIZE(chars), ());
    osmUrl += chars[digit];
  }

  for (int i = 0; i < (zoom + 8) % 3; ++i)
    osmUrl += "-";
  // ?m tells OSM to display a marker 
  return osmUrl + "?m";
}

bool OSMDistanceToMeters(std::string const & osmRawValue, double & outMeters)
{
  using strings::is_finite;

  char * stop;
  char const * s = osmRawValue.c_str();
  outMeters = strtod(s, &stop);

  // Not a number, was not parsed at all.
  if (s == stop)
    return false;

  if (!is_finite(outMeters))
    return false;

  switch (*stop)
  {
  // Default units - meters.
  case 0: return true;

  // Feet and probably inches.
  case '\'':
    {
      outMeters = FeetToMeters(outMeters);
      s = stop + 1;
      double const inches = strtod(s, &stop);
      if (s != stop && *stop == '"' && is_finite(inches))
        outMeters += InchesToMeters(inches);

      return true;
    }

  // Inches.
  case '"': outMeters = InchesToMeters(outMeters); return true;

  // It's probably a range. Use maximum value (if possible) for a range.
  case '-':
    {
      s = stop + 1;
      double const newValue = strtod(s, &stop);
      if (s != stop && is_finite(newValue))
        outMeters = newValue;
    }
    break;

  // It's probably a list. We don't support them.
  case ';': return false;
  }

  while (*stop && isspace(*stop))
    ++stop;

  // Default units - meters.
  if (*stop == 0)
    return true;

  if (strstr(stop, "nmi") == stop)
    outMeters = NauticalMilesToMeters(outMeters);
  else if (strstr(stop, "mi") == stop)
    outMeters = MilesToMeters(outMeters);
  else if (strstr(stop, "ft") == stop || strstr(stop, "feet") == stop)
    outMeters = FeetToMeters(outMeters);
  else if (strstr(stop, "km") == stop)
    outMeters = outMeters * 1000;

  // Count all other cases as meters.
  return true;
}

std::string OSMDistanceToMetersString(std::string const & osmRawValue,
                                      bool supportZeroAndNegativeValues,
                                      int digitsAfterComma)
{
  double meters;
  if (OSMDistanceToMeters(osmRawValue, meters))
  {
    if (!supportZeroAndNegativeValues && meters <= 0)
      return {};
    return strings::to_string_dac(meters, digitsAfterComma);
  }
  return {};
}
}  // namespace measurement_utils
