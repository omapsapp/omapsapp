#include "transit/world_feed/feed_helpers.hpp"

#include "geometry/mercator.hpp"
#include "geometry/parametrized_segment.hpp"
#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"

#include <cmath>

namespace
{
// Epsilon for m2::PointD comparison.
double constexpr kEps = 1e-5;

struct ProjectionData
{
  // Projection to polyline.
  m2::PointD m_proj;
  // Index before which the projection will be inserted.
  size_t m_indexOnShape = 0;
  // Distance from point to its projection.
  double m_distFromPoint = 0.0;
  // Distance from the first ending (start for forward direction, end for backward) point on
  // polyline to the projection.
  double m_distFromEnding = 0.0;
  // Point on polyline almost equal to the projection can already exist, so we don't need to
  // insert projection. Or we insert it to the polyline.
  bool m_needsInsertion = false;
};

// Returns true if |p1| is much closer to the first ending (start for forward direction, end for
// backward) then |p2| (parameter |distDeltaEnding|) and its distance to projections to polyline
// |m_distFromPoint| is comparable.
bool CloserToEndingAndOnSimilarDistToLine(ProjectionData const & p1, ProjectionData const & p2)
{
  // Delta between two points distances from start point on polyline.
  double constexpr distDeltaStart = 100.0;
  // Delta between two points distances from their corresponding projections to polyline.
  double constexpr distDeltaProj = 90.0;

  return (p1.m_distFromEnding + distDeltaStart < p2.m_distFromEnding &&
          std::abs(p2.m_distFromPoint - p1.m_distFromPoint) <= distDeltaProj);
}
}  // namespace

