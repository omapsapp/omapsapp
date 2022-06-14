#include "testing/testing.hpp"

#include "drape/drape_tests/memory_comparer.hpp"
#include "drape/drape_tests/dummy_texture.hpp"

#include "drape/stipple_pen_resource.hpp"
#include "drape/texture.hpp"

namespace stipple_pen_tests
{
using namespace dp;

namespace
{
void TestPacker(StipplePenPacker & packer, uint32_t width, m2::RectU const & expect)
{
  TEST_EQUAL(packer.PackResource(width), expect, ());
}

bool IsRectsEqual(m2::RectF const & r1, m2::RectF const & r2)
{
  return base::AlmostEqualULPs(r1.minX(), r2.minX()) &&
         base::AlmostEqualULPs(r1.minY(), r2.minY()) &&
         base::AlmostEqualULPs(r1.maxX(), r2.maxX()) &&
         base::AlmostEqualULPs(r1.maxY(), r2.maxY());
}
}  // namespace

UNIT_TEST(StippleTest_Pack)
{
  StipplePenPacker packer(m2::PointU(512, 8));
  TestPacker(packer, 30, m2::RectU(0, 0, 30, 1));
  TestPacker(packer, 254, m2::RectU(0, 1, 254, 2));
  TestPacker(packer, 1, m2::RectU(0, 2, 1, 3));
  TestPacker(packer, 250, m2::RectU(0, 3, 250, 4));
  TestPacker(packer, 249, m2::RectU(0, 4, 249, 5));

  m2::RectF mapped = packer.MapTextureCoords(m2::RectU(0, 0, 256, 1));
  TEST(IsRectsEqual(mapped, m2::RectF(0.5f / 512.0f, 0.5f / 8.0f,
                                      255.5f / 512.0f, 0.5f / 8.0f)), ());
}
} // namespace stipple_pen_tests
