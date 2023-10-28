#pragma once

#include "drape_frontend/gui/compass.hpp"
#include "drape_frontend/gui/scale_fps_helper.hpp"
#include "drape_frontend/gui/skin.hpp"
#include "drape_frontend/gui/speed_limit_helper.hpp"

#include "storage/storage_defines.hpp"

#include "drape/pointers.hpp"

#include <functional>
#include <memory>
#include <mutex>

class ScreenBase;

namespace gui
{
class RulerHelper;

class DrapeGui
{
public:
  static DrapeGui & Instance();
  static RulerHelper & GetRulerHelper();
  static SpeedLimitHelper & GetSpeedLimitHelper();

  static dp::FontDecl GetGuiTextFont();

  void Destroy();
  void SetSurfaceSize(m2::PointF const & size);
  m2::PointF GetSurfaceSize() const;

  bool IsInUserAction() const { return m_inUserAction; }
  void SetInUserAction(bool isInUserAction) { m_inUserAction = isInUserAction; }

  bool IsCopyrightActive() const { return m_isCopyrightActive; }
  void DeactivateCopyright() { m_isCopyrightActive = false; }

  void ConnectOnCompassTappedHandler(Shape::TTapHandler const & handler);
  void CallOnCompassTappedHandler();

  ScaleFpsHelper & GetScaleFpsHelper() { return m_scaleFpsHelper; }
  ScaleFpsHelper const & GetScaleFpsHelper() const { return m_scaleFpsHelper; }

private:
  DrapeGui();
  RulerHelper & GetRulerHelperImpl();
  SpeedLimitHelper & GetSpeedLimitHelperImpl();

  struct Impl;
  std::unique_ptr<Impl> m_impl;
  bool m_isCopyrightActive = true;

  Shape::TTapHandler m_onCompassTappedHandler;
  m2::PointF m_surfaceSize;
  mutable std::mutex m_surfaceSizeMutex;
  bool m_inUserAction = false;
  ScaleFpsHelper m_scaleFpsHelper;
};
}  // namespace gui
