#pragma once

#include "drape/graphics_context.hpp"
#include "drape/pointers.hpp"

#include "base/macros.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <string>

namespace dp
{
extern std::string_view kSupportedAntialiasing;

class SupportManager
{
public:
  // This singleton must be available only from rendering threads.
  static SupportManager & Instance();

  // Initialization must be called only when graphics context is created.
  // Initialization happens once per application launch, so SupportManager
  // must not contain any properties which can be changed in the case of contexts
  // reinitialization.
  void Init(ref_ptr<GraphicsContext> context);

  bool IsSamsungGoogleNexus() const { return m_isSamsungGoogleNexus; }
  bool IsAdreno200Device() const { return m_isAdreno200; }
  bool IsTegraDevice() const { return m_isTegra; }
  bool IsAntialiasingEnabledByDefault() const { return m_isAntialiasingEnabledByDefault; }

  float GetMaxLineWidth() const { return m_maxLineWidth; }
  uint32_t GetMaxTextureSize() const { return m_maxTextureSize; }

  // These functions can be used without manager initialization.
  static void ForbidVulkan();

  using Version = std::array<uint32_t, 3>;
  static bool IsVulkanForbidden();
  static bool IsVulkanForbidden(std::string const & deviceName, Version apiVersion, Version driverVersion, bool isCustomROM);
  static bool IsVulkanTexturePartialUpdateBuggy(int sdkVersion, std::string const & deviceName,
                                                Version apiVersion, Version driverVersion);

private:
  struct Configuration;

  SupportManager() = default;

  std::string m_rendererName;
  std::string m_rendererVersion;
  bool m_isSamsungGoogleNexus = false;
  bool m_isAdreno200 = false;
  bool m_isTegra = false;
  bool m_isAntialiasingEnabledByDefault = false;

  float m_maxLineWidth = 1;
  uint32_t m_maxTextureSize = 1024;

  bool m_isInitialized = false;
  std::mutex m_mutex;

  DISALLOW_COPY_AND_MOVE(SupportManager);
};
}  // namespace dp
