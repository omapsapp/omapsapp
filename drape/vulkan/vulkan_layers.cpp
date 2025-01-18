#include "drape/vulkan/vulkan_layers.hpp"

#include "drape/vulkan/vulkan_utils.hpp"

#include "base/macros.hpp"

#include <algorithm>
#include <cstring>

namespace dp
{
namespace vulkan
{
namespace
{
char const * kDebugReportExtension = "VK_EXT_debug_report";
char const * kValidationFeaturesExtension = "VK_EXT_validation_features";

char const * const kInstanceExtensions[] = {
  "VK_KHR_surface",
  "VK_KHR_android_surface",
  kDebugReportExtension,
  kValidationFeaturesExtension,
#if defined(OMIM_OS_MAC) || defined(OMIM_OS_LINUX)
  "VK_EXT_debug_utils",
#endif
#if defined(OMIM_OS_MAC)
  "VK_KHR_portability_enumeration",
  "VK_MVK_macos_surface",
  "VK_KHR_get_physical_device_properties2",
#endif
#if defined(OMIM_OS_LINUX)
  "VK_KHR_xlib_surface",
#endif
};

char const * const kDeviceExtensions[] =
{
  "VK_KHR_swapchain",
#if defined(OMIM_OS_MAC)
  "VK_KHR_portability_subset",
#endif
};

char const * const kValidationLayers[] =
{
  "VK_LAYER_KHRONOS_validation",
};

std::vector<char const *> CheckLayers(std::vector<VkLayerProperties> const & props)
{
  std::vector<char const *> result;
  result.reserve(props.size());
  for (uint32_t i = 0; i < ARRAY_SIZE(kValidationLayers); ++i)
  {
    auto const it = std::find_if(props.begin(), props.end(),
                                 [i](VkLayerProperties const & p)
    {
      return strcmp(kValidationLayers[i], p.layerName) == 0;
    });
    if (it != props.end())
      result.push_back(kValidationLayers[i]);
  }
  return result;
}

std::vector<char const *> CheckExtensions(std::vector<VkExtensionProperties> const & props,
                                          bool enableDiagnostics, char const * const * extensions,
                                          uint32_t extensionsCount)
{
  std::vector<char const *> result;
  result.reserve(props.size());
  for (uint32_t i = 0; i < extensionsCount; ++i)
  {
    if (!enableDiagnostics)
    {
      if (strcmp(extensions[i], kDebugReportExtension) == 0)
        continue;

      if (strcmp(extensions[i], kValidationFeaturesExtension) == 0)
        continue;
    }

    auto const it = std::find_if(props.begin(), props.end(),
                                 [i, extensions](VkExtensionProperties const & p)
    {
      return strcmp(extensions[i], p.extensionName) == 0;
    });
    if (it != props.end())
      result.push_back(extensions[i]);
  }
  return result;
}

std::string GetReportObjectTypeString(VkDebugReportObjectTypeEXT objectType)
{
  switch (objectType)
  {
  case VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT: return "UNKNOWN";
  case VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT: return "INSTANCE";
  case VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT: return "PHYSICAL_DEVICE";
  case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT: return "DEVICE";
  case VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT: return "QUEUE";
  case VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT: return "SEMAPHORE";
  case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT: return "COMMAND_BUFFER";
  case VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT: return "FENCE";
  case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT: return "DEVICE_MEMORY";
  case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT: return "BUFFER";
  case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT: return "IMAGE";
  case VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT: return "EVENT";
  case VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT: return "QUERY";
  case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT: return "BUFFER_VIEW";
  case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT: return "IMAGE_VIEW";
  case VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT: return "SHADER_MODULE";
  case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT: return "PIPELINE_CACHE";
  case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT: return "PIPELINE_LAYOUT";
  case VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT: return "RENDER_PASS";
  case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT: return "PIPELINE";
  case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT: return "DESCRIPTOR_SET_LAYOUT";
  case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT: return "SAMPLER";
  case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT: return "DESCRIPTOR_POOL";
  case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT: return "DESCRIPTOR_SET";
  case VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT: return "FRAMEBUFFER";
  case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT: return "COMMAND_POOL";
  case VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT: return "SURFACE_KHR";
  case VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT: return "SWAPCHAIN_KHR";
  case VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT_EXT:
    return "DEBUG_REPORT_CALLBACK_EXT";
  case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_KHR_EXT: return "DISPLAY_KHR";
  case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_MODE_KHR_EXT: return "DISPLAY_MODE_KHR";
  case VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT: return "VALIDATION_CACHE_EXT";
  case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION_EXT: return "SAMPLER_YCBCR_CONVERSION";
  case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_EXT:
    return "DESCRIPTOR_UPDATE_TEMPLATE";
  case VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV_EXT:
    return "ACCELERATION_STRUCTURE_NV";
  case VK_DEBUG_REPORT_OBJECT_TYPE_MAX_ENUM_EXT: return "MAX_ENUM";
  case VK_DEBUG_REPORT_OBJECT_TYPE_CU_MODULE_NVX_EXT: return "CU_MODULE_NVX";
  case VK_DEBUG_REPORT_OBJECT_TYPE_CU_FUNCTION_NVX_EXT: return "CU_FUNCTION_NVX";
  case VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR_EXT: return "ACCELERATION_STRUCTURE_KHR";
  case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA_EXT: return "BUFFER_COLLECTION_FUCHSIA";
  case VK_DEBUG_REPORT_OBJECT_TYPE_CUDA_MODULE_NV_EXT: return "CUDA_MODULE_NV";
  case VK_DEBUG_REPORT_OBJECT_TYPE_CUDA_FUNCTION_NV_EXT: return "CUDA_FUNCTION_NV";
  }
  UNREACHABLE();
  return {};
}

bool IsContained(char const * name, std::vector<char const *> const & collection)
{
  return collection.end() != std::find_if(collection.begin(), collection.end(),
                                          [name](char const * v) { return strcmp(name, v) == 0; });
}
}  // namespace

static VkBool32 VKAPI_PTR DebugReportCallbackImpl(VkDebugReportFlagsEXT flags,
                                           VkDebugReportObjectTypeEXT objectType, uint64_t object,
                                           size_t location, int32_t /*messageCode*/,
                                           const char * pLayerPrefix, const char * pMessage,
                                           void * /*pUserData*/)
{
  auto logLevel = base::LogLevel::LINFO;
  if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) ||
      (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT))
  {
    logLevel = base::LogLevel::LWARNING;
  }
  else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
  {
    logLevel = base::LogLevel::LERROR;
  }
#ifdef ENABLE_VULKAN_DEBUG_DIAGNOSTICS_MESSAGES
  else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
  {
    logLevel = base::LogLevel::LDEBUG;
  }
#else
  else
  {
    return VK_FALSE;
  }
#endif

