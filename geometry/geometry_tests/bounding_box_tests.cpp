#include "testing/testing.hpp"

#include "geometry/bounding_box.hpp"

namespace bounding_box_tests
{
UNIT_TEST(BoundingBox_Smoke)
{
  {
    m2::BoundingBox bbox;

    TEST(!bbox.HasPoint(0, 0), ());
    TEST(!bbox.HasPoint(-1, 1), ());
  }

  {
    m2::BoundingBox bbox;

    bbox.Add(0, 0);
    TEST(bbox.HasPoint(0, 0), ());
    TEST(!bbox.HasPoint(1, 0), ());
    TEST(!bbox.HasPoint(0, 1), ());
    TEST(!bbox.HasPoint(1, 1), ());
    TEST(!bbox.HasPoint(0.5, 0.5), ());

    bbox.Add(1, 1);
    TEST(bbox.HasPoint(1, 0), ());
    TEST(bbox.HasPoint(0, 1), ());
    TEST(bbox.HasPoint(1, 1), ());
    TEST(bbox.HasPoint(0.5, 0.5), ());
  }
}
}  // namespace bounding_box_tests