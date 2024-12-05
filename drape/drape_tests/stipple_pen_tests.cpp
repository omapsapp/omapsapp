#include "testing/testing.hpp"

#include "drape/drape_tests/memory_comparer.hpp"
#include "drape/drape_tests/dummy_texture.hpp"

#include "drape/stipple_pen_resource.hpp"
#include "drape/texture.hpp"
#include "drape/tm_read_resources.hpp"


namespace stipple_pen_tests
{
using namespace dp;

namespace
{
void TestPacker(StipplePenPacker & packer, uint32_t width, m2::RectU const & expect)
{
  TEST_EQUAL(packer.PackResource({ width, 1 }), expect, ());
}

bool IsRectsEqual(m2::RectF const & r1, m2::RectF const & r2)
{
  return AlmostEqualULPs(r1.minX(), r2.minX()) &&
         AlmostEqualULPs(r1.minY(), r2.minY()) &&
         AlmostEqualULPs(r1.maxX(), r2.maxX()) &&
         AlmostEqualULPs(r1.maxY(), r2.maxY());
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

UNIT_TEST(StippleTest_EqualPatterns)
{
  using PatternT = std::array<double, 2>;
  std::vector<PatternT> patterns;

  using namespace dp::impl;
  ParsePatternsList("./data/patterns.txt", [&patterns](buffer_vector<double, 8> const & p)
  {
    if (p.size() == 2)
      patterns.push_back({p[0], p[1]});
  });

  auto const IsEqualPatterns = [](PatternT const & p1, PatternT const & p2)
  {
    for (double scale : { 1, 2, 3 })
    {
      if ((PatternFloat2Pixel(scale * p1[0]) != PatternFloat2Pixel(scale * p2[0])) ||
          (PatternFloat2Pixel(scale * p1[1]) != PatternFloat2Pixel(scale * p2[1])))
        return false;
    }
    return true;
  };
  auto const IsAlmostEqualPatterns = [](PatternT const & p1, PatternT const & p2)
  {
    double const scale = 3;
    return (fabs(scale * p1[0] - scale * p2[0]) + fabs(scale * p1[1] - scale * p2[1])) < 1;
  };

  size_t const count = patterns.size();
  for (size_t i = 0; i < count - 1; ++i)
  {
    for (size_t j = i + 1; j < count; ++j)
    {
      if (IsEqualPatterns(patterns[i], patterns[j]))
        LOG(LINFO, ("Equal:", patterns[i], patterns[j]));
      else if (IsAlmostEqualPatterns(patterns[i], patterns[j]))
        LOG(LINFO, ("Almost equal:", patterns[i], patterns[j]));
    }
  }
}
} // namespace stipple_pen_tests
