#include "testing/testing.hpp"

#include "generator/generator_tests/common.hpp"
#include "generator/generator_tests_support/routing_helpers.hpp"
#include "generator/generator_tests_support/test_feature.hpp"
#include "generator/generator_tests_support/test_mwm_builder.hpp"
#include "generator/osm2type.hpp"
#include "generator/road_access_generator.hpp"

#include "routing/road_access_serialization.hpp"

#include "indexer/classificator_loader.hpp"

#include "platform/country_file.hpp"
#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"

#include "geometry/point2d.hpp"

#include "coding/files_container.hpp"

#include "base/file_name_utils.hpp"
#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace road_access_test
{
using namespace feature;
using namespace generator;
using namespace platform::tests_support;
using namespace platform;
using namespace routing;
using namespace routing_builder;
using std::fstream, std::ifstream, std::make_pair, std::string;

string const kTestDir = "road_access_generation_test";
string const kTestMwm = "test";
string const kRoadAccessFilename = "road_access_in_osm_ids.csv";
string const kOsmIdsToFeatureIdsName = "osm_ids_to_feature_ids" OSM2FEATURE_FILE_EXTENSION;

void BuildTestMwmWithRoads(LocalCountryFile & country)
{
  generator::tests_support::TestMwmBuilder builder(country, feature::DataHeader::MapType::Country);

  for (size_t i = 0; i < 10; ++i)
  {
    string const name = "road " + strings::to_string(i);
    string const lang = "en";
    std::vector<m2::PointD> points;
    for (size_t j = 0; j < 10; ++j)
      points.emplace_back(static_cast<double>(i), static_cast<double>(j));

    builder.Add(generator::tests_support::TestRoad(points, name, lang));
  }
}

void LoadRoadAccess(string const & mwmFilePath, VehicleType vehicleType, RoadAccess & roadAccess)
{
  try
  {
    FilesContainerR const cont(mwmFilePath);
    FilesContainerR::TReader const reader = cont.GetReader(ROAD_ACCESS_FILE_TAG);
    ReaderSource<FilesContainerR::TReader> src(reader);
    RoadAccessSerializer::Deserialize(src, vehicleType, roadAccess);
  }
  catch (Reader::Exception const & e)
  {
    TEST(false, ("Error while reading", ROAD_ACCESS_FILE_TAG, "section.", e.Msg()));
  }
}

// todo(@m) This helper function is almost identical to the one in restriction_test.cpp.
RoadAccessCollector::RoadAccessByVehicleType SaveAndLoadRoadAccess(
    string const & raContent, string const & mappingContent, string const & raContitionalContent = {})
{
  classificator::Load();

  Platform & platform = GetPlatform();
  string const & writableDir = platform.WritableDir();

  // Building empty mwm.
  LocalCountryFile country(base::JoinPath(writableDir, kTestDir), CountryFile(kTestMwm),
                           0 /* version */);
  ScopedDir const scopedDir(kTestDir);
  string const mwmRelativePath = base::JoinPath(kTestDir, kTestMwm + DATA_FILE_EXTENSION);
  ScopedFile const scopedMwm(mwmRelativePath, ScopedFile::Mode::Create);
  BuildTestMwmWithRoads(country);

  // Creating a file with road access.
  string const roadAccessRelativePath = base::JoinPath(kTestDir, kRoadAccessFilename);
  ScopedFile const raFile(roadAccessRelativePath, raContent);
  ScopedFile const raConditionalFile(roadAccessRelativePath + CONDITIONAL_EXT, raContitionalContent);

  // Creating osm ids to feature ids mapping.
  string const mappingRelativePath = base::JoinPath(kTestDir, kOsmIdsToFeatureIdsName);
  ScopedFile const mappingFile(mappingRelativePath, ScopedFile::Mode::Create);
  string const & mappingFullPath = mappingFile.GetFullPath();
  ReEncodeOsmIdsToFeatureIdsMapping(mappingContent, mappingFullPath);

  // Adding road access section to mwm.
  string const roadAccessFullPath = base::JoinPath(writableDir, roadAccessRelativePath);
  string const mwmFullPath = base::JoinPath(writableDir, mwmRelativePath);
  BuildRoadAccessInfo(mwmFullPath, roadAccessFullPath, mappingFullPath);

  // Reading from mwm section and testing road access.
  RoadAccessCollector::RoadAccessByVehicleType roadAccessFromMwm;
  for (size_t i = 0; i < static_cast<size_t>(VehicleType::Count); ++i)
  {
    auto const vehicleType = static_cast<VehicleType>(i);
    LoadRoadAccess(mwmFullPath, vehicleType, roadAccessFromMwm[i]);
  }
  RoadAccessCollector const collector(roadAccessFullPath, mappingFullPath);
  TEST(collector.IsValid(), ());
  TEST_EQUAL(roadAccessFromMwm, collector.GetRoadAccessAllTypes(), ());
  return roadAccessFromMwm;
}

OsmElement MakeOsmElementWithNodes(uint64_t id, generator_tests::Tags const & tags,
                                   OsmElement::EntityType t, std::vector<uint64_t> const & nodes)
{
  auto r = generator_tests::MakeOsmElement(id, tags, t);
  r.m_nodes = nodes;
  return r;
}

feature::FeatureBuilder MakeFbForTest(OsmElement element)
{
  feature::FeatureBuilder result;
  ftype::GetNameAndType(&element, result.GetParams());
  return result;
}

string GetFileContent(string const & name)
{
  ifstream stream(name);
  TEST(stream.is_open(), ());
  return string(istreambuf_iterator<char>(stream), istreambuf_iterator<char>());
}

UNIT_TEST(RoadAccess_Smoke)
{
  string const roadAccessContent;
  string const osmIdsToFeatureIdsContent;
  SaveAndLoadRoadAccess(roadAccessContent, osmIdsToFeatureIdsContent);
}

UNIT_TEST(RoadAccess_AccessPrivate)
{
  string const roadAccessContent = R"(Car Private 0 0)";
  string const osmIdsToFeatureIdsContent = R"(0, 0,)";
  auto const roadAccessAllTypes =
      SaveAndLoadRoadAccess(roadAccessContent, osmIdsToFeatureIdsContent);
  auto const & carRoadAccess = roadAccessAllTypes[static_cast<size_t>(VehicleType::Car)];
  TEST_EQUAL(carRoadAccess.GetAccessWithoutConditional(0 /* featureId */),
             make_pair(RoadAccess::Type::Private, RoadAccess::Confidence::Sure), ());
}

UNIT_TEST(RoadAccess_Access_Multiple_Vehicle_Types)
{
  string const roadAccessContent = R"(Car Private 10 0
                                      Car Private 20 0
                                      Bicycle No 30 0
                                      Car Destination 40 0)";
  string const osmIdsToFeatureIdsContent = R"(10, 1,
                                              20, 2,
                                              30, 3,
                                              40, 4,)";
  auto const roadAccessAllTypes =
      SaveAndLoadRoadAccess(roadAccessContent, osmIdsToFeatureIdsContent);
  auto const & carRoadAccess = roadAccessAllTypes[static_cast<size_t>(VehicleType::Car)];
  auto const & bicycleRoadAccess = roadAccessAllTypes[static_cast<size_t>(VehicleType::Bicycle)];
  TEST_EQUAL(carRoadAccess.GetAccessWithoutConditional(1 /* featureId */),
             make_pair(RoadAccess::Type::Private, RoadAccess::Confidence::Sure), ());

  TEST_EQUAL(carRoadAccess.GetAccessWithoutConditional(2 /* featureId */),
             make_pair(RoadAccess::Type::Private, RoadAccess::Confidence::Sure), ());

  TEST_EQUAL(carRoadAccess.GetAccessWithoutConditional(3 /* featureId */),
             make_pair(RoadAccess::Type::Yes, RoadAccess::Confidence::Sure), ());

  TEST_EQUAL(carRoadAccess.GetAccessWithoutConditional(4 /* featureId */),
             make_pair(RoadAccess::Type::Destination, RoadAccess::Confidence::Sure), ());

  TEST_EQUAL(bicycleRoadAccess.GetAccessWithoutConditional(3 /* featureId */),
             make_pair(RoadAccess::Type::No, RoadAccess::Confidence::Sure), ());
}

