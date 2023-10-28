#pragma once

#include "base/math.hpp"

#include <cstdint>
#include <sstream>
#include <string>

namespace dp
{
struct Color
{
  Color();
  explicit Color(uint32_t rgba);
  constexpr Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
    : m_rgba(red << 24 | green << 16 | blue << 8 | alpha)
  {
  }

  uint8_t GetRed() const;
  uint8_t GetGreen() const;
  uint8_t GetBlue() const;
  uint8_t GetAlpha() const;

  float GetRedF() const;
  float GetGreenF() const;
  float GetBlueF() const;
  float GetAlphaF() const;

  bool operator==(Color const & other) const { return m_rgba == other.m_rgba; }
  bool operator!=(Color const & other) const { return m_rgba != other.m_rgba; }
  bool operator<(Color const & other) const { return m_rgba < other.m_rgba; }

  Color operator*(float s) const
  {
    return Color(static_cast<uint8_t>(base::Clamp(GetRedF() * s, 0.0f, 1.0f) * 255.0f),
                 static_cast<uint8_t>(base::Clamp(GetGreenF() * s, 0.0f, 1.0f) * 255.0f),
                 static_cast<uint8_t>(base::Clamp(GetBlueF() * s, 0.0f, 1.0f) * 255.0f), GetAlpha());
  }

  void PremultiplyAlpha(float opacity);

  static constexpr Color Black() { return Color(0, 0, 0, 255); }
  static constexpr Color White() { return Color(255, 255, 255, 255); }
  static constexpr Color Red() { return Color(255, 0, 0, 255); }
  static constexpr Color Blue() { return Color(0, 0, 255, 255); }
  static constexpr Color Green() { return Color(0, 255, 0, 255); }
  static constexpr Color Yellow() { return Color(255, 255, 0, 255); }
  static constexpr Color Transparent() { return Color(0, 0, 0, 0); }

private:
  uint32_t m_rgba;
};

inline uint8_t ExtractRed(uint32_t argb);
inline uint8_t ExtractGreen(uint32_t argb);
inline uint8_t ExtractBlue(uint32_t argb);
inline uint8_t ExtractAlpha(uint32_t argb);
Color Extract(uint32_t argb);
Color Extract(uint32_t xrgb, uint8_t a);

inline std::string DebugPrint(Color const & c)
{
  std::ostringstream out;
  out << "[R = " << static_cast<uint32_t>(c.GetRed())
      << ", G = " << static_cast<uint32_t>(c.GetGreen())
      << ", B = " << static_cast<uint32_t>(c.GetBlue())
      << ", A = " << static_cast<uint32_t>(c.GetAlpha()) << "]";
  return out.str();
}
}  // namespace dp
