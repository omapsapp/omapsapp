#include "testing/testing.hpp"

#include "transit/world_feed/color_picker.hpp"
#include "transit/world_feed/date_time_helpers.hpp"
#include "transit/world_feed/feed_helpers.hpp"
#include "transit/world_feed/world_feed.hpp"

#include "platform/platform.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "3party/just_gtfs/just_gtfs.h"
#include "3party/opening_hours/opening_hours.hpp"

namespace world_feed_tests
{
using namespace transit;

std::vector<gtfs::CalendarAvailability> GetCalendarAvailability(std::vector<size_t> const & data)
{
  CHECK_EQUAL(data.size(), 7, ());
  std::vector<gtfs::CalendarAvailability> res;

  for (auto val : data)
  {
    if (val == 0)
      res.push_back(gtfs::CalendarAvailability::NotAvailable);
    else
      res.push_back(gtfs::CalendarAvailability::Available);
  }

  return res;
}

gtfs::StopTimes GetFakeStopTimes(std::vector<std::string> const & transitIds)
{
  auto ids = transitIds;
  std::sort(ids.begin(), ids.end());
  gtfs::StopTimes res;
  for (size_t i = 0; i < ids.size(); ++i)
  {
    gtfs::StopTime st;
    st.trip_id = ids[i];
    st.stop_sequence = i;
    res.emplace_back(st);
  }
  return res;
}

void TestInterval(WeekdaysInterval const & interval, size_t start, size_t end,
                  osmoh::RuleSequence::Modifier status)
{
  TEST_EQUAL(interval.m_start, start, ());
  TEST_EQUAL(interval.m_end, end, ());
  TEST_EQUAL(interval.m_status, status, ());
}

void TestExceptionIntervals(gtfs::CalendarDates const & dates, size_t intervalsCount,
                            std::string const & resOpeningHoursStr)
{
  osmoh::TRuleSequences rules;
  GetServiceDaysExceptionsOsmoh(dates, rules);
  // TEST_EQUAL(rules.size(), intervalsCount, ());
  auto const openingHours = ToString(osmoh::OpeningHours(rules));
  TEST_EQUAL(openingHours, resOpeningHoursStr, ());
}

void TestStopsRange(IdList const & stopsOnLine, IdSet const & stopsInRegion, size_t firstIdxPlan,
                    size_t lastIdxPlan)
{
  auto const & [firstIdxFact, lastIdxFact] = GetStopsRange(stopsOnLine, stopsInRegion);
  TEST_EQUAL(firstIdxFact, firstIdxPlan, ());
  TEST_EQUAL(lastIdxFact, lastIdxPlan, ());
}

UNIT_TEST(Transit_GTFS_OpenCloseInterval1)
{
  auto const & intervals = GetOpenCloseIntervals(GetCalendarAvailability({1, 1, 1, 1, 1, 0, 0}));
  TEST_EQUAL(intervals.size(), 2, ());

  TestInterval(intervals[0], 0, 4, osmoh::RuleSequence::Modifier::DefaultOpen);
  TestInterval(intervals[1], 5, 6, osmoh::RuleSequence::Modifier::Closed);
}

UNIT_TEST(Transit_GTFS_OpenCloseInterval2)
{
  auto const & intervals = GetOpenCloseIntervals(GetCalendarAvailability({0, 0, 0, 0, 0, 1, 0}));
  TEST_EQUAL(intervals.size(), 3, ());

  TestInterval(intervals[0], 0, 4, osmoh::RuleSequence::Modifier::Closed);
  TestInterval(intervals[1], 5, 5, osmoh::RuleSequence::Modifier::DefaultOpen);
  TestInterval(intervals[2], 6, 6, osmoh::RuleSequence::Modifier::Closed);
}

UNIT_TEST(Transit_GTFS_OpenCloseInterval3)
{
  auto const & intervals = GetOpenCloseIntervals(GetCalendarAvailability({0, 0, 0, 0, 0, 0, 1}));
  TEST_EQUAL(intervals.size(), 2, ());

  TestInterval(intervals[0], 0, 5, osmoh::RuleSequence::Modifier::Closed);
  TestInterval(intervals[1], 6, 6, osmoh::RuleSequence::Modifier::DefaultOpen);
}

UNIT_TEST(Transit_GTFS_GetTimeOsmoh)
{
  size_t const hours = 21;
  size_t const minutes = 5;
  size_t const seconds = 30;
  gtfs::Time const timeGtfs(hours, minutes, seconds);

  auto const timeOsmoh = GetTimeOsmoh(timeGtfs);
  TEST_EQUAL(timeOsmoh.GetMinutesCount(), minutes, ());
  TEST_EQUAL(timeOsmoh.GetHoursCount(), hours, ());
}

UNIT_TEST(Transit_GTFS_ServiceDaysExceptions1)
{
  gtfs::CalendarDates const exceptionDays{
      {"serviceId1", gtfs::Date(2015, 01, 30), gtfs::CalendarDateException::Removed},
      {"serviceId1", gtfs::Date(2015, 01, 31), gtfs::CalendarDateException::Removed},
      {"serviceId1", gtfs::Date(2015, 02, 01), gtfs::CalendarDateException::Removed},
      {"serviceId1", gtfs::Date(2015, 04, 03), gtfs::CalendarDateException::Added}};
  TestExceptionIntervals(
      exceptionDays, 2 /* intervalsCount */,
      "2015 Apr 03-2015 Apr 03; 2015 Jan 30-2015 Feb 01 closed" /* resOpeningHoursStr */);
}

UNIT_TEST(Transit_GTFS_ServiceDaysExceptions2)
{
  gtfs::CalendarDates const exceptionDays{
      {"serviceId2", gtfs::Date(1999, 11, 14), gtfs::CalendarDateException::Removed}};
  TestExceptionIntervals(exceptionDays, 1 /* intervalsCount */,
                         "1999 Nov 14-1999 Nov 14 closed" /* resOpeningHoursStr */);
}

UNIT_TEST(Transit_GTFS_ServiceDaysExceptions3)
{
  gtfs::CalendarDates const exceptionDays{
      {"serviceId2", gtfs::Date(2005, 8, 01), gtfs::CalendarDateException::Added},
      {"serviceId2", gtfs::Date(2005, 8, 12), gtfs::CalendarDateException::Added},
      {"serviceId2", gtfs::Date(2005, 10, 11), gtfs::CalendarDateException::Removed},
      {"serviceId2", gtfs::Date(2005, 10, 12), gtfs::CalendarDateException::Removed},
      {"serviceId2", gtfs::Date(2005, 10, 13), gtfs::CalendarDateException::Added},
      {"serviceId2", gtfs::Date(1999, 10, 14), gtfs::CalendarDateException::Removed}};
  TestExceptionIntervals(
      exceptionDays, 2 /* intervalsCount */,
      "2005 Aug 01-2005 Aug 01, 2005 Aug 12-2005 Aug 12, 2005 Oct 13-2005 Oct 13; 2005 Oct 11-2005 "
      "Oct 12, 1999 Oct 14-1999 Oct 14 closed" /* resOpeningHoursStr */);
}

UNIT_TEST(Transit_GTFS_FindStopTimesByTransitId)
{
  auto const allStopTimes = GetFakeStopTimes({"4", "5", "6", "2", "10", "2", "2", "6"});
  auto const stopTimes1 = GetStopTimesForTrip(allStopTimes, "2");
  TEST_EQUAL(stopTimes1.size(), 3, ());

  auto const stopTimes10 = GetStopTimesForTrip(allStopTimes, "10");
  TEST_EQUAL(stopTimes10.size(), 1, ());

  auto const stopTimes6 = GetStopTimesForTrip(allStopTimes, "6");
  TEST_EQUAL(stopTimes6.size(), 2, ());

  auto const stopTimesNonExistent1 = GetStopTimesForTrip(allStopTimes, "11");
  TEST(stopTimesNonExistent1.empty(), ());

  auto const stopTimesNonExistent2 = GetStopTimesForTrip(allStopTimes, "1");
  TEST(stopTimesNonExistent1.empty(), ());
}

UNIT_TEST(Transit_GTFS_FindStopTimesByTransitId2)
{
  auto const allStopTimes = GetFakeStopTimes({"28", "28", "28", "28"});
  auto const stopTimes = GetStopTimesForTrip(allStopTimes, "28");
  TEST_EQUAL(stopTimes.size(), 4, ());

  auto const stopTimesNonExistent = GetStopTimesForTrip(allStopTimes, "3");
  TEST(stopTimesNonExistent.empty(), ());
}

// Stops are marked as *, points on polyline as +. Points have indexes, stops have letters.
//
//    *A
//
//  +----+---------------+----------------------+
//  0    1               2                      3
//
//                                          *B  *C
//
UNIT_TEST(Transit_GTFS_ProjectStopToLine_Simple)
{
  // index, was inserted
  using ResT = std::pair<size_t, bool>;

  double const y = 0.0002;
  std::vector<m2::PointD> shape{{0.001, y}, {0.0015, y}, {0.004, y}, {0.005, y}};

  m2::PointD const point_A{0.0012, 0.0003};
  m2::PointD const point_B{0.00499, 0.0001};
  m2::PointD const point_C{0.005, 0.0001};

  // Test that point_A is projected between two existing polyline points and the new point is
  // added in the place of its projection.
  TEST_EQUAL(ResT(1, true),
               PrepareNearestPointOnTrack(point_A, std::nullopt, 0 /* prevIndex */,
                                          Direction::Forward, shape), ());

  TEST_EQUAL(shape.size(), 5, ());
  TEST_EQUAL(shape[1 /* expectedIndex */], m2::PointD(point_A.x, y), ());

  // Test that repeated point_A projection to the polyline doesn't lead to the second insertion.
  // Expected point projection index is the same.
  // But this projection is not inserted (it is already present).
  TEST_EQUAL(ResT(1, false),
               PrepareNearestPointOnTrack(point_A, std::nullopt, 0 /* prevIndex */,
                                          Direction::Forward, shape), ());
  // So the shape size remains the same.
  TEST_EQUAL(shape.size(), 5, ());

  // Test that point_B insertion leads to addition of the new projection to the shape.
  TEST_EQUAL(ResT(4, true),
               PrepareNearestPointOnTrack(point_B, std::nullopt, 1 /* prevIndex */,
                                          Direction::Forward, shape), ());

  // Test that point_C insertion does not lead to the addition of the new projection.
  TEST_EQUAL(ResT(5, false),
               PrepareNearestPointOnTrack(point_C, std::nullopt, 4 /* prevIndex */,
                                          Direction::Forward, shape), ());

  // Test point_C projection in backward direction.
  TEST_EQUAL(ResT(5, false),
               PrepareNearestPointOnTrack(point_C, std::nullopt, shape.size() - 1 /* prevIndex */,
                                          Direction::Backward, shape), ());

  // Test point_B projection in backward direction.
  TEST_EQUAL(ResT(4, false),
               PrepareNearestPointOnTrack(point_B, std::nullopt, 5 /* prevIndex */,
                                          Direction::Backward, shape), ());

  // Test point_A projection in backward direction.
  TEST_EQUAL(ResT(1, false),
               PrepareNearestPointOnTrack(point_A, std::nullopt, 4 /* prevIndex */,
                                          Direction::Backward, shape), ());
}

// Stop is on approximately the same distance from the segment (0, 1) and segment (1, 2).
// Its projection index and projection coordinate depend on the |startIndex| parameter.
//
// 1 +----------+ 2
//   |
//   |    *A
//   |
// 0 +
//
UNIT_TEST(Transit_GTFS_ProjectStopToLine_DifferentStartIndexes)
{
  // index, was inserted
  using ResT = std::pair<size_t, bool>;

  std::vector<m2::PointD> const referenceShape{{0.001, 0.001}, {0.001, 0.002}, {0.003, 0.002}};
  m2::PointD const point_A{0.0015, 0.0015};

  // Test for |startIndex| = 0.
  {
    auto shape = referenceShape;
    TEST_EQUAL(ResT(1, true),
                 PrepareNearestPointOnTrack(point_A, std::nullopt, 0 /* prevIndex */,
                                            Direction::Forward, shape), ());
    TEST_EQUAL(shape.size(), 4, ());
    TEST_EQUAL(shape[1 /* expectedIndex */], m2::PointD(0.001, point_A.y), ());
  }

  // Test for |startIndex| = 1.
  {
    auto shape = referenceShape;
    TEST_EQUAL(ResT(2, true),
                 PrepareNearestPointOnTrack(point_A, std::nullopt, 1 /* prevIndex */,
                                            Direction::Forward, shape), ());
    TEST_EQUAL(shape.size(), 4, ());
    TEST_EQUAL(shape[2 /* expectedIndex */], m2::PointD(point_A.x, 0.002), ());
  }
}

// Real-life example of stop being closer to the other side of the route (4, 5) then to its real
// destination (0, 1).
// We handle this type of situations by using constant max distance of departing from this stop
// on the polyline in |PrepareNearestPointOnTrack()|.
//
//  5                                4
//  +--------------------------------+---------------------------------------+ 3
//                                                                           |
//                        /+-------------------------------------------------+ 2
//      *A               / 1
//                      /
//                     + 0
//
UNIT_TEST(Transit_GTFS_ProjectStopToLine_MaxDistance)
{
  // index, was inserted
  using ResT = std::pair<size_t, bool>;

  std::vector<m2::PointD> shape{{0.002, 0.001},  {0.003, 0.003},  {0.010, 0.003},
                                {0.010, 0.0031}, {0.005, 0.0031}, {0.001, 0.0031}};
  m2::PointD const point_A{0.0028, 0.0029};
  TEST_EQUAL(ResT(1, true),
               PrepareNearestPointOnTrack(point_A, std::nullopt, 0 /* prevIndex */,
                                          Direction::Forward, shape), ());
}

/* Complex shape with multiple points on it and multiple stops for projection.
 *
 *                   +-----+
 *              C*  /       \
 *          /+\    /         \  *D
 *       + /    \+/           \
 *      /                     +
 *     /                      |       *E
 *    +                       +-----+
 *    |                             |
 *    |                             |
 *    +---+\                  +-----+
 *          \                 |
 *       B*  +                |
 *       A*   \     +---------+
 *             +    |
 *             |    +
 *             +         *F
 */
UNIT_TEST(Transit_GTFS_ProjectStopToLine_NearCircle)
{
  // index, was inserted
  using ResT = std::pair<size_t, bool>;

  double constexpr kEps = 1e-5;
  std::vector<m2::PointD> const initialShape{
      {0.003, 0.001},   {0.003, 0.0015},  {0.0025, 0.002},  {0.002, 0.0025},  {0.001, 0.0025},
      {0.001, 0.0035},  {0.0015, 0.0045}, {0.0025, 0.005},  {0.0035, 0.0045}, {0.004, 0.0055},
      {0.0055, 0.0055}, {0.0065, 0.0045}, {0.0065, 0.0035}, {0.0075, 0.0035}, {0.0075, 0.0025},
      {0.0065, 0.0025}, {0.0065, 0.0015}, {0.004, 0.0015},  {0.004, 0.001}};

  m2::PointD const point_A{0.0024, 0.0018};
  m2::PointD const point_B{0.002499, 0.00199};
  m2::PointD const point_C{0.0036, 0.0049};
  m2::PointD const point_D{0.0063, 0.005};
  m2::PointD const point_E{0.008, 0.004};
  m2::PointD const point_F{0.0047, 0.0005};

  // Forward
  auto shape = initialShape;
  TEST_EQUAL(ResT(2, true),
               PrepareNearestPointOnTrack(point_A, std::nullopt, 0 /* prevIndex */,
                                          Direction::Forward, shape), ());
  auto const coordA = shape[2];

  TEST_EQUAL(ResT(3, false),
               PrepareNearestPointOnTrack(point_B, std::nullopt, 2 /* prevIndex */,
                                          Direction::Forward, shape), ());
  auto const coordB = shape[3];

  TEST_EQUAL(ResT(10, true),
               PrepareNearestPointOnTrack(point_C, std::nullopt, 3 /* prevIndex */,
                                          Direction::Forward, shape), ());
  auto const coordC = shape[10];

  TEST_EQUAL(ResT(12, false),
               PrepareNearestPointOnTrack(point_D, std::nullopt, 10 /* prevIndex */,
                                          Direction::Forward, shape), ());
  auto const coordD = shape[12];

  TEST_EQUAL(ResT(14, true),
               PrepareNearestPointOnTrack(point_E, std::nullopt, 12 /* prevIndex */,
                                          Direction::Forward, shape), ());
  auto const coordE = shape[14];

  TEST_EQUAL(ResT(20, true),
               PrepareNearestPointOnTrack(point_F, std::nullopt, 14 /* prevIndex */,
                                          Direction::Forward, shape), ());

  // Backward processing of reversed shape
  shape = initialShape;
  reverse(shape.begin(), shape.end());
  TEST_EQUAL(ResT(17, true),
               PrepareNearestPointOnTrack(point_A, std::nullopt, shape.size() - 1/* prevIndex */,
                                          Direction::Backward, shape), ());
  TEST(AlmostEqualAbs(coordA, shape[17], kEps), (coordA, shape[17]));

  TEST_EQUAL(ResT(16, false),
               PrepareNearestPointOnTrack(point_B, std::nullopt, 17 /* prevIndex */,
                                          Direction::Backward, shape), ());
  TEST(AlmostEqualAbs(coordB, shape[16], kEps), (coordA, shape[17]));

  TEST_EQUAL(ResT(10, true),
               PrepareNearestPointOnTrack(point_C, std::nullopt, 16 /* prevIndex */,
                                          Direction::Backward, shape), ());
  TEST(AlmostEqualAbs(coordC, shape[10], kEps), (coordA, shape[17]));

  TEST_EQUAL(ResT(8, false),
               PrepareNearestPointOnTrack(point_D, std::nullopt, 10 /* prevIndex */,
                                          Direction::Backward, shape), ());
  TEST(AlmostEqualAbs(coordD, shape[8], kEps), (coordA, shape[17]));

  TEST_EQUAL(ResT(7, true),
               PrepareNearestPointOnTrack(point_E, std::nullopt, 8 /* prevIndex */,
                                          Direction::Backward, shape), ());
  TEST(AlmostEqualAbs(coordE, shape[7], kEps), (coordA, shape[17]));

  // point_F has different position because we do not insert before point 0.
  TEST_EQUAL(ResT(2, true),
               PrepareNearestPointOnTrack(point_F, std::nullopt, 7 /* prevIndex */,
                                          Direction::Backward, shape), ());
}

UNIT_TEST(Transit_ColorPicker)
{
  ColorPicker colorPicker;

  // We check that we don't match with the 'text' colors subset. This is the color of transit
  // text lime_light and we expect not to pick it.
  TEST_EQUAL(colorPicker.GetNearestColor("827717"), "lime_dark", ());

  // We check the default color for invalid input.
  TEST_EQUAL(colorPicker.GetNearestColor("94141230"), "default", ());

  // We check that we really find nearest colors. This input is really close to pink light.
  TEST_EQUAL(colorPicker.GetNearestColor("d18aa2"), "pink_light", ());
}

UNIT_TEST(Transit_BuildHash1Arg)
{
  TEST_EQUAL(BuildHash(std::string("Title")), "Title", ());
  TEST_EQUAL(BuildHash(std::string("Id1"), std::string("Id2")), "Id1_Id2", ());
  TEST_EQUAL(BuildHash(std::string("A"), std::string("B"), std::string("C")), "A_B_C", ());
}

UNIT_TEST(IntersectionSimple)
{
  auto const & factRes =
      FindIntersections({{1.0, 1.0}, {2.0, 2.0}, {3.0, 3.0}},
                        {{4.0, 4.0}, {1.0, 1.0}, {2.0, 2.0}, {3.0, 3.0}, {4.0, 4.0}});

  std::pair<LineSegments, LineSegments> planRes{{LineSegment(0, 2)}, {LineSegment(1, 3)}};
  TEST(factRes == planRes, ());
}

std::vector<m2::PointD> Get2DVector(std::vector<size_t> const & v)
{
  std::vector<m2::PointD> res;
  res.reserve(v.size());

  for (size_t val : v)
    res.emplace_back(static_cast<double>(val), 1.0);

  return res;
}

std::pair<LineSegments, LineSegments> GetIntersections(std::vector<size_t> const & line1,
                                                       std::vector<size_t> const & line2)
{
  return FindIntersections(Get2DVector(line1), Get2DVector(line2));
}

UNIT_TEST(IntersectionShortest)
{
  auto const & factRes = GetIntersections({10, 15}, {10, 15});
  std::pair<LineSegments, LineSegments> planRes{{LineSegment(0, 1)}, {LineSegment(0, 1)}};
  TEST(factRes == planRes, ());
}

UNIT_TEST(IntersectionNone)
{
  auto const & factRes = GetIntersections({100, 105}, {101, 110});
  std::pair<LineSegments, LineSegments> planRes{{}, {}};
  TEST(factRes == planRes, ());
}

UNIT_TEST(IntersectionDouble)
{
  auto const & factRes = GetIntersections({1, 2, 3, 4, 5, 6, 7, 8}, {3, 4, 5, 100, 7, 8});

  std::pair<LineSegments, LineSegments> planRes{{LineSegment(2, 4), LineSegment(6, 7)},
                                                {LineSegment(0, 2), LineSegment(4, 5)}};
  TEST(factRes == planRes, ());
}

UNIT_TEST(IntersectionTriple)
{
  auto const & factRes = GetIntersections({1, 2, 3, 6, 6, 6, 7, 8, 6, 6, 9, 10},
                                          {0, 0, 1, 2, 3, 0, 7, 8, 0, 9, 10, 0});

  std::pair<LineSegments, LineSegments> planRes{
      {LineSegment(0, 2), LineSegment(6, 7), LineSegment(10, 11)},
      {LineSegment(2, 4), LineSegment(6, 7), LineSegment(9, 10)}};
  TEST(factRes == planRes, ());
}

UNIT_TEST(GetIntersectionInner)
{
  auto inter = GetIntersection(0, 5, 2, 4);
  TEST(inter, ());
  TEST_EQUAL(inter->m_startIdx, 2, ());
  TEST_EQUAL(inter->m_endIdx, 4, ());
}

UNIT_TEST(GetIntersectionNone)
{
  auto inter = GetIntersection(1, 10, 20, 40);
  TEST(!inter, ());
}

UNIT_TEST(GetIntersectionLeft)
{
  auto inter = GetIntersection(10, 100, 5, 15);
  TEST(inter, ());
  TEST_EQUAL(inter->m_startIdx, 10, ());
  TEST_EQUAL(inter->m_endIdx, 15, ());
}

UNIT_TEST(GetIntersectionRight)
{
  auto inter = GetIntersection(0, 8, 5, 10);
  TEST(inter, ());
  TEST_EQUAL(inter->m_startIdx, 5, ());
  TEST_EQUAL(inter->m_endIdx, 8, ());
}

UNIT_TEST(GetIntersectionSingle)
{
  auto inter = GetIntersection(0, 8, 8, 10);
  TEST(!inter, ());
}

UNIT_TEST(CalcSegmentOrder)
{
  TEST_EQUAL(CalcSegmentOrder(0 /* segIndex */, 1 /* totalSegCount */), 0, ());

  TEST_EQUAL(CalcSegmentOrder(0 /* segIndex */, 2 /* totalSegCount */), -1, ());
  TEST_EQUAL(CalcSegmentOrder(1 /* segIndex */, 2 /* totalSegCount */), 1, ());

  TEST_EQUAL(CalcSegmentOrder(0 /* segIndex */, 3 /* totalSegCount */), -2, ());
  TEST_EQUAL(CalcSegmentOrder(1 /* segIndex */, 3 /* totalSegCount */), 0, ());
  TEST_EQUAL(CalcSegmentOrder(2 /* segIndex */, 3 /* totalSegCount */), 2, ());

  TEST_EQUAL(CalcSegmentOrder(0 /* segIndex */, 4 /* totalSegCount */), -3, ());
  TEST_EQUAL(CalcSegmentOrder(1 /* segIndex */, 4 /* totalSegCount */), -1, ());
  TEST_EQUAL(CalcSegmentOrder(2 /* segIndex */, 4 /* totalSegCount */), 1, ());
  TEST_EQUAL(CalcSegmentOrder(3 /* segIndex */, 4 /* totalSegCount */), 3, ());

  TEST_EQUAL(CalcSegmentOrder(0 /* segIndex */, 5 /* totalSegCount */), -4, ());
  TEST_EQUAL(CalcSegmentOrder(1 /* segIndex */, 5 /* totalSegCount */), -2, ());
  TEST_EQUAL(CalcSegmentOrder(2 /* segIndex */, 5 /* totalSegCount */), 0, ());
  TEST_EQUAL(CalcSegmentOrder(3 /* segIndex */, 5 /* totalSegCount */), 2, ());
  TEST_EQUAL(CalcSegmentOrder(4 /* segIndex */, 5 /* totalSegCount */), 4, ());
}

UNIT_TEST(SplitLineToRegions)
{
  TestStopsRange({1, 2, 3, 4, 5} /* stopsOnLine */, {1, 2, 3, 4, 5} /* stopsInRegion */,
                 0 /* firstIdxPlan */, 4 /* lastIdxPlan */);
  TestStopsRange({1, 2, 3, 4, 5, 6, 7} /* stopsOnLine */, {1, 2, 3} /* stopsInRegion */,
                 0 /* firstIdxPlan */, 3 /* lastIdxPlan */);
  TestStopsRange({1, 2, 3, 4, 5, 6, 7} /* stopsOnLine */, {3, 4} /* stopsInRegion */,
                 1 /* firstIdxPlan */, 4 /* lastIdxPlan */);
}
}  // namespace world_feed_tests
