#pragma once

#include "geometry/cellid.hpp"
#include "geometry/mercator.hpp"
#include "geometry/rect2d.hpp"

#include "base/assert.hpp"

#include <cstdint>
#include <utility>

using RectId = m2::CellId<19>;

// 24 is enough to have cell size < 2.5m * 2.5m for world.
constexpr int kGeoObjectsDepthLevels = 24;

// Cell size < 40m * 40m for world is good for regions.
constexpr int kRegionsDepthLevels = 20;

template <typename Bounds, typename CellId>
class CellIdConverter
{
public:
  static double XToCellIdX(double x)
  {
    return (x - Bounds::kMinX) / StepX();
  }
  static double YToCellIdY(double y)
  {
    return (y - Bounds::kMinY) / StepY();
  }

  static double CellIdXToX(double  x)
  {
    return (x*StepX() + Bounds::kMinX);
  }
  static double CellIdYToY(double y)
  {
    return (y*StepY() + Bounds::kMinY);
  }

  static CellId ToCellId(double x, double y)
  {
    uint32_t const ix = static_cast<uint32_t>(XToCellIdX(x));
    uint32_t const iy = static_cast<uint32_t>(YToCellIdY(y));
    CellId id = CellId::FromXY(ix, iy, CellId::DEPTH_LEVELS - 1);
#if 0 // DEBUG
    std::pair<uint32_t, uint32_t> ixy = id.XY();
    ASSERT(Abs(ixy.first  - ix) <= 1, (x, y, id, ixy));
    ASSERT(Abs(ixy.second - iy) <= 1, (x, y, id, ixy));
    CoordT minX, minY, maxX, maxY;
    GetCellBounds(id, minX, minY, maxX, maxY);
    ASSERT(minX <= x && x <= maxX, (x, y, id, minX, minY, maxX, maxY));
    ASSERT(minY <= y && y <= maxY, (x, y, id, minX, minY, maxX, maxY));
#endif
    return id;
  }

  static CellId Cover2PointsWithCell(double x1, double y1, double x2, double y2)
  {
    CellId id1 = ToCellId(x1, y1);
    CellId id2 = ToCellId(x2, y2);
    while (id1 != id2)
    {
      id1 = id1.Parent();
      id2 = id2.Parent();
    }
#if 0 // DEBUG
    double minX, minY, maxX, maxY;
    GetCellBounds(id1, minX, minY, maxX, maxY);
    ASSERT(math::Between(minX, maxX, x1), (x1, minX, maxX));
    ASSERT(math::Between(minX, maxX, x2), (x2, minX, maxX));
    ASSERT(math::Between(minY, maxY, y1), (y1, minY, maxY));
    ASSERT(math::Between(minY, maxY, y2), (y2, minY, maxY));
#endif
    return id1;
  }

  static m2::PointD FromCellId(CellId id)
  {
    std::pair<uint32_t, uint32_t> const xy = id.XY();
    return m2::PointD(CellIdXToX(xy.first), CellIdYToY(xy.second));
  }

  static void GetCellBounds(CellId id,
                            double & minX, double & minY, double & maxX, double & maxY)
  {
    std::pair<uint32_t, uint32_t> const xy = id.XY();
    uint32_t const r = id.Radius();
    minX = (xy.first - r) * StepX() + Bounds::kMinX;
    maxX = (xy.first + r) * StepX() + Bounds::kMinX;
    minY = (xy.second - r) * StepY() + Bounds::kMinY;
    maxY = (xy.second + r) * StepY() + Bounds::kMinY;
  }

private:
  inline static double StepX()
  {
    return static_cast<double>(Bounds::kRangeX) / CellId::MAX_COORD;
  }

  inline static double StepY()
  {
    return static_cast<double>(Bounds::kRangeY) / CellId::MAX_COORD;
  }
};
