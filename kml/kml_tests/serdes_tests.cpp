#include "testing/testing.hpp"

#include "kml/kml_tests/tests_data.hpp"

#include "kml/serdes.hpp"
#include "kml/serdes_binary.hpp"

#include "map/bookmark_helpers.hpp"

#include "indexer/classificator_loader.hpp"

#include "platform/platform.hpp"

#include "coding/file_reader.hpp"
#include "coding/file_writer.hpp"
#include "coding/hex.hpp"
#include "coding/reader.hpp"
#include "coding/string_utf8_multilang.hpp"
#include "coding/writer.hpp"

#include "base/file_name_utils.hpp"
#include "base/scope_guard.hpp"

#include <cstring>
#include <functional>
#include <sstream>
#include <vector>

namespace
{
// This function can be used to generate textual representation of vector<uint8_t> like you see above.
std::string FormatBytesFromBuffer(std::vector<uint8_t> const & buffer)
{
  std::stringstream ss;
  for (size_t i = 1; i <= buffer.size(); i++)
  {
    ss << "0x" << NumToHex(buffer[i - 1]) << ", ";
    if (i % 16 == 0)
      ss << "\n";
  }
  return ss.str();
}

auto const kDefaultLang = StringUtf8Multilang::kDefaultCode;
auto const kEnLang = StringUtf8Multilang::kEnglishCode;
auto const kRuLang = static_cast<int8_t>(8);

kml::FileData GenerateKmlFileData()
{
  kml::FileData result;
  result.m_deviceId = "AAAA";
  result.m_serverId = "AAAA-BBBB-CCCC-DDDD";

  result.m_categoryData.m_name[kDefaultLang] = "Test category";
  result.m_categoryData.m_name[kRuLang] = "Тестовая категория";
  result.m_categoryData.m_description[kDefaultLang] = "Test description";
  result.m_categoryData.m_description[kRuLang] = "Тестовое описание";
  result.m_categoryData.m_annotation[kDefaultLang] = "Test annotation";
  result.m_categoryData.m_annotation[kEnLang] = "Test annotation";
  result.m_categoryData.m_imageUrl = "https://localhost/123.png";
  result.m_categoryData.m_visible = true;
  result.m_categoryData.m_authorName = "Organic Maps";
  result.m_categoryData.m_authorId = "12345";
  result.m_categoryData.m_rating = 8.9;
  result.m_categoryData.m_reviewsNumber = 567;
  result.m_categoryData.m_lastModified = kml::TimestampClock::from_time_t(1000);
  result.m_categoryData.m_accessRules = kml::AccessRules::Public;
  result.m_categoryData.m_tags = {"mountains", "ski", "snowboard"};
  result.m_categoryData.m_toponyms = {"12345", "54321"};
  result.m_categoryData.m_languageCodes = {1, 2, 8};
  result.m_categoryData.m_properties = {{"property1", "value1"}, {"property2", "value2"}};

  kml::BookmarkData bookmarkData;
  bookmarkData.m_name[kDefaultLang] = "Test bookmark";
  bookmarkData.m_name[kRuLang] = "Тестовая метка";
  bookmarkData.m_description[kDefaultLang] = "Test bookmark description";
  bookmarkData.m_description[kRuLang] = "Тестовое описание метки";
  bookmarkData.m_featureTypes = {718, 715};
  bookmarkData.m_customName[kDefaultLang] = "Мое любимое место";
  bookmarkData.m_customName[kEnLang] = "My favorite place";
  bookmarkData.m_color = {kml::PredefinedColor::Blue, 0};
  bookmarkData.m_icon = kml::BookmarkIcon::None;
  bookmarkData.m_viewportScale = 15;
  bookmarkData.m_timestamp = kml::TimestampClock::from_time_t(800);
  bookmarkData.m_point = m2::PointD(45.9242, 56.8679);
  bookmarkData.m_boundTracks = {0};
  bookmarkData.m_visible = false;
  bookmarkData.m_nearestToponym = "12345";
  bookmarkData.m_minZoom = 10;
  bookmarkData.m_properties = {{"bm_property1", "value1"},
                               {"bm_property2", "value2"},
                               {"score", "5"}};
  bookmarkData.m_compilations = {1, 2, 3, 4, 5};
  result.m_bookmarksData.emplace_back(std::move(bookmarkData));

  kml::TrackData trackData;
  trackData.m_localId = 0;
  trackData.m_name[kDefaultLang] = "Test track";
  trackData.m_name[kRuLang] = "Тестовый трек";
  trackData.m_description[kDefaultLang] = "Test track description";
  trackData.m_description[kRuLang] = "Тестовое описание трека";
  trackData.m_layers = {{6.0, {kml::PredefinedColor::None, 0xff0000ff}},
                        {7.0, {kml::PredefinedColor::None, 0x00ff00ff}}};
  trackData.m_timestamp = kml::TimestampClock::from_time_t(900);

  trackData.m_geometry.AddLine({
    {{45.9242, 56.8679}, 1}, {{45.2244, 56.2786}, 2}, {{45.1964, 56.9832}, 3}
  });

  trackData.m_visible = false;
  trackData.m_nearestToponyms = {"12345", "54321", "98765"};
  trackData.m_properties = {{"tr_property1", "value1"}, {"tr_property2", "value2"}};
  result.m_tracksData.emplace_back(std::move(trackData));

  kml::CategoryData compilationData1;
  compilationData1.m_compilationId = 1;
  compilationData1.m_type = kml::CompilationType::Collection;
  compilationData1.m_name[kDefaultLang] = "Test collection";
  compilationData1.m_name[kRuLang] = "Тестовая коллекция";
  compilationData1.m_description[kDefaultLang] = "Test collection description";
  compilationData1.m_description[kRuLang] = "Тестовое описание коллекции";
  compilationData1.m_annotation[kDefaultLang] = "Test collection annotation";
  compilationData1.m_annotation[kEnLang] = "Test collection annotation";
  compilationData1.m_imageUrl = "https://localhost/1234.png";
  compilationData1.m_visible = true;
  compilationData1.m_authorName = "Organic Maps";
  compilationData1.m_authorId = "54321";
  compilationData1.m_rating = 5.9;
  compilationData1.m_reviewsNumber = 333;
  compilationData1.m_lastModified = kml::TimestampClock::from_time_t(999);
  compilationData1.m_accessRules = kml::AccessRules::Public;
  compilationData1.m_tags = {"mountains", "ski"};
  compilationData1.m_toponyms = {"8", "9"};
  compilationData1.m_languageCodes = {1, 2, 8};
  compilationData1.m_properties = {{"property1", "value1"}, {"property2", "value2"}};
  result.m_compilationsData.push_back(std::move(compilationData1));

  kml::CategoryData compilationData2;
  compilationData2.m_compilationId = 4;
  compilationData2.m_type = kml::CompilationType::Category;
  compilationData2.m_name[kDefaultLang] = "Test category";
  compilationData2.m_name[kRuLang] = "Тестовая категория";
  compilationData2.m_description[kDefaultLang] = "Test category description";
  compilationData2.m_description[kRuLang] = "Тестовое описание категории";
  compilationData2.m_annotation[kDefaultLang] = "Test category annotation";
  compilationData2.m_annotation[kEnLang] = "Test category annotation";
  compilationData2.m_imageUrl = "https://localhost/134.png";
  compilationData2.m_visible = false;
  compilationData2.m_authorName = "Organic Maps";
  compilationData2.m_authorId = "11111";
  compilationData2.m_rating = 3.3;
  compilationData2.m_reviewsNumber = 222;
  compilationData2.m_lastModified = kml::TimestampClock::from_time_t(323);
  compilationData2.m_accessRules = kml::AccessRules::Public;
  compilationData2.m_tags = {"mountains", "bike"};
  compilationData2.m_toponyms = {"10", "11"};
  compilationData2.m_languageCodes = {1, 2, 8};
  compilationData2.m_properties = {{"property1", "value1"}, {"property2", "value2"}};
  result.m_compilationsData.push_back(std::move(compilationData2));

  return result;
}

kml::FileData GenerateKmlFileDataForTrackWithoutTimestamps()
{
  auto data = GenerateKmlFileData();
  auto & trackData = data.m_tracksData[0];
  trackData.m_geometry.Clear();
  trackData.m_geometry.AddLine({
    {{45.9242, 56.8679}, 1}, {{45.2244, 56.2786}, 2}, {{45.1964, 56.9832}, 3}
  });
  trackData.m_geometry.AddTimestamps({});
  return data;
}

kml::FileData GenerateKmlFileDataForTrackWithTimestamps()
{
  auto data = GenerateKmlFileData();
  auto & trackData = data.m_tracksData[0];
  trackData.m_geometry.Clear();

  // track 1 (without timestamps)
  trackData.m_geometry.AddLine({
    {{45.9242, 56.8679}, 1}, {{45.2244, 56.2786}, 2}, {{45.1964, 56.9832}, 3}
  });
  trackData.m_geometry.AddTimestamps({});

  // track 2
  trackData.m_geometry.AddLine({
    {{45.9242, 56.8679}, 1}, {{45.2244, 56.2786}, 2}, {{45.1964, 56.9832}, 3}
  });
  trackData.m_geometry.AddTimestamps({0.0, 1.0, 2.0});

  // track 3
  trackData.m_geometry.AddLine({
    {{45.9242, 56.8679}, 1}, {{45.2244, 56.2786}, 2}
  });
  trackData.m_geometry.AddTimestamps({0.0, 1.0});
  return data;
}
}  // namespace

