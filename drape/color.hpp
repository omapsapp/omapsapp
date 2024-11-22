#pragma once

#include "base/math.hpp"

#include <cstdint>
#include <sstream>
#include <string>

namespace dp
{
struct Color
{
  constexpr Color() : Color(0, 0, 0, kMaxChannelValue) {}
  constexpr explicit Color(uint32_t rgba) : m_rgba(rgba) {}
  constexpr Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
    : Color(red << 24 | green << 16 | blue << 8 | alpha)
  {
  }
  constexpr Color(uint32_t rgb, uint8_t alpha)
    : Color(ExtractByte(rgb, 2), ExtractByte(rgb, 1), ExtractByte(rgb, 0), alpha)
  {
  }

  constexpr uint8_t GetRed() const { return ExtractByte(m_rgba, 3); }
  constexpr uint8_t GetGreen() const { return ExtractByte(m_rgba, 2); }
  constexpr uint8_t GetBlue() const { return ExtractByte(m_rgba, 1); }
  constexpr uint8_t GetAlpha() const { return ExtractByte(m_rgba, 0); }
  constexpr uint32_t GetRGBA() const { return m_rgba; }

  constexpr float GetRedF() const { return ChannelToFloat(GetRed()); }
  constexpr float GetGreenF() const { return ChannelToFloat(GetGreen()); }
  constexpr float GetBlueF() const { return ChannelToFloat(GetBlue()); }
  constexpr float GetAlphaF() const { return ChannelToFloat(GetAlpha()); }

  constexpr bool operator==(Color const & other) const { return m_rgba == other.m_rgba; }
  constexpr bool operator!=(Color const & other) const { return m_rgba != other.m_rgba; }
  constexpr bool operator<(Color const & other) const { return m_rgba < other.m_rgba; }

  constexpr Color operator*(float s) const
  {
    return {static_cast<uint8_t>(math::Clamp(GetRedF() * s, 0.0f, 1.0f) * 255.0f),
            static_cast<uint8_t>(math::Clamp(GetGreenF() * s, 0.0f, 1.0f) * 255.0f),
            static_cast<uint8_t>(math::Clamp(GetBlueF() * s, 0.0f, 1.0f) * 255.0f), GetAlpha()};
  }

  constexpr static Color Black() { return {0, 0, 0, 255}; }
  constexpr static Color White() { return {255, 255, 255, 255}; }
  constexpr static Color Red() { return {255, 0, 0, 255}; }
  constexpr static Color Blue() { return {0, 0, 255, 255}; }
  constexpr static Color Green() { return {0, 255, 0, 255}; }
  constexpr static Color Yellow() { return {255, 255, 0, 255}; }
  constexpr static Color Transparent() { return {0, 0, 0, 0}; }

  constexpr static Color FromARGB(uint32_t argb)
  {
    return {ExtractByte(argb, 2), ExtractByte(argb, 1), ExtractByte(argb, 0), ExtractByte(argb, 3)};
  }

private:
  constexpr static uint8_t ExtractByte(uint32_t number, uint8_t byteIdx) { return (number >> (8 * byteIdx)) & 0xFF; }

  constexpr static float ChannelToFloat(uint8_t channelValue)
  {
    return static_cast<float>(channelValue) / kMaxChannelValue;
  }

  constexpr static uint8_t kMaxChannelValue = 255;
  uint32_t m_rgba;
};

inline std::string DebugPrint(Color const & c)
{
  std::ostringstream out;
  out << "[R = " << static_cast<uint32_t>(c.GetRed()) << ", G = " << static_cast<uint32_t>(c.GetGreen())
      << ", B = " << static_cast<uint32_t>(c.GetBlue()) << ", A = " << static_cast<uint32_t>(c.GetAlpha()) << "]";
  return out.str();
}
}  // namespace dp
