#include "kml/serdes_gpx.hpp"
#include "kml/serdes_common.hpp"

#include "coding/hex.hpp"
#include "coding/point_coding.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/string_utils.hpp"


namespace kml
{
namespace gpx
{
namespace
{
std::string_view constexpr kTrk = "trk";
std::string_view constexpr kTrkSeg = "trkseg";
std::string_view constexpr kRte = "rte";
std::string_view constexpr kTrkPt = "trkpt";
std::string_view constexpr kWpt = "wpt";
std::string_view constexpr kRtePt = "rtept";
std::string_view constexpr kName = "name";
std::string_view constexpr kColor = "color";
std::string_view constexpr kOsmandColor = "osmand:color";
std::string_view constexpr kGpx = "gpx";
std::string_view constexpr kGarminColor = "gpxx:DisplayColor";
std::string_view constexpr kDesc = "desc";
std::string_view constexpr kMetadata = "metadata";
std::string_view constexpr kEle = "ele";
std::string_view constexpr kCmt = "cmt";
std::string_view constexpr kTime = "time";

std::string_view constexpr kGpxHeader =
    "<?xml version=\"1.0\"?>\n"
    "<gpx xmlns=\"http://www.topografix.com/GPX/1/1\" version=\"1.1\">\n";

std::string_view constexpr kGpxFooter = "</gpx>";

int constexpr kInvalidColor = 0;
}  // namespace

GpxParser::GpxParser(FileData & data)
: m_data{data}
, m_categoryData{&m_data.m_categoryData}
, m_globalColor{kInvalidColor}
{
  ResetPoint();
}

void GpxParser::ResetPoint()
{
  m_name.clear();
  m_description.clear();
  m_comment.clear();
  m_org = {};
  m_predefinedColor = PredefinedColor::None;
  m_color = kInvalidColor;
  m_customName.clear();
  m_geometry.Clear();
  m_geometryType = GEOMETRY_TYPE_UNKNOWN;
  m_lat = 0.;
  m_lon = 0.;
  m_altitude = geometry::kInvalidAltitude;
  m_timestamp = base::INVALID_TIME_STAMP;
}

bool GpxParser::MakeValid()
{
  if (GEOMETRY_TYPE_POINT == m_geometryType)
  {
    m2::PointD const & pt = m_org.GetPoint();
    if (mercator::ValidX(pt.x) && mercator::ValidY(pt.y))
    {
      // Set default name.
      if (m_name.empty())
        m_name = kml::PointToLineString(m_org);

      // Set default pin.
      if (m_predefinedColor == PredefinedColor::None)
        m_predefinedColor = PredefinedColor::Red;

      return true;
    }
    return false;
  }
  else if (GEOMETRY_TYPE_LINE == m_geometryType)
  {
    return m_geometry.IsValid();
  }

  return false;
}

bool GpxParser::Push(std::string tag)
{
  if (tag == gpx::kWpt)
    m_geometryType = GEOMETRY_TYPE_POINT;
  else if (tag == gpx::kTrkPt || tag == gpx::kRtePt)
    m_geometryType = GEOMETRY_TYPE_LINE;

  m_tags.emplace_back(std::move(tag));

  return true;
}

bool GpxParser::IsValidCoordinatesPosition() const
{
  std::string const & lastTag = GetTagFromEnd(0);
  return lastTag == gpx::kWpt
      || (lastTag == gpx::kTrkPt && GetTagFromEnd(1) == gpx::kTrkSeg)
      || (lastTag == gpx::kRtePt && GetTagFromEnd(1) == gpx::kRte);
}

void GpxParser::AddAttr(std::string_view attr, char const * value)
{
  if (IsValidCoordinatesPosition())
  {
    if (attr == "lat" && !strings::to_double(value, m_lat))
      LOG(LERROR, ("Bad gpx latitude:", value));
    else if (attr == "lon" && !strings::to_double(value, m_lon))
      LOG(LERROR, ("Bad gpx longitude:", value));
  }
}

std::string const & GpxParser::GetTagFromEnd(size_t n) const
{
  ASSERT_LESS(n, m_tags.size(), ());
  return m_tags[m_tags.size() - n - 1];
}

void GpxParser::ParseColor(std::string const & value)
{
  auto const colorBytes = FromHex(value);
  if (colorBytes.size() != 3)
  {
    LOG(LWARNING, ("Invalid color value", value));
    return;
  }
  m_color = kml::ToRGBA(colorBytes[0], colorBytes[1], colorBytes[2], (char)255);
}

// https://osmand.net/docs/technical/osmand-file-formats/osmand-gpx/. Supported colors: #AARRGGBB/#RRGGBB/AARRGGBB/RRGGBB
void GpxParser::ParseOsmandColor(std::string const & value)
{
  if (value.empty())
  {
    LOG(LWARNING, ("Empty color value"));
    return;
  }
  std::string_view colorStr = value;
  if (colorStr.at(0) == '#')
    colorStr = colorStr.substr(1, colorStr.size() - 1);
  auto const colorBytes = FromHex(colorStr);
  uint32_t color;
  switch (colorBytes.size())
  {
    case 3:
      color = kml::ToRGBA(colorBytes[0], colorBytes[1], colorBytes[2], (char)255);
      break;
    case 4:
      color = kml::ToRGBA(colorBytes[1], colorBytes[2], colorBytes[3], colorBytes[0]);
      break;
    default:
      LOG(LWARNING, ("Invalid color value", value));
      return;
  }
  if (m_tags.size() > 2 && GetTagFromEnd(2) == gpx::kGpx)
  {
    m_globalColor = color;
    for (auto & track : m_data.m_tracksData)
      for (auto & layer : track.m_layers)
        layer.m_color.m_rgba = m_globalColor;
  }
  else
  {
    m_color = color;
  }
}

// Garmin extensions spec: https://www8.garmin.com/xmlschemas/GpxExtensionsv3.xsd
// Color mapping: https://help.locusmap.eu/topic/extend-garmin-gpx-compatibilty
void GpxParser::ParseGarminColor(std::string const & v)
{
  static std::unordered_map<std::string, std::string> const kGarminToHex = {
      {"Black", "000000"},
      {"DarkRed", "8b0000"},
      {"DarkGreen", "006400"},
      {"DarkYellow", "b5b820"},
      {"DarkBlue", "00008b"},
      {"DarkMagenta", "8b008b"},
      {"DarkCyan", "008b8b"},
      {"LightGray", "cccccc"},
      {"DarkGray", "444444"},
      {"Red", "ff0000"},
      {"Green", "00ff00"},
      {"Yellow", "ffff00"},
      {"Blue", "0000ff"},
      {"Magenta", "ff00ff"},
      {"Cyan", "00ffff"},
      {"White", "ffffff"},
      {"Transparent", "ff0000"}
  };
  auto const it = kGarminToHex.find(v);
  if (it != kGarminToHex.end())
  {
    return ParseColor(it->second);
  }
  else
  {
    LOG(LWARNING, ("Unsupported color value", v));
    return ParseColor("ff0000");  // default to red
  }
}

void GpxParser::CheckAndCorrectTimestamps()
{
  ASSERT_EQUAL(m_line.size(), m_timestamps.size(), ());

  size_t const numInvalid = std::count(m_timestamps.begin(), m_timestamps.end(), base::INVALID_TIME_STAMP);
  if (numInvalid * 2 > m_timestamps.size())
  {
    // >50% invalid
    m_timestamps.clear();
  }
  else if (numInvalid > 0)
  {
    // Find INVALID_TIME_STAMP ranges and interpolate them.
    for (size_t i = 0; i < m_timestamps.size();)
    {
      if (m_timestamps[i] == base::INVALID_TIME_STAMP)
      {
        size_t j = i + 1;
        for (; j < m_timestamps.size(); ++j)
        {
          if (m_timestamps[j] != base::INVALID_TIME_STAMP)
          {
            if (i == 0)
            {
              // Beginning range assign to the first valid timestamp.
              while (i < j)
                m_timestamps[i++] = m_timestamps[j];
            }
            else
            {
              // naive interpolation
              auto const last = m_timestamps[i-1];
              auto count = j - i + 1;
              double const delta = (m_timestamps[j] - last) / double(count);
              for (size_t k = 1; k < count; ++k)
                m_timestamps[i++] = last + k * delta;
            }
            break;
          }
        }

        if (j == m_timestamps.size())
        {
          // Ending range assign to the last valid timestamp.
          ASSERT(i > 0, ());
          auto const last = m_timestamps[i-1];
          while (i < j)
            m_timestamps[i++] = last;
        }

        i = j + 1;
      }
      else
        ++i;
    }
  }
}

void GpxParser::Pop(std::string_view tag)
{
  ASSERT_EQUAL(m_tags.back(), tag, ());

  if (tag == gpx::kTrkPt || tag == gpx::kRtePt)
  {
    m2::PointD const p = mercator::FromLatLon(m_lat, m_lon);
    if (m_line.empty() || !AlmostEqualAbs(m_line.back().GetPoint(), p, kMwmPointAccuracy))
    {
      m_line.emplace_back(p, m_altitude);
      m_timestamps.emplace_back(m_timestamp);
    }
    m_altitude = geometry::kInvalidAltitude;
    m_timestamp = base::INVALID_TIME_STAMP;
  }
  else if (tag == gpx::kTrkSeg || tag == gpx::kRte)
  {
    if (m_line.size() > 1)
    {
      CheckAndCorrectTimestamps();

      m_geometry.m_lines.push_back(std::move(m_line));
      m_geometry.m_timestamps.push_back(std::move(m_timestamps));
    }

    // Clear segment (it may be incomplete).
    m_line.clear();
    m_timestamps.clear();
  }
  else if (tag == gpx::kWpt)
  {
    m_org.SetPoint(mercator::FromLatLon(m_lat, m_lon));
    m_org.SetAltitude(m_altitude);
    m_altitude = geometry::kInvalidAltitude;
  }

  if (tag == gpx::kRte || tag == gpx::kTrk || tag == gpx::kWpt)
  {
    if (MakeValid())
    {
      if (GEOMETRY_TYPE_POINT == m_geometryType)
      {
        BookmarkData data;
        if (!m_name.empty())
          data.m_name[kDefaultLang] = std::move(m_name);
        if (!m_description.empty() || !m_comment.empty())
          data.m_description[kDefaultLang] = BuildDescription();
        data.m_color.m_predefinedColor = m_predefinedColor;
        data.m_color.m_rgba = m_color;
        data.m_point = m_org;
        if (!m_customName.empty())
          data.m_customName[kDefaultLang] = std::move(m_customName);
        else if (!data.m_name.empty())
        {
          // Here we set custom name from 'name' field for KML-files exported from 3rd-party services.
          data.m_customName = data.m_name;
        }

        m_data.m_bookmarksData.push_back(std::move(data));
      }
      else if (GEOMETRY_TYPE_LINE == m_geometryType)
      {
#ifdef DEBUG
        // Default gpx parser doesn't check points and timestamps count as the kml parser does.
        for (size_t lineIndex = 0; lineIndex < m_geometry.m_lines.size(); ++lineIndex)
        {
          auto const pointsSize = m_geometry.m_lines[lineIndex].size();
          auto const timestampsSize = m_geometry.m_timestamps[lineIndex].size();
          ASSERT(!m_geometry.HasTimestampsFor(lineIndex) || pointsSize == timestampsSize, (pointsSize, timestampsSize));
        }
#endif

        TrackLayer layer;
        layer.m_lineWidth = kml::kDefaultTrackWidth;
        if (m_color != kInvalidColor)
          layer.m_color.m_rgba = m_color;
        else if (m_globalColor != kInvalidColor)
          layer.m_color.m_rgba = m_globalColor;
        else
          layer.m_color.m_rgba = kml::kDefaultTrackColor;

        TrackData data;
        if (!m_name.empty())
          data.m_name[kDefaultLang] = std::move(m_name);
        if (!m_description.empty() || !m_comment.empty())
          data.m_description[kDefaultLang] = BuildDescription();
        data.m_layers.push_back(layer);
        data.m_geometry = std::move(m_geometry);

        m_data.m_tracksData.push_back(std::move(data));
      }
    }
    ResetPoint();
  }

  if (tag == gpx::kMetadata)
  {
    /// @todo(KK): Process the <metadata><time> tag.
    m_timestamp = base::INVALID_TIME_STAMP;
  }

  m_tags.pop_back();
}

void GpxParser::CharData(std::string & value)
{
  strings::Trim(value);

  size_t const count = m_tags.size();
  if (count > 1 && !value.empty())
  {
    std::string const & currTag = m_tags[count - 1];
    std::string const & prevTag = m_tags[count - 2];

    if (currTag == gpx::kName)
      ParseName(value, prevTag);
    else if (currTag == gpx::kDesc)
      ParseDescription(value, prevTag);
    else if (currTag == gpx::kGarminColor)
      ParseGarminColor(value);
    else if (currTag == gpx::kOsmandColor)
      ParseOsmandColor(value);
    else if (currTag == gpx::kColor)
      ParseColor(value);
    else if (currTag == gpx::kEle)
      ParseAltitude(value);
    else if (currTag == gpx::kCmt)
      m_comment = value;
    else if (currTag == gpx::kTime)
      ParseTimestamp(value);
  }
}

void GpxParser::ParseDescription(std::string const & value, std::string const & prevTag)
{
  if (prevTag == kWpt)
  {
    m_description = value;
  }
  else if (prevTag == kTrk || prevTag == kRte)
  {
    m_description = value;
    if (m_categoryData->m_description[kDefaultLang].empty())
      m_categoryData->m_description[kDefaultLang] = value;
  }
  else if (prevTag == kMetadata)
  {
    m_categoryData->m_description[kDefaultLang] = value;
  }
}

void GpxParser::ParseName(std::string const & value, std::string const & prevTag)
{
  if (prevTag == kWpt)
  {
    m_name = value;
  }
  else if (prevTag == kTrk || prevTag == kRte)
  {
    m_name = value;
    if (m_categoryData->m_name[kDefaultLang].empty())
      m_categoryData->m_name[kDefaultLang] = value;
  }
  else if (prevTag == kMetadata)
  {
    m_categoryData->m_name[kDefaultLang] = value;
  }
}

void GpxParser::ParseAltitude(std::string const & value)
{
  double rawAltitude;
  if (strings::to_double(value, rawAltitude))
    m_altitude = static_cast<geometry::Altitude>(round(rawAltitude));
  else
    m_altitude = geometry::kInvalidAltitude;
}

void GpxParser::ParseTimestamp(std::string const & value)
{
  m_timestamp = base::StringToTimestamp(value);
}

std::string GpxParser::BuildDescription() const
{
  if (m_description.empty())
    return m_comment;
  else if (m_comment.empty() || m_description == m_comment)
    return m_description;
  return m_description + "\n\n" + m_comment;
}

namespace
{

std::string CoordToString(double c)
{
  std::ostringstream ss;
  ss.precision(8);
  ss << c;
  return ss.str();
}

void SaveColorToRGB(Writer & writer, uint32_t rgba)
{
  writer << NumToHex(static_cast<uint8_t>(rgba >> 24 & 0xFF))
         << NumToHex(static_cast<uint8_t>((rgba >> 16) & 0xFF))
         << NumToHex(static_cast<uint8_t>((rgba >> 8) & 0xFF));
}


void SaveCategoryData(Writer & writer, CategoryData const & categoryData)
{
  writer << "<metadata>\n";
  if (auto const name = GetDefaultLanguage(categoryData.m_name))
    writer << kIndent2 << "<name>" << *name << "</name>\n";
  if (auto const description = GetDefaultLanguage(categoryData.m_description))
  {
    writer << kIndent2 << "<desc>";
    SaveStringWithCDATA(writer, *description);
    writer << "</desc>\n";
  }
  writer << "</metadata>\n";
}

void SaveBookmarkData(Writer & writer, BookmarkData const & bookmarkData)
{
  auto const [lat, lon] = mercator::ToLatLon(bookmarkData.m_point);
  writer << "<wpt lat=\"" << CoordToString(lat) << "\" lon=\"" << CoordToString(lon) << "\">\n";
  // If user customized the default bookmark name, it's saved in m_customName.
  auto name = GetDefaultLanguage(bookmarkData.m_customName);
  if (!name)
    name = GetDefaultLanguage(bookmarkData.m_name);  // Original POI name stored when bookmark was created.
  if (name)
  {
    writer << kIndent2 << "<name>";
    SaveStringWithCDATA(writer, *name);
    writer << "</name>\n";
  }
  if (auto const description = GetDefaultLanguage(bookmarkData.m_description))
  {
    writer << kIndent2 << "<desc>";
    SaveStringWithCDATA(writer, *description);
    writer << "</desc>\n";
  }
  writer << "</wpt>\n";
}

bool TrackHasAltitudes(TrackData const & trackData)
{
  auto const & lines = trackData.m_geometry.m_lines;
  if (lines.empty() || lines.front().empty())
    return false;
  auto const altitude = lines.front().front().GetAltitude();
  return altitude != geometry::kDefaultAltitudeMeters && altitude != geometry::kInvalidAltitude;
}

uint32_t TrackColor(TrackData const & trackData)
{
  if (trackData.m_layers.empty())
    return kDefaultTrackColor;
  return trackData.m_layers.front().m_color.m_rgba;
}

void SaveTrackData(Writer & writer, TrackData const & trackData)
{
  writer << "<trk>\n";
  auto name = GetDefaultLanguage(trackData.m_name);
  if (name)
  {
    writer << kIndent2 << "<name>";
    SaveStringWithCDATA(writer, *name);
    writer << "</name>\n";
  }
  if (auto const description = GetDefaultLanguage(trackData.m_description))
  {
    writer << kIndent2 << "<desc>";
    SaveStringWithCDATA(writer, *description);
    writer << "</desc>\n";
  }
  if (auto const color = TrackColor(trackData); color != kDefaultTrackColor)
  {
    writer << kIndent2 << "<extensions>\n" << kIndent4 << "<color>";
    SaveColorToRGB(writer, color);
    writer << "</color>\n" << kIndent2 << "</extensions>\n";
  }
  bool const trackHasAltitude = TrackHasAltitudes(trackData);
  auto const & geom = trackData.m_geometry;
  for (size_t lineIndex = 0; lineIndex < geom.m_lines.size(); ++lineIndex)
  {
    auto const & line = geom.m_lines[lineIndex];
    auto const & timestampsForLine = geom.m_timestamps[lineIndex];
    auto const lineHasTimestamps = geom.HasTimestampsFor(lineIndex);

    if (lineHasTimestamps)
      CHECK_EQUAL(line.size(), timestampsForLine.size(), ());

    writer << kIndent2 << "<trkseg>\n";

    for (size_t pointIndex = 0; pointIndex < line.size(); ++pointIndex)
    {
      auto const & point = line[pointIndex];
      auto const [lat, lon] = mercator::ToLatLon(point);

      writer << kIndent4 << "<trkpt lat=\"" << CoordToString(lat) << "\" lon=\"" << CoordToString(lon) << "\">\n";

      if (trackHasAltitude)
        writer << kIndent6 << "<ele>" << CoordToString(point.GetAltitude()) << "</ele>\n";

      if (lineHasTimestamps)
        writer << kIndent6 << "<time>" << base::SecondsSinceEpochToString(timestampsForLine[pointIndex]) << "</time>\n";

      writer << kIndent4 << "</trkpt>\n";
    }
    writer << kIndent2 << "</trkseg>\n";
  }
  writer << "</trk>\n";
}
}  // namespace

void GpxWriter::Write(FileData const & fileData)
{
  m_writer << kGpxHeader;
  SaveCategoryData(m_writer, fileData.m_categoryData);
  for (auto const & bookmarkData : fileData.m_bookmarksData)
    SaveBookmarkData(m_writer, bookmarkData);
  for (auto const & trackData : fileData.m_tracksData)
    SaveTrackData(m_writer, trackData);
  m_writer << kGpxFooter;
}

}  // namespace gpx

DeserializerGpx::DeserializerGpx(FileData & fileData)
: m_fileData(fileData)
{
  m_fileData = {};
}

}  // namespace kml
