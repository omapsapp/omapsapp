#include "testing/testing.hpp"

#include "geometry/diamond_box.hpp"

namespace diamond_box_tests
{
UNIT_TEST(DiamondBox_Smoke)
{
  {
    m2::DiamondBox dbox;
    TEST(!dbox.HasPoint(0, 0), ());
  }

  {
    m2::DiamondBox dbox;
    dbox.Add(0, 0);
    TEST(dbox.HasPoint(0, 0), ());
    TEST(!dbox.HasPoint(0, 1), ());
    TEST(!dbox.HasPoint(1, 0), ());
    TEST(!dbox.HasPoint(1, 1), ());
    TEST(!dbox.HasPoint(0.5, 0.5), ());

    dbox.Add(1, 1);
    TEST(dbox.HasPoint(0, 0), ());
    TEST(dbox.HasPoint(1, 1), ());
    TEST(dbox.HasPoint(0.5, 0.5), ());
    TEST(!dbox.HasPoint(1, 0), ());
    TEST(!dbox.HasPoint(0, 1), ());
  }

  {
    m2::DiamondBox dbox;

    dbox.Add(0, 1);
    dbox.Add(0, -1);
    dbox.Add(-1, 0);
    dbox.Add(1, 0);
    TEST(dbox.HasPoint(0, 0), ());
    TEST(dbox.HasPoint(0.5, 0.5), ());
    TEST(dbox.HasPoint(0.5, -0.5), ());
    TEST(dbox.HasPoint(-0.5, 0.5), ());
    TEST(dbox.HasPoint(-0.5, -0.5), ());

    TEST(!dbox.HasPoint(0.51, 0.51), ());
    TEST(!dbox.HasPoint(0.51, -0.51), ());
    TEST(!dbox.HasPoint(-0.51, 0.51), ());
    TEST(!dbox.HasPoint(-0.51, -0.51), ());
  }
}
}  // namespace diamond_box_tests
