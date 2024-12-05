#include "routing/fake_ending.hpp"

#include "routing/index_graph.hpp"
#include "routing/world_graph.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/mercator.hpp"
#include "geometry/parametrized_segment.hpp"

#include "base/math.hpp"

#include <tuple>
#include <utility>

namespace routing
{
using namespace routing;
using namespace std;

LatLonWithAltitude CalcProjectionToSegment(LatLonWithAltitude const & begin,
                                           LatLonWithAltitude const & end,
                                           m2::PointD const & point)
{
  m2::ParametrizedSegment<m2::PointD> segment(mercator::FromLatLon(begin.GetLatLon()),
                                              mercator::FromLatLon(end.GetLatLon()));

  auto const projectedPoint = segment.ClosestPointTo(point);
  auto const distBeginToEnd = ms::DistanceOnEarth(begin.GetLatLon(), end.GetLatLon());

  auto const projectedLatLon = mercator::ToLatLon(projectedPoint);

  double constexpr kEpsMeters = 2.0;
  if (AlmostEqualAbs(distBeginToEnd, 0.0, kEpsMeters))
    return LatLonWithAltitude(projectedLatLon, begin.GetAltitude());

  auto const distBeginToProjection =
      ms::DistanceOnEarth(begin.GetLatLon(), projectedLatLon);

  auto const altitude = begin.GetAltitude() + (end.GetAltitude() - begin.GetAltitude()) *
                                                  distBeginToProjection / distBeginToEnd;
  return LatLonWithAltitude(projectedLatLon, altitude);
}

bool Projection::operator==(const Projection & other) const
{
  return tie(m_segment, m_isOneWay, m_segmentFront, m_segmentBack, m_junction) ==
         tie(other.m_segment, other.m_isOneWay, other.m_segmentFront, other.m_segmentBack,
             other.m_junction);
}

FakeEnding MakeFakeEnding(vector<Segment> const & segments, m2::PointD const & point,
                          WorldGraph & graph)
{
  FakeEnding ending;
  double averageAltitude = 0.0;

  for (size_t i = 0; i < segments.size(); ++i)
  {
    auto const & segment = segments[i];

    bool const oneWay = graph.IsOneWay(segment.GetMwmId(), segment.GetFeatureId());
    auto const & frontJunction = graph.GetJunction(segment, true /* front */);
    auto const & backJunction = graph.GetJunction(segment, false /* front */);
    auto const & projectedJunction = CalcProjectionToSegment(backJunction, frontJunction, point);

    ending.m_projections.emplace_back(segment, oneWay, frontJunction, backJunction,
                                      projectedJunction);

    averageAltitude = (i * averageAltitude + projectedJunction.GetAltitude()) / (i + 1);
  }

  ending.m_originJunction =
      LatLonWithAltitude(mercator::ToLatLon(point), static_cast<geometry::Altitude>(averageAltitude));
  return ending;
}

FakeEnding MakeFakeEnding(Segment const & segment, m2::PointD const & point, IndexGraph & graph)
{
  auto const & road = graph.GetRoadGeometry(segment.GetFeatureId());
  bool const oneWay = road.IsOneWay();
  auto const & frontJunction = road.GetJunction(segment.GetPointId(true /* front */));
  auto const & backJunction = road.GetJunction(segment.GetPointId(false /* front */));
  auto const & projectedJunction = CalcProjectionToSegment(backJunction, frontJunction, point);

  FakeEnding ending;
  ending.m_originJunction =
      LatLonWithAltitude(mercator::ToLatLon(point), projectedJunction.GetAltitude());
  ending.m_projections.emplace_back(segment, oneWay, frontJunction, backJunction,
                                    projectedJunction);
  return ending;
}
}  // namespace routing
