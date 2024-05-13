#pragma once

#include "app/organicmaps/core/jni_helper.hpp"

#include "drape/graphics_context_factory.hpp"
#include "drape/vulkan/vulkan_base_context.hpp"
#include "drape/vulkan/vulkan_object_manager.hpp"
#include "drape/vulkan/vulkan_layers.hpp"
#include "drape/pointers.hpp"

#include <vulkan/vulkan_android.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

namespace android
{
class AndroidVulkanContextFactory : public dp::GraphicsContextFactory
{
public:
  explicit AndroidVulkanContextFactory(uint32_t appVersionCode, int sdkVersion);
  ~AndroidVulkanContextFactory();

  bool IsVulkanSupported() const;
  bool IsValid() const;

  dp::GraphicsContext * GetDrawContext() override;
  dp::GraphicsContext * GetResourcesUploadContext() override;
  bool IsDrawContextCreated() const override;
  bool IsUploadContextCreated() const override;
  void SetPresentAvailable(bool available) override;

  void SetSurface(JNIEnv * env, jobject jsurface);
  void ResetSurface(bool allowPipelineDump);
  void ChangeSurface(JNIEnv * env, jobject jsurface, int w, int h);

  int GetWidth() const;
  int GetHeight() const;

private:
  void SetVulkanSurface();
  void ResetVulkanSurface(bool allowPipelineDump);
  bool QuerySurfaceSize();

  VkInstance m_vulkanInstance = nullptr;
  drape_ptr<dp::vulkan::Layers> m_layers;
  VkPhysicalDevice m_gpu = nullptr;
  VkDevice m_device = nullptr;
  drape_ptr<dp::vulkan::VulkanObjectManager> m_objectManager;
  drape_ptr<dp::vulkan::VulkanBaseContext> m_drawContext;
  drape_ptr<dp::vulkan::VulkanBaseContext> m_uploadContext;

  ANativeWindow * m_nativeWindow = nullptr;
  bool m_windowSurfaceValid = false;
  VkSurfaceKHR m_surface = 0;
  VkSurfaceFormatKHR m_surfaceFormat;
  VkSurfaceCapabilitiesKHR m_surfaceCapabilities;

  int m_surfaceWidth = 0;
  int m_surfaceHeight = 0;
};
}  // namespace android
