#include "testing/testing.hpp"

#include "routing/cross_border_graph.hpp"

#include "storage/routing_helpers.hpp"
#include "storage/storage.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "platform/platform.hpp"

#include "coding/file_reader.hpp"
#include "coding/file_writer.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/latlon.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace routing
{
//  Test for serializing/deserializing of the chain of 4 points passing through 4 mwms:
//  *-------------------------*-------------------------*-------------------------*
//  p1                        p2                        p3                        p4

std::pair<ms::LatLon, NumMwmId> GetCoordInRegion(double lat, double lon, std::string const & region,
                                                 std::shared_ptr<NumMwmIds> numMwmIds)
{
  return {ms::LatLon(lat, lon), numMwmIds->GetId(platform::CountryFile(region))};
}

void FillGraphWithTestInfo(CrossBorderGraph & gaph, std::shared_ptr<NumMwmIds> numMwmIds)
{
  auto const [p1, id1] = GetCoordInRegion(50.88010, 4.41923, "Belgium_Flemish Brabant", numMwmIds);
  auto const [p2, id2] = GetCoordInRegion(50.99043, 5.48125, "Belgium_Limburg", numMwmIds);
  auto const [p3, id3] = GetCoordInRegion(50.88751, 5.92378, "Netherlands_Limburg", numMwmIds);
  auto const [p4, id4] = GetCoordInRegion(
      50.8734, 6.27417, "Germany_North Rhine-Westphalia_Regierungsbezirk Koln_Aachen", numMwmIds);

  double constexpr avgSpeedMpS = 14.0;

  CrossBorderSegment d12;
  d12.m_weight = ms::DistanceOnEarth(p1, p2) / avgSpeedMpS;
  d12.m_start = CrossBorderSegmentEnding(p1, id1);
  d12.m_end = CrossBorderSegmentEnding(p2, id2);
  gaph.m_segments.emplace(10, d12);

  CrossBorderSegment d23;
  d23.m_weight = ms::DistanceOnEarth(p2, p3) / avgSpeedMpS;
  d23.m_start = CrossBorderSegmentEnding(p2, id2);
  d23.m_end = CrossBorderSegmentEnding(p3, id3);
  gaph.m_segments.emplace(20, d23);

  CrossBorderSegment d34;
  d34.m_weight = ms::DistanceOnEarth(p3, p4) / avgSpeedMpS;
  d34.m_start = CrossBorderSegmentEnding(p3, id3);
  d34.m_end = CrossBorderSegmentEnding(p4, id4);
  gaph.m_segments.emplace(30, d34);

  gaph.m_mwms.emplace(id1, std::vector<RegionSegmentId>{10});
  gaph.m_mwms.emplace(id2, std::vector<RegionSegmentId>{10, 20});
  gaph.m_mwms.emplace(id3, std::vector<RegionSegmentId>{20, 30});
  gaph.m_mwms.emplace(id4, std::vector<RegionSegmentId>{30});
}

template <class T>
T GetSorted(T const & cont)
{
  auto contSorted = cont;
  std::sort(contSorted.begin(), contSorted.end());
  return contSorted;
}

void TestEqualMwm(MwmIdToSegmentIds const & mwmIds1, MwmIdToSegmentIds const & mwmIds2)
{
  TEST_EQUAL(mwmIds1.size(), mwmIds2.size(), ());

  for (auto const & [mwmId, segIds] : mwmIds1)
  {
    auto itMwmId = mwmIds2.find(mwmId);
    TEST(itMwmId != mwmIds2.end(), ());
    TEST_EQUAL(GetSorted(segIds), GetSorted(itMwmId->second), ());
  }
}

void TestEqualSegments(CrossBorderSegments const & s1, CrossBorderSegments const & s2)
{
  TEST_EQUAL(s1.size(), s2.size(), ());

  for (auto const & [segId1, data1] : s1)
  {
    auto itSegId = s2.find(segId1);
    TEST(itSegId != s2.end(), ());

    auto const & data2 = itSegId->second;

    static double constexpr epsCoord = 1e-5;

    TEST(AlmostEqualAbs(data1.m_start.m_point.GetLatLon(), data2.m_start.m_point.GetLatLon(),
                              epsCoord),
         ());
    TEST(AlmostEqualAbs(data1.m_end.m_point.GetLatLon(), data2.m_end.m_point.GetLatLon(),
                              epsCoord),
         ());
    TEST_EQUAL(static_cast<uint32_t>(std::ceil(data1.m_weight)),
               static_cast<uint32_t>(std::ceil(data2.m_weight)), ());
  }
}

UNIT_TEST(CrossBorderGraph_SerDes)
{
  std::string const fileName = "CrossBorderGraph_SerDes.test";

  storage::Storage storage;
  std::shared_ptr<NumMwmIds> numMwmIds = CreateNumMwmIds(storage);

  CrossBorderGraph graph1;
  FillGraphWithTestInfo(graph1, numMwmIds);

  {
    FilesContainerW cont(fileName, FileWriter::OP_WRITE_TRUNCATE);
    auto writer = cont.GetWriter(ROUTING_WORLD_FILE_TAG);
    CrossBorderGraphSerializer::Serialize(graph1, writer, numMwmIds);
  }

  uint64_t sizeBytes;
  CHECK(GetPlatform().GetFileSizeByFullPath(fileName, sizeBytes), (fileName, sizeBytes));
  LOG(LINFO, ("File size:", sizeBytes, "bytes"));

  FilesContainerR cont(fileName);
  auto src = std::make_unique<FilesContainerR::TReader>(cont.GetReader(ROUTING_WORLD_FILE_TAG));
  ReaderSource<FilesContainerR::TReader> reader(*src);

  CrossBorderGraph graph2;
  CrossBorderGraphSerializer::Deserialize(graph2, reader, numMwmIds);

  TestEqualMwm(graph1.m_mwms, graph2.m_mwms);
  TestEqualSegments(graph1.m_segments, graph2.m_segments);
}
}  // namespace routing