// 1. Check text and binary deserialization from the prepared sources in memory.
UNIT_TEST(Kml_Deserialization_Text_Bin_Memory)
{
  UNUSED_VALUE(FormatBytesFromBuffer({}));

  kml::FileData dataFromText;
  TEST_NO_THROW(
  {
    kml::DeserializerKml des(dataFromText);
    MemReader reader(kTextKml, strlen(kTextKml));
    des.Deserialize(reader);
  }, ());

// TODO: uncomment to output bytes to the log.
//  std::vector<uint8_t> buffer;
//  {
//    kml::binary::SerializerKml ser(dataFromText);
//    MemWriter<decltype(buffer)> sink(buffer);
//    ser.Serialize(sink);
//  }
//  LOG(LINFO, (FormatBytesFromBuffer(buffer)));

  kml::FileData dataFromBin;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKml.data(), kBinKml.size());
    kml::binary::DeserializerKml des(dataFromBin);
    des.Deserialize(reader);
  }, ());

  TEST_EQUAL(dataFromText, dataFromBin, ());
}

// 2. Check text serialization to the memory blob and compare with prepared data.
UNIT_TEST(Kml_Serialization_Text_Memory)
{
  kml::FileData data;
  {
    kml::DeserializerKml des(data);
    MemReader reader(kTextKml, strlen(kTextKml));
    des.Deserialize(reader);
  }

  std::string resultBuffer;
  {
    MemWriter<decltype(resultBuffer)> sink(resultBuffer);
    kml::SerializerKml ser(data);
    ser.Serialize(sink);
  }

  kml::FileData data2;
  {
    kml::DeserializerKml des(data2);
    MemReader reader(resultBuffer.c_str(), resultBuffer.length());
    des.Deserialize(reader);
  }

  TEST_EQUAL(data, data2, ());
}