UNIT_TEST(RoadAccessWriter_Permit)
{
  classificator::Load();

  auto const filename = generator_tests::GetFileName();
  SCOPE_GUARD(_, bind(Platform::RemoveFileIfExists, cref(filename)));

  auto const w = MakeOsmElementWithNodes(1 /* id */,
                                         {{"highway", "motorway"}, {"access", "no"}, {"motor_vehicle", "permit"}},
                                         OsmElement::EntityType::Way, {1, 2});

  auto c = make_shared<RoadAccessWriter>(filename);
  c->CollectFeature(MakeFbForTest(w), w);

  c->Finish();
  c->Finalize();

  string const correctAnswer = "Pedestrian No 1 0\n"
                               "Bicycle No 1 0\n"
                               "Car Private 1 0\n";
  TEST_EQUAL(GetFileContent(filename), correctAnswer, ());
}

UNIT_TEST(RoadAccessWriter_Merge)
{
  classificator::Load();
  auto const filename = generator_tests::GetFileName();
  SCOPE_GUARD(_, bind(Platform::RemoveFileIfExists, cref(filename)));

  auto const w1 = MakeOsmElementWithNodes(1 /* id */, {{"highway", "service"}} /* tags */,
                                          OsmElement::EntityType::Way, {10, 11, 12, 13});
  auto const w2 = MakeOsmElementWithNodes(2 /* id */, {{"highway", "service"}} /* tags */,
                                          OsmElement::EntityType::Way, {20, 21, 22, 23});
  auto const w3 = MakeOsmElementWithNodes(3 /* id */, {{"highway", "motorway"}} /* tags */,
                                          OsmElement::EntityType::Way, {30, 31, 32, 33});

  auto const p1 = generator_tests::MakeOsmElement(
      11 /* id */, {{"barrier", "lift_gate"}, {"motor_vehicle", "private"}},
      OsmElement::EntityType::Node);

  auto const p2 = generator_tests::MakeOsmElement(
      22 /* id */, {{"barrier", "lift_gate"}, {"motor_vehicle", "private"}},
      OsmElement::EntityType::Node);

  // We should ignore this barrier because it's without access tag and placed on highway-motorway.
  auto const p3 = generator_tests::MakeOsmElement(
      32 /* id */, {{"barrier", "lift_gate"}},
      OsmElement::EntityType::Node);

  // Ignore all motorway_junction access.
  auto const p4 = generator_tests::MakeOsmElement(
      31 /* id */, {{"highway", "motorway_junction"}, {"access", "private"}},
      OsmElement::EntityType::Node);

  auto c1 = std::make_shared<RoadAccessWriter>(filename);
  auto c2 = c1->Clone(nullptr);
  auto c3 = c1->Clone(nullptr);

  c1->CollectFeature(MakeFbForTest(p1), p1);
  c2->CollectFeature(MakeFbForTest(p2), p2);
  c3->CollectFeature(MakeFbForTest(p3), p3);
  c1->CollectFeature(MakeFbForTest(p4), p4);

  c1->CollectFeature(MakeFbForTest(w1), w1);
  c2->CollectFeature(MakeFbForTest(w2), w2);
  c3->CollectFeature(MakeFbForTest(w3), w3);

  c1->Finish();
  c2->Finish();
  c3->Finish();

  c1->Merge(*c2);
  c1->Merge(*c3);

  c1->Finalize();

  string const correctAnswer = "Car Private 1 2\n"
                               "Car Private 2 3\n";
  TEST_EQUAL(GetFileContent(filename), correctAnswer, ());
}

