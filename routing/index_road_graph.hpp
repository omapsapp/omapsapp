#pragma once

#include "routing/road_graph.hpp"
#include "routing/segment.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "geometry/point_with_altitude.hpp"

#include <map>
#include <memory>
#include <vector>

namespace routing
{
class MwmDataSource;
class IndexGraphStarter;

class IndexRoadGraph : public RoadGraphBase
{
public:
  IndexRoadGraph(IndexGraphStarter & starter, std::vector<Segment> const & segments,
                 std::vector<geometry::PointWithAltitude> const & junctions,
                 MwmDataSource & dataSource);

  // IRoadGraphBase overrides:
  virtual void GetOutgoingEdges(geometry::PointWithAltitude const & junction,
                                EdgeListT & edges) const override;
  virtual void GetIngoingEdges(geometry::PointWithAltitude const & junction,
                               EdgeListT & edges) const override;
  virtual double GetMaxSpeedKMpH() const override;
  virtual void GetEdgeTypes(Edge const & edge, feature::TypesHolder & types) const override;
  virtual void GetJunctionTypes(geometry::PointWithAltitude const & junction,
                                feature::TypesHolder & types) const override;
  virtual void GetRouteEdges(EdgeVector & edges) const override;
  virtual void GetRouteSegments(std::vector<Segment> & segments) const override;

private:
  void GetEdges(geometry::PointWithAltitude const & junction, bool isOutgoing, EdgeListT & edges) const;

  using SegmentListT = SmallList<Segment>;
  SegmentListT const & GetSegments(geometry::PointWithAltitude const & junction, bool isOutgoing) const;

  MwmDataSource & m_dataSource;
  IndexGraphStarter & m_starter;
  std::vector<Segment> m_segments;
  std::map<geometry::PointWithAltitude, SegmentListT> m_beginToSegment;
  std::map<geometry::PointWithAltitude, SegmentListT> m_endToSegment;
};
}  // namespace routing