// 3. Check binary serialization to the memory blob and compare with prepared data.
UNIT_TEST(Kml_Serialization_Bin_Memory)
{
  kml::FileData data;
  {
    kml::binary::DeserializerKml des(data);
    MemReader reader(kBinKml.data(), kBinKml.size());
    des.Deserialize(reader);
  }

  std::vector<uint8_t> buffer;
  {
    kml::binary::SerializerKml ser(data);
    MemWriter<decltype(buffer)> sink(buffer);
    ser.Serialize(sink);
  }

  TEST_EQUAL(kBinKml, buffer, ());

  kml::FileData data2;
  {
    kml::binary::DeserializerKml des(data2);
    MemReader reader(buffer.data(), buffer.size());
    des.Deserialize(reader);
  }

  TEST_EQUAL(data, data2, ());
}

// 4. Check deserialization from the text file.
UNIT_TEST(Kml_Deserialization_Text_File)
{
  std::string const kmlFile = base::JoinPath(GetPlatform().TmpDir(), "tmp.kml");
  SCOPE_GUARD(fileGuard, std::bind(&FileWriter::DeleteFileX, kmlFile));
  TEST_NO_THROW(
  {
    FileWriter file(kmlFile);
    file.Write(kTextKml, strlen(kTextKml));
  }, ());

  kml::FileData dataFromFile;
  TEST_NO_THROW(
  {
    kml::DeserializerKml des(dataFromFile);
    FileReader reader(kmlFile);
    des.Deserialize(reader);
  }, ());

  kml::FileData dataFromText;
  TEST_NO_THROW(
  {
    kml::DeserializerKml des(dataFromText);
    MemReader reader(kTextKml, strlen(kTextKml));
    des.Deserialize(reader);
  }, ());
  TEST_EQUAL(dataFromFile, dataFromText, ());
}

// 5. Check deserialization from the binary file.
UNIT_TEST(Kml_Deserialization_Bin_File)
{
  std::string const kmbFile = base::JoinPath(GetPlatform().TmpDir(), "tmp.kmb");
  SCOPE_GUARD(fileGuard, std::bind(&FileWriter::DeleteFileX, kmbFile));
  TEST_NO_THROW(
  {
    FileWriter file(kmbFile);
    file.Write(kBinKml.data(), kBinKml.size());
  }, ());

  kml::FileData dataFromFile;
  TEST_NO_THROW(
  {
    kml::binary::DeserializerKml des(dataFromFile);
    FileReader reader(kmbFile);
    des.Deserialize(reader);
  }, ());

  kml::FileData dataFromBin;
  TEST_NO_THROW(
  {
    kml::binary::DeserializerKml des(dataFromBin);
    MemReader reader(kBinKml.data(), kBinKml.size());
    des.Deserialize(reader);
  }, ());

  TEST_EQUAL(dataFromFile, dataFromBin, ());
}