  LOG(logLevel, ("Vulkan Diagnostics [", pLayerPrefix, "] [", GetReportObjectTypeString(objectType),
                 "] [OBJ:", object, "LOC:", location, "]:", pMessage));
  return VK_FALSE;
}

Layers::Layers(bool enableDiagnostics)
  : m_enableDiagnostics(enableDiagnostics)
  , m_vkCreateDebugReportCallbackEXT(vkCreateDebugReportCallbackEXT)
  , m_vkDestroyDebugReportCallbackEXT(vkDestroyDebugReportCallbackEXT)
  , m_vkDebugReportMessageEXT(vkDebugReportMessageEXT)
{
  if (m_enableDiagnostics)
  {
    // Get instance layers count.
    uint32_t instLayerCount = 0;
    auto statusCode = vkEnumerateInstanceLayerProperties(&instLayerCount, nullptr);
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkEnumerateInstanceLayerProperties, statusCode);
      return;
    }

    // Enumerate instance layers.
    std::vector<VkLayerProperties> layerProperties;
    if (instLayerCount != 0)
    {
      layerProperties.resize(instLayerCount);
      statusCode = vkEnumerateInstanceLayerProperties(&instLayerCount, layerProperties.data());
      if (statusCode != VK_SUCCESS)
      {
        LOG_ERROR_VK_CALL(vkEnumerateInstanceLayerProperties, statusCode);
        return;
      }
      m_instanceLayers = CheckLayers(layerProperties);

      for (auto layer : m_instanceLayers)
        LOG(LDEBUG, ("Vulkan instance layer prepared", layer));
    }
  }

  // Get instance extensions count.
  uint32_t instExtensionsCount = 0;
  auto statusCode = vkEnumerateInstanceExtensionProperties(nullptr, &instExtensionsCount, nullptr);
  if (statusCode != VK_SUCCESS)
  {
    LOG_ERROR_VK_CALL(vkEnumerateInstanceExtensionProperties, statusCode);
    return;
  }

  // Enumerate instance extensions.
  std::vector<VkExtensionProperties> extensionsProperties;
  if (instExtensionsCount != 0)
  {
    extensionsProperties.resize(instExtensionsCount);
    statusCode = vkEnumerateInstanceExtensionProperties(nullptr, &instExtensionsCount,
                                                        extensionsProperties.data());
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkEnumerateInstanceExtensionProperties, statusCode);
      return;
    }
  }

  // Enumerate instance extensions for each layer.
  for (auto layerName : m_instanceLayers)
  {
    uint32_t cnt = 0;
    statusCode = vkEnumerateInstanceExtensionProperties(layerName, &cnt, nullptr);
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkEnumerateInstanceExtensionProperties, statusCode);
      return;
    }
    if (cnt == 0)
      continue;

    std::vector<VkExtensionProperties> props(cnt);
    statusCode = vkEnumerateInstanceExtensionProperties(layerName, &cnt, props.data());
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkEnumerateInstanceExtensionProperties, statusCode);
      return;
    }
    extensionsProperties.insert(extensionsProperties.end(), props.begin(), props.end());
  }

  m_instanceExtensions = CheckExtensions(extensionsProperties, m_enableDiagnostics,
                                         kInstanceExtensions, ARRAY_SIZE(kInstanceExtensions));

  for (auto ext : m_instanceExtensions)
  {
    if (strcmp(ext, kValidationFeaturesExtension) == 0)
      m_validationFeaturesEnabled = true;

    LOG(LINFO, ("Vulkan instance extension prepared", ext));
  }

  if (m_enableDiagnostics && !IsContained(kDebugReportExtension, m_instanceExtensions))
    LOG(LWARNING, ("Vulkan diagnostics in not available on this device."));
}

