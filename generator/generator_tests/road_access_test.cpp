#include "testing/testing.hpp"

#include "generator/generator_tests/common.hpp"
#include "generator/intermediate_data.hpp"
#include "generator/osm2type.hpp"
#include "generator/road_access_generator.hpp"
#include "generator/routing_helpers.hpp"

#include "indexer/classificator_loader.hpp"

#include "platform/platform.hpp"

#include "geometry/point2d.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace road_access_test
{
using namespace generator;
using namespace generator_tests;
using namespace platform;
using namespace routing;
using namespace routing_builder;
using std::fstream, std::ifstream, std::make_pair, std::string;

/*
string const kTestDir = "road_access_generation_test";
string const kTestMwm = "test";
string const kRoadAccessFilename = "road_access_in_osm_ids.bin";
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
RoadAccessByVehicleType SaveAndLoadRoadAccess(
    string const & raContent, string const & mappingContent, string const & raContitionalContent = {})
{
  classificator::Load();

  Platform & platform = GetPlatform();
  string const & writableDir = platform.WritableDir();

  using namespace platform::tests_support;

  // Building empty mwm.
  LocalCountryFile country(base::JoinPath(writableDir, kTestDir), CountryFile(kTestMwm), 0);
  ScopedDir const scopedDir(kTestDir);
  ScopedFile const scopedMwm(base::JoinPath(kTestDir, kTestMwm + DATA_FILE_EXTENSION), ScopedFile::Mode::Create);
  string const mwmFullPath = scopedMwm.GetFullPath();
  BuildTestMwmWithRoads(country);

  // Creating a file with road access.
  ScopedFile const raFile(base::JoinPath(kTestDir, kRoadAccessFilename), raContent);
  string const roadAccessFullPath = raFile.GetFullPath();

  // Creating osm ids to feature ids mapping.
  ScopedFile const mappingFile(base::JoinPath(kTestDir, kOsmIdsToFeatureIdsName), ScopedFile::Mode::Create);
  string const mappingFullPath = mappingFile.GetFullPath();
  ReEncodeOsmIdsToFeatureIdsMapping(mappingContent, mappingFullPath);

  // Adding road access section to mwm.
  auto osm2feature = CreateWay2FeatureMapper(mwmFullPath, mappingFullPath);
  BuildRoadAccessInfo(mwmFullPath, roadAccessFullPath, *osm2feature);

  // Reading from mwm section and testing road access.
  RoadAccessByVehicleType roadAccessFromMwm, roadAccessFromFile;
  for (size_t i = 0; i < static_cast<size_t>(VehicleType::Count); ++i)
  {
    auto const vehicleType = static_cast<VehicleType>(i);
    LoadRoadAccess(mwmFullPath, vehicleType, roadAccessFromMwm[i]);
  }

  ReadRoadAccess(roadAccessFullPath, *osm2feature, roadAccessFromFile);
  TEST_EQUAL(roadAccessFromMwm, roadAccessFromFile, ());
  return roadAccessFromMwm;
}
*/

OsmElement MakeOsmElementWithNodes(uint64_t id, generator_tests::Tags const & tags,
                                   OsmElement::EntityType t, std::vector<uint64_t> const & nodes)
{
  auto r = generator_tests::MakeOsmElement(id, tags, t);
  r.m_nodes = nodes;
  return r;
}

class IntermediateDataTest : public cache::IntermediateDataReaderInterface
{
  std::map<cache::Key, WayElement> m_map;

public:
  virtual bool GetNode(cache::Key id, double & y, double & x) const { UNREACHABLE(); }
  virtual bool GetWay(cache::Key id, WayElement & e)
  {
    auto it = m_map.find(id);
    if (it != m_map.end())
    {
      e = it->second;
      return true;
    }
    return false;
  }
  virtual bool GetRelation(cache::Key id, RelationElement & e) { UNREACHABLE(); }

  void Add(OsmElement const & e)
  {
    TEST(m_map.try_emplace(e.m_id, e.m_id, e.m_nodes).second, ());
  }
};


/*
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
  TEST_EQUAL(carRoadAccess.GetAccessWithoutConditional(0),
             make_pair(RoadAccess::Type::Private, RoadAccess::Confidence::Sure), ());
}

UNIT_TEST(RoadAccess_Multiple_Vehicle_Types)
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
  TEST_EQUAL(carRoadAccess.GetAccessWithoutConditional(1),
             make_pair(RoadAccess::Type::Private, RoadAccess::Confidence::Sure), ());

  TEST_EQUAL(carRoadAccess.GetAccessWithoutConditional(2),
             make_pair(RoadAccess::Type::Private, RoadAccess::Confidence::Sure), ());

  TEST_EQUAL(carRoadAccess.GetAccessWithoutConditional(3),
             make_pair(RoadAccess::Type::Yes, RoadAccess::Confidence::Sure), ());

  TEST_EQUAL(carRoadAccess.GetAccessWithoutConditional(4),
             make_pair(RoadAccess::Type::Destination, RoadAccess::Confidence::Sure), ());

  TEST_EQUAL(bicycleRoadAccess.GetAccessWithoutConditional(3),
             make_pair(RoadAccess::Type::No, RoadAccess::Confidence::Sure), ());
}
*/

class TestAccessFixture
{
  std::shared_ptr<IntermediateDataTest> m_cache;
  std::vector<std::shared_ptr<RoadAccessCollector>> m_collectors;
  std::string m_fileName;

  class Way2Feature : public OsmWay2FeaturePoint
  {
  public:
    virtual void ForEachFeature(uint64_t wayID, std::function<void (uint32_t)> const & fn) override
    {
      fn(base::checked_cast<uint32_t>(wayID));
    }
    virtual void ForEachNodeIdx(uint64_t wayID, uint32_t candidateIdx, m2::PointU pt,
                                std::function<void (uint32_t, uint32_t)> const & fn) override
    {
      auto const ll = mercator::ToLatLon(PointUToPointD(pt, kPointCoordBits, mercator::Bounds::FullRect()));

      // We use nodes like id = 1 {10, 11, 12}; id = 2 {20, 21, 22} for ways.
      uint32_t const node = round(ll.m_lon);
      uint32_t const featureID = base::checked_cast<uint32_t>(wayID);
      TEST_EQUAL(featureID, node / 10, ());
      fn(featureID, node % 10);
    }
  } m_wya2feature;

  RoadAccessByVehicleType m_roadAccess;

public:
  TestAccessFixture()
    : m_cache(std::make_shared<IntermediateDataTest>())
    , m_fileName(generator_tests::GetFileName(ROAD_ACCESS_FILENAME))
  {
    classificator::Load();
  }
  ~TestAccessFixture()
  {
    TEST(Platform::RemoveFileIfExists(m_fileName), ());
  }

  void CreateCollectors(size_t count = 1)
  {
    for (size_t i = 0; i < count; ++i)
      m_collectors.push_back(std::make_shared<RoadAccessCollector>(m_fileName, m_cache));
  }

  void AddWay(OsmElement way, size_t idx = 0)
  {
    feature::FeatureBuilder builder;
    ftype::GetNameAndType(&way, builder.GetParams());
    builder.SetLinear();

    m_cache->Add(way);

    m_collectors[idx]->CollectFeature(builder, way);
  }

  void AddNode(OsmElement node, size_t idx = 0)
  {
    // Assign unique coordinates as id.
    node.m_lat = node.m_lon = node.m_id;

    feature::FeatureBuilder builder;
    ftype::GetNameAndType(&node, builder.GetParams());
    builder.SetCenter(mercator::FromLatLon(node.m_lat, node.m_lon));

    m_collectors[idx]->CollectFeature(builder, node);
  }

  void Finish()
  {
    for (auto const & c : m_collectors)
      c->Finish();

    for (size_t i = 1; i < m_collectors.size(); ++i)
      m_collectors[0]->Merge(*m_collectors[i]);

    m_collectors[0]->Finalize();

    ReadRoadAccess(m_fileName, m_wya2feature, m_roadAccess);
  }

  RoadAccess const & Get(VehicleType vehicle) const
  {
    return m_roadAccess[static_cast<uint8_t>(vehicle)];
  }
};

UNIT_CLASS_TEST(TestAccessFixture, CarPermit)
{
  CreateCollectors();
  AddWay(MakeOsmElementWithNodes(1 /* id */,
                                 {{"highway", "motorway"}, {"access", "no"}, {"motor_vehicle", "permit"}},
                                 OsmElement::EntityType::Way, {1, 2}));
  Finish();

  auto const noSure = make_pair(RoadAccess::Type::No, RoadAccess::Confidence::Sure);
  TEST_EQUAL(Get(VehicleType::Pedestrian).GetAccessWithoutConditional(1), noSure, ());
  TEST_EQUAL(Get(VehicleType::Bicycle).GetAccessWithoutConditional(1), noSure, ());

  TEST_EQUAL(Get(VehicleType::Car).GetAccessWithoutConditional(1),
             make_pair(RoadAccess::Type::Private, RoadAccess::Confidence::Sure), ());
}

// https://www.openstreetmap.org/way/797145238#map=19/-34.61801/-58.36501
UNIT_CLASS_TEST(TestAccessFixture, HgvDesignated)
{
  CreateCollectors();
  AddWay(MakeOsmElementWithNodes(1 /* id */,
                                 {{"highway", "motorway"}, {"access", "no"}, {"emergency", "yes"},
                                  {"bus", "yes"}, {"hgv", "designated"}, {"motor_vehicle", "yes"}},
                                 OsmElement::EntityType::Way, {1, 2}));
  AddWay(MakeOsmElementWithNodes(2 /* id */,
                                 {{"highway", "motorway"}, {"access", "no"}, {"emergency", "yes"},
                                  {"bus", "yes"}, {"hgv", "designated"}},
                                 OsmElement::EntityType::Way, {2, 3}));
  Finish();

  auto const noSure = make_pair(RoadAccess::Type::No, RoadAccess::Confidence::Sure);
  TEST_EQUAL(Get(VehicleType::Pedestrian).GetAccessWithoutConditional(1), noSure, ());
  TEST_EQUAL(Get(VehicleType::Bicycle).GetAccessWithoutConditional(1), noSure, ());

  TEST_EQUAL(Get(VehicleType::Car).GetAccessWithoutConditional(1),
             make_pair(RoadAccess::Type::Yes, RoadAccess::Confidence::Sure), ());
  TEST_EQUAL(Get(VehicleType::Car).GetAccessWithoutConditional(2),
             make_pair(RoadAccess::Type::No, RoadAccess::Confidence::Sure), ());
}

UNIT_CLASS_TEST(TestAccessFixture, Merge)
{
  CreateCollectors(3);

  AddWay(MakeOsmElementWithNodes(1 /* id */, {{"highway", "service"}} /* tags */,
                                 OsmElement::EntityType::Way, {10, 11, 12, 13}), 0);
  AddWay(MakeOsmElementWithNodes(2 /* id */, {{"highway", "service"}} /* tags */,
                                 OsmElement::EntityType::Way, {20, 21, 22, 23}), 1);
  AddWay(MakeOsmElementWithNodes(3 /* id */, {{"highway", "motorway"}} /* tags */,
                                 OsmElement::EntityType::Way, {30, 31, 32, 33}), 2);

  AddNode(MakeOsmElement(11 /* id */, {{"barrier", "lift_gate"}, {"motor_vehicle", "private"}},
                         OsmElement::EntityType::Node), 0);

  AddNode(MakeOsmElement(22 /* id */, {{"barrier", "lift_gate"}, {"motor_vehicle", "private"}},
                         OsmElement::EntityType::Node), 1);

  // We should ignore this barrier because it's without access tag and placed on highway-motorway.
  AddNode(MakeOsmElement(32 /* id */, {{"barrier", "lift_gate"}},
                         OsmElement::EntityType::Node), 2);

  // Ignore all motorway_junction access.
  AddNode(MakeOsmElement(31 /* id */, {{"highway", "motorway_junction"}, {"access", "private"}},
                         OsmElement::EntityType::Node), 0);

  Finish();

  auto const privateSure = make_pair(RoadAccess::Type::Private, RoadAccess::Confidence::Sure);
  auto const yesSure = make_pair(RoadAccess::Type::Yes, RoadAccess::Confidence::Sure);

  auto const & car = Get(VehicleType::Car);
  TEST_EQUAL(car.GetAccessWithoutConditional({1, 1}), privateSure, ());
  TEST_EQUAL(car.GetAccessWithoutConditional({2, 2}), privateSure, ());
  TEST_EQUAL(car.GetAccessWithoutConditional({3, 1}), yesSure, ());
  TEST_EQUAL(car.GetAccessWithoutConditional({3, 2}), yesSure, ());
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

UNIT_CLASS_TEST(TestAccessFixture, ExoticConditionals)
{
  CreateCollectors();

  AddWay(MakeOsmElementWithNodes(1 /* id */, {{"highway", "motorway"}, {"access", "no @ (wind_speed>=65)"}},
                                 OsmElement::EntityType::Way, {10, 11, 12, 13}));
  Finish();

  auto const yesSure = make_pair(RoadAccess::Type::Yes, RoadAccess::Confidence::Sure);
  auto const & car = Get(VehicleType::Car);
  TEST_EQUAL(car.GetAccess(1, RouteWeight()), yesSure, ());
  TEST_EQUAL(car.GetAccessWithoutConditional(1), yesSure, ());
}

UNIT_CLASS_TEST(TestAccessFixture, ConditionalMerge)
{
  CreateCollectors(3);

  AddWay(MakeOsmElementWithNodes(
      1 /* id */, {{"highway", "primary"}, {"vehicle:conditional", "no @ (Mo-Su)"}} /* tags */,
      OsmElement::EntityType::Way, {10, 11, 12, 13}), 0);

  AddWay(MakeOsmElementWithNodes(
      2 /* id */,
      {{"highway", "service"}, {"vehicle:conditional", "private @ (10:00-20:00)"}} /* tags */,
      OsmElement::EntityType::Way, {20, 21, 22, 23}), 1);

  AddWay(MakeOsmElementWithNodes(
      3 /* id */,
      {{"highway", "service"},
       {"vehicle:conditional", "private @ (12:00-19:00) ; no @ (Mo-Su)"}} /* tags */,
      OsmElement::EntityType::Way, {30, 31, 32, 33}), 2);

  Finish();

  auto const & car = Get(VehicleType::Car);
  // Here can be any RouteWeight start time.
  TEST_EQUAL(car.GetAccess(1, RouteWeight(666)), make_pair(RoadAccess::Type::No, RoadAccess::Confidence::Sure), ());

  for (uint64_t id = 1; id <= 3; ++id)
    TEST_GREATER(car.GetWayToAccessConditional().at(id).Size(), 0, ());

  /// @todo Add more timing tests using
  /// GetUnixtimeByDate(2020, Month::Apr, Weekday::Monday, 11 /* hh */, 50 /* mm */)
}

UNIT_CLASS_TEST(TestAccessFixture, WinterRoads)
{
  CreateCollectors();

  AddWay(MakeOsmElementWithNodes(
      1 /* id */, {{"highway", "primary"}, {"ice_road", "yes"}} /* tags */,
      OsmElement::EntityType::Way, {10, 11, 12, 13}));

  AddWay(MakeOsmElementWithNodes(
      2 /* id */,
      {{"highway", "service"}, {"winter_road", "yes"}} /* tags */,
      OsmElement::EntityType::Way, {20, 21, 22, 23}));

  Finish();

  for (auto vehicle : { VehicleType::Pedestrian, VehicleType::Bicycle, VehicleType::Car })
  {
    auto const & ra = Get(vehicle);
    for (uint64_t id = 1; id <= 2; ++id)
    {
      auto const & wac = ra.GetWayToAccessConditional();
      auto const it = wac.find(id);
      TEST(it != wac.end(), (id, vehicle));
      TEST_GREATER(it->second.Size(), 0, ());
    }
  }

  /// @todo Add more timing tests using
  /// GetUnixtimeByDate(2020, Month::Apr, Weekday::Monday, 11 /* hh */, 50 /* mm */)
}

UNIT_CLASS_TEST(TestAccessFixture, Locked)
{
  CreateCollectors();

  AddWay(MakeOsmElementWithNodes(1 /* id */, {{"highway", "service"}} /* tags */,
                                 OsmElement::EntityType::Way, {10, 11, 12, 13}));
  AddWay(MakeOsmElementWithNodes(2 /* id */, {{"highway", "secondary"}} /* tags */,
                                 OsmElement::EntityType::Way, {20, 21, 22, 23}));

  AddNode(MakeOsmElement(11 /* id */, {{"barrier", "gate"}, {"locked", "yes"}},
                         OsmElement::EntityType::Node));
  AddNode(MakeOsmElement(21 /* id */, {{"barrier", "gate"}, {"locked", "yes"}, {"access", "permissive"}},
                         OsmElement::EntityType::Node));

  Finish();

  auto const privateSure = make_pair(RoadAccess::Type::Private, RoadAccess::Confidence::Sure);
  auto const yesSure = make_pair(RoadAccess::Type::Yes, RoadAccess::Confidence::Sure);

  for (VehicleType t : {VehicleType::Car, VehicleType::Bicycle, VehicleType::Pedestrian})
  {
    auto const & vehicle = Get(t);
    TEST_EQUAL(vehicle.GetAccessWithoutConditional({1, 1}), privateSure, ());
    TEST_EQUAL(vehicle.GetAccessWithoutConditional({2, 1}), yesSure, (t));
  }
}

UNIT_CLASS_TEST(TestAccessFixture, CycleBarrier)
{
  CreateCollectors();

  AddWay(MakeOsmElementWithNodes(1, {{"highway", "track"}},
                                 OsmElement::EntityType::Way, {10, 11, 12}));
  AddNode(MakeOsmElement(10, {{"barrier", "cycle_barrier"}},
                         OsmElement::EntityType::Node));
  AddNode(MakeOsmElement(11, {{"barrier", "cycle_barrier"}, {"bicycle", "dismount"}},
                         OsmElement::EntityType::Node));
  AddNode(MakeOsmElement(12, {{"barrier", "cycle_barrier"}, {"bicycle", "no"}},
                         OsmElement::EntityType::Node));
  Finish();
  auto const bicycle = Get(VehicleType::Bicycle);
  TEST_EQUAL(bicycle.GetAccessWithoutConditional({1, 0}),
             make_pair(RoadAccess::Type::Yes, RoadAccess::Confidence::Sure), ());
  TEST_EQUAL(bicycle.GetAccessWithoutConditional({1, 1}),
             make_pair(RoadAccess::Type::Yes, RoadAccess::Confidence::Sure), ());
  TEST_EQUAL(bicycle.GetAccessWithoutConditional({1, 2}),
             make_pair(RoadAccess::Type::No, RoadAccess::Confidence::Sure), ());
}

}  // namespace road_access_test
