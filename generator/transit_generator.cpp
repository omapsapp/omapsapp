#include "generator/transit_generator.hpp"

#include "generator/borders.hpp"
#include "generator/utils.hpp"

#include "routing/index_router.hpp"
#include "routing/road_graph.hpp"
#include "routing/routing_exceptions.hpp"
#include "routing/vehicle_mask.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/routing_helpers.hpp"

#include "traffic/traffic_cache.hpp"

#include "transit/transit_types.hpp"

#include "indexer/mwm_set.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"
#include "geometry/region2d.hpp"

#include "coding/file_writer.hpp"

#include "platform/country_file.hpp"
#include "platform/platform.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"

#include <algorithm>
#include <vector>

#include "defines.hpp"

using namespace generator;
using namespace platform;
using namespace routing;
using namespace routing::transit;
using namespace storage;

namespace
{
void LoadBorders(std::string const & dir, CountryId const & countryId, std::vector<m2::RegionD> & borders)
{
  std::string const polyFile = base::JoinPath(dir, BORDERS_DIR, countryId + BORDERS_EXTENSION);
  borders.clear();
  borders::LoadBorders(polyFile, borders);
}

void FillOsmIdToFeatureIdsMap(std::string const & osmIdToFeatureIdsPath, OsmIdToFeatureIdsMap & mapping)
{
  CHECK(ForEachOsmId2FeatureId(osmIdToFeatureIdsPath,
                               [&mapping](auto const & compositeId, auto featureId) {
                                 mapping[compositeId.m_mainId].push_back(featureId);
                               }),
        (osmIdToFeatureIdsPath));
}

std::string GetMwmPath(std::string const & mwmDir, CountryId const & countryId)
{
  return base::JoinPath(mwmDir, countryId + DATA_FILE_EXTENSION);
}

/// \brief Calculates best pedestrian segment for every gate in |graphData.m_gates|.
/// The result of the calculation is set to |Gate::m_bestPedestrianSegment| of every gate
/// from |graphData.m_gates|.
/// \note All gates in |graphData.m_gates| must have a valid |m_point| field before the call.
void CalculateBestPedestrianSegments(std::string const & mwmPath, CountryId const & countryId,
                                     GraphData & graphData)
{
  // Creating IndexRouter.
  SingleMwmDataSource dataSource(mwmPath);

  auto infoGetter = storage::CountryInfoReader::CreateCountryInfoGetter(GetPlatform());
  CHECK(infoGetter, ());

  auto const countryFileGetter = [&infoGetter](m2::PointD const & pt) {
    return infoGetter->GetRegionCountryId(pt);
  };

  auto const getMwmRectByName = [&](std::string const & c) -> m2::RectD {
    CHECK_EQUAL(countryId, c, ());
    return infoGetter->GetLimitRectForLeaf(c);
  };

  CHECK_EQUAL(dataSource.GetMwmId().GetInfo()->GetType(), MwmInfo::COUNTRY, ());
  auto numMwmIds = std::make_shared<NumMwmIds>();
  numMwmIds->RegisterFile(CountryFile(countryId));

  // Note. |indexRouter| is valid while |dataSource| is valid.
  IndexRouter indexRouter(VehicleType::Pedestrian, false /* load altitudes */,
                          CountryParentNameGetterFn(), countryFileGetter, getMwmRectByName,
                          numMwmIds, MakeNumMwmTree(*numMwmIds, *infoGetter),
                          traffic::TrafficCache(), dataSource.GetDataSource());
  auto worldGraph = indexRouter.MakeSingleMwmWorldGraph();

  // Looking for the best segment for every gate.
  auto const & gates = graphData.GetGates();
  for (size_t i = 0; i < gates.size(); ++i)
  {
    auto const & gate = gates[i];
    if (countryFileGetter(gate.GetPoint()) != countryId)
      continue;

    // Note. For pedestrian routing all the segments are considered as two way segments so
    // IndexRouter::FindBestSegments() method finds the same segments for |isOutgoing| == true
    // and |isOutgoing| == false.
    std::vector<routing::Edge> bestEdges;
    try
    {
      if (countryFileGetter(gate.GetPoint()) != countryId)
        continue;

      if (indexRouter.GetBestOutgoingEdges(gate.GetPoint(), *worldGraph, bestEdges))
      {
        CHECK(!bestEdges.empty(), ());
        IndexRouter::BestEdgeComparator bestEdgeComparator(gate.GetPoint(),
                                                           m2::PointD::Zero() /* direction */);
        // Looking for the edge which is the closest to |gate.GetPoint()|.
        // @TODO It should be considered to change the format of transit section to keep all
        // candidates for every gate.
        auto const & bestEdge = *min_element(
            bestEdges.cbegin(), bestEdges.cend(),
            [&bestEdgeComparator](routing::Edge const & lhs, routing::Edge const & rhs) {
              return bestEdgeComparator.Compare(lhs, rhs) < 0;
            });

        CHECK(bestEdge.GetFeatureId().IsValid(), ());

        graphData.SetGateBestPedestrianSegment(i, SingleMwmSegment(
            bestEdge.GetFeatureId().m_index, bestEdge.GetSegId(), bestEdge.IsForward()));
      }
    }
    catch (MwmIsNotAliveException const & e)
    {
      LOG(LCRITICAL, ("Point of a gate belongs to the processed mwm:", countryId, ","
          "but the mwm is not alive. Gate:", gate, e.what()));
    }
    catch (RootException const & e)
    {
      LOG(LCRITICAL, ("Exception while looking for the best segment of a gate. CountryId:",
          countryId, ". Gate:", gate, e.what()));
    }
  }
}
}  // namespace