UNIT_TEST(RoadAccessCoditional_Parse)
{
  AccessConditionalTagParser parser;

  using ConditionalVector = std::vector<AccessConditional>;
  std::vector<std::pair<string, ConditionalVector>> const tests = {
      {"no @ Mo-Su",
       {{RoadAccess::Type::No, "Mo-Su"}}},

      {"no @ Mo-Su;",
       {{RoadAccess::Type::No, "Mo-Su"}}},

      {"yes @ (10:00 - 20:00)",
       {{RoadAccess::Type::Yes, "10:00 - 20:00"}}},

      {"private @ Mo-Fr 15:00-20:00",
       {{RoadAccess::Type::Private, "Mo-Fr 15:00-20:00"}}},

      {"destination @ 10:00-20:00",
       {{RoadAccess::Type::Destination, "10:00-20:00"}}},

      {"yes @ Mo-Fr ; Sa-Su",
       {{RoadAccess::Type::Yes, "Mo-Fr ; Sa-Su"}}},

      {"no @ (Mo-Su) ; yes @ (Fr-Su)",
       {{RoadAccess::Type::No, "Mo-Su"},

        {RoadAccess::Type::Yes, "Fr-Su"}}},
      {"private @ (18:00-09:00; Oct-Mar)", {{RoadAccess::Type::Private, "18:00-09:00; Oct-Mar"}}},

      {"no @ (Nov-May); no @ (20:00-07:00)",
       {{RoadAccess::Type::No, "Nov-May"},
        {RoadAccess::Type::No, "20:00-07:00"}}},

      {"no @ 22:30-05:00",
       {{RoadAccess::Type::No, "22:30-05:00"}}},

      {"destination @ (Mo-Fr 06:00-15:00); yes @ (Mo-Fr 15:00-21:00; Sa,Su,SH,PH 09:00-21:00)",
       {{RoadAccess::Type::Destination, "Mo-Fr 06:00-15:00"},
        {RoadAccess::Type::Yes, "Mo-Fr 15:00-21:00; Sa,Su,SH,PH 09:00-21:00"}}},

      {"no @ (Mar 15-Jul 15); private @ (Jan- Dec)",
       {{RoadAccess::Type::No, "Mar 15-Jul 15"},
        {RoadAccess::Type::Private, "Jan- Dec"}}},

      {"no @ (06:30-08:30);destination @ (06:30-08:30 AND agricultural)",
       {{RoadAccess::Type::No, "06:30-08:30"},
        {RoadAccess::Type::Destination, "06:30-08:30 AND agricultural"}}},

      {"no @ (Mo-Fr 00:00-08:00,20:00-24:00; Sa-Su 00:00-24:00; PH 00:00-24:00)",
       {{RoadAccess::Type::No, "Mo-Fr 00:00-08:00,20:00-24:00; Sa-Su 00:00-24:00; PH 00:00-24:00"}}},

      // Not valid cases
      {"trash @ (Mo-Fr 00:00-10:00)", {{RoadAccess::Type::Count, "Mo-Fr 00:00-10:00"}}},
      {"yes Mo-Fr", {}},
      {"yes (Mo-Fr)", {}},
      {"no ; Mo-Fr", {}},
      {"asdsadasdasd", {}}
  };

  std::vector<string> tags = {
      "motorcar:conditional",
      "vehicle:conditional",
      "motor_vehicle:conditional",
      "bicycle:conditional",
      "foot:conditional"
  };

  for (auto const & tag : tags)
  {
    for (auto const & [value, answer] : tests)
    {
      auto const access = parser.ParseAccessConditionalTag(tag, value);
      TEST(access == answer, (value, tag));
    }
  }
}

