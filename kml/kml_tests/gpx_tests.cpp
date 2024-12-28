#include "testing/testing.hpp"

#include "kml/serdes_common.hpp"
#include "kml/serdes_gpx.hpp"

#include "geometry/mercator.hpp"

#include "coding/file_reader.hpp"

#include "platform/platform.hpp"

namespace gpx_tests
{
static kml::FileData loadGpxFromString(std::string_view content) {
  TEST_NO_THROW(
  {
    kml::FileData dataFromText;
    kml::DeserializerGpx(dataFromText).Deserialize(MemReader(content));
    return dataFromText;
  }, ());
}

static kml::FileData loadGpxFromFile(std::string const & file) {
  auto const fileName = GetPlatform().TestsDataPathForFile(file);
  std::string text;
  FileReader(fileName).ReadAsString(text);
  return loadGpxFromString(text);
}

void importExportCompare(char const * testFile)
{
  auto const fileName = GetPlatform().TestsDataPathForFile(testFile);
  std::string sourceFileText;
  FileReader(fileName).ReadAsString(sourceFileText);
  kml::FileData const dataFromFile = loadGpxFromFile(testFile);
  std::string resultBuffer;
  MemWriter<decltype(resultBuffer)> sink(resultBuffer);
  kml::gpx::SerializerGpx ser(dataFromFile);
  ser.Serialize(sink);
  TEST_EQUAL(resultBuffer, sourceFileText, ());
}

UNIT_TEST(Gpx_ImportExport_Test)
{
  importExportCompare("test_data/gpx/export_test.gpx");
}

UNIT_TEST(Gpx_ImportExportEmpty_Test)
{
  importExportCompare("test_data/gpx/export_test_empty.gpx");
}

UNIT_TEST(Gpx_Test_Point_With_Valid_Timestamp)
{
  std::string_view constexpr input = R"(<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.0">
 <wpt lat="42.81025" lon="-1.65727">
  <time>2022-09-05T08:39:39.3700Z</time>
  <name>Waypoint 1</name>
 </wpt>
)";

  kml::FileData data;
  kml::BookmarkData bookmarkData;
  bookmarkData.m_name[kml::kDefaultLang] = "Waypoint 1";
  bookmarkData.m_point = mercator::FromLatLon(42.81025, -1.65727);
  bookmarkData.m_customName[kml::kDefaultLang] = "Waypoint 1";
  bookmarkData.m_color = {kml::PredefinedColor::Red, 0};
  data.m_bookmarksData.emplace_back(std::move(bookmarkData));

  kml::FileData const dataFromText = loadGpxFromString(input);

  TEST_EQUAL(dataFromText, data, ());
}

UNIT_TEST(Gpx_Test_Point_With_Invalid_Timestamp)
{
  std::string_view constexpr input = R"(<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.0">
 <wpt lat="42.81025" lon="-1.65727">
  <time>2022-09-05T08:39:39.3700X</time>
  <name>Waypoint 1</name>
 </wpt>
)";

  kml::FileData const dataFromText = loadGpxFromString(input);
  TEST_EQUAL(dataFromText.m_bookmarksData.size(), 1, ());
}

UNIT_TEST(Gpx_Test_Track_Without_Timestamps)
{
  auto const fileName = "test_data/gpx/track_without_timestamps.gpx";
  kml::FileData const dataFromText = loadGpxFromFile(fileName);
  auto const & lines = dataFromText.m_tracksData[0].m_geometry.m_lines;
  TEST_EQUAL(lines.size(), 2, ());
  {
    auto const & line = lines[0];
    TEST_EQUAL(line.size(), 3, ());
    TEST_EQUAL(line.back(), geometry::PointWithAltitude(mercator::FromLatLon(54.05293900056246, 25.72998046875), 0), ());
  }
  {
    auto const & line = lines[1];
    TEST_EQUAL(line.size(), 2, ());
    TEST_EQUAL(line.back(), geometry::PointWithAltitude(mercator::FromLatLon(54.32933804825253, 25.136718750000004), 0), ());
  }
  // Also test default colors for tracks.
  {
    TEST_EQUAL(dataFromText.m_tracksData.size(), 1, ());
    TEST_EQUAL(dataFromText.m_tracksData[0].m_layers.size(), 1, ());
    auto const & layer = dataFromText.m_tracksData[0].m_layers[0];
    TEST_EQUAL(layer.m_color.m_rgba, kml::kDefaultTrackColor, ());
    TEST_EQUAL(layer.m_color.m_predefinedColor, kml::PredefinedColor::None, ());
    TEST_EQUAL(layer.m_lineWidth, kml::kDefaultTrackWidth, ());
    auto const & geometry = dataFromText.m_tracksData[0].m_geometry;
    TEST_EQUAL(geometry.m_timestamps.size(), 2, ());
    TEST(geometry.m_timestamps[0].empty(), ());
    TEST(geometry.m_timestamps[1].empty(), ());
    TEST(!geometry.HasTimestamps(), ());
    TEST(!geometry.HasTimestampsFor(0), ());
    TEST(!geometry.HasTimestampsFor(1), ());
  }
}