// 6. Check serialization to the binary file. Here we use generated data.
// The data in RAM must be completely equal to the data in binary file.
UNIT_TEST(Kml_Serialization_Bin_File)
{
  auto data = GenerateKmlFileData();

  std::string const kmbFile = base::JoinPath(GetPlatform().TmpDir(), "tmp.kmb");
  SCOPE_GUARD(fileGuard, std::bind(&FileWriter::DeleteFileX, kmbFile));
  TEST_NO_THROW(
  {
    kml::binary::SerializerKml ser(data);
    FileWriter writer(kmbFile);
    ser.Serialize(writer);
  }, ());

  kml::FileData dataFromFile;
  TEST_NO_THROW(
  {
    kml::binary::DeserializerKml des(dataFromFile);
    FileReader reader(kmbFile);
    des.Deserialize(reader);
  }, ());

  TEST_EQUAL(data, dataFromFile, ());
}

// 7. Check serialization to the text file. Here we use generated data.
// The text in the file can be not equal to the original generated data, because
// text representation does not support all features, e.g. we do not store ids for
// bookmarks and tracks.
UNIT_TEST(Kml_Serialization_Text_File_Track_Without_Timestamps)
{
  classificator::Load();

  auto data = GenerateKmlFileDataForTrackWithoutTimestamps();

  std::string const kmlFile = base::JoinPath(GetPlatform().TmpDir(), "tmp.kml");
  SCOPE_GUARD(fileGuard, std::bind(&FileWriter::DeleteFileX, kmlFile));
  TEST_NO_THROW(
  {
    kml::SerializerKml ser(data);
    FileWriter sink(kmlFile);
    ser.Serialize(sink);
  }, ());

// TODO: uncomment to output KML to the log.
//  std::string buffer;
//  {
//    kml::SerializerKml ser(data);
//    MemWriter<decltype(buffer)> sink(buffer);
//    ser.Serialize(sink);
//  }
//  LOG(LINFO, (buffer));

  kml::FileData dataFromGeneratedFile;
  TEST_NO_THROW(
  {
    kml::DeserializerKml des(dataFromGeneratedFile);
    FileReader reader(kmlFile);
    des.Deserialize(reader);
  }, ());
  TEST_EQUAL(dataFromGeneratedFile, data, ());

  kml::FileData dataFromFile;
  TEST_NO_THROW(
  {
    kml::DeserializerKml des(dataFromFile);
    FileReader reader(GetPlatform().TestsDataPathForFile("test_data/kml/generated.kml"));
    des.Deserialize(reader);
  }, ());
  TEST_EQUAL(dataFromFile, data, ());

  std::string dataFromFileBuffer;
  {
    MemWriter<decltype(dataFromFileBuffer)> sink(dataFromFileBuffer);
    kml::SerializerKml ser(dataFromFile);
    ser.Serialize(sink);
  }
  std::string dataFromGeneratedFileBuffer;
  {
    MemWriter<decltype(dataFromGeneratedFileBuffer)> sink(dataFromGeneratedFileBuffer);
    kml::SerializerKml ser(dataFromGeneratedFile);
    ser.Serialize(sink);
  }
  TEST_EQUAL(dataFromFileBuffer, dataFromGeneratedFileBuffer, ());
}

UNIT_TEST(Kml_Serialization_Text_File_Tracks_With_Timestamps)
{
  classificator::Load();

  auto data = GenerateKmlFileDataForTrackWithTimestamps();

  std::string const kmlFile = base::JoinPath(GetPlatform().TmpDir(), "tmp.kml");
  SCOPE_GUARD(fileGuard, std::bind(&FileWriter::DeleteFileX, kmlFile));
  TEST_NO_THROW(
  {
    kml::SerializerKml ser(data);
    FileWriter sink(kmlFile);
    ser.Serialize(sink);
  }, ());

  kml::FileData dataFromGeneratedFile;
  TEST_NO_THROW(
  {
    kml::DeserializerKml des(dataFromGeneratedFile);
    FileReader reader(kmlFile);
    des.Deserialize(reader);
  }, ());
  TEST_EQUAL(dataFromGeneratedFile, data, ());

  kml::FileData dataFromFile;
  TEST_NO_THROW(
  {
    kml::DeserializerKml des(dataFromFile);
    FileReader reader(GetPlatform().TestsDataPathForFile("test_data/kml/generated_mixed_tracks.kml"));
    des.Deserialize(reader);
  }, ());
  TEST_EQUAL(dataFromFile, data, ());
}