UNIT_TEST(RoadAccessCoditional_Collect)
{
  // Exotic cases
  auto const roadAccessAllTypes = SaveAndLoadRoadAccess(
        {}, R"(578127581, 0,)", R"(Car	578127581	1	No	wind_speed>=65)");
  auto const carRoadAccess = roadAccessAllTypes[static_cast<size_t>(VehicleType::Car)];
  TEST_EQUAL(carRoadAccess.GetAccess(0 /* featureId */, RouteWeight{}),
             make_pair(RoadAccess::Type::Yes, RoadAccess::Confidence::Sure), ());
}

UNIT_TEST(RoadAccessWriter_ConditionalMerge)
{
  classificator::Load();
  auto const filename = generator_tests::GetFileName();
  SCOPE_GUARD(_, bind(Platform::RemoveFileIfExists, cref(filename)));

  auto const w1 = MakeOsmElementWithNodes(
      1 /* id */, {{"highway", "primary"}, {"vehicle:conditional", "no @ (Mo-Su)"}} /* tags */,
      OsmElement::EntityType::Way, {10, 11, 12, 13});

  auto const w2 = MakeOsmElementWithNodes(
      2 /* id */,
      {{"highway", "service"}, {"vehicle:conditional", "private @ (10:00-20:00)"}} /* tags */,
      OsmElement::EntityType::Way, {20, 21, 22, 23});

  auto const w3 = MakeOsmElementWithNodes(
      3 /* id */,
      {{"highway", "service"},
       {"vehicle:conditional", "private @ (12:00-19:00) ; no @ (Mo-Su)"}} /* tags */,
      OsmElement::EntityType::Way, {30, 31, 32, 33});

  auto c1 = std::make_shared<RoadAccessWriter>(filename);
  auto c2 = c1->Clone(nullptr);
  auto c3 = c1->Clone(nullptr);

  c1->CollectFeature(MakeFbForTest(w1), w1);
  c2->CollectFeature(MakeFbForTest(w2), w2);
  c3->CollectFeature(MakeFbForTest(w3), w3);

  c1->Finish();
  c2->Finish();
  c3->Finish();

  c1->Merge(*c2);
  c1->Merge(*c3);

  c1->Finalize(true /*isStable*/);

  string const expectedFile =
      "Car\t1\t1\tNo\tMo-Su\t\n"
      "Car\t2\t1\tPrivate\t10:00-20:00\t\n"
      "Car\t3\t2\tPrivate\t12:00-19:00\tNo\tMo-Su\t\n";

  TEST_EQUAL(GetFileContent(filename + CONDITIONAL_EXT), expectedFile, ());
}

