#pragma once

#include "routing/base/small_list.hpp"

#include "indexer/feature_data.hpp"

#include "geometry/point_with_altitude.hpp"
#include "geometry/rect2d.hpp"

#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace routing
{
using IsGoodFeatureFn = std::function<bool(FeatureID const &)>;

/// The Edge class represents an edge description on a road network graph
class Edge
{
  enum class Type
  {
    Real,               // An edge that corresponds to some real segment.
    FakeWithRealPart,   // A fake edge that is a part of some real segment.
    FakeWithoutRealPart  // A fake edge that is not part of any real segment.
  };

public:
  Edge() = default;

  using JunctionPointT = geometry::PointWithAltitude;

  static Edge MakeReal(FeatureID featureId, bool forward, uint32_t segId,
                       JunctionPointT const & startJunction,
                       JunctionPointT const & endJunction);
  static Edge MakeFakeWithRealPart(FeatureID featureId, uint32_t fakeSegmentId,
                                   bool forward, uint32_t segId,
                                   JunctionPointT const & startJunction,
                                   JunctionPointT const & endJunction);
  static Edge MakeFake(JunctionPointT const & startJunction,
                       JunctionPointT const & endJunction);
  static Edge MakeFake(JunctionPointT const & startJunction,
                       JunctionPointT const & endJunction, Edge const & prototype);

  inline FeatureID const & GetFeatureId() const { return m_featureId; }
  inline bool IsForward() const { return m_forward; }
  inline uint32_t GetSegId() const { return m_segId; }
  inline uint32_t GetFakeSegmentId() const { return m_fakeSegmentId; }

  inline JunctionPointT const & GetStartJunction() const { return m_startJunction; }
  inline JunctionPointT const & GetEndJunction() const { return m_endJunction; }

  inline m2::PointD const & GetStartPoint() const { return m_startJunction.GetPoint(); }
  inline m2::PointD const & GetEndPoint() const { return m_endJunction.GetPoint(); }

  inline bool IsFake() const { return  m_type != Type::Real; }
  inline bool HasRealPart() const { return m_type != Type::FakeWithoutRealPart; }

  inline m2::PointD GetDirection() const
  {
    return GetEndJunction().GetPoint() - GetStartJunction().GetPoint();
  }

  Type GetType() const { return m_type; }

  Edge GetReverseEdge() const;

  bool SameRoadSegmentAndDirection(Edge const & r) const;

  bool operator==(Edge const & r) const;
  bool operator!=(Edge const & r) const { return !(*this == r); }
  bool operator<(Edge const & r) const;

  friend std::string DebugPrint(Edge const & r);
  std::string PrintLatLon() const;

private:
  Edge(Type type, FeatureID featureId, uint32_t fakeSegmentId, bool forward, uint32_t segId,
       JunctionPointT const & startJunction, JunctionPointT const & endJunction);

  Type m_type = Type::FakeWithoutRealPart;

  // Feature for which edge is defined.
  FeatureID m_featureId;

  // Is the feature along the road.
  bool m_forward = true;

  // Ordinal number of the segment on the road.
  uint32_t m_segId = 0;

  // Start point of the segment on the road.
  JunctionPointT m_startJunction;

  // End point of the segment on the road.
  JunctionPointT m_endJunction;

  // Note. If |m_forward| == true index of |m_startJunction| point at the feature |m_featureId|
  // is less than index |m_endJunction|.
  // If |m_forward| == false index of |m_startJunction| point at the feature |m_featureId|
  // is more than index |m_endJunction|.

  static auto constexpr kInvalidFakeSegmentId = std::numeric_limits<uint32_t>::max();
  // In case of |m_type| == Type::FakeWithRealPart, save it's segmentId from IndexGraphStarter.
  uint32_t m_fakeSegmentId;
};

class RoadGraphBase
{
public:
  using JunctionPointT = Edge::JunctionPointT;

  /// Small buffered vector to store ingoing/outgoing node's edges.
  using EdgeListT = SmallList<Edge>;
  /// Big container to store full path edges.
  using EdgeVector = std::vector<Edge>;

  /// Finds all nearest outgoing edges, that route to the junction.
  virtual void GetOutgoingEdges(JunctionPointT const & junction,
                                EdgeListT & edges) const = 0;

  /// Finds all nearest ingoing edges, that route to the junction.
  virtual void GetIngoingEdges(JunctionPointT const & junction,
                               EdgeListT & edges) const = 0;

  /// @return Types for the specified edge
  virtual void GetEdgeTypes(Edge const & edge, feature::TypesHolder & types) const = 0;

  /// @return Types for specified junction
  virtual void GetJunctionTypes(JunctionPointT const & junction,
                                feature::TypesHolder & types) const = 0;

  virtual void GetRouteEdges(EdgeVector & routeEdges) const;

protected:
  virtual ~RoadGraphBase() = default;
};

class IRoadGraph : public RoadGraphBase
{
public:
  using Vertex = JunctionPointT;
  using Edge = routing::Edge;
  using Weight = double;
  using PointWithAltitudeVec = buffer_vector<JunctionPointT, 32>;

  enum class Mode
  {
    ObeyOnewayTag,
    IgnoreOnewayTag,
  };

  /// This struct contains the part of a feature's metadata that is
  /// relevant for routing.
  struct RoadInfo
  {
    RoadInfo();
    RoadInfo(RoadInfo && ri);
    RoadInfo(bool bidirectional, double speedKMPH,
             std::initializer_list<JunctionPointT> const & points);
    RoadInfo(RoadInfo const &) = default;
    RoadInfo & operator=(RoadInfo const &) = default;

    PointWithAltitudeVec m_junctions;
    double m_speedKMPH;
    bool m_bidirectional;
  };

  struct FullRoadInfo
  {
    FullRoadInfo(FeatureID const & featureId, RoadInfo const & roadInfo)
      : m_featureId(featureId)
      , m_roadInfo(roadInfo)
    {
    }

    FeatureID m_featureId;
    RoadInfo m_roadInfo;
  };

  /// This class is responsible for loading edges in a cross.
  class ICrossEdgesLoader
  {
  public:
    ICrossEdgesLoader(JunctionPointT const & cross, IRoadGraph::Mode mode,
                      EdgeListT & edges)
      : m_cross(cross), m_mode(mode), m_edges(edges)
    {
    }

    virtual ~ICrossEdgesLoader() = default;

    void operator()(FeatureID const & featureId, PointWithAltitudeVec const & junctions,
                    bool bidirectional)
    {
      LoadEdges(featureId, junctions, bidirectional);
    }

  private:
    virtual void LoadEdges(FeatureID const & featureId, PointWithAltitudeVec const & junctions,
                           bool bidirectional) = 0;

  protected:
    template <typename TFn>
    void ForEachEdge(PointWithAltitudeVec const & junctions, TFn && fn)
    {
      for (size_t i = 0; i < junctions.size(); ++i)
      {
        if (!AlmostEqualAbs(m_cross.GetPoint(), junctions[i].GetPoint(), kMwmPointAccuracy))
          continue;

        if (i + 1 < junctions.size())
        {
          // Head of the edge.
          // m_cross
          //     o------------>o
          fn(i, junctions[i + 1], true /* forward */);
        }
        if (i > 0)
        {
          // Tail of the edge.
          //                m_cross
          //     o------------>o
          fn(i - 1, junctions[i - 1], false /* backward */);
        }
      }
    }

    JunctionPointT const m_cross;
    IRoadGraph::Mode const m_mode;
    EdgeListT & m_edges;
  };

  class CrossOutgoingLoader : public ICrossEdgesLoader
  {
  public:
    CrossOutgoingLoader(JunctionPointT const & cross, IRoadGraph::Mode mode,
                        EdgeListT & edges)
      : ICrossEdgesLoader(cross, mode, edges)
    {
    }

  private:
    // ICrossEdgesLoader overrides:
    void LoadEdges(FeatureID const & featureId, PointWithAltitudeVec const & junctions,
                   bool bidirectional) override;
  };

  class CrossIngoingLoader : public ICrossEdgesLoader
  {
  public:
    CrossIngoingLoader(JunctionPointT const & cross, IRoadGraph::Mode mode,
                       EdgeListT & edges)
      : ICrossEdgesLoader(cross, mode, edges)
    {
    }

  private:
    // ICrossEdgesLoader overrides:
    void LoadEdges(FeatureID const & featureId, PointWithAltitudeVec const & junctions,
                   bool bidirectional) override;
  };

  void GetOutgoingEdges(JunctionPointT const & junction,
                        EdgeListT & edges) const override;

  void GetIngoingEdges(JunctionPointT const & junction,
                       EdgeListT & edges) const override;

  /// Removes all fake turns and vertices from the graph.
  void ResetFakes();

  /// Adds fake edges from fake position rp to real vicinity
  /// positions.
  void AddFakeEdges(JunctionPointT const & junction,
                    std::vector<std::pair<Edge, JunctionPointT>> const & vicinities);
  void AddOutgoingFakeEdge(Edge const & e);
  void AddIngoingFakeEdge(Edge const & e);

  /// Calls edgesLoader on each feature which is close to cross.
  virtual void ForEachFeatureClosestToCross(m2::PointD const & cross,
                                            ICrossEdgesLoader & edgesLoader) const = 0;

  /// Finds the closest edges to the center of |rect|.
  /// @return Array of pairs of Edge and projection point on the Edge. If there is no the closest edges
  /// then returns empty array.
  using EdgeProjectionT = std::pair<Edge, JunctionPointT>;
  virtual void FindClosestEdges(m2::RectD const & /*rect*/, uint32_t /*count*/,
                                std::vector<EdgeProjectionT> & /*vicinities*/) const {};

  /// \returns Vector of pairs FeatureID and corresponding RoadInfo for road features
  /// lying in |rect|.
  /// \note |RoadInfo::m_speedKMPH| is set to |kInvalidSpeedKMPH|.
  virtual std::vector<FullRoadInfo> FindRoads(
      m2::RectD const & /*rect*/, IsGoodFeatureFn const & /*isGoodFeature*/) const { return {}; }

  /// @return Types for the specified feature
  virtual void GetFeatureTypes(FeatureID const & featureId, feature::TypesHolder & types) const = 0;

  /// @return Types for the specified edge
  virtual void GetEdgeTypes(Edge const & edge, feature::TypesHolder & types) const override;

  virtual IRoadGraph::Mode GetMode() const = 0;

  /// Clear all temporary buffers.
  virtual void ClearState() {}

  /// \brief Finds all outgoing regular (non-fake) edges for junction.
  void GetRegularOutgoingEdges(JunctionPointT const & junction,
                               EdgeListT & edges) const;
  /// \brief Finds all ingoing regular (non-fake) edges for junction.
  void GetRegularIngoingEdges(JunctionPointT const & junction,
                              EdgeListT & edges) const;
  /// \brief Finds all outgoing fake edges for junction.
  void GetFakeOutgoingEdges(JunctionPointT const & junction, EdgeListT & edges) const;
  /// \brief Finds all ingoing fake edges for junction.
  void GetFakeIngoingEdges(JunctionPointT const & junction, EdgeListT & edges) const;

private:
  using EdgeCacheT = std::map<JunctionPointT, EdgeListT>;
  void AddEdge(JunctionPointT const & j, Edge const & e, EdgeCacheT & edges);

  template <typename Fn>
  void ForEachFakeEdge(Fn && fn)
  {
    for (auto const & m : m_fakeIngoingEdges)
    {
      for (auto const & e : m.second)
        fn(e);
    }

    for (auto const & m : m_fakeOutgoingEdges)
    {
      for (auto const & e : m.second)
        fn(e);
    }
  }

  /// \note |m_fakeIngoingEdges| and |m_fakeOutgoingEdges| map junctions to sorted vectors.
  /// Items to these maps should be inserted with AddEdge() method only.
  EdgeCacheT m_fakeIngoingEdges;
  EdgeCacheT m_fakeOutgoingEdges;
};

std::string DebugPrint(IRoadGraph::Mode mode);

IRoadGraph::RoadInfo MakeRoadInfoForTesting(bool bidirectional, double speedKMPH,
                                            std::initializer_list<m2::PointD> const & points);

template <class PointT>
void JunctionsToPoints(std::vector<PointT> const & junctions, std::vector<m2::PointD> & points)
{
  points.resize(junctions.size());
  for (size_t i = 0; i < junctions.size(); ++i)
    points[i] = junctions[i].GetPoint();
}

template <class PointT>
void JunctionsToAltitudes(std::vector<PointT> const & junctions, geometry::Altitudes & altitudes)
{
  altitudes.resize(junctions.size());
  for (size_t i = 0; i < junctions.size(); ++i)
    altitudes[i] = junctions[i].GetAltitude();
}
}  // namespace routing