UNIT_TEST(Gpx_Test_Track_With_Timestamps)
{
  auto const fileName = "test_data/gpx/track_with_timestamps.gpx";
  kml::FileData const dataFromText = loadGpxFromFile(fileName);
  auto const & geometry = dataFromText.m_tracksData[0].m_geometry;
  TEST_EQUAL(geometry.m_lines.size(), 2, ());
  TEST_EQUAL(geometry.m_timestamps.size(), 2, ());
  TEST(geometry.IsValid(), ());
  TEST(geometry.HasTimestamps(), ());
  TEST(geometry.HasTimestampsFor(0), ());
  TEST(geometry.HasTimestampsFor(1), ());
}

UNIT_TEST(Gpx_Test_Track_With_Timestamps_Mismatch)
{
  auto const fileName = GetPlatform().TestsDataPathForFile("test_data/gpx/track_with_timestamps_broken.gpx");
  std::string text;
  FileReader(fileName).ReadAsString(text);

  kml::FileData data;
  kml::DeserializerGpx(data).Deserialize(MemReader(text));

  TEST_EQUAL(data.m_tracksData.size(), 1, ());
  TEST_EQUAL(data.m_tracksData[0].m_geometry.m_timestamps.size(), 2, ());
  TEST(data.m_tracksData[0].m_geometry.HasTimestampsFor(0), ());
  TEST(data.m_tracksData[0].m_geometry.HasTimestampsFor(1), ());
}

UNIT_TEST(Gpx_Altitude_Issues)
{
  std::string_view constexpr input = R"(<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.0">
<trk>
    <name>new</name>
    <type>Cycling</type>
    <trkseg>
      <trkpt lat="1" lon="1"></trkpt>
      <trkpt lat="2" lon="2"><ele>1.0</ele></trkpt>
      <trkpt lat="3" lon="3"></trkpt>
      <trkpt lat="4" lon="4"><ele>2.0</ele></trkpt>
      <trkpt lat="5" lon="5"><ele>Wrong</ele></trkpt>
      <trkpt lat="6" lon="6"><ele>3</ele></trkpt>
    </trkseg>
</trk>
</gpx>
)";

  kml::FileData const dataFromText = loadGpxFromString(input);
  auto const & line = dataFromText.m_tracksData[0].m_geometry.m_lines[0];
  TEST_EQUAL(line.size(), 6, ());
  TEST_EQUAL(line[0], geometry::PointWithAltitude(mercator::FromLatLon(1, 1), geometry::kInvalidAltitude), ());
  TEST_EQUAL(line[1], geometry::PointWithAltitude(mercator::FromLatLon(2, 2), 1), ());
  TEST_EQUAL(line[2], geometry::PointWithAltitude(mercator::FromLatLon(3, 3), geometry::kInvalidAltitude), ());
  TEST_EQUAL(line[3], geometry::PointWithAltitude(mercator::FromLatLon(4, 4), 2), ());
  TEST_EQUAL(line[4], geometry::PointWithAltitude(mercator::FromLatLon(5, 5), geometry::kInvalidAltitude), ());
  TEST_EQUAL(line[5], geometry::PointWithAltitude(mercator::FromLatLon(6, 6), 3), ());
}

