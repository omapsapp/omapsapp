#include "testing/testing.hpp"

#include "base/bits.hpp"

#include <cstdint>
#include <cstdlib>

UNIT_TEST(Select1Test)
{
  TEST_EQUAL(0U, bits::select1(1, 1), ());
}

UNIT_TEST(PerfectShuffle)
{
  // 0010 0001 0100 0000
  // 0010 0001 1000 1110
  TEST_EQUAL(bits::PerfectShuffle(557851022), 201547860, ());
  TEST_EQUAL(bits::PerfectUnshuffle(201547860), 557851022, ());
}

UNIT_TEST(BitwiseMerge)
{
  TEST_EQUAL(bits::BitwiseMerge(1, 1), 3, ());
  TEST_EQUAL(bits::BitwiseMerge(3, 1), 7, ());
  TEST_EQUAL(bits::BitwiseMerge(1, 3), 11, ());
  TEST_EQUAL(bits::BitwiseMerge(uint32_t{1} << 31, uint32_t{1} << 31), uint64_t{3} << 62, ());

  auto bitwiseMergeSlow = [](uint32_t x, uint32_t y) -> uint64_t {
    uint64_t result = 0;
    for (uint32_t i = 0; i < 32; ++i)
    {
      uint64_t const bitX = (static_cast<uint64_t>(x) >> i) & 1;
      uint64_t const bitY = (static_cast<uint64_t>(y) >> i) & 1;
      result |= bitX << (2 * i);
      result |= bitY << (2 * i + 1);
    }
    return result;
  };

  for (uint32_t x = 0; x < 16; ++x)
  {
    for (uint32_t y = 0; y < 16; ++y)
      TEST_EQUAL(bits::BitwiseMerge(x, y), bitwiseMergeSlow(x, y), (x, y));
  }
}

UNIT_TEST(ZigZagEncode)
{
  TEST_EQUAL(bits::ZigZagEncode(0),  0, ());
  TEST_EQUAL(bits::ZigZagEncode(-1), 1, ());
  TEST_EQUAL(bits::ZigZagEncode(1),  2, ());
  TEST_EQUAL(bits::ZigZagEncode(-2), 3, ());
  TEST_EQUAL(bits::ZigZagEncode(2),  4, ());
  TEST_EQUAL(bits::ZigZagEncode(127),  254, ());
  TEST_EQUAL(bits::ZigZagEncode(-128), 255, ());
  TEST_EQUAL(bits::ZigZagEncode(128),  256, ());
}

UNIT_TEST(ZigZagDecode)
{
  TEST_EQUAL(bits::ZigZagDecode(0U), 0, ());
  TEST_EQUAL(bits::ZigZagDecode(1U), -1, ());
  TEST_EQUAL(bits::ZigZagDecode(2U),  1, ());
  TEST_EQUAL(bits::ZigZagDecode(3U), -2, ());
  TEST_EQUAL(bits::ZigZagDecode(4U),  2, ());
  TEST_EQUAL(bits::ZigZagDecode(254U),  127, ());
  TEST_EQUAL(bits::ZigZagDecode(255U), -128, ());
  TEST_EQUAL(bits::ZigZagDecode(256U),  128, ());
}

UNIT_TEST(NumHiZeroBits32)
{
  TEST_EQUAL(bits::NumHiZeroBits32(0), 32, ());
  TEST_EQUAL(bits::NumHiZeroBits32(0xFFFFFFFF), 0, ());
  TEST_EQUAL(bits::NumHiZeroBits32(0x0FABCDEF), 4, ());
  TEST_EQUAL(bits::NumHiZeroBits32(0x000000FF), 24, ());
}

UNIT_TEST(NumHiZeroBits64)
{
  TEST_EQUAL(bits::NumHiZeroBits64(0), 64, ());
  TEST_EQUAL(bits::NumHiZeroBits64(0xFFFFFFFFFFFFFFFFULL), 0, ());
  TEST_EQUAL(bits::NumHiZeroBits64(0x0FABCDEF0FABCDEFULL), 4, ());
  TEST_EQUAL(bits::NumHiZeroBits64(0x000000000000FDEFULL), 48, ());
}

UNIT_TEST(NumUsedBits)
{
  TEST_EQUAL(bits::NumUsedBits(0), 0, ());
  TEST_EQUAL(bits::NumUsedBits(0xFFFFFFFFFFFFFFFFULL), 64, ());
  TEST_EQUAL(bits::NumUsedBits(0x0FABCDEF0FABCDEFULL), 60, ());
  TEST_EQUAL(bits::NumUsedBits(0x000000000000FDEFULL), 16, ());
}

UNIT_TEST(CeilLog)
{
  TEST_EQUAL(0, bits::FloorLog(0x0), ());
  TEST_EQUAL(0, bits::FloorLog(0x1), ());
  TEST_EQUAL(1, bits::FloorLog(0x2), ());
  TEST_EQUAL(1, bits::FloorLog(0x3), ());
  TEST_EQUAL(2, bits::FloorLog(0x4), ());

  TEST_EQUAL(6, bits::FloorLog(0x7f), ());
  TEST_EQUAL(7, bits::FloorLog(0x80), ());
  TEST_EQUAL(31, bits::FloorLog(0xFFFFFFFF), ());
  TEST_EQUAL(63, bits::FloorLog(0xFFFFFFFFFFFFFFFF), ());
}
