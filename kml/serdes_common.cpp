#include "base/string_utils.hpp"
#include "kml/serdes_common.hpp"
#include "geometry/mercator.hpp"
#include <sstream>

namespace kml
{

std::string PointToString(m2::PointD const & org, char const separator)
{
  double const lon = mercator::XToLon(org.x);
  double const lat = mercator::YToLat(org.y);

  std::ostringstream ss;
  ss.precision(8);

  ss << lon << separator << lat;
  return ss.str();
}

std::string PointToLineString(geometry::PointWithAltitude const & pt)
{
  char constexpr kSeparator = ',';
  if (pt.GetAltitude() != geometry::kInvalidAltitude)
    return PointToString(pt.GetPoint(), kSeparator) + kSeparator + strings::to_string(pt.GetAltitude());
  return PointToString(pt.GetPoint(), kSeparator);
}

std::string PointToGxString(geometry::PointWithAltitude const & pt)
{
  char constexpr kSeparator = ' ';
  if (pt.GetAltitude() != geometry::kInvalidAltitude)
    return PointToString(pt.GetPoint(), kSeparator) + kSeparator + strings::to_string(pt.GetAltitude());
  return PointToString(pt.GetPoint(), kSeparator);
}

void SaveStringWithCDATA(Writer & writer, std::string s)
{
  if (s.empty())
    return;

  // Expat loads XML 1.0 only. Sometimes users copy and paste bookmark descriptions or even names from the web.
  // Rarely, in these copy-pasted texts, there are invalid XML1.0 symbols.
  // See https://en.wikipedia.org/wiki/Valid_characters_in_XML
  // A robust solution requires parsing invalid XML on loading (then users can restore "bad" XML files), see
  // https://github.com/organicmaps/organicmaps/issues/3837
  // When a robust solution is implemented, this workaround can be removed for better performance/battery.
  //
  // This solution is a simple ASCII-range check that does not check symbols from other unicode ranges
  // (they will require a more complex and slower approach of converting UTF-8 string to unicode first).
  // It should be enough for many cases, according to user reports and wrong characters in their data.
  s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c)
                         {
                           if (c >= 0x20 || c == 0x09 || c == 0x0a || c == 0x0d)
                             return false;
                           return true;
                         }), s.end());

  if (s.empty())
    return;

  // According to kml/xml spec, we need to escape special symbols with CDATA.
  if (s.find_first_of("<&") != std::string::npos)
    writer << "<![CDATA[" << s << "]]>";
  else
    writer << s;
}

std::string const * GetDefaultLanguage(LocalizableString const & lstr)
{
  auto const find = lstr.find(kDefaultLang);
  if (find != lstr.end()) {
    return &find->second;
  }
  return nullptr;
}

}  // namespace kml