UNIT_TEST(Gpx_Timestamp_Issues)
{
  std::string_view constexpr input = R"(<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.0">
<trk>
    <name>new</name>
    <type>Cycling</type>
    <trkseg>
      <trkpt lat="0" lon="0"></trkpt>
      <trkpt lat="1" lon="1"><time>2024-05-04T19:00:00Z</time></trkpt>
      <trkpt lat="2" lon="2"><time>2024-05-04T19:00:01Z</time></trkpt>
      <trkpt lat="3" lon="3"></trkpt>
      <trkpt lat="4" lon="4"><time>Abra-hadabra</time></trkpt>
      <trkpt lat="5" lon="5"><time>2024-05-04T19:00:04Z</time></trkpt>
      <trkpt lat="6" lon="6"><time>2024-05-04T19:00:05Z</time></trkpt>
      <trkpt lat="7" lon="7"></trkpt>
    </trkseg>
</trk>
</gpx>
)";

  kml::FileData const dataFromText = loadGpxFromString(input);
  auto const & times = dataFromText.m_tracksData[0].m_geometry.m_timestamps[0];
  TEST_EQUAL(times.size(), 8, ());
  TEST_EQUAL(times[0], base::StringToTimestamp("2024-05-04T19:00:00Z"), ());
  TEST_EQUAL(times[1], base::StringToTimestamp("2024-05-04T19:00:00Z"), ());
  TEST_EQUAL(times[2], base::StringToTimestamp("2024-05-04T19:00:01Z"), ());
  TEST_EQUAL(times[3], base::StringToTimestamp("2024-05-04T19:00:02Z"), ());
  TEST_EQUAL(times[4], base::StringToTimestamp("2024-05-04T19:00:03Z"), ());
  TEST_EQUAL(times[5], base::StringToTimestamp("2024-05-04T19:00:04Z"), ());
  TEST_EQUAL(times[6], base::StringToTimestamp("2024-05-04T19:00:05Z"), ());
  TEST_EQUAL(times[7], base::StringToTimestamp("2024-05-04T19:00:05Z"), ());
}

UNIT_TEST(GoMap)
{
  kml::FileData const dataFromFile = loadGpxFromFile("test_data/gpx/go_map.gpx");
  auto const & line = dataFromFile.m_tracksData[0].m_geometry.m_lines[0];
  TEST_EQUAL(line.size(), 101, ());
}

UNIT_TEST(GpxStudio)
{
  kml::FileData const dataFromFile = loadGpxFromFile("test_data/gpx/gpx_studio.gpx");
  auto const & line = dataFromFile.m_tracksData[0].m_geometry.m_lines[0];
  TEST_EQUAL(line.size(), 328, ());
}

UNIT_TEST(OsmTrack)
{
  kml::FileData const dataFromFile = loadGpxFromFile("test_data/gpx/osm_track.gpx");
  auto const & line = dataFromFile.m_tracksData[0].m_geometry.m_lines[0];
  auto const & timestamps = dataFromFile.m_tracksData[0].m_geometry.m_timestamps[0];
  TEST_EQUAL(line.size(), 182, ());
  TEST_EQUAL(timestamps.size(), 182, ());
}

UNIT_TEST(TowerCollector)
{
  kml::FileData const dataFromFile = loadGpxFromFile("test_data/gpx/tower_collector.gpx");
  auto line = dataFromFile.m_tracksData[0].m_geometry.m_lines[0];
  TEST_EQUAL(line.size(), 35, ());
}

UNIT_TEST(PointsOnly)
{
  kml::FileData const dataFromFile = loadGpxFromFile("test_data/gpx/points.gpx");
  auto bookmarks = dataFromFile.m_bookmarksData;
  TEST_EQUAL(bookmarks.size(), 3, ());
  TEST_EQUAL("Point 1", bookmarks[0].m_name[kml::kDefaultLang], ());
  TEST_EQUAL(bookmarks[0].m_point, mercator::FromLatLon(48.20984622935899, 16.376023292541507), ());
}

UNIT_TEST(Route)
{
  kml::FileData dataFromFile = loadGpxFromFile("test_data/gpx/route.gpx");
  auto line = dataFromFile.m_tracksData[0].m_geometry.m_lines[0];
  TEST_EQUAL(line.size(), 2, ());
  TEST_EQUAL(dataFromFile.m_categoryData.m_name[kml::kDefaultLang], "Some random route", ());
  TEST_EQUAL(line[0], geometry::PointWithAltitude(mercator::FromLatLon(48.20984622935899, 16.376023292541507), 184), ());
  TEST_EQUAL(line[1], geometry::PointWithAltitude(mercator::FromLatLon(48.209503040543545, 16.381065845489506), 187), ());
}

UNIT_TEST(Color)
{
  kml::FileData const dataFromFile = loadGpxFromFile("test_data/gpx/color.gpx");
  uint32_t const red = 0xFF0000FF;
  uint32_t const blue = 0x0000FFFF;
  uint32_t const black = 0x000000FF;
  TEST_EQUAL(red, dataFromFile.m_tracksData[0].m_layers[0].m_color.m_rgba, ());
  TEST_EQUAL(blue, dataFromFile.m_tracksData[1].m_layers[0].m_color.m_rgba, ());
  TEST_EQUAL(black, dataFromFile.m_tracksData[2].m_layers[0].m_color.m_rgba, ());
  TEST_EQUAL(dataFromFile.m_tracksData.size(), 3, ());
}

