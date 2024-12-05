#include "testing/testing.hpp"

#include "routing/position_accumulator.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "base/math.hpp"

namespace position_accumulator_tests
{
using namespace routing;

double constexpr kEps = 1e-10;
double constexpr kEpsMeters = 0.1;

UNIT_CLASS_TEST(PositionAccumulator, Smoke)
{
  PushNextPoint({0.0, 0.0});
  PushNextPoint({0.00005, 0.0});
  PushNextPoint({0.0001, 0.0});
  TEST_EQUAL(GetPointsForTesting().size(), 2, ());
  TEST(AlmostEqualAbs(GetDirection(), m2::PointD(0.0001, 0.0), kEps), (GetDirection()));
  TEST(AlmostEqualAbs(GetTrackLengthMForTesting(), 11.13, kEpsMeters),
       (GetTrackLengthMForTesting()));
}

UNIT_CLASS_TEST(PositionAccumulator, GoodSegment)
{
  double constexpr kStepShortButValid = 0.00001;

  // The distance from points with i = -15 to point with i = -6 is a little more then
  // |PositionAccumulator::kMinValidSegmentLengthM| meters. But the distance from
  // point with i = -4 to point with i = 0 is less then |PositionAccumulator::kMinValidSegmentLengthM|.
  // So the direction is calculated based on two points.
  for (signed i = -15; i <= 0; ++i)
    PushNextPoint({kStepShortButValid * i, 0.0});
  TEST_EQUAL(GetPointsForTesting().size(), 2, ());
  TEST(AlmostEqualAbs(GetDirection(), m2::PointD(9 * kStepShortButValid, 0.0), kEps),
       (GetDirection()));
  TEST(AlmostEqualAbs(GetTrackLengthMForTesting(), 10.02, kEpsMeters),
       (GetTrackLengthMForTesting()));

  double constexpr kStepGood = 0.0001;
  for (size_t i = 0; i < 10; ++i)
    PushNextPoint({kStepGood * i, 0.0});

  TEST_EQUAL(GetPointsForTesting().size(), 8, ());
  TEST(AlmostEqualAbs(GetDirection(), m2::PointD(0.0007, 0.0), kEps), (GetDirection()));
  TEST(AlmostEqualAbs(GetTrackLengthMForTesting(), 77.92, kEpsMeters),
       (GetTrackLengthMForTesting()));
}

// Test on removing too outdated elements from the position accumulator.
// Adding short but still valid segments.
UNIT_CLASS_TEST(PositionAccumulator, RemovingOutdated)
{
  double constexpr kStep = 0.00002;
  for (size_t i = 0; i < 100; ++i)
    PushNextPoint({kStep * i, 0.0});

  TEST_EQUAL(GetPointsForTesting().size(), 8, ());
  TEST(AlmostEqualAbs(GetDirection(), m2::PointD(0.0007, 0.0), kEps), (GetDirection()));
}

// Test on adding segments longer than |PositionAccumulator::kMaxValidSegmentLengthM|
// and shorter than |PositionAccumulator::kMinValidSegmentLengthM|.
UNIT_CLASS_TEST(PositionAccumulator, LongSegment)
{
  PushNextPoint({0.0, 0.0});
  PushNextPoint({0.001, 0.0}); // Longer than |MaxValidSegmentLengthM|.
  TEST_EQUAL(GetPointsForTesting().size(), 1, ());
  PushNextPoint({0.001001, 0.0}); // Shorter than |kMinValidSegmentLengthM|, so it's ignored.
  PushNextPoint({0.00102, 0.0});
  PushNextPoint({0.00104, 0.0});
  // The distance from point {0.001, 0.0} to this one is greater then
  // |PositionAccumulator::kMinValidSegmentLengthM|, so the point is added.
  PushNextPoint({0.0012, 0.0});
  TEST_EQUAL(GetPointsForTesting().size(), 2, ());
  TEST(AlmostEqualAbs(GetDirection(), m2::PointD(0.0002, 0.0), kEps), (GetDirection()));
  PushNextPoint({0.00201, 0.0}); // Longer than |PositionAccumulator::kMaxValidSegmentLengthM|.
  TEST_EQUAL(GetPointsForTesting().size(), 1, ());
  TEST(AlmostEqualAbs(GetDirection(), m2::PointD(0.0, 0.0), kEps), (GetDirection()));
}
}  // namespace position_accumulator_tests
