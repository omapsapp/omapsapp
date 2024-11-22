#pragma once

#include "base/assert.hpp"

#include <algorithm>    // std::max
#include <cmath>
#include <functional>   // std::hash
#include <type_traits>

namespace math
{
double constexpr pi = 3.14159265358979323846;
double constexpr pi2 = pi / 2.0;
double constexpr pi4 = pi / 4.0;

template <typename T>
T Abs(T x)
{
  static_assert(std::is_signed<T>::value, "");
  return (x < 0 ? -x : x);
}

template <typename Number,
          typename EnableIf = typename std::enable_if_t<
              std::is_integral_v<Number> || std::is_floating_point_v<Number>, void>>
int constexpr Sign(Number const number) noexcept
{
  return number == 0 ? 0 : number > 0 ? 1 : -1;
}
}  // namespace math

// Compare floats or doubles for almost equality.
// maxULPs - number of closest floating point values that are considered equal.
// Infinity is treated as almost equal to the largest possible floating point values.
// NaN produces undefined result.
//
// This function is deprecated. Use AlmostEqualAbs, AlmostEqualRel or AlmostEqualAbsOrRel instead.
// See https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
// for details.
// NOTE: Intentionally in the global namespace for ADL (see point2d.hpp)
template <typename Float>
bool AlmostEqualULPs(Float x, Float y, uint32_t maxULPs = 256);

// Returns true if x and y are equal up to the absolute difference eps.
// Does not produce a sensible result if any of the arguments is NaN or infinity.
// The default value for eps is deliberately not provided: the intended usage
// is for the client to choose the precision according to the problem domain,
// explicitly define the precision constant and call this function.
// NOTE: Intentionally in the global namespace for ADL (see point2d.hpp)
template <typename Float>
bool AlmostEqualAbs(Float x, Float y, Float eps)
{
  return fabs(x - y) < eps;
}

// Returns true if x and y are equal up to the relative difference eps.
// Does not produce a sensible result if any of the arguments is NaN, infinity or zero.
// The same considerations as in AlmostEqualAbs apply.
template <typename Float>
bool AlmostEqualRel(Float x, Float y, Float eps)
{
  return fabs(x - y) < eps * std::max(fabs(x), fabs(y));
}

// Returns true if x and y are equal up to the absolute or relative difference eps.
template <typename Float>
bool AlmostEqualAbsOrRel(Float x, Float y, Float eps)
{
  return AlmostEqualAbs(x, y, eps) || AlmostEqualRel(x, y, eps);
}

namespace math
{
template <typename Float>
Float constexpr DegToRad(Float deg)
{
  return deg * Float(math::pi) / Float(180);
}

template <typename Float>
Float constexpr RadToDeg(Float rad)
{
  return rad * Float(180) / Float(math::pi);
}

template <typename T>
constexpr T Clamp(T const x, T const xmin, T const xmax)
{
  if (x > xmax)
    return xmax;
  if (x < xmin)
    return xmin;
  return x;
}

template <typename T>
bool Between(T const a, T const b, T const x)
{
  return a <= x && x <= b;
}

// This function is deprecated. Use std::round instead.
inline int SignedRound(double x)
{
  return x > 0.0 ? static_cast<int>(x + 0.5) : static_cast<int>(x - 0.5);
}

// Computes x^n.
template <typename T>
T PowUint(T x, uint64_t n)
{
  T res = 1;
  for (T t = x; n > 0; n >>= 1, t *= t)
  {
    if (n & 1)
      res *= t;
  }
  return res;
}

template <typename T>
T Pow2(T x)
{
  return x * x;
}

template <typename T>
T NextModN(T x, T n)
{
  ASSERT_GREATER(n, 0, ());
  return x + 1 == n ? 0 : x + 1;
}

template <typename T>
T PrevModN(T x, T n)
{
  ASSERT_GREATER(n, 0, ());
  return x == 0 ? n - 1 : x - 1;
}

inline uint32_t NextPowOf2(uint32_t v)
{
  v = v - 1;
  v |= (v >> 1);
  v |= (v >> 2);
  v |= (v >> 4);
  v |= (v >> 8);
  v |= (v >> 16);

  return v + 1;
}

// Greatest Common Divisor.
template <typename Number,
          typename EnableIf = typename std::enable_if_t<std::is_integral_v<Number>, void>>
Number constexpr GCD(Number const a, Number const b)
{
  return b == 0 ? a : GCD(b, a % b);
}

// Least Common Multiple.
template <typename Number,
          typename EnableIf = typename std::enable_if_t<std::is_integral_v<Number>, void>>
Number constexpr LCM(Number const a, Number const b)
{
  return a / GCD(a, b) * b;
}

// Calculate hash for the pair of values.
template <typename T1, typename T2>
size_t Hash(T1 const & t1, T2 const & t2)
{
  /// @todo Probably, we need better hash for 2 integral types.
  return (std::hash<T1>()(t1) ^ (std::hash<T2>()(t2) << 1));
}
}  // namespace math