// 8. Check binary deserialization of v.3 format.
UNIT_TEST(Kml_Deserialization_From_Bin_V3_And_V4)
{
  kml::FileData dataFromBinV3;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKmlV3.data(), kBinKmlV3.size());
    kml::binary::DeserializerKml des(dataFromBinV3);
    des.Deserialize(reader);
  }, ());

  kml::FileData dataFromBinV4;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKmlV4.data(), kBinKmlV4.size());
    kml::binary::DeserializerKml des(dataFromBinV4);
    des.Deserialize(reader);
  }, ());
  TEST_EQUAL(dataFromBinV3, dataFromBinV4, ());
}

UNIT_TEST(Kml_Deserialization_From_Bin_V6_And_V7)
{
  kml::FileData dataFromBinV6;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKmlV6.data(), kBinKmlV6.size());
    kml::binary::DeserializerKml des(dataFromBinV6);
    des.Deserialize(reader);
  }, ());

  kml::FileData dataFromBinV7;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKmlV7.data(), kBinKmlV7.size());
    kml::binary::DeserializerKml des(dataFromBinV7);
    des.Deserialize(reader);
  }, ());
  TEST_EQUAL(dataFromBinV6, dataFromBinV7, ());
}


UNIT_TEST(Kml_Deserialization_From_Bin_V7_And_V8)
{
  kml::FileData dataFromBinV7;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKmlV7.data(), kBinKmlV7.size());
    kml::binary::DeserializerKml des(dataFromBinV7);
    des.Deserialize(reader);
  }, ());

  kml::FileData dataFromBinV8;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKmlV8.data(), kBinKmlV8.size());
    kml::binary::DeserializerKml des(dataFromBinV8);
    des.Deserialize(reader);
  }, ());
  TEST_EQUAL(dataFromBinV7, dataFromBinV8, ());
}

UNIT_TEST(Kml_Deserialization_From_Bin_V8_And_V8MM)
{
  kml::FileData dataFromBinV8;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKmlV8.data(), kBinKmlV8.size());
    kml::binary::DeserializerKml des(dataFromBinV8);
    des.Deserialize(reader);
  }, ());

  kml::FileData dataFromBinV8MM;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKmlV8MM.data(), kBinKmlV8MM.size());
    kml::binary::DeserializerKml des(dataFromBinV8MM);
    des.Deserialize(reader);
  }, ());

// Can't compare dataFromBinV8.m_categoryData and dataFromBinV8MM.m_categoryData directly
// because new format has less properties and different m_id. Compare some properties here:
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_name, dataFromBinV8MM.m_categoryData.m_name, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_description, dataFromBinV8MM.m_categoryData.m_description, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_annotation, dataFromBinV8MM.m_categoryData.m_annotation, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_accessRules, dataFromBinV8MM.m_categoryData.m_accessRules, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_visible, dataFromBinV8MM.m_categoryData.m_visible, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_rating, dataFromBinV8MM.m_categoryData.m_rating, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_reviewsNumber, dataFromBinV8MM.m_categoryData.m_reviewsNumber, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_tags, dataFromBinV8MM.m_categoryData.m_tags, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_properties, dataFromBinV8MM.m_categoryData.m_properties, ());

  TEST_EQUAL(dataFromBinV8.m_bookmarksData, dataFromBinV8MM.m_bookmarksData, ());
  TEST_EQUAL(dataFromBinV8.m_tracksData, dataFromBinV8MM.m_tracksData, ());
}

UNIT_TEST(Kml_Deserialization_From_KMB_V8_And_V9MM)
{
  kml::FileData dataFromBinV8;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKmlV8.data(), kBinKmlV8.size());
    kml::binary::DeserializerKml des(dataFromBinV8);
    des.Deserialize(reader);
  }, ());

  kml::FileData dataFromBinV9MM;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKmlV9MM.data(), kBinKmlV9MM.size());
    kml::binary::DeserializerKml des(dataFromBinV9MM);
    des.Deserialize(reader);
  }, ());

  // Can't compare dataFromBinV8.m_categoryData and dataFromBinV9MM.m_categoryData directly
  // because new format has less properties and different m_id. Compare some properties here:
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_name, dataFromBinV9MM.m_categoryData.m_name, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_description, dataFromBinV9MM.m_categoryData.m_description, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_annotation, dataFromBinV9MM.m_categoryData.m_annotation, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_accessRules, dataFromBinV9MM.m_categoryData.m_accessRules, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_visible, dataFromBinV9MM.m_categoryData.m_visible, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_rating, dataFromBinV9MM.m_categoryData.m_rating, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_reviewsNumber, dataFromBinV9MM.m_categoryData.m_reviewsNumber, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_tags, dataFromBinV9MM.m_categoryData.m_tags, ());
  TEST_EQUAL(dataFromBinV8.m_categoryData.m_properties, dataFromBinV9MM.m_categoryData.m_properties, ());

  dataFromBinV8.m_bookmarksData[0].m_id = dataFromBinV9MM.m_bookmarksData[0].m_id; // V8 and V9MM bookmarks have different IDs. Fix ID value manually.
  TEST_EQUAL(dataFromBinV8.m_bookmarksData, dataFromBinV9MM.m_bookmarksData, ());

  dataFromBinV8.m_tracksData[0].m_id = dataFromBinV9MM.m_tracksData[0].m_id; // V8 and V9MM tracks have different IDs. Fix ID value manually.
  TEST_EQUAL(dataFromBinV8.m_tracksData, dataFromBinV9MM.m_tracksData, ());
}

