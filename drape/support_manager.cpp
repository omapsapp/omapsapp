#include "drape/support_manager.hpp"
#include "drape/gl_functions.hpp"
#include "drape/vulkan/vulkan_base_context.hpp"

#include "platform/settings.hpp"

#include "base/logging.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace dp
{
struct SupportManager::Configuration
{
  std::string_view m_deviceName;
  Version m_apiVersion;
  Version m_driverVersion;
};

std::string_view kSupportedAntialiasing = "Antialiasing";
std::string_view constexpr kVulkanForbidden = "VulkanForbidden";

void SupportManager::Init(ref_ptr<GraphicsContext> context)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_isInitialized)
    return;

  m_rendererName = context->GetRendererName();
  m_rendererVersion = context->GetRendererVersion();
  LOG(LINFO, ("Renderer =", m_rendererName, "| Api =", context->GetApiVersion(), "| Version =", m_rendererVersion));

  m_isSamsungGoogleNexus = (m_rendererName == "PowerVR SGX 540" &&
                            m_rendererVersion.find("GOOGLENEXUS.ED945322") != std::string::npos);
  if (m_isSamsungGoogleNexus)
    LOG(LINFO, ("Samsung Google Nexus detected."));

  if (m_rendererName.find("Adreno") != std::string::npos)
  {
    std::array<std::string_view, 5> constexpr models = { "200", "203", "205", "220", "225" };
    for (auto const & model : models)
    {
      if (m_rendererName.find(model) != std::string::npos)
      {
        LOG(LINFO, ("Adreno 200 device detected."));
        m_isAdreno200 = true;
        break;
      }
    }
  }

  m_isTegra = (m_rendererName.find("Tegra") != std::string::npos);
  if (m_isTegra)
    LOG(LINFO, ("NVidia Tegra device detected."));

  auto const apiVersion = context->GetApiVersion();
  if (apiVersion == dp::ApiVersion::OpenGLES2 || apiVersion == dp::ApiVersion::OpenGLES3)
  {
    m_maxLineWidth = static_cast<float>(std::max(1, GLFunctions::glGetMaxLineWidth()));
    m_maxTextureSize = static_cast<uint32_t>(GLFunctions::glGetInteger(gl_const::GLMaxTextureSize));
  }
  else if (apiVersion == dp::ApiVersion::Metal)
  {
    // Metal does not support thick lines.
    m_maxLineWidth = 1.0f;
    m_maxTextureSize = 4096;
  }
  else if (apiVersion == dp::ApiVersion::Vulkan)
  {
    ref_ptr<dp::vulkan::VulkanBaseContext> vulkanContext = context;
    auto const & props = vulkanContext->GetGpuProperties();
    m_maxLineWidth = std::max(props.limits.lineWidthRange[0], props.limits.lineWidthRange[1]);
    m_maxTextureSize = vulkanContext->GetGpuProperties().limits.maxImageDimension2D;
  }
  LOG(LINFO, ("Max line width =", m_maxLineWidth,"| Max texture size =", m_maxTextureSize));

  // Set up default antialiasing value.
  // Turn off AA for a while by energy-saving issues.
//  bool val;
//  if (!settings::Get(kSupportedAntialiasing, val))
//  {
//#ifdef OMIM_OS_ANDROID
//    std::vector<std::string> const models = {"Mali-G71", "Mali-T880", "Adreno (TM) 540",
//                                             "Adreno (TM) 530", "Adreno (TM) 430"};
//    m_isAntialiasingEnabledByDefault = base::IsExist(models, m_rendererName);
//#else
//    m_isAntialiasingEnabledByDefault = true;
//#endif
//    settings::Set(kSupportedAntialiasing, m_isAntialiasingEnabledByDefault);
//  }

  m_isInitialized = true;
}

void SupportManager::ForbidVulkan()
{
  settings::Set(kVulkanForbidden, true);
}

