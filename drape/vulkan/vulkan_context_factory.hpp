#pragma once

#include "drape/graphics_context_factory.hpp"
#include "drape/vulkan/vulkan_base_context.hpp"
#include "drape/vulkan/vulkan_object_manager.hpp"
#include "drape/vulkan/vulkan_layers.hpp"
#include "drape/pointers.hpp"

#include <vulkan/vulkan_android.h>

namespace dp
{
namespace vulkan
{
class VulkanContextFactory : public dp::GraphicsContextFactory
{
public:
  VulkanContextFactory(uint32_t appVersionCode, int sdkVersion, bool isCustomROM);
  ~VulkanContextFactory() override;

  bool IsVulkanSupported() const;

  dp::GraphicsContext * GetDrawContext() override;
  dp::GraphicsContext * GetResourcesUploadContext() override;
  bool IsDrawContextCreated() const override;
  bool IsUploadContextCreated() const override;
  void SetPresentAvailable(bool available) override;

  int GetWidth() const;
  int GetHeight() const;

  VkInstance GetVulkanInstance() const;

protected:
  bool QuerySurfaceSize();

  VkInstance m_vulkanInstance = nullptr;
  drape_ptr<dp::vulkan::Layers> m_layers;
  VkPhysicalDevice m_gpu = nullptr;
  VkDevice m_device = nullptr;
  drape_ptr<dp::vulkan::VulkanObjectManager> m_objectManager;
  drape_ptr<dp::vulkan::VulkanBaseContext> m_drawContext;
  drape_ptr<dp::vulkan::VulkanBaseContext> m_uploadContext;

  VkSurfaceKHR m_surface = 0;
  VkSurfaceFormatKHR m_surfaceFormat;
  VkSurfaceCapabilitiesKHR m_surfaceCapabilities;

  int m_surfaceWidth = 0;
  int m_surfaceHeight = 0;
};
}  // namespace vulkan
}  // namespace dp
