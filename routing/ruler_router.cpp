#include "routing/ruler_router.hpp"
#include "routing/route.hpp"
#include "routing/routing_helpers.hpp"

namespace routing
{
using namespace std;

void RulerRouter::ClearState()
{
}

void RulerRouter::SetGuides(GuidesTracks && guides) { /*m_guides = GuidesConnections(guides);*/ }


/* Ruler router doesn't read roads graph and uses only checkpoints to build a route.

      1-----------2
     /             \
    /               \
   /                 F
  S

  For example we need to build route from start (S) to finish (F) with intermediate
  points (1) and (2).
  Target route should have parameters:

  m_geometry      = [S, S, 1, 1, 2, 2, F, F]
  m_routeSegments = [S, 1, 1, 2, 2, F, F]
  m_subroutes     =
    +-------+--------+-----------------+---------------+
    | start | finish | beginSegmentIdx | endSegmentIdx |
    +-------+--------+-----------------+---------------+
    | S     | 1      | 0               | 2             |
    | 1     | 1      | 1               | 2             |
    | 1     | 2      | 2               | 4             |
    | 2     | 2      | 3               | 4             |
    | 2     | F      | 4               | 6             |
    | F     | F      | 5               | 6             |
    +-------+--------+-----------------+---------------+

  Constraints:
  *  m_geometry.size() == 2 * checkpoints.size()
  *  m_routeSegments.size() == m_geometry.size()-1
  *  m_subroutes.size() == m_routeSegments.size()-1

 */
RouterResultCode RulerRouter::CalculateRoute(Checkpoints const & checkpoints,
                                             m2::PointD const & startDirection,
                                             bool adjustToPrevRoute,
                                             RouterDelegate const & delegate, Route & route)
{
  vector<m2::PointD> const & points = checkpoints.GetPoints();
  size_t const count = points.size();
  ASSERT(count > 0, ());

  vector<RouteSegment> routeSegments;
  routeSegments.reserve(count * 2 - 1);
  vector<double> times;
  times.reserve(count * 2 - 1);

  Segment const segment(kFakeNumMwmId, 0, 0, false);

  auto const ToPointWA = [](m2::PointD const & p)
  {
    return geometry::PointWithAltitude(p, 0 /* altitude */);
  };

  for (uint32_t i = 0; i < count; ++i)
  {
    turns::TurnItem turn(i, turns::PedestrianDirection::None);
    geometry::PointWithAltitude const junction = ToPointWA(points[i]);
    RouteSegment::RoadNameInfo const roadNameInfo;

    auto routeSegment = RouteSegment(segment, turn, junction, roadNameInfo);
    routeSegments.emplace_back(segment, turn, junction, roadNameInfo);
    times.push_back(0);

    if (i == count - 1)
      turn = turns::TurnItem(i + 1, turns::PedestrianDirection::ReachedYourDestination);
    else if (i == 0)
      continue;

    routeSegments.emplace_back(segment, turn, junction, roadNameInfo);
    times.push_back(0);
  }

  FillSegmentInfo(times, routeSegments);
  route.SetRouteSegments(std::move(routeSegments));

  vector<Route::SubrouteAttrs> subroutes;
  for(size_t i = 1; i < count; ++i)
  {
    subroutes.emplace_back(ToPointWA(points[i-1]), ToPointWA(points[i]), i*2-2, i*2);
    subroutes.emplace_back(ToPointWA(points[i-1]), ToPointWA(points[i]), i*2-1, i*2);
  }

  route.SetCurrentSubrouteIdx(checkpoints.GetPassedIdx());
  route.SetSubroteAttrs(std::move(subroutes));

  vector<m2::PointD> routeGeometry;
  for (auto p: points)
  {
    routeGeometry.push_back(p);
    routeGeometry.push_back(p);
  }

  route.SetGeometry(routeGeometry.begin(), routeGeometry.end());

  return RouterResultCode::NoError;
}

bool RulerRouter::FindClosestProjectionToRoad(m2::PointD const & point,
                                              m2::PointD const & direction, double radius,
                                              EdgeProj & proj)
{
  // Ruler router has no connection to road graph.
  return false;
}

}  // namespace routing
