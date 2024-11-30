#pragma once

#include <cstdint>
#include <string>

class ScreenBase;

namespace gui
{
class RulerHelper
{
public:
  RulerHelper();

  void Update(ScreenBase const & screen);
  static bool IsVisible(ScreenBase const & screen);
  void Invalidate();

  static float GetRulerHalfHeight();
  float GetRulerPixelLength() const;
  static float GetMaxRulerPixelLength();
  static int GetVerticalTextOffset();
  bool IsTextDirty() const;
  std::string const & GetRulerText() const;
  void ResetTextDirtyFlag();
  static void GetTextInitInfo(std::string & alphabet, uint32_t & size);
  static void LocalizeTexts();

private:
  double CalcMetersDiff(double value);
  void SetTextDirty();

private:
  float m_pixelLength;
  int m_rangeIndex;
  std::string m_rulerText;
  bool m_isTextDirty;
  mutable bool m_dirtyTextRequested;
  int m_currentDrawScale = 0;
};
}  // namespace gui
