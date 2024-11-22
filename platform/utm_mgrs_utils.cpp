#include "platform/utm_mgrs_utils.hpp"

#include "base/string_utils.hpp"
#include "base/math.hpp"

#include <string>
#include <cmath>

namespace utm_mgrs_utils
{
namespace
{
struct UTMPoint
{
  double easting;
  double northing;
  int zoneNumber;
  char zoneLetter;
};

double constexpr K0 = 0.9996; // The scale factor at the central meridian.
double constexpr E = 0.00669438; // The square of the eccentricity for the WGS84 ellipsoid.
double constexpr E2 = E * E;
double constexpr E3 = E2 * E;
double constexpr E_P2 = E / (1 - E);
double constexpr R = 6378137.0; // The Earth equitorial radius for the WGS84 ellipsoid.

double constexpr SQRT_E = 0.996647189; // sqrt(1 - E)
double constexpr _E = (1 - SQRT_E) / (1 + SQRT_E);
double constexpr _E2 = _E * _E;
double constexpr _E3 = _E2 * _E;
double constexpr _E4 = _E3 * _E;
double constexpr _E5 = _E4 * _E;

double constexpr M1 = (1.0 - E / 4.0 - 3.0 * E2 / 64.0 - 5.0 * E3 / 256.0);
double constexpr M2 = (3.0 * E / 8.0 + 3.0 * E2 / 32.0 + 45.0 * E3 / 1024.0);
double constexpr M3 = (15.0 * E2 / 256.0 + 45.0 * E3 / 1024.0);
double constexpr M4 = (35.0 * E3 / 3072.0);

double constexpr P2 = (3.0 / 2.0 * _E - 27.0 / 32.0 * _E3 + 269.0 / 512.0 * _E5);
double constexpr P3 = (21.0 / 16.0 * _E2 - 55.0 / 32.0 * _E4);
double constexpr P4 = (151.0 / 96.0 * _E3 - 417.0 / 128.0 * _E5);
double constexpr P5 = (1097.0 / 512.0 * _E4);

int constexpr kInvalidEastingNorthing = -1;

std::string_view constexpr kZoneLetters = "CDEFGHJKLMNPQRSTUVWXX";

std::string_view constexpr kSetOriginColumnLetters = "SAJSAJ";
std::string_view constexpr kSetOriginRowLetters = "FAFAFA";

// Returns angle in radians to be between -π and π.
double NormalizeAngle(double value)
{
  using math::pi;
  if (value < -pi)
    value += - 2 * pi * (static_cast<int>(value - pi) / (2 * pi));
  else if (value > pi)
    value -= 2 * pi * (static_cast<int>(value + pi) / (2 * pi));
  return value;
}

int LatLonToZoneNumber(double lat, double lon)
{
  if (56.0 <= lat && lat < 64.0 && 3.0 <= lon && lon < 12.0)
    return 32;

  if (72.0 <= lat && lat <= 84.0 and lon >= 0.0)
  {
    if (lon < 9.0)
      return 31;
    if (lon < 21.0)
      return 33;
    if (lon < 33.0)
      return 35;
    if (lon < 42.0)
      return 37;
  }

  return static_cast<int>((lon + 180.0) / 6.0) + 1;
}

char LatitudeToZoneLetter(double lat)
{
  if (-80.0 <= lat && lat <= 84.0)
  {
    auto const index = static_cast<size_t>(lat + 80.0) >> 3;
    ASSERT_LESS(index, kZoneLetters.size(), ());
    return kZoneLetters[index];
  }
  return 0;
}

int ZoneNumberToCentralLon(int zoneNumber)
{
  return (zoneNumber - 1) * 6 - 180 + 3;
}

// Main algorithm. Formulas source: https://github.com/Turbo87/utm
UTMPoint LatLonToUtm(double lat, double lon)
{
  using std::sin, std::cos, std::sqrt;

  double const latRad = math::DegToRad(lat);
  double const latSin = sin(latRad);
  double const latCos = cos(latRad);

  double const latTan = latSin / latCos;
  double const latTan2 = latTan * latTan;
  double const latTan4 = latTan2 * latTan2;

  int const zoneNumber = LatLonToZoneNumber(lat, lon);
  auto const zoneLetter = LatitudeToZoneLetter(lat);
  ASSERT(zoneLetter, (lat));

  double const lonRad = math::DegToRad(lon);
  double const centralLon = ZoneNumberToCentralLon(zoneNumber);
  double const centralLonRad = math::DegToRad(centralLon);

  double const n = R / sqrt(1.0 - E * latSin * latSin);
  double const c = E_P2 * latCos * latCos;

  double const a = latCos * NormalizeAngle(lonRad - centralLonRad);
  double const a2 = a * a;
  double const a3 = a2 * a;
  double const a4 = a3 * a;
  double const a5 = a4 * a;
  double const a6 = a5 * a;

  double const m = R * (M1 * latRad -
                   M2 * sin(2 * latRad) +
                   M3 * sin(4 * latRad) -
                   M4 * sin(6 * latRad));

  double const easting = K0 * n * (a +
                         a3 / 6 * (1 - latTan2 + c) +
                         a5 / 120 * (5 - 18 * latTan2 + latTan4 + 72 * c - 58 * E_P2)) + 500000.0;

  double northing = K0 * (m + n * latTan * (a2 / 2 +
                    a4 / 24 * (5 - latTan2 + 9 * c + 4 * c * c) +
                    a6 / 720 * (61 - 58 * latTan2 + latTan4 + 600 * c - 330 * E_P2)));
  if (lat < 0.0)
    northing += 10000000.0;

  return {easting, northing, zoneNumber, zoneLetter};
}

// Generate UTM string from UTM point parameters.
std::string UTMtoStr(UTMPoint const & point)
{
  // Easting and northing are rounded to nearest integer. Because of that in some cases
  // last 5 digits of UTM and MGRS coordinates could differ (inaccuracy is no more then 1 meter).
  // Some UTM converters truncate easting and northing instead of rounding. Consider this option.
  return std::to_string(point.zoneNumber) + point.zoneLetter + ' ' +
         std::to_string(static_cast<int>(std::round(point.easting))) + ' ' +
         std::to_string(static_cast<int>(std::round(point.northing)));
}

// Build 2 chars string with MGRS 100k designator.
std::string Get100kId(double easting, double northing, int zoneNumber)
{
  int const set = zoneNumber % kSetOriginColumnLetters.size();
  int const setColumn = easting / 100000;
  int const setRow = static_cast<int>(northing / 100000) % 20;

  int const colOrigin = kSetOriginColumnLetters[set];
  int const rowOrigin = kSetOriginRowLetters[set];

  int colInt = colOrigin + setColumn - 1;
  int rowInt = rowOrigin + setRow;
  bool rollover = false;

  if (colInt > 'Z')
  {
    colInt = colInt - 'Z' + 'A' - 1;
    rollover = true;
  }

  if (colInt == 'I' || (colOrigin < 'I' && colInt > 'I') || ((colInt > 'I' || colOrigin < 'I') && rollover))
    colInt++;
  if (colInt == 'O' || (colOrigin < 'O' && colInt > 'O') || ((colInt > 'O' || colOrigin < 'O') && rollover))
  {
    colInt++;
    if (colInt == 'I')
      colInt++;
  }

  if (colInt > 'Z')
    colInt = colInt - 'Z' + 'A' - 1;

  if (rowInt > 'V')
  {
    rowInt = rowInt - 'V' + 'A' - 1;
    rollover = true;
  }
  else
    rollover = false;

  if (rowInt == 'I' || (rowOrigin < 'I' && rowInt > 'I') || ((rowInt > 'I' || rowOrigin < 'I') && rollover)) {
    rowInt++;
  }

  if (rowInt == 'O' || (rowOrigin < 'O' && rowInt > 'O') || ((rowInt > 'O' || rowOrigin < 'O') && rollover))
  {
    rowInt++;
    if (rowInt == 'I')
      rowInt++;
  }

  if (rowInt > 'V')
    rowInt = rowInt - 'V' + 'A' - 1;

  return {static_cast<char>(colInt), static_cast<char>(rowInt)};
}

// Convert UTM point parameters to MGRS parameters. Additional 2 char code is deducted.
// Easting and northing parameters are reduced to 5 digits.
std::string UTMtoMgrsStr(UTMPoint const & point, int precision)
{
  if (point.zoneLetter == 'Z')
    return "Latitude limit exceeded";
  auto const eastingStr = strings::to_string_width(static_cast<long>(point.easting), precision + 1);
  auto northingStr = strings::to_string_width(static_cast<long>(point.northing), precision + 1);

  if (northingStr.size() > 6)
    northingStr = northingStr.substr(northingStr.size() - 6);

  return strings::to_string_width(point.zoneNumber, 2) + point.zoneLetter + ' ' +
         Get100kId(point.easting, point.northing, point.zoneNumber) + ' ' +
         eastingStr.substr(1, precision) + ' ' +
         northingStr.substr(1, precision);
}
}  // namespace

// Convert UTM parameters to lat,lon for WSG 84 ellipsoid.
// If UTM parameters are valid lat and lon references are used to output calculated coordinates.
// Otherwise function returns empty optional.
std::optional<ms::LatLon> UTMtoLatLon(int easting, int northing, int zoneNumber, char zoneLetter)
{
  if (zoneNumber < 1 || zoneNumber > 60)
    return {};

  if (easting < 100000 || easting >= 1000000)
    return {};

  if (northing < 0 || northing > 10000000)
    return {};

  if (zoneLetter < 'C' || zoneLetter > 'X' || zoneLetter == 'I' || zoneLetter == 'O')
    return {};

  bool const northern = (zoneLetter >= 'N');
  double const x = easting - 500000.0;
  double y = northing;

  if (!northern)
    y -= 10000000.0;

  double const m = y / K0;
  double const mu = m / (R * M1);

  double const p_rad = (mu +
                        P2 * sin(2.0 * mu) +
                        P3 * sin(4.0 * mu) +
                        P4 * sin(6.0 * mu) +
                        P5 * sin(8.0 * mu));

  double const p_sin = sin(p_rad);
  double const p_sin2 = p_sin * p_sin;

  double const p_cos = cos(p_rad);

  double const p_tan = p_sin / p_cos;
  double const p_tan2 = p_tan * p_tan;
  double const p_tan4 = p_tan2 * p_tan2;

  double const ep_sin = 1 - E * p_sin2;
  double const ep_sin_sqrt = sqrt(1 - E * p_sin2);

  double const n = R / ep_sin_sqrt;
  double const r = (1 - E) / ep_sin;

  double const c = E_P2 * p_cos * p_cos;
  double const c2 = c * c;

  double const d = x / (n * K0);
  double const d2 = d * d;
  double const d3 = d2 * d;
  double const d4 = d3 * d;
  double const d5 = d4 * d;
  double const d6 = d5 * d;

  double const latitude = (p_rad - (p_tan / r) *
                          (d2 / 2.0 -
                           d4 / 24.0 * (5.0 + 3.0 * p_tan2 + 10.0 * c - 4.0 * c2 - 9.0 * E_P2)) +
                           d6 / 720.0 * (61.0 + 90.0 * p_tan2 + 298.0 * c + 45.0 * p_tan4 - 252.0 * E_P2 - 3.0 * c2));

  double longitude = (d -
                      d3 / 6.0 * (1.0 + 2.0 * p_tan2 + c) +
                      d5 / 120.0 * (5.0 - 2.0 * c + 28.0 * p_tan2 - 3.0 * c2 + 8.0 * E_P2 + 24.0 * p_tan4)) / p_cos;

  longitude = NormalizeAngle(longitude + math::DegToRad(static_cast<double>(ZoneNumberToCentralLon(zoneNumber))));

  return ms::LatLon(math::RadToDeg(latitude), math::RadToDeg(longitude));
}

namespace
{
// Given the first letter from a two-letter MGRS 100k zone, and given the
// MGRS table set for the zone number, figure out the easting value that
// should be added to the other, secondary easting value.
int SquareCharToEasting(char e, size_t set)
{
  ASSERT_LESS(set, kSetOriginColumnLetters.size(), ());
  int curCol = kSetOriginColumnLetters[set];
  int eastingValue = 100000;
  bool rewindMarker = false;

  while (curCol != e)
  {
    curCol++;
    if (curCol == 'I')
      curCol++;
    if (curCol == 'O')
      curCol++;
    if (curCol > 'Z')
    {
      if (rewindMarker)
        return kInvalidEastingNorthing;
      curCol = 'A';
      rewindMarker = true;
    }
    eastingValue += 100000;
  }

  return eastingValue;
}


/* Given the second letter from a two-letter MGRS 100k zone, and given the
 * MGRS table set for the zone number, figure out the northing value that
 * should be added to the other, secondary northing value. You have to
 * remember that Northings are determined from the equator, and the vertical
 * cycle of letters mean a 2000000 additional northing meters. This happens
 * approx. every 18 degrees of latitude. This method does *NOT* count any
 * additional northings. You have to figure out how many 2000000 meters need
 * to be added for the zone letter of the MGRS coordinate. */
int SquareCharToNorthing(char n, int set)
{
  if (n > 'V')
    return kInvalidEastingNorthing;

  int curRow = kSetOriginRowLetters[set];
  int northingValue = 0;
  bool rewindMarker = false;

  while (curRow != n)
  {
    curRow++;
    if (curRow == 'I')
      curRow++;
    if (curRow == 'O')
      curRow++;
    if (curRow > 'V')
    {
      if (rewindMarker) // Making sure that this loop ends even if n has invalid value.
        return kInvalidEastingNorthing;
      curRow = 'A';
      rewindMarker = true;
    }
    northingValue += 100000;
  }

  return northingValue;
}


// Get minimum northing value of a MGRS zone.
int ZoneToMinNorthing(char zoneLetter)
{
  switch (zoneLetter)
  {
  case 'C': return 1100000;
  case 'D': return 2000000;
  case 'E': return 2800000;
  case 'F': return 3700000;
  case 'G': return 4600000;
  case 'H': return 5500000;
  case 'J': return 6400000;
  case 'K': return 7300000;
  case 'L': return 8200000;
  case 'M': return 9100000;
  case 'N': return 0;
  case 'P': return 800000;
  case 'Q': return 1700000;
  case 'R': return 2600000;
  case 'S': return 3500000;
  case 'T': return 4400000;
  case 'U': return 5300000;
  case 'V': return 6200000;
  case 'W': return 7000000;
  case 'X': return 7900000;
  default: return kInvalidEastingNorthing;
  }
}
}  // namespace

// Convert MGRS parameters to UTM parameters and then use UTM to lat,lon conversion.
std::optional<ms::LatLon> MGRStoLatLon(int easting, int northing, int zoneCode, char zoneLetter, char squareCode[2])
{
  // Convert easting and northing according to zone_code and square_code
  if (zoneCode < 1 || zoneCode > 60)
    return {};

  if (zoneLetter <= 'B' || zoneLetter >= 'Y' || zoneLetter == 'I' || zoneLetter == 'O')
    return {};

  int const set = zoneCode % kSetOriginColumnLetters.size();

  auto const char1 = squareCode[0];
  auto const char2 = squareCode[1];

  if (char1 < 'A' || char2 < 'A' || char1 > 'Z' || char2 > 'Z' || char1 == 'I' || char2 == 'I' || char1 == 'O' || char2 == 'O')
    return {};

  int const east100k = SquareCharToEasting(char1, set);
  if (east100k == kInvalidEastingNorthing)
    return {};

  int north100k = SquareCharToNorthing(char2, set);
  if (north100k == kInvalidEastingNorthing)
    return {};

  int const minNorthing = ZoneToMinNorthing(zoneLetter);
  if (minNorthing == kInvalidEastingNorthing)
    return {};

  while (north100k < minNorthing)
    north100k += 2000000;

  easting  += east100k;
  northing += north100k;

  return UTMtoLatLon(easting, northing, zoneCode, zoneLetter);
}

// Convert lat,lon for WGS84 ellipsoid to MGRS string.
std::string FormatMGRS(double lat, double lon, int precision)
{
  if (precision > 5)
    precision = 5;
  else if (precision < 1)
    precision = 1;

  if (lat <= -80 || lat > 84)
    return {}; // Latitude limit exceeded.
  if (lon <= -180 || lon > 180)
    return {}; // Longitude limit exceeded.

  UTMPoint mgrsp = LatLonToUtm(lat, lon);

  // Need to set the right letter for the latitude.
  auto const zoneLetter = LatitudeToZoneLetter(lat);
  ASSERT(zoneLetter, (lat));
  mgrsp.zoneLetter = zoneLetter;
  return UTMtoMgrsStr(mgrsp, precision);
}

// Convert lat,lon for WGS84 ellipsoid to UTM string.
std::string FormatUTM(double lat, double lon)
{
  if (lat <= -80 || lat > 84)
    return {}; // Latitude limit exceeded.
  if (lon <= -180 || lon > 180)
    return {}; // Longitude limit exceeded.

  return UTMtoStr(LatLonToUtm(lat, lon));
}

}  // namespace utm_mgrs_utils
