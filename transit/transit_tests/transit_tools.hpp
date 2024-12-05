#pragma once

#include "testing/testing.hpp"

#include "transit/experimental/transit_types_experimental.hpp"

#include "geometry/point2d.hpp"

#include <cstddef>
#include <tuple>
#include <vector>

namespace routing
{
namespace transit
{
template <typename Obj>
void TestForEquivalence(std::vector<Obj> const & actual, std::vector<Obj> const & expected)
{
  TEST_EQUAL(actual.size(), expected.size(), ());
  for (size_t i = 0; i < actual.size(); ++i)
    TEST(actual[i].IsEqualForTesting(expected[i]), (i, actual[i], expected[i]));
}
}  // namespace transit
}  // namespace routing

namespace transit
{
namespace experimental
{
double constexpr kPointsEqualEpsilon = 1e-5;

inline bool Equal(SingleMwmSegment const & s1, SingleMwmSegment const & s2)
{
  return std::make_tuple(s1.GetFeatureId(), s1.GetSegmentIdx(), s1.IsForward()) ==
         std::make_tuple(s2.GetFeatureId(), s2.GetSegmentIdx(), s2.IsForward());
}

inline bool Equal(Network const & n1, Network const & n2)
{
  return std::make_tuple(n1.GetId(), n1.GetTitle()) == std::make_tuple(n2.GetId(), n2.GetTitle());
}

inline bool Equal(Route const & r1, Route const & r2)
{
  return std::make_tuple(r1.GetId(), r1.GetNetworkId(), r1.GetType(), r1.GetTitle(),
                         r1.GetColor()) ==
         std::make_tuple(r2.GetId(), r2.GetNetworkId(), r2.GetType(), r2.GetTitle(), r2.GetColor());
}

inline bool Equal(Line const & l1, Line const & l2)
{
  return std::make_tuple(l1.GetId(), l1.GetRouteId(), l1.GetTitle(), l1.GetStopIds(),
                         l1.GetSchedule(), l1.GetShapeLink()) ==
         std::make_tuple(l2.GetId(), l2.GetRouteId(), l2.GetTitle(), l2.GetStopIds(),
                         l2.GetSchedule(), l2.GetShapeLink());
}

inline bool Equal(LineMetadata const & lm1, LineMetadata const & lm2)
{
  return std::make_tuple(lm1.GetId(), lm1.GetLineSegmentsOrder()) ==
         std::make_tuple(lm2.GetId(), lm2.GetLineSegmentsOrder());
}

inline bool Equal(Stop const & s1, Stop const & s2)
{
  return (std::make_tuple(s1.GetId(), s1.GetFeatureId(), s1.GetOsmId(), s1.GetTitle(),
                          s1.GetTimeTable(), s1.GetTransferIds(), s1.GetBestPedestrianSegments()) ==
          std::make_tuple(s2.GetId(), s2.GetFeatureId(), s2.GetOsmId(), s2.GetTitle(),
                          s2.GetTimeTable(), s2.GetTransferIds(),
                          s2.GetBestPedestrianSegments())) &&
         AlmostEqualAbs(s1.GetPoint(), s2.GetPoint(), kPointsEqualEpsilon);
}

inline bool Equal(Gate const & g1, Gate const & g2)
{
  return (std::make_tuple(g1.GetId(), g1.GetFeatureId(), g1.GetOsmId(), g1.IsEntrance(),
                          g1.IsExit(), g1.GetStopsWithWeight(), g1.GetBestPedestrianSegments()) ==
          std::make_tuple(g2.GetId(), g2.GetFeatureId(), g2.GetOsmId(), g2.IsEntrance(),
                          g2.IsExit(), g2.GetStopsWithWeight(), g2.GetBestPedestrianSegments())) &&
         AlmostEqualAbs(g1.GetPoint(), g2.GetPoint(), kPointsEqualEpsilon);
}

inline bool Equal(Edge const & e1, Edge const & e2)
{
  return (std::make_tuple(e1.GetStop1Id(), e1.GetStop2Id(), e1.GetLineId(), e1.GetWeight(),
                          e1.IsTransfer(), e1.GetShapeLink()) ==
          std::make_tuple(e2.GetStop1Id(), e2.GetStop2Id(), e2.GetLineId(), e2.GetWeight(),
                          e2.IsTransfer(), e2.GetShapeLink()));
}

inline bool Equal(Transfer const & t1, Transfer const & t2)
{
  return (std::make_tuple(t1.GetId(), t1.GetStopIds()) ==
          std::make_tuple(t2.GetId(), t2.GetStopIds())) &&
         AlmostEqualAbs(t1.GetPoint(), t2.GetPoint(), kPointsEqualEpsilon);
}

inline bool Equal(Shape const & s1, Shape const & s2)
{
  return std::make_tuple(s1.GetId(), s1.GetPolyline()) ==
         std::make_tuple(s2.GetId(), s2.GetPolyline());
}

template <class Item>
void TestEqual(std::vector<Item> const & actual, std::vector<Item> const & expected)
{
  TEST_EQUAL(actual.size(), expected.size(), ());
  for (size_t i = 0; i < actual.size(); ++i)
    TEST(Equal(actual[i], expected[i]), (i, actual[i], expected[i]));
}
}  // namespace experimental
}  // namespace transit
