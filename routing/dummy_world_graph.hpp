#pragma once

#include "routing/latlon_with_altitude.hpp"
#include "routing/world_graph.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/latlon.hpp"

#include "base/assert.hpp"

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace routing
{
// This is dummy class inherited from WorldGraph. Some of its overridden methods should never be
// called. This class is used in RegionsRouter as a lightweight replacement of
// SingleVehicleWorldGraph.
class DummyWorldGraph final : public WorldGraph
{
public:
  using WorldGraph::GetEdgeList;

  void GetEdgeList(astar::VertexData<Segment, RouteWeight> const &, bool, bool, bool, SegmentEdgeListT &) override
  {
    UNREACHABLE();
  }

  void GetEdgeList(astar::VertexData<JointSegment, RouteWeight> const &, Segment const &, bool, bool, JointEdgeListT &,
                   WeightListT &) override
  {
    UNREACHABLE();
  }

  bool CheckLength(RouteWeight const &, double) const override
  {
    return true;
  }

  LatLonWithAltitude const & GetJunction(Segment const &, bool) override
  {
    UNREACHABLE();
  }

  ms::LatLon const & GetPoint(Segment const &, bool) override
  {
    UNREACHABLE();
  }

  bool IsOneWay(NumMwmId, uint32_t) override
  {
    UNREACHABLE();
  }

  bool IsPassThroughAllowed(NumMwmId, uint32_t) override
  {
    UNREACHABLE();
  }

  void ClearCachedGraphs() override { UNREACHABLE(); }

  void SetMode(WorldGraphMode) override {}

  WorldGraphMode GetMode() const override { return WorldGraphMode::NoLeaps; }

  RouteWeight HeuristicCostEstimate(ms::LatLon const & from, ms::LatLon const & to) override
  {
    return RouteWeight(ms::DistanceOnEarth(from, to));
  }

  RouteWeight CalcSegmentWeight(Segment const &, EdgeEstimator::Purpose) override
  {
    UNREACHABLE();
  }

  RouteWeight CalcLeapWeight(ms::LatLon const &, ms::LatLon const &, NumMwmId) const override {
    UNREACHABLE();
  }

  RouteWeight CalcOffroadWeight(ms::LatLon const & from, ms::LatLon const & to, EdgeEstimator::Purpose) const override
  {
    return RouteWeight(ms::DistanceOnEarth(from, to));
  }

  double CalculateETA(Segment const &, Segment const &) override
  {
    UNREACHABLE();
  }

  double CalculateETAWithoutPenalty(Segment const &) override
  {
    UNREACHABLE();
  }

  IndexGraph & GetIndexGraph(NumMwmId) override
  {
    UNREACHABLE();
  }

  void GetTwinsInner(Segment const &, bool, std::vector<Segment> &) override {
    CHECK(false, ());
  }
};
}  // namespace routing