UNIT_TEST(Kml_Deserialization_From_KMB_V9MM_With_MultiGeometry)
{
  kml::FileData dataFromBinV9MM;
  TEST_NO_THROW(
  {
    MemReader reader(kBinKmlMultiGeometryV9MM.data(), kBinKmlMultiGeometryV9MM.size());
    kml::binary::DeserializerKml des(dataFromBinV9MM);
    des.Deserialize(reader);
  }, ());

  TEST_EQUAL(dataFromBinV9MM.m_tracksData.size(), 1, ());

  // Verify that geometry has two lines
  auto lines = dataFromBinV9MM.m_tracksData[0].m_geometry.m_lines;
  TEST_EQUAL(lines.size(), 2, ());

  // Verify that each line has 3 points
  auto line1 = lines[0];
  auto line2 = lines[1];

  TEST_EQUAL(line1.size(), 3, ());
  TEST_EQUAL(line2.size(), 3, ());
}

UNIT_TEST(Kml_Ver_2_3)
{
  std::string_view constexpr data = R"(<?xml version="1.0" encoding="UTF-8"?>
    <kml xmlns="http://www.opengis.net/kml/2.2" version="2.3">
      <Placemark id="PM005">
        <Track>
          <when>2010-05-28T02:02:09Z</when>
          <when>2010-05-28T02:02:35Z</when>
          <when>2010-05-28T02:02:44Z</when>
          <when>2010-05-28T02:02:53Z</when>
          <when>2010-05-28T02:02:54Z</when>
          <when>2010-05-28T02:02:55Z</when>
          <when>2010-05-28T02:02:56Z</when>
          <coord>-122.207881 37.371915 156.000000</coord>
          <coord>-122.205712 37.373288 152.000000</coord>
          <coord>-122.204678 37.373939 147.000000</coord>
          <coord>-122.203572 37.374630 142.199997</coord>
          <coord>-122.203451 37.374706 141.800003</coord>
          <coord>-122.203329 37.374780 141.199997</coord>
          <coord>-122.203207 37.374857 140.199997</coord>
        </Track>
        <gx:MultiTrack>
          <altitudeMode>absolute</altitudeMode>
          <gx:interpolate>0</gx:interpolate>
          <gx:Track>
            <gx:coord>9.42666332 52.94270656 95</gx:coord>
            <when>2022-12-25T13:12:01.914Z</when>
            <gx:coord>9.42682572 52.94270115 94</gx:coord>
            <when>2022-12-25T13:12:36Z</when>
            <gx:coord>9.42699411 52.94269624 94</gx:coord>
            <when>2022-12-25T13:12:38Z</when>
            <gx:coord>9.42716915 52.94268793 95</gx:coord>
            <when>2022-12-25T13:12:40Z</when>
            <gx:coord>9.42736231 52.94266046 95</gx:coord>
            <when>2022-12-25T13:12:42Z</when>
            <gx:coord>9.42757536 52.94266963 96</gx:coord>
            <when>2022-12-25T13:12:44Z</when>
            <ExtendedData>
              <SchemaData schemaUrl="#geotrackerTrackSchema">
                <gx:SimpleArrayData name="speed">
                  <gx:value>0</gx:value>
                  <gx:value>3.71</gx:value>
                  <gx:value>5.22</gx:value>
                  <gx:value>6.16</gx:value>
                  <gx:value>7.1</gx:value>
                  <gx:value>7.28</gx:value>
                </gx:SimpleArrayData>
                <gx:SimpleArrayData name="course">
                  <gx:value />
                  <gx:value>1.57</gx:value>
                  <gx:value>1.62</gx:value>
                  <gx:value>1.64</gx:value>
                  <gx:value>1.69</gx:value>
                  <gx:value>1.56</gx:value>
                </gx:SimpleArrayData>
              </SchemaData>
            </ExtendedData>
          </gx:Track>
        </gx:MultiTrack>
      </Placemark>
    </kml>)";

  kml::FileData fData;
  TEST_NO_THROW(
  {
    kml::DeserializerKml(fData).Deserialize(MemReader(data));
  }, ());

  TEST_EQUAL(fData.m_tracksData.size(), 1, ());
  auto const & geom = fData.m_tracksData[0].m_geometry;
  auto const & lines = geom.m_lines;
  auto const & timestamps = geom.m_timestamps;
  TEST_EQUAL(lines.size(), 2, ());
  TEST_EQUAL(lines[0].size(), 7, ());
  TEST_EQUAL(lines[1].size(), 6, ());
  TEST(geom.HasTimestamps(), ());
  TEST(geom.HasTimestampsFor(0), ());
  TEST(geom.HasTimestampsFor(1), ());
  TEST_EQUAL(timestamps.size(), 2, ());
  TEST_EQUAL(timestamps[0].size(), 7, ());
  TEST_EQUAL(timestamps[1].size(), 6, ());
}

