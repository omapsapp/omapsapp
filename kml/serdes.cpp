#include "kml/serdes.hpp"

#include "indexer/classificator.hpp"

#include "coding/hex.hpp"
#include "coding/string_utf8_multilang.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

namespace kml
{
namespace
{
std::string_view constexpr kPlacemark = "Placemark";
std::string_view constexpr kStyle = "Style";
std::string_view constexpr kDocument = "Document";
std::string_view constexpr kStyleMap = "StyleMap";
std::string_view constexpr kStyleUrl = "styleUrl";
std::string_view constexpr kPair = "Pair";
std::string_view constexpr kExtendedData = "ExtendedData";
std::string const kCompilation = "mwm:compilation";

std::string_view const kCoordinates = "coordinates";

bool IsTrack(std::string const & s)
{
  return s == "Track" || s == "gx:Track";
}

bool IsCoord(std::string const & s)
{
  return s == "coord" || s == "gx:coord";
}

bool IsTimestamp(std::string const & s)
{
  return s == "when";
}

std::string_view constexpr kKmlHeader =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<kml xmlns=\"http://www.opengis.net/kml/2.2\" xmlns:gx=\"http://www.google.com/kml/ext/2.2\">\n"
    "<Document>\n";

std::string_view constexpr kKmlFooter =
    "</Document>\n"
    "</kml>\n";

std::string_view constexpr kExtendedDataHeader =
    "<ExtendedData xmlns:mwm=\"https://omaps.app\">\n";

std::string_view constexpr kExtendedDataFooter =
    "</ExtendedData>\n";

std::string const kCompilationFooter = "</" + kCompilation + ">\n";

PredefinedColor ExtractPlacemarkPredefinedColor(std::string const & s)
{
  if (s == "#placemark-red")
    return PredefinedColor::Red;
  if (s == "#placemark-blue")
    return PredefinedColor::Blue;
  if (s == "#placemark-purple")
    return PredefinedColor::Purple;
  if (s == "#placemark-yellow")
    return PredefinedColor::Yellow;
  if (s == "#placemark-pink")
    return PredefinedColor::Pink;
  if (s == "#placemark-brown")
    return PredefinedColor::Brown;
  if (s == "#placemark-green")
    return PredefinedColor::Green;
  if (s == "#placemark-orange")
    return PredefinedColor::Orange;
  if (s == "#placemark-deeppurple")
    return PredefinedColor::DeepPurple;
  if (s == "#placemark-lightblue")
    return PredefinedColor::LightBlue;
  if (s == "#placemark-cyan")
    return PredefinedColor::Cyan;
  if (s == "#placemark-teal")
    return PredefinedColor::Teal;
  if (s == "#placemark-lime")
    return PredefinedColor::Lime;
  if (s == "#placemark-deeporange")
    return PredefinedColor::DeepOrange;
  if (s == "#placemark-gray")
    return PredefinedColor::Gray;
  if (s == "#placemark-bluegray")
    return PredefinedColor::BlueGray;

  // Default color.
  return PredefinedColor::Red;
}

std::string GetStyleForPredefinedColor(PredefinedColor color)
{
  switch (color)
  {
  case PredefinedColor::Red: return "placemark-red";
  case PredefinedColor::Blue: return "placemark-blue";
  case PredefinedColor::Purple: return "placemark-purple";
  case PredefinedColor::Yellow: return "placemark-yellow";
  case PredefinedColor::Pink: return "placemark-pink";
  case PredefinedColor::Brown: return "placemark-brown";
  case PredefinedColor::Green: return "placemark-green";
  case PredefinedColor::Orange: return "placemark-orange";
  case PredefinedColor::DeepPurple: return "placemark-deeppurple";
  case PredefinedColor::LightBlue: return "placemark-lightblue";
  case PredefinedColor::Cyan: return "placemark-cyan";
  case PredefinedColor::Teal: return "placemark-teal";
  case PredefinedColor::Lime: return "placemark-lime";
  case PredefinedColor::DeepOrange: return "placemark-deeporange";
  case PredefinedColor::Gray: return "placemark-gray";
  case PredefinedColor::BlueGray: return "placemark-bluegray";
  case PredefinedColor::None:
  case PredefinedColor::Count:
    return {};
  }
  UNREACHABLE();
}

BookmarkIcon GetIcon(std::string const & iconName)
{
  for (size_t i = 0; i < static_cast<size_t>(BookmarkIcon::Count); ++i)
  {
    auto const icon = static_cast<BookmarkIcon>(i);
    if (iconName == ToString(icon))
      return icon;
  }
  return BookmarkIcon::None;
}

void SaveStyle(Writer & writer, std::string const & style,
               std::string_view const & indent)
{
  if (style.empty())
    return;

  writer << indent << kIndent2 << "<Style id=\"" << style << "\">\n"
         << indent << kIndent4 << "<IconStyle>\n"
         << indent << kIndent6 << "<Icon>\n"
         << indent << kIndent8 << "<href>https://omaps.app/placemarks/" << style << ".png</href>\n"
         << indent << kIndent6 << "</Icon>\n"
         << indent << kIndent4 << "</IconStyle>\n"
         << indent << kIndent2 << "</Style>\n";
}

void SaveColorToABGR(Writer & writer, uint32_t rgba)
{
  writer << NumToHex(static_cast<uint8_t>(rgba & 0xFF))
         << NumToHex(static_cast<uint8_t>((rgba >> 8) & 0xFF))
         << NumToHex(static_cast<uint8_t>((rgba >> 16) & 0xFF))
         << NumToHex(static_cast<uint8_t>((rgba >> 24) & 0xFF));
}

std::string TimestampToString(Timestamp const & timestamp)
{
  auto const ts = TimestampClock::to_time_t(timestamp);
  std::string strTimeStamp = base::TimestampToString(ts);
  if (strTimeStamp.size() != 20)
    MYTHROW(KmlWriter::WriteKmlException, ("We always generate fixed length UTC-format timestamp."));
  return strTimeStamp;
}

void SaveLocalizableString(Writer & writer, LocalizableString const & str,
                           std::string const & tagName, std::string_view const & indent)
{
  writer << indent << "<mwm:" << tagName << ">\n";
  for (auto const & s : str)
  {
    writer << indent << kIndent2 << "<mwm:lang code=\""
           << StringUtf8Multilang::GetLangByCode(s.first) << "\">";
    SaveStringWithCDATA(writer, s.second);
    writer << "</mwm:lang>\n";
  }
  writer << indent << "</mwm:" << tagName << ">\n";
}

template <class StringViewLike>
void SaveStringsArray(Writer & writer,
                      std::vector<StringViewLike> const & stringsArray,
                      std::string const & tagName, std::string_view const & indent)
{
  if (stringsArray.empty())
    return;

  writer << indent << "<mwm:" << tagName << ">\n";
  for (auto const & s : stringsArray)
  {
    writer << indent << kIndent2 << "<mwm:value>";
    // Constants from our code do not need any additional checks or escaping.
    if constexpr (std::is_same_v<StringViewLike, std::string_view>)
    {
      ASSERT_EQUAL(s.find_first_of("<&"), std::string_view::npos, ("Use std::string overload for", s));
      writer << s;
    }
    else
      SaveStringWithCDATA(writer, s);
    writer << "</mwm:value>\n";
  }
  writer << indent << "</mwm:" << tagName << ">\n";
}

void SaveStringsMap(Writer & writer,
                    std::map<std::string, std::string> const & stringsMap,
                    std::string const & tagName, std::string_view const & indent)
{
  if (stringsMap.empty())
    return;

  writer << indent << "<mwm:" << tagName << ">\n";
  for (auto const & p : stringsMap)
  {
    writer << indent << kIndent2 << "<mwm:value key=\"" << p.first << "\">";
    SaveStringWithCDATA(writer, p.second);
    writer << "</mwm:value>\n";
  }
  writer << indent << "</mwm:" << tagName << ">\n";
}

void SaveCategoryData(Writer & writer, CategoryData const & categoryData,
                      std::string const & extendedServerId,
                      std::vector<CategoryData> const * compilationData);

void SaveCategoryExtendedData(Writer & writer, CategoryData const & categoryData,
                              std::string const & extendedServerId,
                              std::vector<CategoryData> const * compilationData)
{
  if (compilationData)
  {
    writer << kIndent2 << kExtendedDataHeader;
  }
  else
  {
    std::string compilationAttributes;
    if (categoryData.m_compilationId != kInvalidCompilationId)
      compilationAttributes += " id=\"" + strings::to_string(categoryData.m_compilationId) + "\"";
    compilationAttributes += " type=\"" + DebugPrint(categoryData.m_type) + "\"";
    writer << kIndent4 << "<" << kCompilation << compilationAttributes << ">\n";
  }

  auto const & indent = compilationData ? kIndent4 : kIndent6;

  if (!extendedServerId.empty() && compilationData)
    writer << indent << "<mwm:serverId>" << extendedServerId << "</mwm:serverId>\n";

  SaveLocalizableString(writer, categoryData.m_name, "name", indent);
  SaveLocalizableString(writer, categoryData.m_annotation, "annotation", indent);
  SaveLocalizableString(writer, categoryData.m_description, "description", indent);

  if (!compilationData)
    writer << indent << "<mwm:visibility>" << (categoryData.m_visible ? "1" : "0")
           << "</mwm:visibility>\n";

  if (!categoryData.m_imageUrl.empty())
    writer << indent << "<mwm:imageUrl>" << categoryData.m_imageUrl << "</mwm:imageUrl>\n";

  if (!categoryData.m_authorId.empty() || !categoryData.m_authorName.empty())
  {
    writer << indent << "<mwm:author id=\"" << categoryData.m_authorId << "\">";
    SaveStringWithCDATA(writer, categoryData.m_authorName);
    writer << "</mwm:author>\n";
  }

  if (categoryData.m_lastModified != Timestamp())
  {
    writer << indent << "<mwm:lastModified>" << TimestampToString(categoryData.m_lastModified)
           << "</mwm:lastModified>\n";
  }

  double constexpr kEps = 1e-5;
  if (fabs(categoryData.m_rating) > kEps)
  {
    writer << indent << "<mwm:rating>" << strings::to_string(categoryData.m_rating)
           << "</mwm:rating>\n";
  }

  if (categoryData.m_reviewsNumber > 0)
  {
    writer << indent << "<mwm:reviewsNumber>" << strings::to_string(categoryData.m_reviewsNumber)
           << "</mwm:reviewsNumber>\n";
  }

  writer << indent << "<mwm:accessRules>" << DebugPrint(categoryData.m_accessRules)
         << "</mwm:accessRules>\n";

  SaveStringsArray(writer, categoryData.m_tags, "tags", indent);

  SaveStringsArray(writer, categoryData.m_toponyms, "toponyms", indent);

  std::vector<std::string_view> languageCodes;
  languageCodes.reserve(categoryData.m_languageCodes.size());
  for (auto const & lang : categoryData.m_languageCodes)
    if (auto const str = StringUtf8Multilang::GetLangByCode(lang); !str.empty())
      languageCodes.push_back(str);

  SaveStringsArray(writer, languageCodes, "languageCodes", indent);

  SaveStringsMap(writer, categoryData.m_properties, "properties", indent);

  if (compilationData)
  {
    for (auto const & compilationDatum : *compilationData)
      SaveCategoryData(writer, compilationDatum, {} /* extendedServerId */,
                       nullptr /* compilationData */);
  }

  if (compilationData)
    writer << kIndent2 << kExtendedDataFooter;
  else
    writer << kIndent4 << kCompilationFooter;
}

void SaveCategoryData(Writer & writer, CategoryData const & categoryData,
                      std::string const & extendedServerId,
                      std::vector<CategoryData> const * compilationData)
{
  if (compilationData)
  {
    for (uint8_t i = 0; i < base::Underlying(PredefinedColor::Count); ++i)
      SaveStyle(writer, GetStyleForPredefinedColor(static_cast<PredefinedColor>(i)), kIndent0);

    // Use CDATA if we have special symbols in the name.
    if (auto name = GetDefaultLanguage(categoryData.m_name))
    {
      writer << kIndent2 << "<name>";
      SaveStringWithCDATA(writer, *name);
      writer << "</name>\n";
    }

    if (auto const description = GetDefaultLanguage(categoryData.m_description))
    {
      writer << kIndent2 << "<description>";
      SaveStringWithCDATA(writer, *description);
      writer << "</description>\n";
    }

    writer << kIndent2 << "<visibility>" << (categoryData.m_visible ? "1" : "0") << "</visibility>\n";
  }

  SaveCategoryExtendedData(writer, categoryData, extendedServerId, compilationData);
}

void SaveBookmarkExtendedData(Writer & writer, BookmarkData const & bookmarkData)
{
  writer << kIndent4 << kExtendedDataHeader;
  if (!bookmarkData.m_name.empty())
    SaveLocalizableString(writer, bookmarkData.m_name, "name", kIndent6);

  if (!bookmarkData.m_description.empty())
    SaveLocalizableString(writer, bookmarkData.m_description, "description", kIndent6);

  if (!bookmarkData.m_featureTypes.empty())
  {
    std::vector<std::string> types;
    types.reserve(bookmarkData.m_featureTypes.size());
    auto const & c = classif();
    if (!c.HasTypesMapping())
      MYTHROW(SerializerKml::SerializeException, ("Types mapping is not loaded."));
    for (auto const & t : bookmarkData.m_featureTypes)
      types.push_back(c.GetReadableObjectName(c.GetTypeForIndex(t)));

    SaveStringsArray(writer, types, "featureTypes", kIndent6);
  }

  if (!bookmarkData.m_customName.empty())
    SaveLocalizableString(writer, bookmarkData.m_customName, "customName", kIndent6);

  if (bookmarkData.m_viewportScale != 0)
  {
    auto const scale = strings::to_string(static_cast<double>(bookmarkData.m_viewportScale));
    writer << kIndent6 << "<mwm:scale>" << scale << "</mwm:scale>\n";
  }

  if (bookmarkData.m_icon != BookmarkIcon::None)
    writer << kIndent6 << "<mwm:icon>" << ToString(bookmarkData.m_icon) << "</mwm:icon>\n";

  if (!bookmarkData.m_boundTracks.empty())
  {
    std::vector<std::string> boundTracks;
    boundTracks.reserve(bookmarkData.m_boundTracks.size());
    for (auto const & t : bookmarkData.m_boundTracks)
      boundTracks.push_back(strings::to_string(static_cast<uint32_t>(t)));
    SaveStringsArray(writer, boundTracks, "boundTracks", kIndent6);
  }

  writer << kIndent6 << "<mwm:visibility>" << (bookmarkData.m_visible ? "1" : "0") << "</mwm:visibility>\n";

  if (!bookmarkData.m_nearestToponym.empty())
  {
    writer << kIndent6 << "<mwm:nearestToponym>";
    SaveStringWithCDATA(writer, bookmarkData.m_nearestToponym);
    writer << "</mwm:nearestToponym>\n";
  }

  if (bookmarkData.m_minZoom > 1)
  {
    writer << kIndent6 << "<mwm:minZoom>" << strings::to_string(bookmarkData.m_minZoom)
           << "</mwm:minZoom>\n";
  }

  SaveStringsMap(writer, bookmarkData.m_properties, "properties", kIndent6);

  if (!bookmarkData.m_compilations.empty())
  {
    writer << kIndent6 << "<mwm:compilations>";
    writer << strings::to_string(bookmarkData.m_compilations.front());
    for (size_t c = 1; c < bookmarkData.m_compilations.size(); ++c)
      writer << "," << strings::to_string(bookmarkData.m_compilations[c]);
    writer << "</mwm:compilations>\n";
  }

  writer << kIndent4 << kExtendedDataFooter;
}

void SaveBookmarkData(Writer & writer, BookmarkData const & bookmarkData)
{
  writer << kIndent2 << "<Placemark>\n";
  writer << kIndent4 << "<name>";
  auto const defaultLang = StringUtf8Multilang::GetLangByCode(kDefaultLangCode);
  SaveStringWithCDATA(writer, GetPreferredBookmarkName(bookmarkData, defaultLang));
  writer << "</name>\n";

  if (auto const description = GetDefaultLanguage(bookmarkData.m_description))
  {
    writer << kIndent4 << "<description>";
    SaveStringWithCDATA(writer, *description);
    writer << "</description>\n";
  }

  if (bookmarkData.m_timestamp != Timestamp())
  {
    writer << kIndent4 << "<TimeStamp><when>" << TimestampToString(bookmarkData.m_timestamp)
           << "</when></TimeStamp>\n";
  }

  auto const style = GetStyleForPredefinedColor(bookmarkData.m_color.m_predefinedColor);
  writer << kIndent4 << "<styleUrl>#" << style << "</styleUrl>\n"
         << kIndent4 << "<Point><coordinates>" << PointToLineString(bookmarkData.m_point)
         << "</coordinates></Point>\n";

  SaveBookmarkExtendedData(writer, bookmarkData);

  writer << kIndent2 << "</Placemark>\n";
}

void SaveTrackLayer(Writer & writer, TrackLayer const & layer,
                    std::string_view const & indent)
{
  writer << indent << "<color>";
  SaveColorToABGR(writer, layer.m_color.m_rgba);
  writer << "</color>\n";
  writer << indent << "<width>" << strings::to_string(layer.m_lineWidth) << "</width>\n";
}

void SaveLineStrings(Writer & writer, MultiGeometry const & geom)
{
  auto linesIndent = kIndent4;
  auto const lineStringsSize = geom.GetNumberOfLinesWithouTimestamps();

  if (lineStringsSize > 1)
  {
    linesIndent = kIndent8;
    writer << kIndent4 << "<MultiGeometry>\n";
  }

  for (size_t lineIndex = 0; lineIndex < geom.m_lines.size(); ++lineIndex)
  {
    auto const & line = geom.m_lines[lineIndex];
    if (line.empty())
    {
      LOG(LERROR, ("Unexpected empty Track"));
      continue;
    }

    // Skip the tracks with the timestamps when writing the <LineString> points.
    if (geom.HasTimestampsFor(lineIndex))
      continue;

    writer << linesIndent << "<LineString><coordinates>";

    writer << PointToLineString(line[0]);
    for (size_t pointIndex = 1; pointIndex < line.size(); ++pointIndex)
      writer << " " << PointToLineString(line[pointIndex]);

    writer << "</coordinates></LineString>\n";
  }

  if (lineStringsSize > 1)
    writer << kIndent4 << "</MultiGeometry>\n";
}

void SaveGxTracks(Writer & writer, MultiGeometry const & geom)
{
  auto linesIndent = kIndent4;
  auto const gxTracksSize = geom.GetNumberOfLinesWithTimestamps();

  if (gxTracksSize > 1)
  {
    linesIndent = kIndent8;
    writer << kIndent4 << "<gx:MultiTrack>\n";
    /// @TODO(KK): add the <altitudeMode>absolute</altitudeMode> if needed
  }

  for (size_t lineIndex = 0; lineIndex < geom.m_lines.size(); ++lineIndex)
  {
    auto const & line = geom.m_lines[lineIndex];
    if (line.empty())
    {
      LOG(LERROR, ("Unexpected empty Track"));
      continue;
    }

    // Skip the tracks without the timestamps when writing the <gx:Track> points.
    if (!geom.HasTimestampsFor(lineIndex))
      continue;

    writer << linesIndent << "<gx:Track>\n";
    /// @TODO(KK): add the <altitudeMode>absolute</altitudeMode> if needed

    auto const & timestamps = geom.m_timestamps[lineIndex];
    CHECK_EQUAL(line.size(), timestamps.size(), ());

    for (auto const & time : timestamps)
      writer << linesIndent << kIndent4 << "<when>" << base::SecondsSinceEpochToString(time) << "</when>\n";

    for (auto const & point : line)
      writer << linesIndent << kIndent4 << "<gx:coord>" << PointToGxString(point) << "</gx:coord>\n";

    writer << linesIndent << "</gx:Track>\n";
  }

  if (gxTracksSize > 1)
    writer << kIndent4 << "</gx:MultiTrack>\n";
}

void SaveTrackGeometry(Writer & writer, MultiGeometry const & geom)
{
  size_t const sz = geom.m_lines.size();
  if (sz == 0)
  {
    LOG(LERROR, ("Unexpected empty MultiGeometry"));
    return;
  }

  CHECK_EQUAL(geom.m_lines.size(), geom.m_timestamps.size(), ("Number of coordinates and timestamps should match"));

  SaveLineStrings(writer, geom);
  SaveGxTracks(writer, geom);
}

void SaveTrackExtendedData(Writer & writer, TrackData const & trackData)
{
  writer << kIndent4 << kExtendedDataHeader;
  SaveLocalizableString(writer, trackData.m_name, "name", kIndent6);
  SaveLocalizableString(writer, trackData.m_description, "description", kIndent6);

  auto const localId = strings::to_string(static_cast<uint32_t>(trackData.m_localId));
  writer << kIndent6 << "<mwm:localId>" << localId << "</mwm:localId>\n";

  writer << kIndent6 << "<mwm:additionalStyle>\n";
  for (size_t i = 1 /* since second layer */; i < trackData.m_layers.size(); ++i)
  {
    writer << kIndent8 << "<mwm:additionalLineStyle>\n";
    SaveTrackLayer(writer, trackData.m_layers[i], kIndent10);
    writer << kIndent8 << "</mwm:additionalLineStyle>\n";
  }
  writer << kIndent6 << "</mwm:additionalStyle>\n";

  writer << kIndent6 << "<mwm:visibility>" << (trackData.m_visible ? "1" : "0") << "</mwm:visibility>\n";

  SaveStringsArray(writer, trackData.m_nearestToponyms, "nearestToponyms", kIndent6);
  SaveStringsMap(writer, trackData.m_properties, "properties", kIndent6);

  writer << kIndent4 << kExtendedDataFooter;
}

void SaveTrackData(Writer & writer, TrackData const & trackData)
{
  writer << kIndent2 << "<Placemark>\n";
  if (auto name = GetDefaultLanguage(trackData.m_name))
  {
    writer << kIndent4 << "<name>";
    SaveStringWithCDATA(writer, *name);
    writer << "</name>\n";
  }

  if (auto const description = GetDefaultLanguage(trackData.m_description))
  {
    writer << kIndent4 << "<description>";
    SaveStringWithCDATA(writer, *description);
    writer << "</description>\n";
  }

  if (trackData.m_layers.empty())
    MYTHROW(KmlWriter::WriteKmlException, ("Layers list is empty."));

  auto const & layer = trackData.m_layers.front();
  writer << kIndent4 << "<Style><LineStyle>\n";
  SaveTrackLayer(writer, layer, kIndent6);
  writer << kIndent4 << "</LineStyle></Style>\n";

  if (trackData.m_timestamp != Timestamp())
  {
    writer << kIndent4 << "<TimeStamp><when>" << TimestampToString(trackData.m_timestamp)
           << "</when></TimeStamp>\n";
  }

  SaveTrackGeometry(writer, trackData.m_geometry);
  SaveTrackExtendedData(writer, trackData);

  writer << kIndent2 << "</Placemark>\n";
}

bool ParsePoint(std::string_view s, char const * delim, m2::PointD & pt,
                geometry::Altitude & altitude)
{
  // Order in string is: lon, lat, z.
  strings::SimpleTokenizer iter(s, delim);
  if (!iter)
    return false;

  double lon;
  if (strings::to_double(*iter, lon) && mercator::ValidLon(lon) && ++iter)
  {
    double lat;
    if (strings::to_double(*iter, lat) && mercator::ValidLat(lat))
    {
      pt = mercator::FromLatLon(lat, lon);

      double rawAltitude;
      if (++iter && strings::to_double(*iter, rawAltitude))
        altitude = static_cast<geometry::Altitude>(round(rawAltitude));

      return true;
    }
  }
  return false;
}

bool ParsePoint(std::string_view s, char const * delim, m2::PointD & pt)
{
  geometry::Altitude dummyAltitude;
  return ParsePoint(s, delim, pt, dummyAltitude);
}

bool ParsePointWithAltitude(std::string_view s, char const * delim,
                            geometry::PointWithAltitude & point)
{
  geometry::Altitude altitude = geometry::kInvalidAltitude;
  m2::PointD pt;
  if (ParsePoint(s, delim, pt, altitude))
  {
    point.SetPoint(pt);
    point.SetAltitude(altitude);
    return true;
  }
  return false;
}
}  // namespace

void KmlWriter::Write(FileData const & fileData)
{
  m_writer << kKmlHeader;

  // Save category.
  SaveCategoryData(m_writer, fileData.m_categoryData, fileData.m_serverId,
                   &fileData.m_compilationsData);

  // Save bookmarks.
  for (auto const & bookmarkData : fileData.m_bookmarksData)
    SaveBookmarkData(m_writer, bookmarkData);

  // Saving tracks.
  for (auto const & trackData : fileData.m_tracksData)
    SaveTrackData(m_writer, trackData);

  m_writer << kKmlFooter;
}

KmlParser::KmlParser(FileData & data)
  : m_data(data)
  , m_categoryData(&m_data.m_categoryData)
  , m_attrCode(StringUtf8Multilang::kUnsupportedLanguageCode)
{
  ResetPoint();
}

void KmlParser::ResetPoint()
{
  m_name.clear();
  m_description.clear();
  m_org = {};
  m_predefinedColor = PredefinedColor::None;
  m_viewportScale = 0;
  m_timestamp = {};

  m_color = 0;
  m_styleId.clear();
  m_mapStyleId.clear();
  m_styleUrlKey.clear();

  m_featureTypes.clear();
  m_customName.clear();
  m_boundTracks.clear();
  m_visible = true;
  m_nearestToponym.clear();
  m_nearestToponyms.clear();
  m_properties.clear();
  m_localId = 0;
  m_trackLayers.clear();
  m_trackWidth = kDefaultTrackWidth;
  m_icon = BookmarkIcon::None;

  m_geometry.Clear();
  m_geometryType = GEOMETRY_TYPE_UNKNOWN;
  m_skipTimes.clear();
  m_lastTrackPointsCount = std::numeric_limits<size_t>::max();
}

void KmlParser::SetOrigin(std::string const & s)
{
  m_geometryType = GEOMETRY_TYPE_POINT;

  m2::PointD pt;
  if (ParsePoint(s, ", \n\r\t", pt))
    m_org = pt;
}

void KmlParser::ParseAndAddPoints(MultiGeometry::LineT & line, std::string_view s,
                                  char const * blockSeparator, char const * coordSeparator)
{
  strings::Tokenize(s, blockSeparator, [&](std::string_view v)
  {
    geometry::PointWithAltitude point;
    if (ParsePointWithAltitude(v, coordSeparator, point))
      line.emplace_back(point);
    else
      LOG(LWARNING, ("Can not parse KML coordinates from", v));
  });
}

void KmlParser::ParseLineString(std::string const & s)
{
  // If m_org is not empty, then it's still a Bookmark but with track data
  if (m_org == m2::PointD::Zero())
    m_geometryType = GEOMETRY_TYPE_LINE;

  MultiGeometry::LineT line;
  ParseAndAddPoints(line, s, " \n\r\t", ",");

  if (line.size() > 1)
  {
    m_geometry.m_lines.push_back(std::move(line));
    m_geometry.m_timestamps.emplace_back();
  }
}

bool KmlParser::MakeValid()
{
  if (m_geometry.IsValid())
  {
    for (size_t lineIdx = 0; lineIdx < m_geometry.m_lines.size(); ++lineIdx)
    {
      auto & timestamps = m_geometry.m_timestamps[lineIdx];
      if (timestamps.empty())
        continue;

      std::set<size_t> * skipSet = nullptr;
      if (auto it = m_skipTimes.find(lineIdx); it != m_skipTimes.end())
        skipSet = &it->second;

      size_t const pointsSize = m_geometry.m_lines[lineIdx].size();
      if (pointsSize + (skipSet ? skipSet->size() : 0) != timestamps.size())
      {
        MYTHROW(kml::DeserializerKml::DeserializeException, ("Timestamps size", timestamps.size(),
                                                             "mismatch with the points size:", pointsSize,
                                                             "for the track:", lineIdx));
      }

      if (skipSet)
      {
        MultiGeometry::TimeT newTimes;
        newTimes.reserve(timestamps.size() - skipSet->size());

        for (size_t i = 0; i < timestamps.size(); ++i)
          if (!skipSet->contains(i))
            newTimes.push_back(timestamps[i]);
        timestamps.swap(newTimes);
      }
    }
  }

  if (GEOMETRY_TYPE_POINT == m_geometryType)
  {
    if (mercator::ValidX(m_org.x) && mercator::ValidY(m_org.y))
    {
      // Set default name.
      if (m_name.empty() && m_featureTypes.empty())
        m_name[kDefaultLang] = PointToLineString(m_org);

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

void KmlParser::ParseColor(std::string const & value)
{
  auto const fromHex = FromHex(value);
  if (fromHex.size() != 4)
    return;

  // Color positions in HEX – aabbggrr.
  m_color = ToRGBA(fromHex[3], fromHex[2], fromHex[1], fromHex[0]);
}

bool KmlParser::GetColorForStyle(std::string const & styleUrl, uint32_t & color) const
{
  if (styleUrl.empty())
    return false;

  // Remove leading '#' symbol
  auto const it = m_styleUrl2Color.find(styleUrl.substr(1));
  if (it != m_styleUrl2Color.cend())
  {
    color = it->second;
    return true;
  }
  return false;
}

double KmlParser::GetTrackWidthForStyle(std::string const & styleUrl) const
{
  if (styleUrl.empty())
    return kDefaultTrackWidth;

  // Remove leading '#' symbol
  auto const it = m_styleUrl2Width.find(styleUrl.substr(1));
  if (it != m_styleUrl2Width.cend())
    return it->second;

  return kDefaultTrackWidth;
}

bool KmlParser::Push(std::string movedTag)
{
  std::string const & tag = m_tags.emplace_back(std::move(movedTag));

  if (tag == kCompilation)
  {
    m_categoryData = &m_compilationData;
    m_compilationData.m_accessRules = m_data.m_categoryData.m_accessRules;
  }
  else if (IsProcessTrackTag())
  {
    m_geometryType = GEOMETRY_TYPE_LINE;
    m_geometry.m_lines.emplace_back();
    m_geometry.m_timestamps.emplace_back();
  }
  else if (IsProcessTrackCoord())
  {
    m_lastTrackPointsCount = m_geometry.m_lines.back().size();
  }
  return true;
}

void KmlParser::AddAttr(std::string attr, std::string value)
{
  strings::AsciiToLower(attr);

  if (IsValidAttribute(kStyle, value, attr))
  {
    m_styleId = value;
  }
  else if (IsValidAttribute(kStyleMap, value, attr))
  {
    m_mapStyleId = value;
  }
  else if (IsValidAttribute(kCompilation, value, attr))
  {
    if (!strings::to_uint64(value, m_categoryData->m_compilationId))
      m_categoryData->m_compilationId = 0;
  }

  if (attr == "code")
  {
    m_attrCode = StringUtf8Multilang::GetLangIndex(value);
  }
  else if (attr == "id")
  {
    m_attrId = value;
  }
  else if (attr == "key")
  {
    m_attrKey = value;
  }
  else if (attr == "type" && !value.empty() && GetTagFromEnd(0) == kCompilation)
  {
    strings::AsciiToLower(value);
    if (value == "category")
      m_categoryData->m_type = CompilationType::Category;
    else if (value == "collection")
      m_categoryData->m_type = CompilationType::Collection;
    else if (value == "day")
      m_categoryData->m_type = CompilationType::Day;
    else
      m_categoryData->m_type = CompilationType::Category;
  }
}

bool KmlParser::IsValidAttribute(std::string_view type, std::string const & value,
                                 std::string const & attrInLowerCase) const
{
  return (GetTagFromEnd(0) == type && !value.empty() && attrInLowerCase == "id");
}

std::string const & KmlParser::GetTagFromEnd(size_t n) const
{
  ASSERT_LESS(n, m_tags.size(), ());
  return m_tags[m_tags.size() - n - 1];
}

bool KmlParser::IsProcessTrackTag() const
{
  size_t const n = m_tags.size();
  return n >= 3 && IsTrack(m_tags[n - 1]) && (m_tags[n - 2] == kPlacemark || m_tags[n - 3] == kPlacemark);
}

bool KmlParser::IsProcessTrackCoord() const
{
  size_t const n = m_tags.size();
  return n >= 4 && IsTrack(m_tags[n - 2]) && IsCoord(m_tags[n - 1]);
}

void KmlParser::Pop(std::string_view tag)
{
  ASSERT_EQUAL(m_tags.back(), tag, ());

  if (tag == kPlacemark)
  {
    if (MakeValid())
    {
      if (GEOMETRY_TYPE_POINT == m_geometryType)
      {
        BookmarkData data;
        data.m_name = std::move(m_name);
        data.m_description = std::move(m_description);
        data.m_color.m_predefinedColor = m_predefinedColor;
        data.m_color.m_rgba = m_color;
        data.m_icon = m_icon;
        data.m_viewportScale = m_viewportScale;
        data.m_timestamp = m_timestamp;
        data.m_point = m_org;
        data.m_featureTypes = std::move(m_featureTypes);
        data.m_customName = std::move(m_customName);
        data.m_boundTracks = std::move(m_boundTracks);
        data.m_visible = m_visible;
        data.m_nearestToponym = std::move(m_nearestToponym);
        data.m_minZoom = m_minZoom;
        data.m_properties = std::move(m_properties);
        data.m_compilations = std::move(m_compilations);

        // Here we set custom name from 'name' field for KML-files exported from 3rd-party services.
        if (data.m_name.size() == 1 && data.m_name.begin()->first == kDefaultLangCode &&
            data.m_customName.empty() && data.m_featureTypes.empty())
        {
          data.m_customName = data.m_name;
        }

        m_data.m_bookmarksData.push_back(std::move(data));

        // There is a track stored inside a bookmark
        if (m_geometry.IsValid())
        {
          BookmarkData const & bookmarkData = m_data.m_bookmarksData.back();
          TrackData trackData;
          trackData.m_localId = m_localId;
          trackData.m_name = bookmarkData.m_name;
          trackData.m_description = bookmarkData.m_description;
          trackData.m_layers = std::move(m_trackLayers);
          trackData.m_timestamp = m_timestamp;
          trackData.m_geometry = std::move(m_geometry);
          trackData.m_visible = m_visible;
          trackData.m_nearestToponyms = std::move(m_nearestToponyms);
          trackData.m_properties = bookmarkData.m_properties;
          m_data.m_tracksData.push_back(std::move(trackData));
        }
      }
      else if (GEOMETRY_TYPE_LINE == m_geometryType)
      {
        TrackData data;
        data.m_localId = m_localId;
        data.m_name = std::move(m_name);
        data.m_description = std::move(m_description);
        data.m_layers = std::move(m_trackLayers);
        data.m_timestamp = m_timestamp;
        data.m_geometry = std::move(m_geometry);
        data.m_visible = m_visible;
        data.m_nearestToponyms = std::move(m_nearestToponyms);
        data.m_properties = std::move(m_properties);
        m_data.m_tracksData.push_back(std::move(data));
      }
    }
    ResetPoint();
  }
  else if (tag == kStyle)
  {
    if (GetTagFromEnd(1) == kDocument)
    {
      if (!m_styleId.empty())
      {
        m_styleUrl2Color[m_styleId] = m_color;
        m_styleUrl2Width[m_styleId] = m_trackWidth;
        m_color = 0;
        m_trackWidth = kDefaultTrackWidth;
      }
    }
  }
  else if ((tag == "LineStyle" && m_tags.size() > 2 && GetTagFromEnd(2) == kPlacemark) ||
           (tag == "mwm:additionalLineStyle" && m_tags.size() > 3 && GetTagFromEnd(3) == kPlacemark))
  {
    // This code assumes that <Style> is stored inside <Placemark>.
    // It is a violation of KML format, but it must be here to support
    // loading of KML files which were stored by older versions of OMaps.
    TrackLayer layer;
    layer.m_lineWidth = m_trackWidth;
    // Fix wrongly parsed transparent color, see https://github.com/organicmaps/organicmaps/issues/5800
    // TODO: Remove this fix in 2024 when all users will have their imported GPX files fixed.
    if (m_color == 0 || (m_color & 0xFF) < 10)
      layer.m_color.m_rgba = kDefaultTrackColor;
    else
      layer.m_color.m_rgba = m_color;
    m_trackLayers.push_back(layer);

    m_trackWidth = kDefaultTrackWidth;
    m_color = 0;
  }
  else if (tag == kCompilation)
  {
    m_data.m_compilationsData.push_back(std::move(m_compilationData));
    m_categoryData = &m_data.m_categoryData;
  }
  else if (IsProcessTrackTag())
  {
    // Simple line validation.
    auto & lines = m_geometry.m_lines;
    ASSERT(!lines.empty(), ());
    if (lines.back().size() < 2)
    {
      lines.pop_back();
      m_geometry.m_timestamps.pop_back();
    }
  }
  else if (IsProcessTrackCoord())
  {
    // Check if coordinate was not added.
    if (m_geometry.m_lines.back().size() == m_lastTrackPointsCount)
    {
      // Add skip coordinate/timestamp index.
      auto & e = m_skipTimes[m_geometry.m_lines.size() - 1];
      e.insert(m_lastTrackPointsCount + e.size());
    }
  }

  m_tags.pop_back();
}

void KmlParser::CharData(std::string & value)
{
  strings::Trim(value);

  size_t const count = m_tags.size();
  if (count > 1 && !value.empty())
  {
    using namespace std;
    string const & currTag = m_tags[count - 1];
    string const & prevTag = m_tags[count - 2];
    string_view const ppTag = count > 2 ? m_tags[count - 3] : string_view{};
    string_view const pppTag = count > 3 ? m_tags[count - 4] : string_view{};
    string_view const ppppTag = count > 4 ? m_tags[count - 5] : string_view{};

    auto const TrackTag = [this, &prevTag, &currTag, &value]()
    {
      if (!IsTrack(prevTag))
        return false;

      if (IsTimestamp(currTag))
      {
        auto & timestamps = m_geometry.m_timestamps;
        ASSERT(!timestamps.empty(), ());
        timestamps.back().emplace_back(base::StringToTimestamp(value));
      }
      else if (IsCoord(currTag))
      {
        auto & lines = m_geometry.m_lines;
        ASSERT(!lines.empty(), ());
        ParseAndAddPoints(lines.back(), value, "\n\r\t", " ");
      }
      return true;
    };

    if (prevTag == kDocument)
    {
      if (currTag == "name")
        m_categoryData->m_name[kDefaultLang] = value;
      else if (currTag == "description")
        m_categoryData->m_description[kDefaultLang] = value;
      else if (currTag == "visibility")
        m_categoryData->m_visible = value != "0";
    }
    else if ((prevTag == kExtendedData && ppTag == kDocument) ||
             (prevTag == kCompilation && ppTag == kExtendedData && pppTag == kDocument))
    {
      if (currTag == "mwm:author")
      {
        m_categoryData->m_authorName = value;
        m_categoryData->m_authorId = m_attrId;
        m_attrId.clear();
      }
      else if (currTag == "mwm:lastModified")
      {
        auto const ts = base::StringToTimestamp(value);
        if (ts != base::INVALID_TIME_STAMP)
          m_categoryData->m_lastModified = TimestampClock::from_time_t(ts);
      }
      else if (currTag == "mwm:accessRules")
      {
        // 'Private' is here for back-compatibility.
        if (value == "Private" || value == "Local")
          m_categoryData->m_accessRules = AccessRules::Local;
        else if (value == "DirectLink")
          m_categoryData->m_accessRules = AccessRules::DirectLink;
        else if (value == "P2P")
          m_categoryData->m_accessRules = AccessRules::P2P;
        else if (value == "Paid")
          m_categoryData->m_accessRules = AccessRules::Paid;
        else if (value == "Public")
          m_categoryData->m_accessRules = AccessRules::Public;
        else if (value == "AuthorOnly")
          m_categoryData->m_accessRules = AccessRules::AuthorOnly;
      }
      else if (currTag == "mwm:imageUrl")
      {
        m_categoryData->m_imageUrl = value;
      }
      else if (currTag == "mwm:rating")
      {
        if (!strings::to_double(value, m_categoryData->m_rating))
          m_categoryData->m_rating = 0.0;
      }
      else if (currTag == "mwm:reviewsNumber")
      {
        if (!strings::to_uint(value, m_categoryData->m_reviewsNumber))
          m_categoryData->m_reviewsNumber = 0;
      }
      else if (currTag == "mwm:serverId")
      {
        m_data.m_serverId = value;
      }
      else if (currTag == "mwm:visibility")
      {
        m_categoryData->m_visible = value != "0";
      }
    }
    else if (((pppTag == kDocument && ppTag == kExtendedData) ||
              (ppppTag == kDocument && pppTag == kExtendedData && ppTag == kCompilation)) &&
             currTag == "mwm:lang")
    {
      if (prevTag == "mwm:name" && m_attrCode >= 0)
        m_categoryData->m_name[m_attrCode] = value;
      else if (prevTag == "mwm:description" && m_attrCode >= 0)
        m_categoryData->m_description[m_attrCode] = value;
      else if (prevTag == "mwm:annotation" && m_attrCode >= 0)
        m_categoryData->m_annotation[m_attrCode] = value;
      m_attrCode = StringUtf8Multilang::kUnsupportedLanguageCode;
    }
    else if (((pppTag == kDocument && ppTag == kExtendedData) ||
              (ppppTag == kDocument && pppTag == kExtendedData && ppTag == kCompilation)) &&
             currTag == "mwm:value")
    {
      if (prevTag == "mwm:tags")
      {
        m_categoryData->m_tags.push_back(value);
      }
      else if (prevTag == "mwm:toponyms")
      {
        m_categoryData->m_toponyms.push_back(value);
      }
      else if (prevTag == "mwm:languageCodes")
      {
        auto const lang = StringUtf8Multilang::GetLangIndex(value);
        if (lang != StringUtf8Multilang::kUnsupportedLanguageCode)
          m_categoryData->m_languageCodes.push_back(lang);
      }
      else if (prevTag == "mwm:properties" && !m_attrKey.empty())
      {
        m_categoryData->m_properties[m_attrKey] = value;
        m_attrKey.clear();
      }
    }
    else if (prevTag == kPlacemark)
    {
      if (currTag == "name")
      {
        // We always prefer extended data. There is the following logic of "name" data usage:
        // 1. We have read extended data.
        //   1.1. There is "default" language in extended data (m_name is not empty).
        //        The condition protects us from name rewriting.
        //   1.2. There is no "default" language in extended data. Data from "name" are merged
        //        with extended data. It helps us in the case when we did not save "default"
        //        language in extended data.
        // 2. We have NOT read extended data yet (or at all). In this case m_name must be empty.
        // If extended data will be read, it can rewrite "default" language, since we prefer extended data.
        if (m_name.find(kDefaultLang) == m_name.end())
          m_name[kDefaultLang] = value;
      }
      else if (currTag == kStyleUrl)
      {
        // Bookmark draw style.
        m_predefinedColor = ExtractPlacemarkPredefinedColor(value);

        // Here we support old-style hotel placemarks.
        if (value == "#placemark-hotel")
        {
          m_predefinedColor = PredefinedColor::Blue;
          m_icon = BookmarkIcon::Hotel;
        }

        // Track draw style.
        if (!GetColorForStyle(value, m_color))
        {
          // Remove leading '#' symbol.
          std::string const styleId = m_mapStyle2Style[value.substr(1)];
          if (!styleId.empty())
            GetColorForStyle(styleId, m_color);
        }
        TrackLayer layer;
        layer.m_lineWidth = GetTrackWidthForStyle(value);
        layer.m_color.m_predefinedColor = PredefinedColor::None;
        layer.m_color.m_rgba = (m_color != 0 ? m_color : kDefaultTrackColor);
        m_trackLayers.push_back(std::move(layer));
      }
      else if (currTag == "description")
      {
        m_description[kDefaultLang] = value;
      }
    }
    else if (prevTag == "LineStyle" || prevTag == "mwm:additionalLineStyle")
    {
      if (currTag == "color")
      {
        ParseColor(value);
      }
      else if (currTag == "width")
      {
        double val;
        if (strings::to_double(value, val))
          m_trackWidth = val;
      }
    }
    else if (ppTag == kStyleMap && prevTag == kPair && currTag == kStyleUrl &&
             m_styleUrlKey == "normal")
    {
      if (!m_mapStyleId.empty())
        m_mapStyle2Style[m_mapStyleId] = value;
    }
    else if (ppTag == kStyleMap && prevTag == kPair && currTag == "key")
    {
      m_styleUrlKey = value;
    }
    else if (ppTag == kPlacemark)
    {
      if (prevTag == "Point")
      {
        if (currTag == kCoordinates)
          SetOrigin(value);
      }
      else if (prevTag == "LineString")
      {
        if (currTag == kCoordinates)
          ParseLineString(value);
      }
      else if (TrackTag())
      {
        // noop
      }
      else if (prevTag == kExtendedData)
      {
        if (currTag == "mwm:scale")
        {
          double scale;
          if (!strings::to_double(value, scale))
            scale = 0.0;
          m_viewportScale = static_cast<uint8_t>(scale);
        }
        else if (currTag == "mwm:localId")
        {
          uint32_t localId;
          if (!strings::to_uint(value, localId))
            localId = 0;
          m_localId = static_cast<LocalId>(localId);
        }
        else if (currTag == "mwm:icon")
        {
          m_icon = GetIcon(value);
        }
        else if (currTag == "mwm:visibility")
        {
          m_visible = value != "0";
        }
        else if (currTag == "mwm:nearestToponym")
        {
          m_nearestToponym = value;
        }
        else if (currTag == "mwm:minZoom")
        {
          if (!strings::to_int(value, m_minZoom) || m_minZoom < 1)
            m_minZoom = 1;
          else if (m_minZoom > 19)
            m_minZoom = 19;
        }
        else if (currTag == "mwm:compilations")
        {
          m_compilations.clear();
          for (strings::SimpleTokenizer tupleIter(value, ","); tupleIter; ++tupleIter)
          {
            CompilationId compilationId = kInvalidCompilationId;
            if (!strings::to_uint(*tupleIter, compilationId))
            {
              m_compilations.clear();
              break;
            }
            m_compilations.push_back(compilationId);
          }
        }
      }
      else if (prevTag == "TimeStamp")
      {
        if (IsTimestamp(currTag))
        {
          auto const ts = base::StringToTimestamp(value);
          if (ts != base::INVALID_TIME_STAMP)
            m_timestamp = TimestampClock::from_time_t(ts);
        }
      }
      else if (currTag == kStyleUrl)
      {
        GetColorForStyle(value, m_color);
      }
    }
    else if (ppTag == "MultiGeometry")
    {
      if (prevTag == "Point")
      {
        if (currTag == kCoordinates)
          SetOrigin(value);
      }
      else if (prevTag == "LineString")
      {
        if (currTag == kCoordinates)
          ParseLineString(value);
      }
      else if (TrackTag())
      {
        // noop
      }
    }
    else if (pppTag == kPlacemark)
    {
      if (ppTag == kExtendedData)
      {
        if (currTag == "mwm:lang")
        {
          if (prevTag == "mwm:name" && m_attrCode >= 0)
            m_name[m_attrCode] = value;
          else if (prevTag == "mwm:description" && m_attrCode >= 0)
            m_description[m_attrCode] = value;
          else if (prevTag == "mwm:customName" && m_attrCode >= 0)
            m_customName[m_attrCode] = value;
          m_attrCode = StringUtf8Multilang::kUnsupportedLanguageCode;
        }
        else if (currTag == "mwm:value")
        {
          uint32_t i;
          if (prevTag == "mwm:featureTypes")
          {
            auto const & c = classif();
            if (!c.HasTypesMapping())
              MYTHROW(DeserializerKml::DeserializeException, ("Types mapping is not loaded."));
            auto const type = c.GetTypeByReadableObjectName(value);
            if (c.IsTypeValid(type))
            {
              auto const typeInd = c.GetIndexForType(type);
              m_featureTypes.push_back(typeInd);
            }
          }
          else if (prevTag == "mwm:boundTracks" && strings::to_uint(value, i))
          {
            m_boundTracks.push_back(static_cast<LocalId>(i));
          }
          else if (prevTag == "mwm:nearestToponyms")
          {
            m_nearestToponyms.push_back(value);
          }
          else if (prevTag == "mwm:properties" && !m_attrKey.empty())
          {
            m_properties[m_attrKey] = value;
            m_attrKey.clear();
          }
        }
      }
      else if ((ppTag == "MultiTrack" || ppTag == "gx:MultiTrack") && TrackTag())
      {
        // noop
      }
    }
  }
}

// static
kml::TrackLayer KmlParser::GetDefaultTrackLayer()
{
  kml::TrackLayer layer;
  layer.m_lineWidth = kDefaultTrackWidth;
  layer.m_color.m_rgba = kDefaultTrackColor;
  return layer;
}

DeserializerKml::DeserializerKml(FileData & fileData)
  : m_fileData(fileData)
{
  m_fileData = {};
}
}  // namespace kml
