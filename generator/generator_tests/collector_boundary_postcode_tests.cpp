#include "testing/testing.hpp"

#include "generator/generator_tests_support/test_with_classificator.hpp"

#include "generator/collector_boundary_postcode.hpp"
#include "generator/generator_tests/common.hpp"
#include "generator/osm_element.hpp"

#include "platform/platform.hpp"

#include "coding/read_write_utils.hpp"

#include "geometry/point2d.hpp"

#include "base/scope_guard.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace collector_boundary_postcode_tests
{
using std::string, std::vector, std::unordered_map;
using generator::tests_support::TestWithClassificator;

static string const kDumpFileName = "dump.bin";

// 0--1- 2
// |  |  |
// 3--4--5
// |  |  |
// 6--7--8
unordered_map<uint64_t, m2::PointD> const kNodes = {
    {0, m2::PointD{-1.0, 1.0}},
    {1, m2::PointD{0.0, 1.0}},
    {2, m2::PointD{1.0, 1.0}},
    {3, m2::PointD{-1.0, 0.0}},
    {4, m2::PointD{0.0, 0.0}},
    {5, m2::PointD{1.0, 0.0}},
    {6, m2::PointD{-1.0, -1.0}},
    {7, m2::PointD{0.0, -1.0}},
    {8, m2::PointD{1.0, -1.0}}};

vector<uint64_t> const kPolygon1 = {4, 5, 2, 1, 4};
vector<uint64_t> const kPolygon2 = {6, 3, 4, 7, 6};
vector<uint64_t> const kPolygon3 = {8, 7, 4, 5, 8};
vector<uint64_t> const kPolygon4 = {0, 1, 4, 4, 0};

unordered_map<uint64_t, WayElement> const kWays = {
    {1, WayElement{1, kPolygon1}},
    {2, WayElement{2, kPolygon2}},
    {3, WayElement{3, kPolygon3}},
    {4, WayElement{4, kPolygon4}}};

class IntermediateDataReaderTest : public generator::cache::IntermediateDataReaderInterface
{
  bool GetNode(uint64_t id, double & lat, double & lon) const override
  {
    auto const it = kNodes.find(id);
    CHECK(it != kNodes.end(), ("Unexpected node requested:", id));
    lat = it->second.y;
    lon = it->second.x;
    return true;
  }

  bool GetWay(uint64_t id, WayElement & e) override
  {
    auto const it = kWays.find(id);
    CHECK(it != kWays.end(), ("Unexpected way requested:", id));
    e = it->second;
    return true;
  }

  bool GetRelation(uint64_t /* id */, RelationElement & /* e */) override
  {
    return false;
  }
};

OsmElement MakePostcodeAreaRelation(uint64_t id, string postcode, uint64_t wayId)
{
  auto postcodeAreaRelation =
      generator_tests::MakeOsmElement(id, {{"type", "boundary"}, {"boundary", "postal_code"}, {"postal_code", postcode}},
                                      OsmElement::EntityType::Relation);
  postcodeAreaRelation.AddMember(wayId, OsmElement::EntityType::Way, "outer");
  return postcodeAreaRelation;
}

auto const postcodeAreaRelation1 =
    MakePostcodeAreaRelation(1 /* id */, "127001" /* postcode */, 1 /* wayId */);
auto const postcodeAreaRelation2 =
    MakePostcodeAreaRelation(2 /* id */, "127002" /* postcode */, 2 /* wayId */);
auto const postcodeAreaRelation3 =
    MakePostcodeAreaRelation(3 /* id */, "127003" /* postcode */, 3 /* wayId */);
auto const postcodeAreaRelation4 =
    MakePostcodeAreaRelation(4 /* id */, "127004" /* postcode */, 4 /* wayId */);

unordered_map<string, vector<m2::PointD>> Read(string const & dumpFilename)
{
  FileReader reader(dumpFilename);
  ReaderSource<FileReader> src(reader);

  unordered_map<string, vector<m2::PointD>> result;
  while (src.Size() > 0)
  {
    string postcode;
    utils::ReadString(src, postcode);
    vector<m2::PointD> geometry;
    rw::ReadVectorOfPOD(src, geometry);
    result.emplace(std::move(postcode), std::move(geometry));
  }

  return result;
}

bool CheckPostcodeExists(unordered_map<string, vector<m2::PointD>> const & data,
                         string const & postcode, vector<m2::PointD> const & geometry)
{
  auto const it = data.find(postcode);
  if (it == data.end())
    return false;

  if (it->second.size() != geometry.size())
    return false;

  for (size_t i = 0; i < geometry.size(); ++i)
  {
    if (!AlmostEqualAbs(geometry[i], it->second[i], kMwmPointAccuracy))
      return false;
  }

  return true;
}

vector<m2::PointD> ConvertIdsToPoints(vector<uint64_t> const & ids)
{
  vector<m2::PointD> result(ids.size());
  for (size_t i = 0; i < ids.size(); ++i)
  {
    auto const it = kNodes.find(ids[i]);
    CHECK(it != kNodes.end(), ("Unexpected node requested."));
    result[i] = it->second;
  }
  return result;
}

void Check(string const & dumpFilename)
{
  auto const data = Read(dumpFilename);

  TEST(CheckPostcodeExists(data, "127001", ConvertIdsToPoints(kPolygon1)), (data));
  TEST(CheckPostcodeExists(data, "127002", ConvertIdsToPoints(kPolygon2)), (data));
  TEST(CheckPostcodeExists(data, "127003", ConvertIdsToPoints(kPolygon3)), (data));
  TEST(CheckPostcodeExists(data, "127004", ConvertIdsToPoints(kPolygon4)), (data));
}


UNIT_CLASS_TEST(TestWithClassificator, CollectorBoundaryPostcode_1)
{
  SCOPE_GUARD(rmDump, std::bind(Platform::RemoveFileIfExists, cref(kDumpFileName)));

  auto cache = std::make_shared<IntermediateDataReaderTest>();
  auto collector = std::make_shared<generator::BoundaryPostcodeCollector>(kDumpFileName, cache);
  collector->Collect(postcodeAreaRelation1);
  collector->Collect(postcodeAreaRelation2);
  collector->Collect(postcodeAreaRelation3);
  collector->Collect(postcodeAreaRelation4);

  collector->Finish();
  collector->Finalize();

  Check(kDumpFileName);
}

UNIT_CLASS_TEST(TestWithClassificator, CollectorBoundaryPostcode_2)
{
  SCOPE_GUARD(rmDump, std::bind(Platform::RemoveFileIfExists, cref(kDumpFileName)));

  auto cache = std::make_shared<IntermediateDataReaderTest>();
  auto collector1 = std::make_shared<generator::BoundaryPostcodeCollector>(kDumpFileName, cache);
  auto collector2 = collector1->Clone();

  collector1->Collect(postcodeAreaRelation1);
  collector1->Collect(postcodeAreaRelation3);

  collector2->Collect(postcodeAreaRelation2);
  collector2->Collect(postcodeAreaRelation4);

  collector1->Finish();
  collector2->Finish();
  collector1->Merge(*collector2);
  collector1->Finalize();

  Check(kDumpFileName);
}
}  // namespace collector_boundary_postcode_tests