UNIT_TEST(MultiTrackNames)
{
  kml::FileData dataFromFile = loadGpxFromFile("test_data/gpx/color.gpx");
  TEST_EQUAL("new", dataFromFile.m_categoryData.m_name[kml::kDefaultLang], ());
  TEST_EQUAL("Short description", dataFromFile.m_categoryData.m_description[kml::kDefaultLang], ());
  TEST_EQUAL("new red", dataFromFile.m_tracksData[0].m_name[kml::kDefaultLang], ());
  TEST_EQUAL("description 1", dataFromFile.m_tracksData[0].m_description[kml::kDefaultLang], ());
  TEST_EQUAL("new blue", dataFromFile.m_tracksData[1].m_name[kml::kDefaultLang], ());
  TEST_EQUAL("description 2", dataFromFile.m_tracksData[1].m_description[kml::kDefaultLang], ());
}

UNIT_TEST(Empty)
{
  kml::FileData dataFromFile = loadGpxFromFile("test_data/gpx/empty.gpx");
  TEST_EQUAL("new", dataFromFile.m_categoryData.m_name[kml::kDefaultLang], ());
  TEST_EQUAL(0, dataFromFile.m_tracksData.size(), ());
}

UNIT_TEST(OsmandColor1)
{
  kml::FileData const dataFromFile = loadGpxFromFile("test_data/gpx/osmand1.gpx");
  uint32_t constexpr expected = 0xFF7800FF;
  TEST_EQUAL(dataFromFile.m_tracksData.size(), 4, ());
  TEST_EQUAL(expected, dataFromFile.m_tracksData[0].m_layers[0].m_color.m_rgba, ());
  TEST_EQUAL(expected, dataFromFile.m_tracksData[1].m_layers[0].m_color.m_rgba, ());
  TEST_EQUAL(expected, dataFromFile.m_tracksData[2].m_layers[0].m_color.m_rgba, ());
  TEST_EQUAL(expected, dataFromFile.m_tracksData[3].m_layers[0].m_color.m_rgba, ());
}

UNIT_TEST(OsmandColor2)
{
  kml::FileData const dataFromFile = loadGpxFromFile("test_data/gpx/osmand2.gpx");
  uint32_t const expected1 = 0x00FF00FF;
  uint32_t const expected2 = 0x1010A0FF;
  TEST_EQUAL(expected1, dataFromFile.m_bookmarksData[0].m_color.m_rgba, ());
  TEST_EQUAL(expected2, dataFromFile.m_bookmarksData[1].m_color.m_rgba, ());
}

UNIT_TEST(Gpx_Test_Cmt)
{
  std::string const input = R"(<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.0">
 <wpt lat="1" lon="2"><name>1</name><desc>d1</desc></wpt>
 <wpt lat="1" lon="2"><name>2</name><desc>d2</desc><cmt>c2</cmt></wpt>
 <wpt lat="1" lon="2"><name>3</name><cmt>c3</cmt></wpt>
 <wpt lat="1" lon="2"><name>4</name>
  <desc>
d4
d5


  </desc>
  <cmt>c4</cmt>
 </wpt>
 <wpt lat="1" lon="2"><name>5</name><cmt>qqq</cmt><desc>qqq</desc></wpt>
)";
  kml::FileData const dataFromText = loadGpxFromString(input);
  TEST_EQUAL("d1", dataFromText.m_bookmarksData[0].m_description.at(kml::kDefaultLang), ());
  TEST_EQUAL("d2\n\nc2", dataFromText.m_bookmarksData[1].m_description.at(kml::kDefaultLang), ());
  TEST_EQUAL("c3", dataFromText.m_bookmarksData[2].m_description.at(kml::kDefaultLang), ());
  TEST_EQUAL("d4\nd5\n\nc4", dataFromText.m_bookmarksData[3].m_description.at(kml::kDefaultLang), ());
  TEST_EQUAL("qqq", dataFromText.m_bookmarksData[4].m_description.at(kml::kDefaultLang), ());
}

UNIT_TEST(OpentracksColor)
{
  kml::FileData dataFromFile = loadGpxFromFile("test_data/gpx/opentracks_color.gpx");
  uint32_t const expected = 0xC0C0C0FF;
  TEST_EQUAL(expected, dataFromFile.m_tracksData[0].m_layers[0].m_color.m_rgba, ());
}

}  // namespace gpx_tests
