#pragma once

#include "routing/cross_mwm_ids.hpp"
#include "routing/segment.hpp"

#include "routing/base/small_list.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/buffer_vector.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace routing
{
namespace connector
{
double constexpr kNoRoute = 0.0;

using Weight = uint32_t;

enum class WeightsLoadState
{
  Unknown,
  NotExists,
  ReadyToLoad,
  Loaded
};

std::string DebugPrint(WeightsLoadState state);
}  // namespace connector

/// @param CrossMwmId Encoded OSM feature (way) ID that should be equal and unique in all MWMs.
template <typename CrossMwmId>
class CrossMwmConnector final
{
public:
  /// Used in generator or tests. Should initialize with some valid mwm id here
  /// not to conflict with @see JointSegment::IsFake().
  CrossMwmConnector() : m_mwmId(kGeneratorMwmId) {}

  /// Used in client router.
  CrossMwmConnector(NumMwmId mwmId, uint32_t featureNumerationOffset)
    : m_mwmId(mwmId), m_featureNumerationOffset(featureNumerationOffset)
  {
  }

  /// Builder component, which makes inner containers optimization after adding transitions.
  class Builder
  {
    CrossMwmConnector & m_c;

  public:
    Builder(CrossMwmConnector & c, size_t count) : m_c(c)
    {
      m_c.m_transitions.reserve(count);
    }
    ~Builder()
    {
      // Sort by FeatureID to make lower_bound queries.
      std::sort(m_c.m_transitions.begin(), m_c.m_transitions.end(), LessKT());
    }

    void AddTransition(CrossMwmId const & crossMwmId, uint32_t featureId, uint32_t segmentIdx,
                       bool oneWay, bool forwardIsEnter)
    {
      featureId += m_c.m_featureNumerationOffset;

      Transition transition(m_c.m_entersCount, m_c.m_exitsCount, crossMwmId, oneWay, forwardIsEnter);

      if (forwardIsEnter)
        ++m_c.m_entersCount;
      else
        ++m_c.m_exitsCount;

      if (!oneWay)
      {
        if (forwardIsEnter)
          ++m_c.m_exitsCount;
        else
          ++m_c.m_entersCount;
      }

      m_c.m_transitions.emplace_back(Key(featureId, segmentIdx), transition);
      m_c.m_crossMwmIdToFeatureId.emplace(crossMwmId, featureId);
    }
  };

  template <class FnT> void ForEachTransitSegmentId(uint32_t featureId, FnT && fn) const
  {
    auto it = std::lower_bound(m_transitions.begin(), m_transitions.end(), Key{featureId, 0}, LessKT());
    while (it != m_transitions.end() && it->first.m_featureId == featureId)
    {
      if (fn(it->first.m_segmentIdx))
        break;
      ++it;
    }
  }

  bool IsTransition(Segment const & segment, bool isOutgoing) const
  {
    Key const key(segment.GetFeatureId(), segment.GetSegmentIdx());
    auto const it = std::lower_bound(m_transitions.begin(), m_transitions.end(), key, LessKT());
    if (it == m_transitions.end() || !(it->first == key))
      return false;

    auto const & transition = it->second;
    if (transition.m_oneWay && !segment.IsForward())
      return false;

    // Note. If |isOutgoing| == true |segment| should be an exit transition segment
    // (|isEnter| == false) to be a transition segment.
    // Otherwise |segment| should be an enter transition segment (|isEnter| == true)
    // to be a transition segment. If not, |segment| is not a transition segment.
    // Please see documentation on CrossMwmGraph::IsTransition() method for details.
    bool const isEnter = (segment.IsForward() == transition.m_forwardIsEnter);
    return isEnter != isOutgoing;
  }

  CrossMwmId const & GetCrossMwmId(Segment const & segment) const
  {
    return GetTransition(segment).m_crossMwmId;
  }

  /// @return {} if there is no transition for such cross mwm id.
  std::optional<Segment> GetTransition(CrossMwmId const & crossMwmId, uint32_t segmentIdx, bool isEnter) const
  {
    auto const fIt = m_crossMwmIdToFeatureId.find(crossMwmId);
    if (fIt == m_crossMwmIdToFeatureId.cend())
      return {};

    uint32_t const featureId = fIt->second;

    Transition const * transition = GetTransition(featureId, segmentIdx);
    if (transition == nullptr)
    {
      /// @todo By VNG: Workaround until cross-mwm transitions generator investigation.
      /// https://github.com/organicmaps/organicmaps/issues/1736
      /// Actually, the fix is valid, because transition features can have segment = 1 when leaving MWM
      /// and segment = 2 when entering MWM due to *not precise* packed MWM borders.
      if (isEnter)
        transition = GetTransition(featureId, ++segmentIdx);
      else if (segmentIdx > 0)
        transition = GetTransition(featureId, --segmentIdx);

      if (transition == nullptr)
        return {};
    }

    ASSERT_EQUAL(transition->m_crossMwmId, crossMwmId, ("fId:", featureId, ", segId:", segmentIdx));
    bool const isForward = transition->m_forwardIsEnter == isEnter;
    if (transition->m_oneWay && !isForward)
      return {};

    return Segment(m_mwmId, featureId, segmentIdx, isForward);
  }

  using EdgeListT = SmallList<SegmentEdge>;

  template <class FnT> void ForEachEnter(FnT && fn) const
  {
    for (auto const & [key, transit] : m_transitions)
    {
      if (transit.m_forwardIsEnter)
        fn(transit.m_enterIdx, Segment(m_mwmId, key.m_featureId, key.m_segmentIdx, true));

      if (!transit.m_oneWay && !transit.m_forwardIsEnter)
        fn(transit.m_enterIdx, Segment(m_mwmId, key.m_featureId, key.m_segmentIdx, false));
    }
  }

  template <class FnT> void ForEachExit(FnT && fn) const
  {
    for (auto const & [key, transit] : m_transitions)
    {
      if (!transit.m_forwardIsEnter)
        fn(transit.m_exitIdx, Segment(m_mwmId, key.m_featureId, key.m_segmentIdx, true));

      if (!transit.m_oneWay && transit.m_forwardIsEnter)
        fn(transit.m_exitIdx, Segment(m_mwmId, key.m_featureId, key.m_segmentIdx, false));
    }
  }

  void GetOutgoingEdgeList(Segment const & segment, EdgeListT & edges) const
  {
    auto const enterIdx = GetTransition(segment).m_enterIdx;
    ForEachExit([enterIdx, this, &edges](uint32_t exitIdx, Segment const & s)
    {
      AddEdge(s, enterIdx, exitIdx, edges);
    });
  }

  void GetIngoingEdgeList(Segment const & segment, EdgeListT & edges) const
  {
    auto const exitIdx = GetTransition(segment).m_exitIdx;
    ForEachEnter([exitIdx, this, &edges](uint32_t enterIdx, Segment const & s)
    {
      AddEdge(s, enterIdx, exitIdx, edges);
    });
  }

  uint32_t GetNumEnters() const { return m_entersCount; }
  uint32_t GetNumExits() const { return m_exitsCount; }

  bool HasWeights() const { return !m_weights.empty(); }
  bool IsEmpty() const { return m_entersCount == 0 && m_exitsCount == 0; }

  bool WeightsWereLoaded() const
  {
    switch (m_weightsLoadState)
    {
    case connector::WeightsLoadState::Unknown:
    case connector::WeightsLoadState::ReadyToLoad: return false;
    case connector::WeightsLoadState::NotExists:
    case connector::WeightsLoadState::Loaded: return true;
    }
    UNREACHABLE();
  }

  template <typename CalcWeight>
  void FillWeights(CalcWeight && calcWeight)
  {
    CHECK_EQUAL(m_weightsLoadState, connector::WeightsLoadState::Unknown, ());
    CHECK(m_weights.empty(), ());

    m_weights.resize(m_entersCount * m_exitsCount);
    ForEachEnter([&](uint32_t enterIdx, Segment const & enter)
    {
      ForEachExit([&](uint32_t exitIdx, Segment const & exit)
      {
        auto const weight = calcWeight(enter, exit);
        // Edges weights should be >= astar heuristic, so use std::ceil.
        m_weights[GetWeightIndex(enterIdx, exitIdx)] = static_cast<connector::Weight>(std::ceil(weight));
      });
    });
  }

private:
  struct Key
  {
    Key(uint32_t featureId, uint32_t segmentIdx) : m_featureId(featureId), m_segmentIdx(segmentIdx)
    {
    }

    bool operator==(Key const & key) const
    {
      return (m_featureId == key.m_featureId && m_segmentIdx == key.m_segmentIdx);
    }

    bool operator<(Key const & key) const
    {
      if (m_featureId == key.m_featureId)
        return m_segmentIdx < key.m_segmentIdx;
      return m_featureId < key.m_featureId;
    }

    uint32_t m_featureId = 0;
    uint32_t m_segmentIdx = 0;
  };

  struct Transition
  {
    Transition(uint32_t enterIdx, uint32_t exitIdx, CrossMwmId crossMwmId, bool oneWay, bool forwardIsEnter)
      : m_enterIdx(enterIdx)
      , m_exitIdx(exitIdx)
      , m_crossMwmId(crossMwmId)
      , m_oneWay(oneWay)
      , m_forwardIsEnter(forwardIsEnter)
    {
    }

    uint32_t m_enterIdx;
    uint32_t m_exitIdx;
    CrossMwmId m_crossMwmId;

    // false - Transition represents both forward and backward segments with same featureId, segmentIdx.
    bool m_oneWay : 1;
    // true - forward segment is enter to mwm, enter means: m_backPoint is outside mwm borders, m_frontPoint is inside.
    bool m_forwardIsEnter : 1;
  };

  friend class CrossMwmConnectorSerializer;

  void AddEdge(Segment const & segment, uint32_t enterIdx, uint32_t exitIdx, EdgeListT & edges) const
  {
    auto const weight = GetWeight(enterIdx, exitIdx);
    if (weight != connector::kNoRoute)
      edges.emplace_back(segment, RouteWeight::FromCrossMwmWeight(weight));
  }

  Transition const * GetTransition(uint32_t featureId, uint32_t segmentIdx) const
  {
    Key key(featureId, segmentIdx);
    auto const it = std::lower_bound(m_transitions.begin(), m_transitions.end(), key, LessKT());
    if (it == m_transitions.end() || !(it->first == key))
      return nullptr;
    return &(it->second);
  }

  Transition const & GetTransition(Segment const & segment) const
  {
    Transition const * tr = GetTransition(segment.GetFeatureId(), segment.GetSegmentIdx());
    CHECK(tr, (segment));
    return *tr;
  }

  size_t GetWeightIndex(size_t enterIdx, size_t exitIdx) const
  {
    ASSERT_LESS(enterIdx, m_entersCount, ());
    ASSERT_LESS(exitIdx, m_exitsCount, ());

    size_t const i = enterIdx * m_exitsCount + exitIdx;
    ASSERT_LESS(i, m_weights.size(), ());
    return i;
  }

  connector::Weight GetWeight(size_t enterIdx, size_t exitIdx) const
  {
    return m_weights[GetWeightIndex(enterIdx, exitIdx)];
  }

  NumMwmId const m_mwmId;
  uint32_t m_entersCount = 0;
  uint32_t m_exitsCount = 0;

  using KeyTransitionT = std::pair<Key, Transition>;
  struct LessKT
  {
    bool operator()(KeyTransitionT const & l, KeyTransitionT const & r) const
    {
      return l.first < r.first;
    }
    bool operator()(KeyTransitionT const & l, Key const & r) const
    {
      return l.first < r;
    }
    bool operator()(Key const & l, KeyTransitionT const & r) const
    {
      return l < r.first;
    }
  };
  std::vector<KeyTransitionT> m_transitions;

  std::unordered_map<CrossMwmId, uint32_t, connector::HashKey> m_crossMwmIdToFeatureId;

  connector::WeightsLoadState m_weightsLoadState = connector::WeightsLoadState::Unknown;
  // For some connectors we may need to shift features with some offset.
  // For example for versions and transit section compatibility we number transit features
  // starting from 0 in mwm and shift them with |m_featureNumerationOffset| in runtime.
  uint32_t const m_featureNumerationOffset = 0;
  uint64_t m_weightsOffset = 0;
  connector::Weight m_granularity = 0;

  // Weight is the time required for the route to pass edge, measured in seconds rounded upwards.
  /// @todo Store some fast! succinct vector instead of raw matrix m_entersCount * m_exitsCount.
  std::vector<connector::Weight> m_weights;
};
}  // namespace routing