UNIT_TEST(Kml_Placemark_contains_both_Bookmark_and_Track_data)
{
  std::string_view constexpr input = R"(<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2">
  <Placemark>
    <MultiGeometry>
      <Point>
        <coordinates>28.968447783842,41.009030507129,0</coordinates>
      </Point>
      <LineString>
        <coordinates>28.968447783842,41.009030507129,0 28.965858,41.018449,0</coordinates>
      </LineString>
    </MultiGeometry>
  </Placemark>
  <Placemark>
  <MultiGeometry>
    <LineString>
      <coordinates>28.968447783842,41.009030507129,0 28.965858,41.018449,0</coordinates>
    </LineString>
    <Point>
      <coordinates>28.968447783842,41.009030507129,0</coordinates>
    </Point>
  </MultiGeometry>
</Placemark>
</kml>)";

  kml::FileData fData;
  TEST_NO_THROW(
  {
    kml::DeserializerKml(fData).Deserialize(MemReader(input));
  }, ());

  TEST_EQUAL(fData.m_bookmarksData.size(), 2, ());
  TEST_EQUAL(fData.m_tracksData.size(), 2, ());

  TEST(!fData.m_tracksData[0].m_geometry.HasTimestamps(), ());
  TEST(!fData.m_tracksData[1].m_geometry.HasTimestamps(), ());
}

// See https://github.com/organicmaps/organicmaps/issues/5800
UNIT_TEST(Fix_Invisible_Color_Bug_In_Gpx_Tracks)
{
  std::string_view constexpr input = R"(<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://earth.google.com/kml/2.2">
<Document>
  <name>2023-08-20 Malente Radtour</name>
  <visibility>1</visibility>
  <Placemark>
    <name>2023-08-20 Malente Radtour</name>
    <Style><LineStyle>
      <color>01000000</color>
      <width>3</width>
    </LineStyle></Style>
    <LineString><coordinates>10.565979,54.16597,26 10.565956,54.165997,26</coordinates></LineString>
  </Placemark>
  <Placemark>
    <name>Test default colors and width</name>
    <LineString><coordinates>10.465979,54.16597,26 10.465956,54.165997,26</coordinates></LineString>
  </Placemark>
</Document>
</kml>)";

  kml::FileData fData;
  TEST_NO_THROW(
  {
    kml::DeserializerKml(fData).Deserialize(MemReader(input));
  }, ());

  TEST_EQUAL(fData.m_tracksData.size(), 2, ());
  TEST_EQUAL(fData.m_tracksData[0].m_layers.size(), 1, ());
  auto const & layer = fData.m_tracksData[0].m_layers[0];
  TEST_EQUAL(layer.m_color.m_rgba, kml::kDefaultTrackColor, ("Wrong transparency should be fixed"));
  TEST_EQUAL(layer.m_lineWidth, 3, ());
}

UNIT_TEST(Kml_Tracks_With_Different_Points_And_Timestamps_Order)
{
  kml::FileData dataFromFile;
  TEST_NO_THROW(
  {
    kml::DeserializerKml des(dataFromFile);
    FileReader reader(GetPlatform().TestsDataPathForFile("test_data/kml/track_with_timestams_different_orders.kml"));
    des.Deserialize(reader);
  }, ());

  TEST_EQUAL(dataFromFile.m_tracksData.size(), 1, ());
  auto const & geom = dataFromFile.m_tracksData[0].m_geometry;
  TEST_EQUAL(geom.m_lines.size(), 4, ());
  TEST_EQUAL(geom.m_timestamps.size(), 4, ());
  TEST_EQUAL(geom.m_lines[0], geom.m_lines[1], ());
  TEST_EQUAL(geom.m_lines[0], geom.m_lines[2], ());
  TEST_EQUAL(geom.m_lines[0], geom.m_lines[3], ());
  TEST_EQUAL(geom.m_timestamps[0], geom.m_timestamps[1], ());
  TEST_EQUAL(geom.m_timestamps[0], geom.m_timestamps[2], ());
  TEST_EQUAL(geom.m_timestamps[0], geom.m_timestamps[3], ());
}

