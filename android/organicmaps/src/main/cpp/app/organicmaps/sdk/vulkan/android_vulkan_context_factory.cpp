#include "android_vulkan_context_factory.hpp"

#include "app/organicmaps/platform/AndroidPlatform.hpp"

#include "drape/drape_diagnostics.hpp"
#include "drape/support_manager.hpp"
#include "drape/vulkan/vulkan_pipeline.hpp"
#include "drape/vulkan/vulkan_utils.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/macros.hpp"
#include "base/src_point.hpp"

#include <array>
#include <vector>

namespace android
{
namespace
{
class DrawVulkanContext : public dp::vulkan::VulkanBaseContext
{
public:
  DrawVulkanContext(VkInstance vulkanInstance, VkPhysicalDevice gpu,
                    VkPhysicalDeviceProperties const & gpuProperties, VkDevice device,
                    uint32_t renderingQueueFamilyIndex,
                    ref_ptr<dp::vulkan::VulkanObjectManager> objectManager, uint32_t appVersionCode,
                    bool hasPartialTextureUpdates)
    : dp::vulkan::VulkanBaseContext(
          vulkanInstance, gpu, gpuProperties, device, renderingQueueFamilyIndex, objectManager,
          make_unique_dp<dp::vulkan::VulkanPipeline>(device, appVersionCode),
          hasPartialTextureUpdates)
  {
    VkQueue queue;
    vkGetDeviceQueue(device, renderingQueueFamilyIndex, 0, &queue);
    SetRenderingQueue(queue);
    CreateCommandPool();
  }

  void MakeCurrent() override
  {
    m_objectManager->RegisterThread(dp::vulkan::VulkanObjectManager::Frontend);
  }
};

class UploadVulkanContext : public dp::vulkan::VulkanBaseContext
{
public:
  UploadVulkanContext(VkInstance vulkanInstance, VkPhysicalDevice gpu,
                      VkPhysicalDeviceProperties const & gpuProperties, VkDevice device,
                      uint32_t renderingQueueFamilyIndex,
                      ref_ptr<dp::vulkan::VulkanObjectManager> objectManager,
                      bool hasPartialTextureUpdates)
    : dp::vulkan::VulkanBaseContext(vulkanInstance, gpu, gpuProperties, device,
                                    renderingQueueFamilyIndex, objectManager,
                                    nullptr /* pipeline */, hasPartialTextureUpdates)
  {}

  void MakeCurrent() override
  {
    m_objectManager->RegisterThread(dp::vulkan::VulkanObjectManager::Backend);
  }

  void Present() override {}

  void Resize(int w, int h) override {}
  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override {}
  void Init(dp::ApiVersion apiVersion) override
  {
    CHECK_EQUAL(apiVersion, dp::ApiVersion::Vulkan, ());
  }

