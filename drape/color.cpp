#include "drape/color.hpp"

#include <algorithm>

namespace dp
{

namespace
{

static const uint8_t MaxChannelValue = 255;

} // namespace

#define EXTRACT_BYTE(x, n) (((x) >> 8 * (n)) & 0xFF);
#define CHANNEL_TO_FLOAT(x) ((x) / static_cast<float>(MaxChannelValue))

Color::Color()
  : m_rgba(MaxChannelValue)
{}

Color::Color(uint32_t rgba)
  : m_rgba(rgba)
{}

uint8_t Color::GetRed() const
{
  return EXTRACT_BYTE(m_rgba, 3);
}

uint8_t Color::GetGreen() const
{
  return EXTRACT_BYTE(m_rgba, 2);
}

uint8_t Color::GetBlue() const
{
  return EXTRACT_BYTE(m_rgba, 1);
}

uint8_t Color::GetAlpha() const
{
  return EXTRACT_BYTE(m_rgba, 0);
}

float Color::GetRedF() const
{
  return CHANNEL_TO_FLOAT(GetRed());
}

float Color::GetGreenF() const
{
  return CHANNEL_TO_FLOAT(GetGreen());
}

float Color::GetBlueF() const
{
  return CHANNEL_TO_FLOAT(GetBlue());
}

float Color::GetAlphaF() const
{
  return CHANNEL_TO_FLOAT(GetAlpha());
}

void Color::PremultiplyAlpha(float opacity)
{
  float const newAlpha =
      std::clamp<float>(GetAlpha() * opacity, 0.0f, static_cast<float>(MaxChannelValue));
  m_rgba = GetRed() << 24 | GetGreen() << 16 | GetBlue() << 8 | static_cast<uint8_t>(newAlpha);
}

uint8_t ExtractRed(uint32_t argb)
{
  return (argb >> 16) & 0xFF;
}

uint8_t ExtractGreen(uint32_t argb)
{
  return (argb >> 8) & 0xFF;
}

uint8_t ExtractBlue(uint32_t argb)
{
  return argb & 0xFF;
}

uint8_t ExtractAlpha(uint32_t argb)
{
  return (argb >> 24) & 0xFF;
}

Color Extract(uint32_t argb)
{
  return Color(ExtractRed(argb),
               ExtractGreen(argb),
               ExtractBlue(argb),
               ExtractAlpha(argb));
}

Color Extract(uint32_t xrgb, uint8_t a)
{
  return Color(ExtractRed(xrgb),
               ExtractGreen(xrgb),
               ExtractBlue(xrgb),
               a);
}

} // namespace dp