UNIT_TEST(Kml_Track_Points_And_Timestamps_Sizes_Mismatch)
{
  kml::FileData dataFromFile;
  TEST_ANY_THROW(
  {
    kml::DeserializerKml des(dataFromFile);
    FileReader reader(GetPlatform().TestsDataPathForFile("test_data/kml/track_with_timestamps_mismatch.kml"));
    des.Deserialize(reader);
  }, ());
  TEST_EQUAL(dataFromFile.m_tracksData.size(), 0, ());
}

// https://github.com/organicmaps/organicmaps/issues/9290
UNIT_TEST(Kml_Import_OpenTracks)
{
  std::string_view constexpr input = R"(<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://earth.google.com/kml/2.2">
    <Placemark>
      <Track>
        <when>2010-05-28T02:00Z</when>
        <when>2010-05-28T02:01Z</when>
        <when>2010-05-28T02:02Z</when>
        <when>2010-05-28T02:03Z</when>
        <when>2010-05-28T02:04Z</when>
        <coord/>
        <coord>-122.205712 37.373288 152.000000</coord>
        <coord>Abra-cadabra</coord>
        <coord>-122.203572 37.374630 142.199997</coord>
        <coord/>
      </Track>
    </Placemark>
</kml>)";

  kml::FileData fData;
  TEST_NO_THROW(
  {
    kml::DeserializerKml(fData).Deserialize(MemReader(input));
  }, ());

  {
    TEST_EQUAL(fData.m_tracksData.size(), 1, ());
    auto const & geom = fData.m_tracksData[0].m_geometry;
    TEST_EQUAL(geom.m_lines.size(), 1, ());
    TEST_EQUAL(geom.m_lines.size(), geom.m_timestamps.size(), ());
    TEST_EQUAL(geom.m_lines[0].size(), 2, ());
    TEST_EQUAL(geom.m_lines[0].size(), geom.m_timestamps[0].size(), ());
    TEST_EQUAL(geom.m_timestamps[0][0], base::StringToTimestamp("2010-05-28T02:01Z"), ());
    TEST_EQUAL(geom.m_timestamps[0][1], base::StringToTimestamp("2010-05-28T02:03Z"), ());
  }

  fData = {};
  TEST_NO_THROW(
  {
    kml::DeserializerKml des(fData);
    FileReader reader(GetPlatform().TestsDataPathForFile("test_data/kml/track_from_OpenTracks.kml"));
    des.Deserialize(reader);
  }, ());

  {
    TEST_EQUAL(fData.m_tracksData.size(), 1, ());
    auto const & geom = fData.m_tracksData[0].m_geometry;
    TEST_EQUAL(geom.m_lines.size(), 1, ());
    TEST_EQUAL(geom.m_lines.size(), geom.m_timestamps.size(), ());
    TEST_GREATER(geom.m_lines[0].size(), 10, ());
  }
}

UNIT_TEST(Kml_BadTracks)
{
  std::string_view constexpr input = R"(<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://earth.google.com/kml/2.2">
    <Placemark>
      <Track>
        <when>2010-05-28T02:00Z</when>
        <coord>-122.205712 37.373288 152.000000</coord>
      </Track>
      <gx:Track>
        <gx:coord>9.42666332 52.94270656 95</gx:coord>
        <when>2022-12-25T13:12:01.914Z</when>
      </gx:Track>
      <gx:Track>
        <gx:coord>9.42666332 52.94270656 95</gx:coord>
        <when>2022-12-25T13:12:01.914Z</when>
        <gx:coord>9.42682572 52.94270115 94</gx:coord>
        <when>2022-12-25T13:12:36Z</when>
      </gx:Track>
    </Placemark>
</kml>)";

  kml::FileData fData;
  TEST_NO_THROW(
  {
    kml::DeserializerKml(fData).Deserialize(MemReader(input));
  }, ());

  {
    TEST_EQUAL(fData.m_tracksData.size(), 1, ());
    auto const & geom = fData.m_tracksData[0].m_geometry;
    TEST_EQUAL(geom.m_lines.size(), 1, ());
    TEST_EQUAL(geom.m_lines.size(), geom.m_timestamps.size(), ());
    TEST_EQUAL(geom.m_lines[0].size(), 2, ());
    TEST_EQUAL(geom.m_lines[0].size(), geom.m_timestamps[0].size(), ());
  }
}
