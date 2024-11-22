#include "math.hpp"

#include <boost/integer.hpp>

#include <cstring>
#include <limits>

template <typename Float>
bool AlmostEqualULPs(Float x, Float y, uint32_t maxULPs)
{
  static_assert(std::is_floating_point<Float>::value, "");
  static_assert(std::numeric_limits<Float>::is_iec559, "");

  // Make sure maxUlps is non-negative and small enough that the
  // default NaN won't compare as equal to anything.
  ASSERT_LESS(maxULPs, 4 * 1024 * 1024, ());

  int constexpr bits = CHAR_BIT * sizeof(Float);
  typedef typename boost::int_t<bits>::exact IntType;
  typedef typename boost::uint_t<bits>::exact UIntType;

  // Same as *reinterpret_cast<IntType const *>(&x), but without warnings.
  IntType xInt, yInt;
  static_assert(sizeof(xInt) == sizeof(x), "bit_cast impossible");
  std::memcpy(&xInt, &x, sizeof(x));
  std::memcpy(&yInt, &y, sizeof(y));

  // Make xInt and yInt lexicographically ordered as a twos-complement int.
  IntType const highestBit = IntType(1) << (bits - 1);
  if (xInt < 0)
    xInt = highestBit - xInt;
  if (yInt < 0)
    yInt = highestBit - yInt;

  // Calculate diff with special case to avoid IntType overflow.
  UIntType diff;
  if ((xInt >= 0) == (yInt >= 0))
    diff = math::Abs(xInt - yInt);
  else
    diff = UIntType(math::Abs(xInt)) + UIntType(math::Abs(yInt));

  return diff <= maxULPs;
}

template bool AlmostEqualULPs<float>(float x, float y, uint32_t maxULPs);
template bool AlmostEqualULPs<double>(double x, double y, uint32_t maxULPs);
