#include "search/utm_mgrs_coords_match.hpp"

#include "base/string_utils.hpp"

#include "platform/utm_mgrs_utils.hpp"

#include <string>

#include "base/math.hpp"
#include "base/string_utils.hpp"

namespace search
{
static std::string_view constexpr kSpaceChars = " \t\r\n";

// Matches 2 digits zone code. Returns end position of matched chars or string::npos if no match.
static size_t MatchZoneCode(std::string const & query, int & zoneCode)
{
  auto const pos = query.find_first_not_of(kSpaceChars);
  if (pos == std::string::npos || query.size() < pos + 2)
    return std::string::npos;

  char const dig1 = query[pos];
  char const dig2 = query[pos + 1];
  if (dig1 > '9' || dig1 < '0' || dig2 > '9' || dig2 < '0')
    return std::string::npos;

  zoneCode = (dig1 - '0') * 10 + (dig2 - '0');
  return pos + 2;
}


// Matches zone letter ignoring spaces. Returns end position of matched chars or string::npos if no match.
size_t MatchZoneLetter(std::string const & query, char & zoneLetter, size_t startPos)
{
  auto const pos = query.find_first_not_of(kSpaceChars, startPos);
  if (pos == std::string::npos || query.size() == pos)
    return std::string::npos;

  char const l = query[pos];
  if (l < 'A' || l > 'Z')
    return std::string::npos;

  zoneLetter = l;
  return pos + 1;
}

// Matches long number ignoring spaces. Returns end position of matched chars or string::npos if no match.
size_t MatchInt(std::string const & query, int & value, size_t startPos)
{
  auto pos = query.find_first_not_of(kSpaceChars, startPos);
  if (pos == std::string::npos || query.size() == pos)
    return std::string::npos;

  int n = 0;
  while (pos < query.size())
  {
    char const ch = query[pos];
    if (ch >= '0' && ch <= '9') // Found digit
    {
      n = n * 10 + (ch - '0');
      pos++;
    }
    else if (kSpaceChars.find(ch) != std::string::npos) // Found space char matching end of the number
      break;
    else // Found invalid char
      return std::string::npos;
  }

  value = n;
  return pos;
}

// Parse UTM format "(\d\d)\s?(\W)\s+(\d+)\s+(\d+)" and converts it to lat,lon.
// Return true if parsed successfully or false otherwise.
// See utm_mgrs_coords_match_test.cpp for sample UTM strings
// TODO: Add support of Polar regions. E.g. "A 1492875 2040624"
// TODO: Support additional formats listed here: https://www.killetsoft.de/t_0901_e.htm
std::optional<ms::LatLon> MatchUTMCoords(std::string const & query)
{
  int  easting, northing;
  int  zoneCode;
  char zoneLetter;

  size_t pos = MatchZoneCode(query, zoneCode);
  if (pos == std::string::npos)
    return {};

  pos = MatchZoneLetter(query, zoneLetter, pos);
  if (pos == std::string::npos)
    return {};

  pos = MatchInt(query, easting, pos);
  if (pos == std::string::npos)
    return {};

  pos = MatchInt(query, northing, pos);
  if (pos == std::string::npos)
    return {};

  return utm_mgrs_utils::UTMtoLatLon(easting, northing, zoneCode, zoneLetter);
}

// Parse MGRS format "(\d\d\W)\s*(\W\W)\s*(\d+)\s*(\d+)" and converts it to lat,lon.
// Returns true if parsed successfully or false otherwise.
// See utm_mgrs_coords_match_test.cpp for sample MGRS strings
// TODO: Add support of Polar regions. E.g. "A SN 92875 40624"
std::optional<ms::LatLon> MatchMGRSCoords(std::string const & query)
{
  int zoneCode;
  char zoneLetter;
  char squareCode[2];
  std::string eastingStr;
  std::string northingStr;
  int32_t easting;
  int32_t northing;

  strings::SimpleTokenizer it(query, " \t\r");
  if (!it)
    return {};

  auto token = std::string(*it);
  // Parse 2 digit zone code and 1 char zone letter
  if (token.size() >= 3)
  {
    char dig1 = token[0];
    char dig2 = token[1];
    if (dig1 < '0' || dig1 > '9' || dig2 < '0' || dig2 > '9')
      return {};

    zoneCode = (dig1 - '0') * 10 + (dig2 - '0');
    if (zoneCode < 1 || zoneCode > 60)
      return {};

    zoneLetter = token[2];
    token = token.substr(3);
  }
  else
    return {};

  // Read next token if needed.
  if (token.size() == 0)
  {
    ++it;
    if (!it)
      return {};
    token = std::string(*it);
  }

  // Parse 2 chars zone code.
  if (token.size() >= 2)
  {
    squareCode[0] = token[0];
    squareCode[1] = token[1];
    token = token.substr(2);
  }

  // Read next token if needed.
  if (token.size() == 0)
  {
    ++it;
    if (!it)
      return {};
    token = std::string(*it);
  }

  // Parse easting and norhing.
  eastingStr = token;

  // Get next token if available.
  ++it;
  if (it)
    northingStr = std::string(*it);

  // Convert eastingStr & northingStr to numbers.
  if (northingStr.empty())
  {
    // eastingStr contains both easting and northing. Let's split
    if (eastingStr.size() % 2 != 0)
      return {};

    size_t const eastingSize = eastingStr.size() / 2;
    northingStr = eastingStr.substr(eastingSize);
    eastingStr = eastingStr.substr(0, eastingSize);
  }

  if (eastingStr.size() != northingStr.size() || eastingStr.size() > 5 || northingStr.size() > 5)
    return {};

  if (!strings::to_int32(eastingStr, easting))
    return {};
  if (eastingStr.size() < 5)
  {
    uint64_t const decShift = 5 - eastingStr.size();
    easting *= math::PowUint(10, decShift);
  }

  if (!strings::to_int32(northingStr, northing))
    return {};
  if (northingStr.size() < 5)
  {
    uint64_t const decShift = 5 - northingStr.size();
    northing *= math::PowUint(10, decShift);
  }

  return utm_mgrs_utils::MGRStoLatLon(easting, northing, zoneCode, zoneLetter, squareCode);
}

}
