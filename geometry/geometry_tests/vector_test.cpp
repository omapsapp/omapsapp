#include "testing/testing.hpp"

#include "geometry/avg_vector.hpp"

namespace
{
template <class T, size_t N>
bool EqualArrays(T (&a1)[N], T (&a2)[N])
{
  for (size_t i = 0; i < N; ++i)
  {
    if (!AlmostEqualULPs(a1[i], a2[i]))
      return false;
  }
  return true;
}
}  // namespace

UNIT_TEST(AvgVector_Smoke)
{
  math::AvgVector<double, 3> holder(3);

  double ethalon1[] = { 5, 5, 5 };
  double ethalon2[] = { 5.5, 5.5, 5.5 };
  double ethalon3[] = { 6, 6, 6 };

  double arr1[] = { 5, 5, 5 };
  double arr2[] = { 6, 6, 6 };
  double arr3[] = { 5, 5, 5 };
  double arr4[] = { 6, 6, 6 };

  holder.Next(arr1);
  TEST(EqualArrays(arr1, ethalon1), ());

  holder.Next(arr2);
  TEST(EqualArrays(arr2, ethalon2), ());

  holder.Next(arr3);
  TEST(EqualArrays(arr3, ethalon1), ());

  holder.Next(arr4);
  TEST(EqualArrays(arr4, ethalon3), ());
}