bool SupportManager::IsVulkanForbidden()
{
  bool forbidden;
  if (!settings::Get(kVulkanForbidden, forbidden))
    forbidden = false;
  return forbidden;
}

bool SupportManager::IsVulkanForbidden(std::string const & deviceName, Version apiVersion,
                                       Version driverVersion, bool isCustomROM)
{
  LOG(LINFO, ("Device =", deviceName, "API =", apiVersion, "Driver =", driverVersion));

  static char const * kBannedDevices[] = {
    /// @todo Should we ban all PowerVR Rogue devices?
    // https://github.com/organicmaps/organicmaps/issues/1379
    "PowerVR Rogue G6110", "PowerVR Rogue GE8100", "PowerVR Rogue GE8300",
    // https://github.com/organicmaps/organicmaps/issues/5539
    "Adreno (TM) 418",
  };

  for (auto const d : kBannedDevices)
  {
    if (d == deviceName)
      return true;
  }

  if (isCustomROM)
  {
    // Crash on LineageOS, stock Android works ok (with same api = 1.0.82; driver = 28.0.0).
    // https://github.com/organicmaps/organicmaps/issues/2739
    // https://github.com/organicmaps/organicmaps/issues/9255
    // SM-G930F (S7, heroltexx, hero2ltexx). Crash on vkCreateSwapchainKHR and we don't even get to SupportManager::Init.
    // SM-G920F (S6)
    if (deviceName.starts_with("Mali-T"))
      return true;
  }

  // On these configurations we've detected fatal driver-specific Vulkan errors.
  static Configuration constexpr kBannedConfigurations[] = {
      Configuration{"Adreno (TM) 506", {1, 0, 31}, {42, 264, 975}},
      Configuration{"Adreno (TM) 506", {1, 1, 66}, {512, 313, 0}},
      // Xiaomi Redmi Note 5
      Configuration{"Adreno (TM) 506", {1, 1, 128}, {512, 502, 0}},
      Configuration{"Adreno (TM) 530", {1, 1, 66}, {512, 313, 0}},

      /// @todo Route line is flickering in nav mode.
      /// Samsung Galaxy S8 (SM-G950F)
      Configuration{"Mali-G71", {1, 0, 97}, {16, 0, 0}},

      /// @todo Dashed lines stopped drawing after updating LineShape::Construct<DashedLineBuilder>.
      /// Huawei P20
      Configuration{"Mali-G72", {1, 1, 97}, {18, 0, 0}},
      /// Samsung SM-A505FN (a50), hangs when showing the subway layer.
      Configuration{"Mali-G72", {1, 1, 131}, {26, 0, 0}},
  };

  for (auto const & c : kBannedConfigurations)
  {
    if (c.m_deviceName == deviceName && c.m_apiVersion == apiVersion && c.m_driverVersion == driverVersion)
      return true;
  }
  return false;
}

// Finally, the result of this function is used in GraphicsContext::HasPartialTextureUpdates.
bool SupportManager::IsVulkanTexturePartialUpdateBuggy(int sdkVersion,
                                                       std::string const & deviceName,
                                                       Version apiVersion,
                                                       Version driverVersion)
{
  /// @todo Assume that all Android 10+ (API 29) doesn't support Vulkan partial texture updates.
  /// Can't say for sure is it right or not ..
  if (sdkVersion >= 29)
    return true;

  // For these configurations partial updates of texture clears whole texture except part updated
  static Configuration constexpr kBadConfigurations[] = {
      {"Mali-G76", {1, 1, 97}, {18, 0, 0}},
  };

  for (auto const & c : kBadConfigurations)
  {
    if (c.m_deviceName == deviceName && c.m_apiVersion == apiVersion && c.m_driverVersion == driverVersion)
      return true;
  }
  return false;
}

SupportManager & SupportManager::Instance()
{
  static SupportManager manager;
  return manager;
}
}  // namespace dp
