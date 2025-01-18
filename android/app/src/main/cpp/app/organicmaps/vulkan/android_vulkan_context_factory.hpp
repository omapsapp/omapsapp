#pragma once

#include "app/organicmaps/core/jni_helper.hpp"

#include "drape/vulkan/vulkan_context_factory.hpp"

#include <vulkan/vulkan_android.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

namespace android
{
class AndroidVulkanContextFactory : public dp::vulkan::VulkanContextFactory
{
public:
  AndroidVulkanContextFactory(uint32_t appVersionCode, int sdkVersion, bool isCustomROM);

  bool IsValid() const;

  void SetSurface(JNIEnv * env, jobject jsurface);
  void ResetSurface(bool allowPipelineDump);
  void ChangeSurface(JNIEnv * env, jobject jsurface, int w, int h);

private:
  void SetVulkanSurface();
  void ResetVulkanSurface(bool allowPipelineDump);

  ANativeWindow * m_nativeWindow = nullptr;
  bool m_windowSurfaceValid = false;
};
}  // namespace android