uint32_t Layers::GetInstanceLayersCount() const
{
  if (!m_enableDiagnostics)
    return 0;

  return static_cast<uint32_t>(m_instanceLayers.size());
}

char const * const * Layers::GetInstanceLayers() const
{
  if (!m_enableDiagnostics)
    return nullptr;

  return m_instanceLayers.data();
}

uint32_t Layers::GetInstanceExtensionsCount() const
{
  return static_cast<uint32_t>(m_instanceExtensions.size());
}

char const * const * Layers::GetInstanceExtensions() const
{
  return m_instanceExtensions.data();
}

bool Layers::Initialize(VkInstance instance, VkPhysicalDevice physicalDevice)
{
  if (m_enableDiagnostics)
  {
    // Get device layers count.
    uint32_t devLayerCount = 0;
    auto statusCode = vkEnumerateDeviceLayerProperties(physicalDevice, &devLayerCount,
                                                       nullptr);
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkEnumerateDeviceLayerProperties, statusCode);
      return false;
    }

    // Enumerate device layers.
    std::vector<VkLayerProperties> layerProperties;
    if (devLayerCount != 0)
    {
      layerProperties.resize(devLayerCount);
      statusCode = vkEnumerateDeviceLayerProperties(physicalDevice, &devLayerCount,
                                                    layerProperties.data());
      if (statusCode != VK_SUCCESS)
      {
        LOG_ERROR_VK_CALL(vkEnumerateDeviceLayerProperties, statusCode);
        return false;
      }
      m_deviceLayers = CheckLayers(layerProperties);

      for (auto layer : m_deviceLayers)
        LOG(LDEBUG, ("Vulkan device layer prepared", layer));
    }
  }

  // Get device extensions count.
  uint32_t devExtensionsCount = 0;
  auto statusCode = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                                         &devExtensionsCount, nullptr);
  if (statusCode != VK_SUCCESS)
  {
    LOG_ERROR_VK_CALL(vkEnumerateDeviceExtensionProperties, statusCode);
    return false;
  }

  // Enumerate device extensions.
  std::vector<VkExtensionProperties> extensionsProperties;
  if (devExtensionsCount != 0)
  {
    extensionsProperties.resize(devExtensionsCount);
    statusCode = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                                      &devExtensionsCount,
                                                      extensionsProperties.data());
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkEnumerateDeviceExtensionProperties, statusCode);
      return false;
    }
  }

  // Enumerate device extensions for each layer.
  for (auto layerName : m_deviceLayers)
  {
    uint32_t cnt = 0;
    statusCode = vkEnumerateDeviceExtensionProperties(physicalDevice, layerName,
                                                      &cnt, nullptr);
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkEnumerateDeviceExtensionProperties, statusCode);
      return false;
    }
    if (cnt == 0)
      continue;

    std::vector<VkExtensionProperties> props(cnt);
    statusCode = vkEnumerateDeviceExtensionProperties(physicalDevice, layerName,
                                                      &cnt, props.data());
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkEnumerateDeviceExtensionProperties, statusCode);
      return false;
    }
    extensionsProperties.insert(extensionsProperties.end(), props.begin(), props.end());
  }

  m_deviceExtensions = CheckExtensions(extensionsProperties, m_enableDiagnostics,
                                       kDeviceExtensions, ARRAY_SIZE(kDeviceExtensions));
  for (auto ext : m_deviceExtensions)
    LOG(LINFO, ("Vulkan device extension prepared", ext));

  if (m_enableDiagnostics && IsContained(kDebugReportExtension, m_instanceExtensions))
  {
    if (m_vkCreateDebugReportCallbackEXT == nullptr)
    {
      m_vkCreateDebugReportCallbackEXT =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,
        "vkCreateDebugReportCallbackEXT");
      m_vkDestroyDebugReportCallbackEXT =
        (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,
        "vkDestroyDebugReportCallbackEXT");
      m_vkDebugReportMessageEXT =
        (PFN_vkDebugReportMessageEXT)vkGetInstanceProcAddr(instance,
        "vkDebugReportMessageEXT");
    }

    if (m_vkCreateDebugReportCallbackEXT == nullptr)
    {
      LOG_ERROR_VK("Function vkCreateDebugReportCallbackEXT is not found.");
      return false;
    }

    VkDebugReportCallbackCreateInfoEXT dbgInfo = {};
    dbgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    dbgInfo.pNext = nullptr;
    dbgInfo.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
                    VK_DEBUG_REPORT_WARNING_BIT_EXT |
                    VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
                    VK_DEBUG_REPORT_ERROR_BIT_EXT |
                    VK_DEBUG_REPORT_DEBUG_BIT_EXT;
    dbgInfo.pfnCallback = DebugReportCallbackImpl;
    dbgInfo.pUserData = nullptr;
    statusCode = m_vkCreateDebugReportCallbackEXT(instance, &dbgInfo, nullptr, &m_reportCallback);
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkCreateDebugReportCallbackEXT, statusCode);
      return false;
    }
  }

  return true;
}

void Layers::Uninitialize(VkInstance instance)
{
  if (m_reportCallback != 0 && m_vkDestroyDebugReportCallbackEXT != nullptr)
    m_vkDestroyDebugReportCallbackEXT(instance, m_reportCallback, nullptr);
}

uint32_t Layers::GetDeviceLayersCount() const
{
  if (!m_enableDiagnostics)
    return 0;

  return static_cast<uint32_t>(m_deviceLayers.size());
}

char const * const * Layers::GetDeviceLayers() const
{
  if (!m_enableDiagnostics)
    return nullptr;

  return m_deviceLayers.data();
}

uint32_t Layers::GetDeviceExtensionsCount() const
{
  return static_cast<uint32_t>(m_deviceExtensions.size());
}

char const * const * Layers::GetDeviceExtensions() const
{
  return m_deviceExtensions.data();
}

bool Layers::IsValidationFeaturesEnabled() const 
{
  return m_validationFeaturesEnabled; 
}
}  // namespace vulkan
}  // namespace dp
