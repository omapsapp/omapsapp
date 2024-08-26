#pragma once

#include "base/assert.hpp"

#include <bit>
#include <bitset>
#include <cstdint>
#include <limits>
#include <numeric>
#include <type_traits>

namespace bits
{
inline uint32_t PopCount(uint32_t const * p, uint32_t n)
{
  return std::accumulate(p, p + n, uint32_t{0}, [](uint32_t sum, uint32_t elem){ return sum + std::popcount(elem); });
}

static constexpr int SELECT1_ERROR = -1;

template <typename T>
unsigned int select1(T x, unsigned int i)
{
  ASSERT(i > 0 && i <= std::popcount(x), (i));
  for (unsigned int j = 0; j < sizeof(T) * 8; ++j)
    if (x & (T(1) << j))
      if (--i == 0)
        return j;
  return static_cast<unsigned int>(SELECT1_ERROR);
}

constexpr uint8_t FloorLog(uint64_t x) noexcept
{
  return x == 0 ? 0 : std::bit_width(x) - 1;
}

template <typename T>
constexpr std::make_unsigned_t<T> ZigZagEncode(T x)
{
  static_assert(std::is_signed<T>::value, "Type should be signed");
  return (x << 1) ^ (x >> (sizeof(x) * 8 - 1));
}

template <typename T>
constexpr std::make_signed_t<T> ZigZagDecode(T x)
{
  static_assert(std::is_unsigned<T>::value, "Type should be unsigned.");
  return (x >> 1) ^ -static_cast<std::make_signed_t<T>>(x & 1);
}

constexpr uint32_t PerfectShuffle(uint32_t x)
{
  x = ((x & 0x0000FF00) << 8) | ((x >> 8) & 0x0000FF00) | (x & 0xFF0000FF);
  x = ((x & 0x00F000F0) << 4) | ((x >> 4) & 0x00F000F0) | (x & 0xF00FF00F);
  x = ((x & 0x0C0C0C0C) << 2) | ((x >> 2) & 0x0C0C0C0C) | (x & 0xC3C3C3C3);
  x = ((x & 0x22222222) << 1) | ((x >> 1) & 0x22222222) | (x & 0x99999999);
  return x;
}

constexpr uint32_t PerfectUnshuffle(uint32_t x)
{
  x = ((x & 0x22222222) << 1) | ((x >> 1) & 0x22222222) | (x & 0x99999999);
  x = ((x & 0x0C0C0C0C) << 2) | ((x >> 2) & 0x0C0C0C0C) | (x & 0xC3C3C3C3);
  x = ((x & 0x00F000F0) << 4) | ((x >> 4) & 0x00F000F0) | (x & 0xF00FF00F);
  x = ((x & 0x0000FF00) << 8) | ((x >> 8) & 0x0000FF00) | (x & 0xFF0000FF);
  return x;
}

// Returns the integer that has the bits of |x| at even-numbered positions
// and the bits of |y| at odd-numbered positions without changing the
// relative order of bits coming from |x| and |y|.
// That is, if the bits of |x| are {x31, x30, ..., x0},
//         and the bits of |y| are {y31, y30, ..., y0},
// then the bits of the result are {y31, x31, y30, x30, ..., y0, x0}.
inline constexpr uint64_t BitwiseMerge(uint32_t x, uint32_t y)
{
  uint32_t const hi = PerfectShuffle((y & 0xFFFF0000) | (x >> 16));
  uint32_t const lo = PerfectShuffle(((y & 0xFFFF) << 16 ) | (x & 0xFFFF));
  return (static_cast<uint64_t>(hi) << 32) + lo;
}

inline void BitwiseSplit(uint64_t v, uint32_t & x, uint32_t & y)
{
  uint32_t const hi = PerfectUnshuffle(static_cast<uint32_t>(v >> 32));
  uint32_t const lo = PerfectUnshuffle(static_cast<uint32_t>(v & 0xFFFFFFFFULL));
  x = ((hi & 0xFFFF) << 16) | (lo & 0xFFFF);
  y = (hi & 0xFFFF0000) | (lo >> 16);
}

// Returns 1 if bit is set and 0 otherwise.
inline uint8_t GetBit(void const * p, uint32_t offset)
{
  std::bitset<8> bits(static_cast<uint8_t const *>(p)[offset >> 3]);
  return bits[offset & 7];
}

inline void SetBitTo0(void * p, uint32_t offset)
{
  auto & byte = static_cast<uint8_t *>(p)[offset >> 3];
  byte &= ~(1 << (offset & 7));
}

inline void SetBitTo1(void * p, uint32_t offset)
{
  auto & byte = static_cast<uint8_t *>(p)[offset >> 3];
  byte |= (1 << (offset & 7));
}
  
// Computes number of bits needed to store the number, it is not equal to number of ones.
// E.g. if we have a number (in bit representation) 00001000b then NumUsedBits is 4.
inline uint32_t NumUsedBits(uint64_t n)
{
  uint32_t result = 0;
  while (n != 0) { ++result; n >>= 1; }
  return result;
}

constexpr uint64_t GetFullMask(uint8_t numBits)
{
  ASSERT_LESS_OR_EQUAL(numBits, 64, ());
  return numBits == 64 ? std::numeric_limits<uint64_t>::max()
                       : (static_cast<uint64_t>(1) << numBits) - 1;
}

constexpr bool IsPow2Minus1(uint64_t n) { return (n & (n + 1)) == 0; }
}  // namespace bits
