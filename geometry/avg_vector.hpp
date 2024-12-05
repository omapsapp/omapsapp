#pragma once

#include "base/assert.hpp"
#include "base/math.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <deque>
#include <limits>
#include <type_traits>

namespace math
{
template <class T, size_t Dim>
class AvgVector
{
public:
  explicit AvgVector(size_t count = 1) : m_count(count)
  {
    static_assert(std::is_floating_point<T>::value, "");
  }

  void SetCount(size_t count) { m_count = count; }

  /// @param[in]  Next measurement.
  /// @param[out] Average value.
  void Next(T * arr)
  {
    if (m_vectors.size() == m_count)
      m_vectors.pop_front();

    m_vectors.push_back({});
    std::memcpy(m_vectors.back().data(), arr, Dim * sizeof(T));

    if (m_vectors.size() > 1)
      CalcAverage(arr);
  }

private:
  using Cont = std::deque<std::array<T, Dim>>;
  using Value = typename Cont::value_type;

  Cont m_vectors;
  size_t m_count;

  static T Distance(Value const & a1, Value const & a2)
  {
    T res = 0;
    for (size_t i = 0; i < Dim; ++i)
      res += math::Pow2(a1[i] - a2[i]);

    return std::sqrt(res);
  }

  static void Average(Value const & a1, Value const & a2, T * res)
  {
    for (size_t i = 0; i < Dim; ++i)
      res[i] = (a1[i] + a2[i]) / 2.0;
  }

  void CalcAverage(T * res) const
  {
    T minD = std::numeric_limits<T>::max();
    size_t I = 0, J = 1;

    size_t const count = m_vectors.size();
    ASSERT_GREATER(count, 1, ());
    for (size_t i = 0; i < count - 1; ++i)
    {
      for (size_t j = i + 1; j < count; ++j)
      {
        T const d = Distance(m_vectors[i], m_vectors[j]);
        if (d < minD)
        {
          I = i;
          J = j;
          minD = d;
        }
      }
    }

    Average(m_vectors[I], m_vectors[J], res);
  }
};

// Compass smoothing parameters
// We're using technique described in
// http://windowsteamblog.com/windows_phone/b/wpdev/archive/2010/09/08/using-the-accelerometer-on-windows-phone-7.aspx
// In short it's a combination of low-pass filter to smooth the
// small orientation changes and a threshold filter to get big changes fast.
// k in the following formula: O(n) = O(n-1) + k * (I - O(n - 1));
// smoothed heading angle. doesn't always correspond to the m_headingAngle
// as we change the heading angle only if the delta between
// smoothedHeadingRad and new heading value is bigger than smoothingThreshold.
template <class T, size_t Dim>
class LowPassVector
{
public:
  void SetFactor(T t) { m_factor = t; }

  /// @param[in]  Next measurement.
  /// @param[out] Average value.
  void Next(T * arr)
  {
    for (size_t i = 0; i < Dim; ++i)
    {
      m_val[i] = m_val[i] + m_factor * (arr[i] - m_val[i]);
      arr[i] = m_val[i];
    }
  }

private:
  std::array<T, Dim> m_val{};
  T m_factor = 0.15;
};
}  // namespace math