namespace routing::transit
{
void DeserializeFromJson(OsmIdToFeatureIdsMap const & mapping,
                         std::string const & transitJsonPath, GraphData & data)
{
  Platform::EFileType fileType;
  Platform::EError const errCode = Platform::GetFileType(transitJsonPath, fileType);
  CHECK_EQUAL(errCode, Platform::EError::ERR_OK, ("Transit graph was not found:", transitJsonPath));
  CHECK_EQUAL(fileType, Platform::EFileType::Regular,
              ("Transit graph was not found:", transitJsonPath));

  std::string jsonBuffer;
  try
  {
    GetPlatform().GetReader(transitJsonPath)->ReadAsString(jsonBuffer);
  }
  catch (RootException const & e)
  {
    LOG(LCRITICAL, ("Can't open", transitJsonPath, e.what()));
  }

  base::Json root(jsonBuffer.c_str());
  CHECK(root.get() != nullptr, ("Cannot parse the json file:", transitJsonPath));

  data.Clear();
  try
  {
    data.DeserializeFromJson(root, mapping);
  }
  catch (RootException const & e)
  {
    LOG(LCRITICAL, ("Exception while parsing transit graph json. Json file path:", transitJsonPath, e.what()));
  }
}

void ProcessGraph(std::string const & mwmPath, CountryId const & countryId,
                  OsmIdToFeatureIdsMap const & /* osmIdToFeatureIdsMap */, GraphData & data)
{
  CalculateBestPedestrianSegments(mwmPath, countryId, data);
  data.Sort();
  data.CheckValidSortedUnique();
}

void BuildTransit(std::string const & mwmDir, CountryId const & countryId,
                  std::string const & osmIdToFeatureIdsPath, std::string const & transitDir)
{
  std::string const mwmPath = GetMwmPath(mwmDir, countryId);
  LOG(LINFO, ("Building transit section for", mwmPath));

  OsmIdToFeatureIdsMap mapping;
  FillOsmIdToFeatureIdsMap(osmIdToFeatureIdsPath, mapping);
  std::vector<m2::RegionD> mwmBorders;
  LoadBorders(mwmDir, countryId, mwmBorders);

  Platform::FilesList graphFiles;
  Platform::GetFilesByExt(base::AddSlashIfNeeded(transitDir), TRANSIT_FILE_EXTENSION, graphFiles);

  GraphData jointData;
  for (auto const & fileName : graphFiles)
  {
    auto const filePath = base::JoinPath(transitDir, fileName);
    GraphData data;
    DeserializeFromJson(mapping, filePath, data);
    // @todo(bykoianko) Json should be clipped on feature generation step. It's much more efficient.
    data.ClipGraph(mwmBorders);
    jointData.AppendTo(data);
  }

  if (jointData.IsEmpty())
    return; // Empty transit section.

  ProcessGraph(mwmPath, countryId, mapping, jointData);
  jointData.CheckValidSortedUnique();

  FilesContainerW cont(mwmPath, FileWriter::OP_WRITE_EXISTING);
  auto writer = cont.GetWriter(TRANSIT_FILE_TAG);
  jointData.Serialize(*writer);
}
}  // namespace routing::transit