namespace transit
{
ProjectionToShape ProjectStopOnTrack(m2::PointD const & stopPoint, m2::PointD const & point1,
                                     m2::PointD const & point2)
{
  m2::PointD const stopProjection =
      m2::ParametrizedSegment<m2::PointD>(point1, point2).ClosestPointTo(stopPoint);
  double const distM = mercator::DistanceOnEarth(stopProjection, stopPoint);
  return {stopProjection, distM};
}

ProjectionData GetProjection(std::vector<m2::PointD> const & polyline, size_t index,
                             Direction direction, ProjectionToShape const & proj)
{
  ProjectionData projData;
  projData.m_distFromPoint = proj.m_dist;
  projData.m_proj = proj.m_point;

  int64_t const next = direction == Direction::Forward ? index + 1 : index - 1;
  CHECK_GREATER_OR_EQUAL(next, 0, ());
  CHECK_LESS(static_cast<size_t>(next), polyline.size(), ());

  if (AlmostEqualAbs(proj.m_point, polyline[index], kEps))
  {
    projData.m_indexOnShape = index;
    projData.m_needsInsertion = false;
  }
  else if (AlmostEqualAbs(proj.m_point, polyline[next], kEps))
  {
    projData.m_indexOnShape = next;
    projData.m_needsInsertion = false;
  }
  else
  {
    projData.m_indexOnShape = direction == Direction::Forward ? next : index;
    projData.m_needsInsertion = true;
  }

  return projData;
}

void FillProjections(std::vector<m2::PointD> & polyline, size_t startIndex, size_t endIndex,
                     m2::PointD const & point, double distStopsM, Direction direction,
                     std::vector<ProjectionData> & projections)
{
  CHECK_LESS_OR_EQUAL(startIndex, endIndex, ());

  double distTravelledM = 0.0;
  // Stop can't be further from its projection to line then |maxDistFromStopM|.
  double constexpr maxDistFromStopM = 1000;

  size_t const from = direction == Direction::Forward ? startIndex : endIndex;

  auto const endCriterion = [&](size_t i) {
    return direction == Direction::Forward ? i < endIndex : i > startIndex;
  };

  auto const move = [&](size_t & i) {
    direction == Direction::Forward ? ++i : --i;
    CHECK_LESS_OR_EQUAL(i, polyline.size(), ());
  };

  for (size_t i = from; endCriterion(i); move(i))
  {
    auto const current = i;
    auto const prev = direction == Direction::Forward ? i - 1 : i + 1;
    auto const next = direction == Direction::Forward ? i + 1 : i - 1;

    if (i != from)
      distTravelledM += mercator::DistanceOnEarth(polyline[prev], polyline[current]);

    auto proj = GetProjection(polyline, current, direction,
                              ProjectStopOnTrack(point, polyline[current], polyline[next]));
    proj.m_distFromEnding =
        distTravelledM + mercator::DistanceOnEarth(polyline[current], proj.m_proj);

    // The distance on the polyline between the projections of stops must not be less than the
    // shortest possible distance between the stops themselves.
    if (proj.m_distFromEnding < distStopsM)
      continue;

    if (proj.m_distFromPoint < maxDistFromStopM)
      projections.emplace_back(proj);
  }
}

std::pair<size_t, bool> PrepareNearestPointOnTrack(m2::PointD const & point,
                                                   std::optional<m2::PointD> const & prevPoint,
                                                   size_t prevIndex, Direction direction,
                                                   std::vector<m2::PointD> & polyline)
{
  // We skip 70% of the distance in a straight line between two stops for preventing incorrect
  // projection of the |point| to the polyline of complex shape.
  double const distStopsM = prevPoint ? mercator::DistanceOnEarth(point, *prevPoint) * 0.7 : 0.0;

  std::vector<ProjectionData> projections;
  // Reserve space for points on polyline which are relatively close to the shape.
  // Approximately 1/4 of all points on shape.
  auto const size = direction == Direction::Forward ? polyline.size() - prevIndex : prevIndex;
  projections.reserve(size / 4);

  auto const startIndex = direction == Direction::Forward ? prevIndex : 0;
  auto const endIndex = direction == Direction::Forward ? polyline.size() - 1 : prevIndex;
  FillProjections(polyline, startIndex, endIndex, point, distStopsM, direction, projections);

  if (projections.empty())
    return {polyline.size() + 1, false};

  // We find the most fitting projection of the stop to the polyline. For two different projections
  // with approximately equal distances to the stop the most preferable is the one that is closer
  // to the beginning of the polyline segment.
  auto const cmp = [](ProjectionData const & p1, ProjectionData const & p2) {
    if (CloserToEndingAndOnSimilarDistToLine(p1, p2))
      return true;

    if (CloserToEndingAndOnSimilarDistToLine(p2, p1))
      return false;

    if (p1.m_distFromPoint == p2.m_distFromPoint)
      return p1.m_distFromEnding < p2.m_distFromEnding;

    return p1.m_distFromPoint < p2.m_distFromPoint;
  };

  auto proj = std::min_element(projections.begin(), projections.end(), cmp);

  // This case is possible not only for the first stop on the shape. We try to resolve situation
  // when two stops are projected to the same point on the shape.
  if (proj->m_indexOnShape == prevIndex)
  {
    proj = std::min_element(projections.begin(), projections.end(),
                         [](ProjectionData const & p1, ProjectionData const & p2) {
                           return p1.m_distFromPoint < p2.m_distFromPoint;
                         });
  }

  if (proj->m_needsInsertion)
    polyline.insert(polyline.begin() + proj->m_indexOnShape, proj->m_proj);

  return {proj->m_indexOnShape, proj->m_needsInsertion};
}

bool IsRelevantType(gtfs::RouteType const & routeType)
{
  // All types and constants are described in GTFS:
  // https://developers.google.com/transit/gtfs/reference

  auto const isSubway = [](gtfs::RouteType const & routeType) {
    return routeType == gtfs::RouteType::Subway ||
           routeType == gtfs::RouteType::MetroService ||
           routeType == gtfs::RouteType::UndergroundService;
  };

  // We skip all subways because we extract subway data from OSM, not from GTFS.
  if (isSubway(routeType))
    return false;

  auto const val = static_cast<size_t>(routeType);
  // "Classic" GTFS route types.
  if (val < 8 || (val > 10 && val < 13))
    return true;

  // Extended GTFS route types.
  // We do not handle taxi services.
  if (val >= 1500)
    return false;

  // Other not relevant types - school buses, lorry services etc.
  static std::vector<gtfs::RouteType> const kNotRelevantTypes{
      gtfs::RouteType::CarTransportRailService,
      gtfs::RouteType::LorryTransportRailService,
      gtfs::RouteType::VehicleTransportRailService,
      gtfs::RouteType::PostBusService,
      gtfs::RouteType::SpecialNeedsBus,
      gtfs::RouteType::MobilityBusService,
      gtfs::RouteType::MobilityBusForRegisteredDisabled,
      gtfs::RouteType::SchoolBus,
      gtfs::RouteType::SchoolAndPublicServiceBus};

  return !base::IsExist(kNotRelevantTypes, routeType);
}

std::string ToString(gtfs::RouteType const & routeType)
{
  // GTFS route types.
  switch (routeType)
  {
  case gtfs::RouteType::Tram: return "tram";
  case gtfs::RouteType::Subway: return "subway";
  case gtfs::RouteType::Rail: return "rail";
  case gtfs::RouteType::Bus: return "bus";
  case gtfs::RouteType::Ferry: return "ferry";
  case gtfs::RouteType::CableTram: return "cable_tram";
  case gtfs::RouteType::AerialLift: return "aerial_lift";
  case gtfs::RouteType::Funicular: return "funicular";
  case gtfs::RouteType::Trolleybus: return "trolleybus";
  case gtfs::RouteType::Monorail: return "monorail";
  default:
    // Extended GTFS route types.
    return ToStringExtendedType(routeType);
  }
}

std::string ToStringExtendedType(gtfs::RouteType const & routeType)
{
  // These constants refer to extended GTFS routes types.
  auto const val = static_cast<size_t>(routeType);
  if (val >= 100 && val < 200)
    return "rail";

  if (val >= 200 && val < 300)
    return "bus";

  if (val == 405)
    return "monorail";

  if (val >= 400 && val < 500)
    return "rail";

  if (val >= 700 && val < 800)
    return "bus";

  if (val == 800)
    return "trolleybus";

  if (val >= 900 && val < 1000)
    return "tram";

  if (val == 1000)
    return "water_service";

  if (val == 1100)
    return "air_service";

  if (val == 1200)
    return "ferry";

  if (val == 1300)
    return "aerial_lift";

  if (val == 1400)
    return "funicular";

  LOG(LINFO, ("Unrecognized route type", val));
  return {};
}

gtfs::StopTimes GetStopTimesForTrip(gtfs::StopTimes const & allStopTimes,
                                    std::string const & tripId)
{
  gtfs::StopTime reference;
  reference.trip_id = tripId;

  auto itStart = std::lower_bound(
      allStopTimes.begin(), allStopTimes.end(), reference,
      [](const gtfs::StopTime & t1, const gtfs::StopTime & t2) { return t1.trip_id < t2.trip_id; });

  if (itStart == allStopTimes.end())
    return {};
  auto itEnd = itStart;
  while (itEnd != allStopTimes.end() && itEnd->trip_id == tripId)
    ++itEnd;

  gtfs::StopTimes res(itStart, itEnd);
  std::sort(res.begin(), res.end(), [](gtfs::StopTime const & t1, gtfs::StopTime const & t2) {
    return t1.stop_sequence < t2.stop_sequence;
  });
  return res;
}

void UpdateLinePart(LineParts & lineParts, LineSegment const & segment,
                    m2::PointD const & startPoint, TransitId commonLineId,
                    m2::PointD const & startPointParallel)
{
  if (auto it = FindLinePart(lineParts, segment); it == lineParts.end())
  {
    LinePart lp;
    lp.m_segment = segment;
    lp.m_commonLines[commonLineId] = startPointParallel;
    lp.m_firstPoint = startPoint;
    lineParts.push_back(lp);
  }
  else
  {
    it->m_commonLines[commonLineId] = startPointParallel;
  }
}

std::pair<LineSegments, LineSegments> FindIntersections(std::vector<m2::PointD> const & line1,
                                                        std::vector<m2::PointD> const & line2)
{
  double constexpr eps = 1e-5;
  size_t constexpr minIntersection = 2;

  CHECK_GREATER_OR_EQUAL(line1.size(), minIntersection, ());
  CHECK_GREATER_OR_EQUAL(line2.size(), minIntersection, ());

  std::pair<LineSegments, LineSegments> intersections;

  // Find start indexes of line1 and line2 intersections.
  size_t i = 0;

  while (i < line1.size() - minIntersection + 1)
  {
    size_t j = 0;
    size_t delta = 1;

    while (j < line2.size() - minIntersection + 1)
    {
      size_t intersection = 0;
      size_t const len = std::min(line1.size() - i, line2.size() - j);

      for (size_t k = 0; k < len; ++k)
      {
        if (!AlmostEqualAbs(line1[i + k], line2[j + k], eps))
          break;
        ++intersection;
      }

      if (intersection >= minIntersection)
      {
        intersections.first.emplace_back(i, i + intersection - 1);
        intersections.second.emplace_back(j, j + intersection - 1);
        delta = intersection;
        break;
      }

      ++j;
    }

    i += delta;
  }

  CHECK_EQUAL(intersections.first.size(), intersections.second.size(), ());

  return intersections;
}

LineParts::iterator FindLinePart(LineParts & lineParts, LineSegment const & segment)
{
  return std::find_if(lineParts.begin(), lineParts.end(), [&segment](LinePart const & linePart) {
    return linePart.m_segment == segment;
  });
}

std::optional<LineSegment> GetIntersection(size_t start1, size_t finish1, size_t start2,
                                           size_t finish2)
{
  int const maxStart = static_cast<int>(std::max(start1, start2));
  int const minFinish = static_cast<int>(std::min(finish1, finish2));

  size_t const intersectionLen = std::max(minFinish - maxStart, 0);

  if (intersectionLen == 0)
    return std::nullopt;

  return LineSegment(maxStart, static_cast<uint32_t>(maxStart + intersectionLen));
}

int CalcSegmentOrder(size_t segIndex, size_t totalSegCount)
{
  int constexpr shapeOffsetIncrement = 2;

  int const shapeOffset =
      -static_cast<int>(totalSegCount / 2) * 2 - static_cast<int>(totalSegCount % 2) + 1;
  int const curSegOffset = shapeOffset + shapeOffsetIncrement * static_cast<int>(segIndex);

  return curSegOffset;
}

bool StopIndexIsSet(size_t stopIndex)
{
  return stopIndex != std::numeric_limits<size_t>::max();
}

std::pair<size_t, size_t> GetStopsRange(IdList const & lineStopIds, IdSet const & stopIdsInRegion)
{
  size_t first = std::numeric_limits<size_t>::max();
  size_t last = std::numeric_limits<size_t>::max();

  for (size_t i = 0; i < lineStopIds.size(); ++i)
  {
    auto const & stopId = lineStopIds[i];
    if (stopIdsInRegion.count(stopId) != 0)
    {
      if (!StopIndexIsSet(first))
      {
        first = i;
      }
      last = i;
    }
  }

  if (StopIndexIsSet(first))
  {
    if (first > 0)
      --first;

    if (last < lineStopIds.size() - 1)
      ++last;
  }

  CHECK_GREATER_OR_EQUAL(last, first, ());
  return {first, last};
}

// Returns indexes of nearest to the |point| elements in |shape|.
std::vector<size_t> GetMinDistIndexes(std::vector<m2::PointD> const & shape,
                                      m2::PointD const & point)
{
  double minDist = std::numeric_limits<double>::max();

  std::vector<size_t> indexes;

  for (size_t i = 0; i < shape.size(); ++i)
  {
    double dist = mercator::DistanceOnEarth(shape[i], point);

    if (AlmostEqualAbs(dist, minDist, kEps))
    {
      indexes.push_back(i);
      continue;
    }

    if (dist < minDist)
    {
      minDist = dist;
      indexes.clear();
      indexes.push_back(i);
    }
  }

  CHECK(std::is_sorted(indexes.begin(), indexes.end()), ());
  return indexes;
}

// Returns minimal distance between |val| and element in |vals| and the nearest element value.
std::pair<size_t, size_t> FindMinDist(size_t val, std::vector<size_t> const & vals)
{
  size_t minDist = std::numeric_limits<size_t>::max();
  size_t minVal;

  CHECK(!vals.empty(), ());

  for (size_t curVal : vals)
  {
    auto const & [min, max] = std::minmax(val, curVal);
    size_t const dist = max - min;

    if (dist < minDist)
    {
      minVal = curVal;
      minDist = dist;
    }
  }

  return {minDist, minVal};
}

std::pair<size_t, size_t> FindSegmentOnShape(std::vector<m2::PointD> const & shape,
                                             std::vector<m2::PointD> const & segment)
{
  auto const & intersectionsShape = FindIntersections(shape, segment).first;

  if (intersectionsShape.empty())
    return {0, 0};

  auto const & firstIntersection = intersectionsShape.front();
  return {firstIntersection.m_startIdx, firstIntersection.m_endIdx};
}

std::pair<size_t, size_t> FindPointsOnShape(std::vector<m2::PointD> const & shape,
                                            m2::PointD const & p1, m2::PointD const & p2)
{
  // We find indexes of nearest points in |shape| to |p1| and |p2| correspondingly.
  std::vector<size_t> const & indexes1 = GetMinDistIndexes(shape, p1);
  std::vector<size_t> const & indexes2 = GetMinDistIndexes(shape, p2);

  // We fill mapping of distance (between p1 and p2 on the shape) to pairs of indexes of p1 and p2.
  std::map<size_t, std::pair<size_t, size_t>> distToIndexes;

  for (size_t i1 : indexes1)
  {
    auto [minDist, i2] = FindMinDist(i1, indexes2);
    distToIndexes.emplace(minDist, std::make_pair(i1, i2));
  }

  CHECK(!distToIndexes.empty(), ());

  // If index of |p1| equals index of |p2| on the |shape| we return the next pair of the nearest
  // indexes. It is possible in case if |p1| and |p2| are ends of the edge which represents the loop
  // on the route.
  auto const & [first, last] = distToIndexes.begin()->second;
  if (first == last)
  {
    LOG(LINFO,
        ("Edge with equal indexes of first and last points on the shape. Index on the shape:",
         first));
    CHECK_GREATER(distToIndexes.size(), 1, ());

    auto const & nextPair = std::next(distToIndexes.begin());
    CHECK_NOT_EQUAL(nextPair->second.first, nextPair->second.second, ());

    return nextPair->second;
  }

  return distToIndexes.begin()->second;
}
}  // namespace transit
