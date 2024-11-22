#pragma once

#include "geometry/point2d.hpp"
#include "geometry/segment2d.hpp"
#include "geometry/triangle2d.hpp"

#include "base/assert.hpp"
#include "base/base.hpp"
#include "base/buffer_vector.hpp"
#include "base/logging.hpp"
#include "base/math.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace covering
{
// Result of an intersection between object and cell.
enum CellObjectIntersection
{
  // No intersection. It is important, that its value is 0, so one can do if (intersection) ... .
  CELL_OBJECT_NO_INTERSECTION = 0,
  CELL_OBJECT_INTERSECT = 1,
  CELL_INSIDE_OBJECT = 2,
  OBJECT_INSIDE_CELL = 3
};

template <class CellId>
CellObjectIntersection IntersectCellWithLine(CellId const cell, m2::PointD const & a,
                                             m2::PointD const & b)
{
  std::pair<uint32_t, uint32_t> const xy = cell.XY();
  uint32_t const r = cell.Radius();
  m2::PointD const cellCorners[4] = {
      m2::PointD(xy.first - r, xy.second - r), m2::PointD(xy.first - r, xy.second + r),
      m2::PointD(xy.first + r, xy.second + r), m2::PointD(xy.first + r, xy.second - r)};
  for (int i = 0; i < 4; ++i)
  {
    if (m2::SegmentsIntersect(a, b, cellCorners[i], cellCorners[i == 0 ? 3 : i - 1]))
      return CELL_OBJECT_INTERSECT;
  }
  if (xy.first - r <= a.x && a.x <= xy.first + r && xy.second - r <= a.y && a.y <= xy.second + r)
    return OBJECT_INSIDE_CELL;
  return CELL_OBJECT_NO_INTERSECTION;
}

template <class CellId>
CellObjectIntersection IntersectCellWithTriangle(CellId const cell, m2::PointD const & a,
                                                 m2::PointD const & b, m2::PointD const & c)
{
  CellObjectIntersection const i1 = IntersectCellWithLine(cell, a, b);
  if (i1 == CELL_OBJECT_INTERSECT)
    return CELL_OBJECT_INTERSECT;
  CellObjectIntersection const i2 = IntersectCellWithLine(cell, b, c);
  if (i2 == CELL_OBJECT_INTERSECT)
    return CELL_OBJECT_INTERSECT;
  CellObjectIntersection const i3 = IntersectCellWithLine(cell, c, a);
  if (i3 == CELL_OBJECT_INTERSECT)
    return CELL_OBJECT_INTERSECT;
  // At this point either:
  // 1. Triangle is inside cell.
  // 2. Cell is inside triangle.
  // 3. Cell and triangle do not intersect.
  ASSERT_EQUAL(i1, i2, (cell, a, b, c));
  ASSERT_EQUAL(i2, i3, (cell, a, b, c));
  ASSERT_EQUAL(i3, i1, (cell, a, b, c));
  if (i1 == OBJECT_INSIDE_CELL || i2 == OBJECT_INSIDE_CELL || i3 == OBJECT_INSIDE_CELL)
    return OBJECT_INSIDE_CELL;
  std::pair<uint32_t, uint32_t> const xy = cell.XY();
  if (m2::IsPointStrictlyInsideTriangle(m2::PointD(xy.first, xy.second), a, b, c))
    return CELL_INSIDE_OBJECT;
  return CELL_OBJECT_NO_INTERSECTION;
}

template <class CellId, class CellIdContainerT, typename IntersectF>
void CoverObject(IntersectF const & intersect, uint64_t cellPenaltyArea, CellIdContainerT & out,
                 int cellDepth, CellId cell)
{
  if (cell.Level() == cellDepth - 1)
  {
    out.push_back(cell);
    return;
  }

  uint64_t const cellArea = math::Pow2(uint64_t(1 << (cellDepth - 1 - cell.Level())));
  CellObjectIntersection const intersection = intersect(cell);

  if (intersection == CELL_OBJECT_NO_INTERSECTION)
    return;
  if (intersection == CELL_INSIDE_OBJECT || cellPenaltyArea >= cellArea)
  {
    out.push_back(cell);
    return;
  }

  buffer_vector<CellId, 32> subdiv;
  for (uint8_t i = 0; i < 4; ++i)
    CoverObject(intersect, cellPenaltyArea, subdiv, cellDepth, cell.Child(i));

  uint64_t subdivArea = 0;
  for (size_t i = 0; i < subdiv.size(); ++i)
    subdivArea += math::Pow2(uint64_t(1 << (cellDepth - 1 - subdiv[i].Level())));

  ASSERT(!subdiv.empty(), (cellPenaltyArea, out, cell));

  // This criteria is more clear for me. Let's divide if we can save more than cellPenaltyArea.
  if (subdiv.size() > 1 && cellPenaltyArea >= cellArea - subdivArea)
  {
    out.push_back(cell);
  }
  else
  {
    for (size_t i = 0; i < subdiv.size(); ++i)
      out.push_back(subdiv[i]);
  }
}
}  // namespace covering
