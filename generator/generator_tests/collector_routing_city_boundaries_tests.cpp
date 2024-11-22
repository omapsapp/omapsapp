#include "testing/testing.hpp"

#include "generator/collector_routing_city_boundaries.hpp"
#include "generator/generator_tests/common.hpp"
#include "generator/osm2type.hpp"
#include "generator/osm_element.hpp"
#include "generator/routing_city_boundaries_processor.hpp"

#include "geometry/area_on_earth.hpp"
#include "geometry/mercator.hpp"

#include "base/geo_object_id.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace collector_routing_city_boundaries_tests
{
using namespace generator_tests;
using namespace generator;
using namespace feature;

using BoundariesCollector = RoutingCityBoundariesCollector;

std::string const kDumpFileName = "dump.bin";

std::vector<m2::PointD> const kPolygon1 = {{1, 0}, {0, 2}, {2, 2}, {2, 0}, {0, 0}, {1, 0}};
std::vector<m2::PointD> const kPolygon2 = {{2, 0}, {0, 2}, {2, 2}, {2, 0}, {0, 0}, {2, 0}};
std::vector<m2::PointD> const kPolygon3 = {{3, 0}, {0, 2}, {2, 2}, {2, 0}, {0, 0}, {3, 0}};
std::vector<m2::PointD> const kPolygon4 = {{4, 0}, {0, 2}, {2, 2}, {2, 0}, {0, 0}, {4, 0}};

feature::FeatureBuilder MakeAreaFeatureBuilder(OsmElement element, std::vector<m2::PointD> && geometry)
{
  feature::FeatureBuilder result;
  auto const filterType = [](uint32_t) { return true; };
  ftype::GetNameAndType(&element, result.GetParams(), filterType);
  result.SetOsmId(base::MakeOsmRelation(element.m_id));

  result.AddPolygon(std::move(geometry));
  result.SetArea();
  return result;
}

feature::FeatureBuilder MakeNodeFeatureBuilder(OsmElement element)
{
  feature::FeatureBuilder result;
  auto const filterType = [](uint32_t) { return true; };
  ftype::GetNameAndType(&element, result.GetParams(), filterType);
  result.SetOsmId(base::MakeOsmNode(element.m_id));
  result.SetCenter(mercator::FromLatLon(element.m_lat, element.m_lon));
  return result;
}

OsmElement MakeAreaWithPlaceNode(uint64_t id, uint64_t placeId, std::string const & role)
{
  auto area = MakeOsmElement(id, {{"boundary", "administrative"}}, OsmElement::EntityType::Relation);
  area.m_members.emplace_back(placeId, OsmElement::EntityType::Node, role);
  return area;
}

auto const placeRelation1 = MakeOsmElement(1 /* id */, {{"place", "city"}}, OsmElement::EntityType::Relation);
auto const placeRelation2 = MakeOsmElement(2 /* id */, {{"place", "town"}}, OsmElement::EntityType::Relation);
auto const placeRelation3 = MakeOsmElement(3 /* id */, {{"place", "village"}}, OsmElement::EntityType::Relation);
auto const placeRelation4 = MakeOsmElement(4 /* id */, {{"place", "country"}}, OsmElement::EntityType::Relation);

auto const placeNode1 = MakeOsmElement(9 /* id */, {{"place", "city"}, {"population", "200.000"}}, OsmElement::EntityType::Node);
auto const placeNode2 = MakeOsmElement(10 /* id */, {{"place", "town"}, {"population", "10 000"}}, OsmElement::EntityType::Node);
auto const placeNode3 = MakeOsmElement(11 /* id */, {{"place", "village"}, {"population", "1000"}}, OsmElement::EntityType::Node);
auto const placeNode4 = MakeOsmElement(12 /* id */, {{"place", "country"}, {"population", "147000000"}}, OsmElement::EntityType::Node);

auto const relationWithLabel1 = MakeAreaWithPlaceNode(5 /* id */, 9 /* placeId */, "label" /* role */);
auto const relationWithLabel2 = MakeAreaWithPlaceNode(6 /* id */, 10 /* placeId */, "admin_centre" /* role */);
auto const relationWithLabel3 = MakeAreaWithPlaceNode(7 /* id */, 11 /* placeId */, "label" /* role */);
auto const relationWithLabel4 = MakeAreaWithPlaceNode(8 /* id */, 12 /* placeId */, "country" /* role */);

/*
void Collect(BoundariesCollector & collector, std::vector<OsmElement> const & elements,
             std::vector<std::vector<m2::PointD>> geometries = {})
{
  for (size_t i = 0; i < elements.size(); ++i)
  {
    auto const & element = elements[i];
    auto featureBuilder = element.IsNode() ? MakeNodeFeatureBuilder(element)
                                           : MakeAreaFeatureBuilder(element, std::move(geometries[i]));

    if (BoundariesCollector::FilterOsmElement(element))
      collector.Process(featureBuilder, element);
  }
}

void Collect(std::shared_ptr<CollectorInterface> & collector,
             std::vector<OsmElement> const & elements,
             std::vector<std::vector<m2::PointD>> const & geometries = {})
{
  auto boundariesCollector = dynamic_cast<BoundariesCollector*>(collector.get());
  Collect(*boundariesCollector, elements, geometries);
}

std::vector<std::vector<m2::PointD>> ReadBoundaries(std::string const & dumpFilename)
{
  FileReader reader(dumpFilename);
  ReaderSource<FileReader> src(reader);

  std::vector<std::vector<m2::PointD>> result;
  std::vector<m2::PointD> boundary;
  while (src.Size() > 0)
  {
    rw::ReadVectorOfPOD(src, boundary);
    result.emplace_back(std::move(boundary));
  }
  return result;
}

bool CheckPolygonExistance(std::vector<std::vector<m2::PointD>> const & polygons,
                           std::vector<m2::PointD> const & polygonToFind)
{
  for (auto const & polygon : polygons)
  {
    bool same = true;
    if (polygon.size() != polygonToFind.size())
      continue;

    for (size_t i = 0; i < polygon.size(); ++i)
    {
      if (!AlmostEqualAbs(polygon[i], polygonToFind[i], 1e-6))
      {
        same = false;
        break;
      }
    }

    if (same)
      return true;
  }

  return false;
}

void Check(std::string const & filename, std::string const & dumpFilename)
{
  using Writer = RoutingCityBoundariesWriter;

  auto const boundaries = ReadBoundaries(dumpFilename);

  TEST(CheckPolygonExistance(boundaries, kPolygon1), ());
  TEST(CheckPolygonExistance(boundaries, kPolygon2), ());
  TEST(CheckPolygonExistance(boundaries, kPolygon3), ());
  TEST(!CheckPolygonExistance(boundaries, kPolygon4), ());

  auto const nodeToBoundaryFilename = Writer::GetNodeToBoundariesFilename(filename);
  auto nodeToBoundary = routing_city_boundaries::LoadNodeToBoundariesData(nodeToBoundaryFilename);
  TEST(nodeToBoundary.count(9), ());
  TEST(nodeToBoundary.count(10), ());
  TEST(nodeToBoundary.count(11), ());
  TEST(!nodeToBoundary.count(12), ());

  TEST(CheckPolygonExistance({nodeToBoundary[9].back().GetOuterGeometry()}, kPolygon1), ());
  TEST(CheckPolygonExistance({nodeToBoundary[10].back().GetOuterGeometry()}, kPolygon2), ());
  TEST(CheckPolygonExistance({nodeToBoundary[11].back().GetOuterGeometry()}, kPolygon3), ());

  auto const nodeToLocalityFilename = Writer::GetNodeToLocalityDataFilename(filename);
  auto nodeToLocality = routing_city_boundaries::LoadNodeToLocalityData(nodeToLocalityFilename);
  TEST(nodeToLocality.count(9), ());
  TEST(nodeToLocality.count(10), ());
  TEST(nodeToLocality.count(11), ());
  TEST(!nodeToLocality.count(12), ());

  TEST_EQUAL(nodeToLocality[9].m_place, ftypes::LocalityType::City, ());
  TEST_EQUAL(nodeToLocality[9].m_population, 200000, ());

  TEST_EQUAL(nodeToLocality[10].m_place, ftypes::LocalityType::Town, ());
  TEST_EQUAL(nodeToLocality[10].m_population, 10000, ());

  TEST_EQUAL(nodeToLocality[11].m_place, ftypes::LocalityType::Village, ());
  TEST_EQUAL(nodeToLocality[11].m_population, 1000, ());
}
*/

std::vector<m2::PointD> FromLatLons(std::vector<ms::LatLon> const & latlons)
{
  std::vector<m2::PointD> points;
  for (auto const & latlon : latlons)
    points.emplace_back(mercator::FromLatLon(latlon));
  return points;
}

double CalculateEarthAreaForConvexPolygon(std::vector<ms::LatLon> const & latlons)
{
  CHECK(!latlons.empty(), ());
  double area = 0.0;
  auto const & base = latlons.front();
  for (size_t i = 1; i < latlons.size() - 1; ++i)
    area += ms::AreaOnEarth(base, latlons[i], latlons[i + 1]);

  return area;
}

/*
UNIT_CLASS_TEST(TestWithClassificator, CollectorRoutingCityBoundaries_1)
{
  auto const filename = generator_tests::GetFileName();
  SCOPE_GUARD(_, std::bind(Platform::RemoveFileIfExists, std::cref(filename)));
  SCOPE_GUARD(rmDump, std::bind(Platform::RemoveFileIfExists, std::cref(kDumpFileName)));

  std::shared_ptr<generator::cache::IntermediateDataReader> cache;
  auto c1 = std::make_shared<BoundariesCollector>(filename, kDumpFileName, cache);

  Collect(*c1, {placeRelation1, placeRelation2, placeRelation3, placeRelation4},
               {kPolygon1, kPolygon2, kPolygon3, kPolygon4});
  Collect(*c1, {relationWithLabel1, relationWithLabel2, relationWithLabel3, relationWithLabel4},
               {kPolygon1, kPolygon2, kPolygon3, kPolygon4});
  Collect(*c1, {placeNode1, placeNode2, placeNode3, placeNode4});

  c1->Finish();
  c1->Finalize();

  Check(filename, kDumpFileName);
}

UNIT_CLASS_TEST(TestWithClassificator, CollectorRoutingCityBoundaries_2)
{
  auto const filename = generator_tests::GetFileName();
  SCOPE_GUARD(_, std::bind(Platform::RemoveFileIfExists, std::cref(filename)));
  SCOPE_GUARD(rmDump, std::bind(Platform::RemoveFileIfExists, std::cref(kDumpFileName)));

  std::shared_ptr<generator::cache::IntermediateDataReader> cache;
  auto c1 = std::make_shared<BoundariesCollector>(filename, kDumpFileName, cache);
  auto c2 = c1->Clone();

  Collect(*c1, {placeRelation1, placeRelation2}, {kPolygon1, kPolygon2});
  Collect(c2, {placeRelation3, placeRelation4}, {kPolygon3, kPolygon4});

  Collect(*c1, {relationWithLabel1, relationWithLabel2}, {kPolygon1, kPolygon2});
  Collect(c2, {relationWithLabel3, relationWithLabel4}, {kPolygon3, kPolygon4});

  Collect(*c1, {placeNode1, placeNode2});
  Collect(c2, {placeNode3, placeNode4});

  c1->Finish();
  c2->Finish();
  c1->Merge(*c2);
  c1->Finalize();

  Check(filename, kDumpFileName);
}
*/

UNIT_TEST(AreaOnEarth_Convex_Polygon_1)
{
  auto const a = ms::LatLon(55.8034965, 37.696754);
  auto const b = ms::LatLon(55.7997909, 37.7830427);
  auto const c = ms::LatLon(55.8274225, 37.8150381);
  auto const d = ms::LatLon(55.843037, 37.7401892);
  auto const e = ms::LatLon(55.8452096, 37.7019668);

  std::vector<ms::LatLon> const latlons = {a, b, c, d, e};
  std::vector<m2::PointD> const points = FromLatLons(latlons);

  double const areaTriangulated =
      ms::AreaOnEarth(a, b, c) + ms::AreaOnEarth(a, c, d) + ms::AreaOnEarth(a, d, e);

  double const areaOnEarth = generator::AreaOnEarth(points);

  TEST(AlmostEqualRel(areaTriangulated,
                            areaOnEarth,
                            1e-6), (areaTriangulated, areaOnEarth));
}

UNIT_TEST(AreaOnEarth_Convex_Polygon_2)
{
  std::vector<ms::LatLon> const latlons = {
      {55.6348484, 36.025526},
      {55.0294112, 36.8959709},
      {54.9262448, 38.3719426},
      {55.3561515, 39.3275397},
      {55.7548279, 39.4458067},
      {56.3020039, 39.3322704},
      {56.5140099, 38.6368606},
      {56.768935, 37.0473526},
      {56.4330113, 35.6234183},
  };

  std::vector<m2::PointD> const points = FromLatLons(latlons);

  double const areaOnEarth = generator::AreaOnEarth(points);
  double const areaForConvexPolygon = CalculateEarthAreaForConvexPolygon(latlons);

  TEST(AlmostEqualRel(areaForConvexPolygon,
                            areaOnEarth,
                            1e-6), (areaForConvexPolygon, areaOnEarth));
}

UNIT_TEST(AreaOnEarth_Concave_Polygon)
{
  auto const a = ms::LatLon(40.3429746, -7.6513617);
  auto const b = ms::LatLon(40.0226711, -7.7356029);
  auto const c = ms::LatLon(39.7887079, -7.0626206);
  auto const d = ms::LatLon(40.1038622, -7.0394143);
  auto const e = ms::LatLon(40.0759637, -6.7145263);
  auto const f = ms::LatLon(40.2861884, -6.8637096);
  auto const g = ms::LatLon(40.4175634, -6.5123);
  auto const h = ms::LatLon(40.4352289, -6.9101221);
  auto const i = ms::LatLon(40.6040786, -7.0825117);
  auto const j = ms::LatLon(40.4301821, -7.2482709);

  std::vector<ms::LatLon> const latlons = {a, b, c, d, e, f, g, h, i, j};
  std::vector<m2::PointD> const points = FromLatLons(latlons);

  double areaTriangulated =
      ms::AreaOnEarth(a, b, c) +
      ms::AreaOnEarth(a, c, d) +
      ms::AreaOnEarth(a, d, f) +
      ms::AreaOnEarth(d, e, f) +
      ms::AreaOnEarth(a, f, j) +
      ms::AreaOnEarth(f, h, j) +
      ms::AreaOnEarth(f, g, h) +
      ms::AreaOnEarth(h, i, j);

  double const areaOnEarth = generator::AreaOnEarth(points);

  TEST(AlmostEqualRel(areaTriangulated,
                            areaOnEarth,
                            1e-6), ());
}
}  // namespace collector_routing_city_boundaries_tests
