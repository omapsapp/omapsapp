#include "android_vulkan_context_factory.hpp"

#include "app/organicmaps/platform/AndroidPlatform.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/macros.hpp"
#include "base/src_point.hpp"

namespace android
{
AndroidVulkanContextFactory::AndroidVulkanContextFactory(uint32_t appVersionCode, int sdkVersion, bool isCustomROM)
  : dp::vulkan::VulkanContextFactory(appVersionCode, sdkVersion, isCustomROM)
{}

void AndroidVulkanContextFactory::SetSurface(JNIEnv * env, jobject jsurface)
{
  if (!jsurface)
  {
    LOG_ERROR_VK("Java surface is not found.");
    return;
  }

  m_nativeWindow = ANativeWindow_fromSurface(env, jsurface);
  if (!m_nativeWindow)
  {
    LOG_ERROR_VK("Can't get native window from Java surface.");
    return;
  }

  SetVulkanSurface();
}

void AndroidVulkanContextFactory::SetVulkanSurface()
{
  if (m_windowSurfaceValid)
    return;

  VkAndroidSurfaceCreateInfoKHR createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  createInfo.pNext = nullptr;
  createInfo.flags = 0;
  createInfo.window = m_nativeWindow;

  VkResult statusCode;
  statusCode = vkCreateAndroidSurfaceKHR(m_vulkanInstance, &createInfo, nullptr,
                                         &m_surface);
  if (statusCode != VK_SUCCESS)
  {
    LOG_ERROR_VK_CALL(vkCreateAndroidSurfaceKHR, statusCode);
    return;
  }

  uint32_t const renderingQueueIndex = m_drawContext->GetRenderingQueueFamilyIndex();
  VkBool32 supportsPresent;
  statusCode = vkGetPhysicalDeviceSurfaceSupportKHR(m_gpu, renderingQueueIndex, m_surface, &supportsPresent);
  if (statusCode != VK_SUCCESS)
  {
    LOG_ERROR_VK_CALL(vkGetPhysicalDeviceSurfaceSupportKHR, statusCode);
    return;
  }
  CHECK_EQUAL(supportsPresent, VK_TRUE, ());

  CHECK(QuerySurfaceSize(), ());

  if (m_drawContext)
    m_drawContext->SetSurface(m_surface, m_surfaceFormat, m_surfaceCapabilities);

  m_windowSurfaceValid = true;
}

void AndroidVulkanContextFactory::ResetSurface(bool allowPipelineDump)
{
  ResetVulkanSurface(allowPipelineDump);

  if (m_nativeWindow != nullptr)
  {
    ANativeWindow_release(m_nativeWindow);
    m_nativeWindow = nullptr;
  }
}

void AndroidVulkanContextFactory::ResetVulkanSurface(bool allowPipelineDump)
{
  if (!m_windowSurfaceValid)
    return;

  if (m_drawContext)
    m_drawContext->ResetSurface(allowPipelineDump);

  vkDestroySurfaceKHR(m_vulkanInstance, m_surface, nullptr);
  m_surface = 0;
  m_windowSurfaceValid = false;
}

void AndroidVulkanContextFactory::ChangeSurface(JNIEnv * env, jobject jsurface, int w, int h)
{
  if (w == m_surfaceWidth && m_surfaceHeight == h)
    return;

  auto nativeWindow = ANativeWindow_fromSurface(env, jsurface);
  if (m_nativeWindow == nullptr)
  {
    CHECK(!m_windowSurfaceValid, ());
    m_nativeWindow = nativeWindow;
  }
  else
  {
    ResetVulkanSurface(false /* allowPipelineDump */);
    if (nativeWindow != m_nativeWindow)
    {
      ANativeWindow_release(m_nativeWindow);
      m_nativeWindow = nativeWindow;
    }
  }

  SetVulkanSurface();
  LOG(LINFO, ("Surface changed", m_surfaceWidth, m_surfaceHeight));
}

bool AndroidVulkanContextFactory::IsValid() const
{
  return IsVulkanSupported() && m_windowSurfaceValid;
}
}  // namespace android
