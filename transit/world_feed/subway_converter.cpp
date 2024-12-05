#include "transit/world_feed/subway_converter.hpp"

#include "generator/transit_generator.hpp"

#include "routing/fake_feature_ids.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <algorithm>
#include <iterator>
#include <limits>

namespace transit
{
std::string const kHashPrefix = "mapsme_transit";
std::string const kDefaultLang = "default";
std::string const kSubwayRouteType = "subway";

namespace
{
double constexpr kEps = 1e-5;

// Returns route id of the line. Route id is calculated in the same way as in the script
// tools/transit/transit_graph_generator.py.
uint32_t GetSubwayRouteId(routing::transit::LineId lineId)
{
  return static_cast<uint32_t>(lineId >> 4);
}

// Increments |lineSegment| indexes by |shapeLink| start index.
void ShiftSegmentOnShape(transit::LineSegment & lineSegment, transit::ShapeLink const & shapeLink)
{
  lineSegment.m_startIdx += shapeLink.m_startIndex;
  lineSegment.m_endIdx += shapeLink.m_startIndex;
}

// Returns segment edge points on the polyline.
std::pair<m2::PointD, m2::PointD> GetSegmentEdgesOnPolyline(
    std::vector<m2::PointD> const & polyline, transit::LineSegment const & segment)
{
  CHECK_GREATER(polyline.size(), std::max(segment.m_startIdx, segment.m_endIdx), ());

  m2::PointD const startPoint = polyline[segment.m_startIdx];
  m2::PointD const endPoint = polyline[segment.m_endIdx];

  return {startPoint, endPoint};
}

// Calculates |segment| start and end indexes on the polyline with length |polylineSize| in
// assumption that this segment is reversed. Example: we have polyline [1, 2, 3, 4, 5, 6] and
// segment [5, 4]. We reversed this segment so it transformed to [4, 5] and found it on polyline.
// Its start and end indexes on the polyline are 3, 4. We want to calculate start and end indexes of
// the original segment [5, 4]. These indexes are 4, 3.
void UpdateReversedSegmentIndexes(transit::LineSegment & segment, size_t polylineSize)
{
  size_t const len = segment.m_endIdx - segment.m_startIdx + 1;
  segment.m_endIdx = static_cast<uint32_t>(polylineSize - segment.m_startIdx - 1);
  segment.m_startIdx = static_cast<uint32_t>(segment.m_endIdx - len + 1);

  CHECK_GREATER(segment.m_endIdx, segment.m_startIdx, ());
  CHECK_GREATER(polylineSize, segment.m_endIdx, ());
}
}  // namespace



SubwayConverter::SubwayConverter(std::string const & subwayJson, WorldFeed & feed)
  : m_subwayJson(subwayJson), m_feed(feed)
{
}

bool SubwayConverter::Convert()
{
  routing::transit::OsmIdToFeatureIdsMap emptyMapping;
  routing::transit::DeserializeFromJson(emptyMapping, m_subwayJson, m_graphData);

  if (!ConvertNetworks())
    return false;

  if (!SplitEdges())
    return false;

  if (!ConvertLinesBasedData())
    return false;

  m_feed.ModifyLinesAndShapes();
  MinimizeReversedLinesCount();

  if (!ConvertStops())
    return false;

  ConvertTransfers();

  // In contrast to the GTFS gates OSM gates for subways shouldn't be empty.
  if (!ConvertGates())
    return false;

  if (!ConvertEdges())
    return false;

  m_feed.SplitFeedIntoRegions();

  PrepareLinesMetadata();

  return true;
}

bool SubwayConverter::ConvertNetworks()
{
  auto const & networksSubway = m_graphData.GetNetworks();
  m_feed.m_networks.m_data.reserve(networksSubway.size());

  for (auto const & networkSubway : networksSubway)
  {
    // Subway network id is city id index approximately in interval (0, 400).
    TransitId const networkId = networkSubway.GetId();
    CHECK(!routing::FakeFeatureIds::IsTransitFeature(networkId), (networkId));

    m_feed.m_networks.m_data.emplace(networkId, networkSubway.GetTitle());
  }

  LOG(LINFO,
      ("Converted", m_feed.m_networks.m_data.size(), "networks from subways to public transport."));

  return !m_feed.m_networks.m_data.empty();
}

bool SubwayConverter::SplitEdges()
{
  auto & edgesSubway = m_graphData.GetEdges();

  for (size_t i = 0; i < edgesSubway.size(); ++i)
  {
    auto const & edgeSubway = edgesSubway[i];

    if (edgeSubway.GetTransfer())
      m_edgesTransferSubway.emplace(edgeSubway, i);
    else
      m_edgesSubway.emplace(edgeSubway, i);
  }

  return !m_edgesSubway.empty() && !m_edgesTransferSubway.empty();
}

std::pair<TransitId, RouteData> SubwayConverter::MakeRoute(
    routing::transit::Line const & lineSubway)
{
  uint32_t routeSubwayId = GetSubwayRouteId(lineSubway.GetId());

  std::string const routeHash = BuildHash(kHashPrefix, std::to_string(lineSubway.GetNetworkId()),
                                          std::to_string(routeSubwayId));

  TransitId const routeId = m_feed.m_idGenerator.MakeId(routeHash);

  RouteData routeData;
  routeData.m_title = lineSubway.GetNumber();
  routeData.m_routeType = kSubwayRouteType;
  routeData.m_networkId = lineSubway.GetNetworkId();
  routeData.m_color = lineSubway.GetColor();

  return {routeId, routeData};
}

std::pair<TransitId, GateData> SubwayConverter::MakeGate(routing::transit::Gate const & gateSubway)
{
  // This id is used only for storing gates in gtfs_converter tool. It is not saved to json.
  TransitId const gateId =
      m_feed.m_idGenerator.MakeId(BuildHash(kHashPrefix, std::to_string(gateSubway.GetOsmId())));
  GateData gateData;

  gateData.m_isEntrance = gateSubway.GetEntrance();
  gateData.m_isExit = gateSubway.GetExit();
  gateData.m_point = gateSubway.GetPoint();
  gateData.m_osmId = gateSubway.GetOsmId();

  for (auto stopIdSubway : gateSubway.GetStopIds())
  {
    gateData.m_weights.emplace_back(TimeFromGateToStop(m_stopIdMapping[stopIdSubway] /* stopId */,
                                                       gateSubway.GetWeight() /* timeSeconds */));
  }

  return {gateId, gateData};
}

std::pair<TransitId, TransferData> SubwayConverter::MakeTransfer(
    routing::transit::Transfer const & transferSubway)
{
  TransitId const transferId =
      m_feed.m_idGenerator.MakeId(BuildHash(kHashPrefix, std::to_string(transferSubway.GetId())));

  TransferData transferData;
  transferData.m_point = transferSubway.GetPoint();

  for (auto stopIdSubway : transferSubway.GetStopIds())
    transferData.m_stopsIds.emplace_back(m_stopIdMapping[stopIdSubway]);

  return {transferId, transferData};
}

std::pair<TransitId, LineData> SubwayConverter::MakeLine(routing::transit::Line const & lineSubway,
                                                         TransitId routeId)
{
  TransitId const lineId = lineSubway.GetId();
  CHECK(!routing::FakeFeatureIds::IsTransitFeature(lineId), (lineId));

  LineData lineData;
  lineData.m_routeId = routeId;
  lineData.m_title = lineSubway.GetTitle();
  lineData.m_schedule.SetDefaultFrequency(lineSubway.GetInterval());

  return {lineId, lineData};
}

std::pair<EdgeId, EdgeData> SubwayConverter::MakeEdge(routing::transit::Edge const & edgeSubway,
                                                      uint32_t index)
{
  auto const lineId = edgeSubway.GetLineId();
  EdgeId const edgeId(m_stopIdMapping[edgeSubway.GetStop1Id()],
                      m_stopIdMapping[edgeSubway.GetStop2Id()], lineId);
  EdgeData edgeData;
  edgeData.m_weight = edgeSubway.GetWeight();
  edgeData.m_featureId = index;

  CHECK(m_feed.m_edgesOnShapes.find(edgeId) != m_feed.m_edgesOnShapes.end(), (lineId));

  edgeData.m_shapeLink.m_shapeId = m_feed.m_lines.m_data[lineId].m_shapeLink.m_shapeId;

  return {edgeId, edgeData};
}

std::pair<EdgeTransferId, EdgeData> SubwayConverter::MakeEdgeTransfer(
    routing::transit::Edge const & edgeSubway, uint32_t index)
{
  EdgeTransferId const edgeTransferId(m_stopIdMapping[edgeSubway.GetStop1Id()] /* fromStopId */,
                                      m_stopIdMapping[edgeSubway.GetStop2Id()] /* toStopId */);
  EdgeData edgeData;
  edgeData.m_weight = edgeSubway.GetWeight();
  edgeData.m_featureId = index;

  return {edgeTransferId, edgeData};
}

std::pair<TransitId, StopData> SubwayConverter::MakeStop(routing::transit::Stop const & stopSubway)
{
  TransitId const stopId = m_stopIdMapping[stopSubway.GetId()];

  StopData stopData;
  stopData.m_point = stopSubway.GetPoint();
  stopData.m_osmId = stopSubway.GetOsmId();

  if (stopSubway.GetFeatureId() != kInvalidFeatureId)
    stopData.m_featureId = stopSubway.GetFeatureId();

  return {stopId, stopData};
}

bool SubwayConverter::ConvertLinesBasedData()
{
  auto const & linesSubway = m_graphData.GetLines();
  m_feed.m_lines.m_data.reserve(linesSubway.size());
  m_feed.m_shapes.m_data.reserve(linesSubway.size());

  auto const & shapesSubway = m_graphData.GetShapes();

  for (auto const & lineSubway : linesSubway)
  {
    auto const [routeId, routeData] = MakeRoute(lineSubway);
    m_feed.m_routes.m_data.emplace(routeId, routeData);

    auto [lineId, lineData] = MakeLine(lineSubway, routeId);

    TransitId const shapeId = m_feed.m_idGenerator.MakeId(
        BuildHash(kHashPrefix, std::string("shape"), std::to_string(lineId)));
    lineData.m_shapeId = shapeId;

    ShapeData shapeData;
    shapeData.m_lineIds.insert(lineId);

    CHECK_EQUAL(lineSubway.GetStopIds().size(), 1, ("Line shouldn't be split into ranges."));

    auto const & stopIdsSubway = lineSubway.GetStopIds().front();

    CHECK_GREATER(stopIdsSubway.size(), 1, ("Range must include at least two stops."));

    for (size_t i = 0; i < stopIdsSubway.size(); ++i)
    {
      auto const stopIdSubway = stopIdsSubway[i];
      std::string const stopHash = BuildHash(kHashPrefix, std::to_string(stopIdSubway));
      TransitId const stopId = m_feed.m_idGenerator.MakeId(stopHash);

      lineData.m_stopIds.emplace_back(stopId);
      m_stopIdMapping.emplace(stopIdSubway, stopId);

      if (i == 0)
        continue;

      auto const stopIdSubwayPrev = stopIdsSubway[i - 1];
      CHECK(stopIdSubwayPrev != stopIdSubway, (stopIdSubway));

      auto const & edge = FindEdge(stopIdSubwayPrev, stopIdSubway, lineId);

      CHECK_LESS_OR_EQUAL(edge.GetShapeIds().size(), 1, (edge));

      std::vector<m2::PointD> edgePoints;

      m2::PointD const prevPoint = FindById(m_graphData.GetStops(), stopIdSubwayPrev)->GetPoint();
      m2::PointD const curPoint = FindById(m_graphData.GetStops(), stopIdSubway)->GetPoint();

      if (edge.GetShapeIds().empty())
      {
        edgePoints.push_back(prevPoint);
        edgePoints.push_back(curPoint);
      }
      else
      {
        routing::transit::ShapeId shapeIdSubway = edge.GetShapeIds().back();

        auto polyline = FindById(shapesSubway, shapeIdSubway)->GetPolyline();

        CHECK(polyline.size() > 1, ());

        double const distToPrevStop = mercator::DistanceOnEarth(polyline.front(), prevPoint);
        double const distToNextStop = mercator::DistanceOnEarth(polyline.front(), curPoint);

        if (distToPrevStop > distToNextStop)
          std::reverse(polyline.begin(), polyline.end());

        // We remove duplicate point from the shape before appending polyline to it.
        if (!shapeData.m_points.empty() && shapeData.m_points.back() == polyline.front())
          shapeData.m_points.pop_back();

        shapeData.m_points.insert(shapeData.m_points.end(), polyline.begin(), polyline.end());

        edgePoints = polyline;
      }

      EdgeId const curEdge(m_stopIdMapping[stopIdSubwayPrev], stopId, lineId);

      auto [itEdgeOnShape, inserted] =
          m_feed.m_edgesOnShapes.emplace(curEdge, std::vector<std::vector<m2::PointD>>{edgePoints});
      if (inserted)
      {
        itEdgeOnShape->second.push_back(edgePoints);
        LOG(LWARNING, ("Edge duplicate in subways. stop1_id", stopIdSubwayPrev, "stop2_id",
                       stopIdSubway, "line_id", lineId));
      }
    }

    m_feed.m_lines.m_data.emplace(lineId, lineData);
    m_feed.m_shapes.m_data.emplace(shapeId, shapeData);
  }

  LOG(LDEBUG, ("Converted", m_feed.m_routes.m_data.size(), "routes,", m_feed.m_lines.m_data.size(),
               "lines."));

  return !m_feed.m_lines.m_data.empty();
}

bool SubwayConverter::ConvertStops()
{
  auto const & stopsSubway = m_graphData.GetStops();
  m_feed.m_stops.m_data.reserve(stopsSubway.size());

  for (auto const & stopSubway : stopsSubway)
    m_feed.m_stops.m_data.emplace(MakeStop(stopSubway));

  LOG(LINFO, ("Converted", m_feed.m_stops.m_data.size(), "stops."));

  return !m_feed.m_stops.m_data.empty();
}

bool SubwayConverter::ConvertTransfers()
{
  auto const & transfersSubway = m_graphData.GetTransfers();
  m_feed.m_transfers.m_data.reserve(transfersSubway.size());

  for (auto const & transferSubway : transfersSubway)
  {
    auto const [transferId, transferData] = MakeTransfer(transferSubway);

    std::map<TransitId, std::set<TransitId>> routeToStops;

    for (auto const & stopId : transferData.m_stopsIds)
    {
      for (auto const & [lineId, lineData] : m_feed.m_lines.m_data)
      {
        if (base::IsExist(lineData.m_stopIds, stopId))
          routeToStops[lineData.m_routeId].insert(stopId);
      }
    }

    // We don't count as transfers transfer points between lines on the same route, so we skip them.
    if (routeToStops.size() < 2)
    {
      LOG(LINFO, ("Skip transfer on route", transferId));
      continue;
    }

    m_feed.m_transfers.m_data.emplace(transferId, transferData);

    // All stops are already present in |m_feed| before the |ConvertTransfers()| call.
    for (auto stopId : transferData.m_stopsIds)
      LinkTransferIdToStop(m_feed.m_stops.m_data.at(stopId), transferId);
  }

  LOG(LINFO, ("Converted", m_feed.m_transfers.m_data.size(), "transfers."));

  return !m_feed.m_transfers.m_data.empty();
}

bool SubwayConverter::ConvertGates()
{
  auto const & gatesSubway = m_graphData.GetGates();
  m_feed.m_gates.m_data.reserve(gatesSubway.size());

  for (auto const & gateSubway : gatesSubway)
  {
    auto const [gateId, gateData] = MakeGate(gateSubway);
    m_feed.m_gates.m_data.emplace(gateId, gateData);
  }

  LOG(LINFO, ("Converted", m_feed.m_gates.m_data.size(), "gates."));

  return !m_feed.m_gates.m_data.empty();
}

bool SubwayConverter::ConvertEdges()
{
  for (auto const & [edgeSubway, index] : m_edgesSubway)
    m_feed.m_edges.m_data.emplace(MakeEdge(edgeSubway, index));

  LOG(LINFO, ("Converted", m_feed.m_edges.m_data.size(), "edges."));

  for (auto const & [edgeTransferSubway, index] : m_edgesTransferSubway)
    m_feed.m_edgesTransfers.m_data.emplace(MakeEdgeTransfer(edgeTransferSubway, index));

  LOG(LINFO, ("Converted", m_feed.m_edgesTransfers.m_data.size(), "transfer edges."));

  return !m_feed.m_edges.m_data.empty() && !m_feed.m_edgesTransfers.m_data.empty();
}

void SubwayConverter::MinimizeReversedLinesCount()
{
  for (auto & [lineId, lineData] : m_feed.m_lines.m_data)
  {
    if (lineData.m_shapeLink.m_startIndex < lineData.m_shapeLink.m_endIndex)
      continue;

    auto revStopIds = GetReversed(lineData.m_stopIds);

    bool reversed = false;

    for (auto const & [lineIdStraight, lineDataStraight] : m_feed.m_lines.m_data)
    {
      if (lineIdStraight == lineId ||
          lineDataStraight.m_shapeLink.m_startIndex > lineDataStraight.m_shapeLink.m_endIndex ||
          lineDataStraight.m_shapeLink.m_shapeId != lineData.m_shapeLink.m_shapeId)
      {
        continue;
      }

      if (revStopIds == lineDataStraight.m_stopIds)
      {
        lineData.m_shapeLink = lineDataStraight.m_shapeLink;
        LOG(LDEBUG, ("Reversed line", lineId, "to line", lineIdStraight, "shapeLink",
                     lineData.m_shapeLink));
        reversed = true;
        break;
      }
    }

    if (!reversed)
    {
      std::swap(lineData.m_shapeLink.m_startIndex, lineData.m_shapeLink.m_endIndex);
      LOG(LDEBUG, ("Reversed line", lineId, "shapeLink", lineData.m_shapeLink));
    }
  }
}

std::vector<LineSchemeData> SubwayConverter::GetLinesOnScheme(
    std::unordered_map<TransitId, LineSegmentInRegion> const & linesInRegion) const
{
  // Color of line to shape link and one of line ids with this link.
  std::map<std::string, std::map<ShapeLink, TransitId>> colorsToLines;

  for (auto const & [lineId, lineData] : linesInRegion)
  {
    if (lineData.m_splineParent)
    {
      LOG(LINFO, ("Line is short spline. We skip it. Id", lineId));
      continue;
    }

    auto itLine = m_feed.m_lines.m_data.find(lineId);
    CHECK(itLine != m_feed.m_lines.m_data.end(), ());

    TransitId const routeId = itLine->second.m_routeId;
    auto itRoute = m_feed.m_routes.m_data.find(routeId);
    CHECK(itRoute != m_feed.m_routes.m_data.end(), ());
    std::string const & color = itRoute->second.m_color;

    ShapeLink const & newShapeLink = lineData.m_shapeLink;

    auto [it, inserted] = colorsToLines.emplace(color, std::map<ShapeLink, TransitId>());

    if (inserted)
    {
      it->second[newShapeLink] = lineId;
      continue;
    }

    bool insert = true;
    std::vector<ShapeLink> linksForRemoval;

    for (auto const & [shapeLink, lineId] : it->second)
    {
      if (shapeLink.m_shapeId != newShapeLink.m_shapeId)
        continue;

      // New shape link is fully included into the existing one.
      if (shapeLink.m_startIndex <= newShapeLink.m_startIndex &&
          shapeLink.m_endIndex >= newShapeLink.m_endIndex)
      {
        insert = false;
        continue;
      }

      // Existing shape link is fully included into the new one. It should be removed.
      if (newShapeLink.m_startIndex <= shapeLink.m_startIndex &&
          newShapeLink.m_endIndex >= shapeLink.m_endIndex)
      {
        linksForRemoval.push_back(shapeLink);
      }
    }

    for (auto const & sl : linksForRemoval)
      it->second.erase(sl);

    if (insert)
      it->second[newShapeLink] = lineId;
  }

  std::vector<LineSchemeData> linesOnScheme;

  for (auto const & [color, linksToLines] : colorsToLines)
  {
    CHECK(!linksToLines.empty(), (color));

    for (auto const & [shapeLink, lineId] : linksToLines)
    {
      LineSchemeData data;
      data.m_lineId = lineId;
      data.m_color = color;
      data.m_shapeLink = shapeLink;

      linesOnScheme.push_back(data);
    }
  }

  return linesOnScheme;
}

enum class LineSegmentState
{
  Start = 0,
  Finish
};

struct LineSegmentInfo
{
  LineSegmentInfo() = default;
  LineSegmentInfo(LineSegmentState const & state, bool codirectional)
    : m_state(state), m_codirectional(codirectional)
  {
  }

