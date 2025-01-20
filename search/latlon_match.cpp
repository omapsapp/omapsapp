#include "search/latlon_match.hpp"

#include "base/macros.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>

using namespace std;

namespace
{
string const kSpaces = " \t";
string const kCharsToSkip = " \n\t,;:.()";
string const kDecimalMarks = ".,";
string const kNegative = "-";

bool IsDecimalMark(char c)
{
  return kDecimalMarks.find(c) != string::npos;
}

bool IsNegativeSymbol(char c)
{
  return kNegative.find(c) != string::npos;
}

template <typename Char>
void SkipSpaces(Char *& s)
{
  while (kSpaces.find(*s) != string::npos)
    ++s;
}

template <typename Char>
void Skip(Char *& s)
{
  while (kCharsToSkip.find(*s) != string::npos)
    ++s;
}

bool MatchDMSArray(char const * & s, char const * arr[], size_t count)
{
  for (size_t i = 0; i < count; ++i)
  {
    size_t const len = strlen(arr[i]);
    if (strncmp(s, arr[i], len) == 0)
    {
      s += len;
      return true;
    }
  }
  return false;
}

int GetDMSIndex(char const * & s)
{
  char const * arrDegree[] = { "*", "°" };
  char const * arrMinutes[] = { "\'", "’", "′" };
  char const * arrSeconds[] = { "\"", "”", "″", "\'\'", "’’", "′′" };

  if (MatchDMSArray(s, arrDegree, ARRAY_SIZE(arrDegree)))
      return 0;
  if (MatchDMSArray(s, arrSeconds, ARRAY_SIZE(arrSeconds)))
    return 2;
  if (MatchDMSArray(s, arrMinutes, ARRAY_SIZE(arrMinutes)))
    return 1;

  return -1;
}

bool SkipNSEW(char const * & s, char const * (&arrPos) [4])
{
  Skip(s);

  int ind;
  switch (*s)
  {
  case 'N': case 'n': ind = 0; break;
  case 'S': case 's': ind = 1; break;
  case 'E': case 'e': ind = 2; break;
  case 'W': case 'w': ind = 3; break;
  default: return false;
  }

  arrPos[ind] = s++;
  return true;
}

// Attempts to read a double from the start of |str|
// in one of what we assume are two most common forms
// for lat/lon: decimal digits separated either
// by a dot or by a comma, with digits on both sides
// of the separator.
// If the attempt fails, falls back to std::strtod.
double EatDouble(char const * str, char ** strEnd)
{
  bool gotDigitBeforeMark = false;
  bool gotMark = false;
  bool gotDigitAfterMark = false;
  char const * markPos = nullptr;
  char const * p = str;
  double modifier = 1.0;
  while (true)
  {
    if (IsDecimalMark(*p))
    {
      if (gotMark)
        break;
      gotMark = true;
      markPos = p;
    }
    else if (isdigit(*p))
    {
      if (gotMark)
        gotDigitAfterMark = true;
      else
        gotDigitBeforeMark = true;
    }
    else if (IsNegativeSymbol(*p))
    {
      modifier = -1.0;
    }
    else
    {
      break;
    }
    ++p;
  }

  if (gotDigitBeforeMark && gotMark && gotDigitAfterMark)
  {
    string const part1(str, markPos);
    string const part2(markPos + 1, p);
    *strEnd = const_cast<char *>(p);
    auto const x1 = atof(part1.c_str());
    auto const x2 = atof(part2.c_str());
    return x1 + x2 * modifier * pow(10.0, -static_cast<double>(part2.size()));
  }

  return strtod(str, strEnd);
}
}  // namespace

namespace search
{
bool MatchLatLonDegree(string const & query, double & lat, double & lon)
{
  // should be default initialization (0, false)
  array<pair<double, bool>, 6> v;

  int base = 0;

  // Positions of N, S, E, W symbols
  char const * arrPos[] = {nullptr, nullptr, nullptr, nullptr};
  bool arrDegreeSymbol[] = { false, false };

  char const * const startQuery = query.c_str();
  char const * s = startQuery;
  while (true)
  {
    char const * s1 = s;
    char const * s11 = s;
    if (SkipNSEW(s, arrPos))
    {
      s11 = s;
      Skip(s);
    }
    else
      SkipSpaces(s);

    if (!*s)
    {
      // End of the string - check matching.
      break;
    }

    char * s2;
    double const x = EatDouble(s, &s2);
    if (s == s2)
    {
      // invalid token
      if (s == s11)
      {
        // Return error if there are no any delimiters.
        return false;
      }
      else
      {
        // Check matching if token is delimited.
        break;
      }
    }
    else if (x < 0 && s == s1 && !(s == startQuery || kSpaces.find(*(s-1)) != string::npos))
    {
      // Skip input like "3-8"
      return false;
    }

    s = s2;
    SkipSpaces(s);

    int i = GetDMSIndex(s);
    bool degreeSymbol = true;
    if (i == -1)
    {
      // try to assign next possible value mark
      if (arrDegreeSymbol[base / 3])
      {
        if (!v[base + 1].second)
          i = 1;
        else
          i = 2;
      }
      else
      {
        i = 0;
        degreeSymbol = false;
      }
    }

    if (i == 0) // degrees
    {
      if (v[base].second)
      {
        if (base == 0)
        {
          base += 3;
        }
        else
        {
          // too many degree values
          return false;
        }
      }
      arrDegreeSymbol[base / 3] = degreeSymbol;
    }
    else  // minutes or seconds
    {
      if (x < 0.0 || x > 60.0 ||            // minutes or seconds should be in [0, 60] range
          v[base + i].second ||             // value already exists
          !v[base].second ||                // no degrees found for value
          (i == 2 && !v[base + 1].second))  // no minutes for seconds
      {
        return false;
      }
    }

    v[base + i].first = x;
    v[base + i].second = true;
  }

  if (!v[0].second || !v[3].second)
  {
    // degree should exist for both coordinates
    return false;
  }

  if ((arrPos[0] && arrPos[1]) || (arrPos[2] && arrPos[3]))
  {
    // control symbols should match only once
    return false;
  }

  // Calculate Lat, Lon with correct sign.
  lat = fabs(v[0].first) + v[1].first / 60.0 + v[2].first / 3600.0;
  if (v[0].first < 0.0) lat = -lat;

  lon = fabs(v[3].first) + v[4].first / 60.0 + v[5].first / 3600.0;
  if (v[3].first < 0.0) lon = -lon;

  if (max(arrPos[0], arrPos[1]) > max(arrPos[2], arrPos[3]))
    swap(lat, lon);

  if (arrPos[1] != nullptr)
    lat = -lat;
  if (arrPos[3] != nullptr)
    lon = -lon;

  // Valid input ranges for longitude are: [0, 360] or [-180, 180].
  // We normalize it to [-180, 180].
  if (lon < -180.0 || lon > 360.0)
    return false;

  if (lon > 180.0)
    lon -= 360.0;

  return fabs(lat) <= 90.0;
}
}  // namespace search
