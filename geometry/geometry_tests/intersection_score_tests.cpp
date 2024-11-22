#include "testing/testing.hpp"

#include "geometry/any_rect2d.hpp"
#include "geometry/intersection_score.hpp"
#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/math.hpp"

#include <vector>

UNIT_TEST(IntersectionScore_PointsToPolygon)
{
  {
    m2::RectD rectD = {0, 0, 10, 10};
    m2::AnyRectD const anyRect1(rectD);
    rectD = {0, 0, 10, 9};
    m2::AnyRectD const anyRect2(rectD);

    m2::AnyRectD::Corners corners1;
    anyRect1.GetGlobalPoints(corners1);

    m2::AnyRectD::Corners corners2;
    anyRect2.GetGlobalPoints(corners2);

    auto const score = geometry::GetIntersectionScoreForPoints(corners1, corners2);

    TEST(AlmostEqualAbs(score, 0.9, 1e-10), ());
  }
  {
    m2::RectD rectD = {0, 0, 10, 10};
    m2::AnyRectD const anyRect1(rectD);
    rectD = {10, 10, 20, 20};
    m2::AnyRectD const anyRect2(rectD);

    m2::AnyRectD::Corners corners1;
    anyRect1.GetGlobalPoints(corners1);

    m2::AnyRectD::Corners corners2;
    anyRect2.GetGlobalPoints(corners2);

    auto const score = geometry::GetIntersectionScoreForPoints(corners1, corners2);

    TEST(AlmostEqualAbs(score, 0.0, 1e-10), ());
  }
  {
    m2::RectD rectD = {0, 0, 10, 10};
    m2::AnyRectD const anyRect1(rectD);
    m2::AnyRectD::Corners corners1;
    anyRect1.GetGlobalPoints(corners1);

    // Backward
    m2::AnyRectD::Corners corners2 = {m2::PointD{10.0, 10.0}, {10.0, 0.0}, {0.0, 0.0}, {0.0, 10.0}};
    auto const score = geometry::GetIntersectionScoreForPoints(corners1, corners2);

    TEST(AlmostEqualAbs(score, 1.0, 1e-10), ());
  }
}

UNIT_TEST(IntersectionScore_TrianglesToPolygon)
{
  {
    std::vector<m2::PointD> triangiulated1 = {{0.0, 0.0},  {0.0, 10.0}, {10.0, 0.0},
                                              {10.0, 0.0}, {0.0, 10.0}, {10.0, 10.0}};
    std::vector<m2::PointD> triangiulated2 = {{0.0, 0.0},  {0.0, 9.0}, {10.0, 0.0},
                                              {10.0, 0.0}, {0.0, 9.0}, {10.0, 9.0}};

    auto const score =
        geometry::GetIntersectionScoreForTriangulated(triangiulated1, triangiulated2);

    TEST(AlmostEqualAbs(score, 0.9, 1e-10), ());
  }
  {
    m2::RectD rectD = {0, 0, 10, 10};
    m2::AnyRectD const anyRect1(rectD);
    rectD = {10, 10, 20, 20};
    m2::AnyRectD const anyRect2(rectD);

    m2::AnyRectD::Corners corners1;
    anyRect1.GetGlobalPoints(corners1);

    m2::AnyRectD::Corners corners2;
    anyRect2.GetGlobalPoints(corners2);

    std::vector<m2::PointD> triangiulated1 = {{0.0, 0.0},  {0.0, 10.0}, {10.0, 0.0},
                                              {10.0, 0.0}, {0.0, 10.0}, {10.0, 10.0}};
    std::vector<m2::PointD> triangiulated2 = {{10.0, 10.0}, {10.0, 20.0}, {20.0, 10.0},
                                              {20.0, 10.0}, {10.0, 20.0}, {20.0, 20.0}};

    auto const score =
        geometry::GetIntersectionScoreForTriangulated(triangiulated1, triangiulated2);

    TEST(AlmostEqualAbs(score, 0.0, 1e-10), ());
  }
}