  LineSegmentState m_state = LineSegmentState::Start;
  bool m_codirectional = false;
};

struct LinePointState
{
  std::map<TransitId, LineSegmentInfo> parallelLineStates;
  m2::PointD m_firstPoint;
};

using LineGeometry = std::vector<m2::PointD>;
using ColorToLinepartsCache = std::map<std::string, std::vector<LineGeometry>>;

bool Equal(LineGeometry const & line1, LineGeometry const & line2)
{
  if (line1.size() != line2.size())
    return false;

  for (size_t i = 0; i < line1.size(); ++i)
  {
    if (!AlmostEqualAbs(line1[i], line2[i], kEps))
      return false;
  }

  return true;
}

bool AddToCache(std::string const & color, LineGeometry const & linePart,
                ColorToLinepartsCache & cache)
{
  auto [it, inserted] = cache.emplace(color, std::vector<LineGeometry>());

  if (inserted)
  {
    it->second.push_back(linePart);
    return true;
  }

  std::vector<m2::PointD> linePartRev = GetReversed(linePart);

  for (LineGeometry const & cachedPart : it->second)
  {
    if (Equal(cachedPart, linePart) || Equal(cachedPart, linePartRev))
      return false;
  }

  it->second.push_back(linePart);
  return true;
}

void SubwayConverter::CalculateLinePriorities(std::vector<LineSchemeData> const & linesOnScheme)
{
  ColorToLinepartsCache routeSegmentsCache;

  for (auto const & lineSchemeData : linesOnScheme)
  {
    auto const lineId = lineSchemeData.m_lineId;
    std::map<size_t, LinePointState> linePoints;

    for (auto const & linePart : lineSchemeData.m_lineParts)
    {
      auto & startPointState = linePoints[linePart.m_segment.m_startIdx];
      auto & endPointState = linePoints[linePart.m_segment.m_endIdx];

      startPointState.m_firstPoint = linePart.m_firstPoint;

      for (auto const & [parallelLineId, parallelFirstPoint] : linePart.m_commonLines)
      {
        bool const codirectional =
            AlmostEqualAbs(linePart.m_firstPoint, parallelFirstPoint, kEps);

        startPointState.parallelLineStates[parallelLineId] =
            LineSegmentInfo(LineSegmentState::Start, codirectional);
        endPointState.parallelLineStates[parallelLineId] =
            LineSegmentInfo(LineSegmentState::Finish, codirectional);
      }
    }

    linePoints.emplace(lineSchemeData.m_shapeLink.m_startIndex, LinePointState());
    linePoints.emplace(lineSchemeData.m_shapeLink.m_endIndex, LinePointState());

    std::map<TransitId, bool> parallelLines;

    for (auto it = linePoints.begin(); it != linePoints.end(); ++it)
    {
      auto itNext = std::next(it);
      if (itNext == linePoints.end())
        break;

      auto & startLinePointState = it->second;
      size_t startIndex = it->first;
      size_t endIndex = itNext->first;

      for (auto const & [id, info] : startLinePointState.parallelLineStates)
      {
        if (info.m_state == LineSegmentState::Start)
        {
          auto [itParLine, insertedParLine] = parallelLines.emplace(id, info.m_codirectional);
          if (!insertedParLine)
            CHECK_EQUAL(itParLine->second, info.m_codirectional, ());
        }
        else
        {
          parallelLines.erase(id);
        }
      }

      TransitId const routeId = m_feed.m_lines.m_data.at(lineId).m_routeId;
      std::string color = m_feed.m_routes.m_data.at(routeId).m_color;

      std::map<std::string, bool> colors{{color, true /* codirectional */}};
      bool colorCopy = false;

      for (auto const & [id, codirectional] : parallelLines)
      {
        TransitId const parallelRoute = m_feed.m_lines.m_data.at(id).m_routeId;
        auto const parallelColor = m_feed.m_routes.m_data.at(parallelRoute).m_color;
        colors.emplace(parallelColor, codirectional);

        if (parallelColor == color && id < lineId)
          colorCopy = true;
      }

      if (colorCopy)
      {
        LOG(LINFO, ("Skip line segment with color copy", color, "line id", lineId));
        continue;
      }

      LineSegmentOrder lso;
      lso.m_segment =
          LineSegment(static_cast<uint32_t>(startIndex), static_cast<uint32_t>(endIndex));

      auto const & polyline =
          m_feed.m_shapes.m_data.at(lineSchemeData.m_shapeLink.m_shapeId).m_points;

      if (!AddToCache(color,
                      GetPolylinePart(polyline, lso.m_segment.m_startIdx, lso.m_segment.m_endIdx),
                      routeSegmentsCache))
      {
        continue;
      }

      auto itColor = colors.find(color);
      CHECK(itColor != colors.end(), ());

      size_t const index = std::distance(colors.begin(), itColor);

      lso.m_order = CalcSegmentOrder(index, colors.size());

      bool reversed = false;

      if (index > 0 && !colors.begin()->second /* codirectional */)
      {
        lso.m_order = -lso.m_order;
        reversed = true;
      }

      m_feed.m_linesMetadata.m_data[lineId].push_back(lso);

      LOG(LINFO,
          ("routeId", routeId, "lineId", lineId, "start", startIndex, "end", endIndex, "len",
           endIndex - startIndex + 1, "order", lso.m_order, "index", index, "reversed", reversed,
           "|| lines count:", parallelLines.size(), "colors count:", colors.size()));
    }
  }
}

void SubwayConverter::PrepareLinesMetadata()
{
  for (auto const & [region, linesInRegion] : m_feed.m_splitting.m_lines)
  {
    LOG(LINFO, ("Preparing metadata for", region, "region"));

    std::vector<LineSchemeData> linesOnScheme = GetLinesOnScheme(linesInRegion);

    for (size_t i = 0; i < linesOnScheme.size() - 1; ++i)
    {
      auto & line1 = linesOnScheme[i];
      auto const & shapeLink1 = linesInRegion.at(line1.m_lineId).m_shapeLink;

      // |polyline1| is sub-polyline of the shapeLink1 geometry.
      auto const polyline1 =
          GetPolylinePart(m_feed.m_shapes.m_data.at(shapeLink1.m_shapeId).m_points,
                          shapeLink1.m_startIndex, shapeLink1.m_endIndex);

      for (size_t j = i + 1; j < linesOnScheme.size(); ++j)
      {
        auto & line2 = linesOnScheme[j];
        auto const & shapeLink2 = linesInRegion.at(line2.m_lineId).m_shapeLink;

        if (line1.m_shapeLink.m_shapeId == line2.m_shapeLink.m_shapeId)
        {
          CHECK_LESS(shapeLink1.m_startIndex, shapeLink1.m_endIndex, ());
          CHECK_LESS(shapeLink2.m_startIndex, shapeLink2.m_endIndex, ());

          std::optional<LineSegment> inter =
              GetIntersection(shapeLink1.m_startIndex, shapeLink1.m_endIndex,
                              shapeLink2.m_startIndex, shapeLink2.m_endIndex);

          if (inter != std::nullopt)
          {
            LineSegment const segment = inter.value();
            m2::PointD const & startPoint = polyline1[segment.m_startIdx];

            UpdateLinePart(line1.m_lineParts, segment, startPoint, line2.m_lineId, startPoint);
            UpdateLinePart(line2.m_lineParts, segment, startPoint, line1.m_lineId, startPoint);
          }
        }
        else
        {
          // |polyline2| is sub-polyline of the shapeLink2 geometry.
          auto polyline2 = GetPolylinePart(m_feed.m_shapes.m_data.at(shapeLink2.m_shapeId).m_points,
                                           shapeLink2.m_startIndex, shapeLink2.m_endIndex);

          auto [segments1, segments2] = FindIntersections(polyline1, polyline2);

          if (segments1.empty())
          {
            auto polyline2Rev = GetReversed(polyline2);
            std::tie(segments1, segments2) = FindIntersections(polyline1, polyline2Rev);

            if (!segments1.empty())
            {
              for (auto & seg : segments2)
                UpdateReversedSegmentIndexes(seg, polyline2.size());
            }
          }

          if (!segments1.empty())
          {
            for (size_t k = 0; k < segments1.size(); ++k)
            {
              auto const & [startPoint1, endPoint1] =
                  GetSegmentEdgesOnPolyline(polyline1, segments1[k]);
              auto const & [startPoint2, endPoint2] =
                  GetSegmentEdgesOnPolyline(polyline2, segments2[k]);

              CHECK((AlmostEqualAbs(startPoint1, startPoint2, kEps) &&
                     AlmostEqualAbs(endPoint1, endPoint2, kEps)) ||
                    (AlmostEqualAbs(startPoint1, endPoint2, kEps) &&
                     AlmostEqualAbs(endPoint1, startPoint2, kEps)), ());

              ShiftSegmentOnShape(segments1[k], shapeLink1);
              ShiftSegmentOnShape(segments2[k], shapeLink2);

              UpdateLinePart(line1.m_lineParts, segments1[k], startPoint1, line2.m_lineId,
                             startPoint2);
              UpdateLinePart(line2.m_lineParts, segments2[k], startPoint2, line1.m_lineId,
                             startPoint1);
            }
          }
        }
      }
    }

    CalculateLinePriorities(linesOnScheme);
    LOG(LINFO, ("Prepared metadata for lines in", region));
  }
}

routing::transit::Edge SubwayConverter::FindEdge(routing::transit::StopId stop1Id,
                                                 routing::transit::StopId stop2Id,
                                                 routing::transit::LineId lineId) const
{
  routing::transit::Edge edge(stop1Id, stop2Id, 0 /* weight */, lineId, false /* transfer */,
                              {} /* shapeIds */);

  auto const itEdge = m_edgesSubway.find(edge);

  CHECK(itEdge != m_edgesSubway.end(), (stop1Id, stop2Id, lineId));

  return itEdge->first;
}
}  // namespace transit