  void SetClearColor(dp::Color const & color) override {}
  void Clear(uint32_t clearBits, uint32_t storeBits) override {}
  void Flush() override {}
  void SetDepthTestEnabled(bool enabled) override {}
  void SetDepthTestFunction(dp::TestFunction depthFunction) override {}
  void SetStencilTestEnabled(bool enabled) override {}
  void SetStencilFunction(dp::StencilFace face,
                          dp::TestFunction stencilFunction) override {}
  void SetStencilActions(dp::StencilFace face,
                         dp::StencilAction stencilFailAction,
                         dp::StencilAction depthFailAction,
                         dp::StencilAction passAction) override {}
};
}  // namespace

AndroidVulkanContextFactory::AndroidVulkanContextFactory(uint32_t appVersionCode, int sdkVersion)
{
  if (InitVulkan() == 0)
  {
    LOG_ERROR_VK("Could not initialize Vulkan library.");
    return;
  }

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pNext = nullptr;
  appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.applicationVersion = appVersionCode;
  appInfo.engineVersion = appVersionCode;
  appInfo.pApplicationName = "OMaps";
  appInfo.pEngineName = "Drape Engine";

  bool enableDiagnostics = false;
#ifdef ENABLE_VULKAN_DIAGNOSTICS
  enableDiagnostics = true;
#endif
  m_layers = make_unique_dp<dp::vulkan::Layers>(enableDiagnostics);

  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pNext = nullptr;
  instanceCreateInfo.pApplicationInfo = &appInfo;
  instanceCreateInfo.enabledExtensionCount = m_layers->GetInstanceExtensionsCount();
  instanceCreateInfo.ppEnabledExtensionNames = m_layers->GetInstanceExtensions();
  instanceCreateInfo.enabledLayerCount = m_layers->GetInstanceLayersCount();
  instanceCreateInfo.ppEnabledLayerNames = m_layers->GetInstanceLayers();

  VkResult statusCode;
  statusCode = vkCreateInstance(&instanceCreateInfo, nullptr, &m_vulkanInstance);
  if (statusCode != VK_SUCCESS)
  {
    LOG_ERROR_VK_CALL(vkCreateInstance, statusCode);
    return;
  }

  uint32_t gpuCount = 0;
  statusCode = vkEnumeratePhysicalDevices(m_vulkanInstance, &gpuCount, nullptr);
  if (statusCode != VK_SUCCESS || gpuCount == 0)
  {
    LOG_ERROR_VK_CALL(vkEnumeratePhysicalDevices, statusCode);
    return;
  }

  std::vector<VkPhysicalDevice> tmpGpus(gpuCount);
  statusCode = vkEnumeratePhysicalDevices(m_vulkanInstance, &gpuCount, tmpGpus.data());
  if (statusCode != VK_SUCCESS)
  {
    LOG_ERROR_VK_CALL(vkEnumeratePhysicalDevices, statusCode);
    return;
  }
  m_gpu = tmpGpus[0];

  VkPhysicalDeviceProperties gpuProperties;
  vkGetPhysicalDeviceProperties(m_gpu, &gpuProperties);
  dp::SupportManager::Version apiVersion{VK_VERSION_MAJOR(gpuProperties.apiVersion),
                                         VK_VERSION_MINOR(gpuProperties.apiVersion),
                                         VK_VERSION_PATCH(gpuProperties.apiVersion)};
  dp::SupportManager::Version driverVersion{VK_VERSION_MAJOR(gpuProperties.driverVersion),
                                            VK_VERSION_MINOR(gpuProperties.driverVersion),
                                            VK_VERSION_PATCH(gpuProperties.driverVersion)};
  if (dp::SupportManager::Instance().IsVulkanForbidden(gpuProperties.deviceName, apiVersion,
                                                       driverVersion))
  {
    LOG_ERROR_VK("GPU/Driver configuration is not supported.");
    return;
  }

  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(m_gpu, &queueFamilyCount, nullptr);
  if (queueFamilyCount == 0)
  {
    LOG_ERROR_VK("Any queue family wasn't found.");
    return;
  }

  std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(m_gpu, &queueFamilyCount,
                                           queueFamilyProperties.data());

  uint32_t renderingQueueFamilyIndex = 0;
  for (; renderingQueueFamilyIndex < queueFamilyCount; ++renderingQueueFamilyIndex)
  {
    if (queueFamilyProperties[renderingQueueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      break;
  }
  if (renderingQueueFamilyIndex == queueFamilyCount)
  {
    LOG_ERROR_VK("Any queue family with VK_QUEUE_GRAPHICS_BIT wasn't found.");
    return;
  }

  if (!dp::vulkan::VulkanFormatUnpacker::Init(m_gpu))
    return;

  if (!m_layers->Initialize(m_vulkanInstance, m_gpu))
    return;

  float priorities[] = {1.0f};
  VkDeviceQueueCreateInfo queueCreateInfo = {};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.pNext = nullptr;
  queueCreateInfo.flags = 0;
  queueCreateInfo.queueCount = 1;
  queueCreateInfo.queueFamilyIndex = renderingQueueFamilyIndex;
  queueCreateInfo.pQueuePriorities = priorities;

  VkDeviceCreateInfo deviceCreateInfo = {};
  VkPhysicalDeviceFeatures enabledFeatures = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.pNext = nullptr;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
  deviceCreateInfo.enabledLayerCount = m_layers->GetDeviceLayersCount();
  deviceCreateInfo.ppEnabledLayerNames = m_layers->GetDeviceLayers();
  deviceCreateInfo.enabledExtensionCount = m_layers->GetDeviceExtensionsCount();
  deviceCreateInfo.ppEnabledExtensionNames = m_layers->GetDeviceExtensions();
  deviceCreateInfo.pEnabledFeatures = nullptr;
  if (enableDiagnostics)
  {
    enabledFeatures.robustBufferAccess = VK_TRUE;
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
  }

  statusCode = vkCreateDevice(m_gpu, &deviceCreateInfo, nullptr, &m_device);
  if (statusCode != VK_SUCCESS)
  {
    LOG_ERROR_VK_CALL(vkCreateDevice, statusCode);
    return;
  }

  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(m_gpu, &memoryProperties);
  m_objectManager = make_unique_dp<dp::vulkan::VulkanObjectManager>(m_device, gpuProperties.limits,
                                                                    memoryProperties,
                                                                    renderingQueueFamilyIndex);

  bool const hasPartialTextureUpdates =
      !dp::SupportManager::Instance().IsVulkanTexturePartialUpdateBuggy(
          sdkVersion, gpuProperties.deviceName, apiVersion, driverVersion);

  m_drawContext = make_unique_dp<DrawVulkanContext>(
      m_vulkanInstance, m_gpu, gpuProperties, m_device, renderingQueueFamilyIndex,
      make_ref(m_objectManager), appVersionCode, hasPartialTextureUpdates);
  m_uploadContext = make_unique_dp<UploadVulkanContext>(
      m_vulkanInstance, m_gpu, gpuProperties, m_device, renderingQueueFamilyIndex,
      make_ref(m_objectManager), hasPartialTextureUpdates);
}

AndroidVulkanContextFactory::~AndroidVulkanContextFactory()
{
  m_drawContext.reset();
  m_uploadContext.reset();
  m_objectManager.reset();

  if (m_device != nullptr)
  {
    vkDeviceWaitIdle(m_device);
    vkDestroyDevice(m_device, nullptr);
  }

  if (m_vulkanInstance != nullptr)
  {
    m_layers->Uninitialize(m_vulkanInstance);
    vkDestroyInstance(m_vulkanInstance, nullptr);
  }
}

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

bool AndroidVulkanContextFactory::QuerySurfaceSize()
{
  auto statusCode = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_gpu, m_surface,
                                                              &m_surfaceCapabilities);
  if (statusCode != VK_SUCCESS)
  {
    LOG_ERROR_VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, statusCode);
    return false;
  }

  uint32_t formatCount = 0;
  statusCode = vkGetPhysicalDeviceSurfaceFormatsKHR(m_gpu, m_surface, &formatCount, nullptr);
  if (statusCode != VK_SUCCESS)
  {
    LOG_ERROR_VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR, statusCode);
    return false;
  }

  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  statusCode = vkGetPhysicalDeviceSurfaceFormatsKHR(m_gpu, m_surface, &formatCount, formats.data());
  if (statusCode != VK_SUCCESS)
  {
    LOG_ERROR_VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR, statusCode);
    return false;
  }

  uint32_t chosenFormat;
  for (chosenFormat = 0; chosenFormat < formatCount; chosenFormat++)
  {
    if (formats[chosenFormat].format == VK_FORMAT_R8G8B8A8_UNORM)
      break;
  }
  if (chosenFormat == formatCount)
  {
    LOG_ERROR_VK("Any supported surface format wasn't found.");
    return false;
  }

  if (!(m_surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR))
  {
    LOG_ERROR_VK("Alpha channel is not supported.");
    return false;
  }

  m_surfaceFormat = formats[chosenFormat];
  m_surfaceWidth = static_cast<int>(m_surfaceCapabilities.currentExtent.width);
  m_surfaceHeight = static_cast<int>(m_surfaceCapabilities.currentExtent.height);
  return true;
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

bool AndroidVulkanContextFactory::IsVulkanSupported() const
{
  return m_vulkanInstance != nullptr && m_gpu != nullptr && m_device != nullptr;
}

bool AndroidVulkanContextFactory::IsValid() const
{
  return IsVulkanSupported() && m_windowSurfaceValid;
}

int AndroidVulkanContextFactory::GetWidth() const
{
  ASSERT(IsValid(), ());
  return m_surfaceWidth;
}

int AndroidVulkanContextFactory::GetHeight() const
{
  ASSERT(IsValid(), ());
  return m_surfaceHeight;
}

dp::GraphicsContext * AndroidVulkanContextFactory::GetDrawContext()
{
  return m_drawContext.get();
}

dp::GraphicsContext * AndroidVulkanContextFactory::GetResourcesUploadContext()
{
  return m_uploadContext.get();
}

bool AndroidVulkanContextFactory::IsDrawContextCreated() const
{
  return m_drawContext != nullptr;
}

bool AndroidVulkanContextFactory::IsUploadContextCreated() const
{
  return m_uploadContext != nullptr;
}

void AndroidVulkanContextFactory::SetPresentAvailable(bool available)
{
  if (m_drawContext)
    m_drawContext->SetPresentAvailable(available);
}
}  // namespace android