UNIT_TEST(RoadAccessWriter_Conditional_WinterRoads)
{
  classificator::Load();
  auto const filename = generator_tests::GetFileName();
  SCOPE_GUARD(_, bind(Platform::RemoveFileIfExists, cref(filename)));

  auto const w1 = MakeOsmElementWithNodes(
      1 /* id */, {{"highway", "primary"}, {"ice_road", "yes"}} /* tags */,
      OsmElement::EntityType::Way, {10, 11, 12, 13});

  auto const w2 = MakeOsmElementWithNodes(
      2 /* id */,
      {{"highway", "service"}, {"winter_road", "yes"}} /* tags */,
      OsmElement::EntityType::Way, {20, 21, 22, 23});

  auto c1 = std::make_shared<RoadAccessWriter>(filename);

  c1->CollectFeature(MakeFbForTest(w1), w1);
  c1->CollectFeature(MakeFbForTest(w2), w2);

  c1->Finish();
  c1->Finalize(true /*isStable*/);

  string const expectedFile =
      "Bicycle\t1\t1\tNo\tMar - Nov\t\n"
      "Bicycle\t2\t1\tNo\tMar - Nov\t\n"
      "Car\t1\t1\tNo\tMar - Nov\t\n"
      "Car\t2\t1\tNo\tMar - Nov\t\n"
      "Pedestrian\t1\t1\tNo\tMar - Nov\t\n"
      "Pedestrian\t2\t1\tNo\tMar - Nov\t\n";

  TEST_EQUAL(GetFileContent(filename + CONDITIONAL_EXT), expectedFile, ());
}
}  // namespace road_access_test
