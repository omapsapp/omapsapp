#include "drape/vulkan/vulkan_object_manager.hpp"

#include "drape/drape_routine.hpp"

#include "platform/trace.hpp"

#include <algorithm>
#include <cstring>

namespace dp
{
namespace vulkan
{
namespace
{
size_t constexpr kBackendQueueIndex = 0;
size_t constexpr kOtherQueueIndex = 0;

// Maximum descriptors sets count in the pool.
uint32_t constexpr kMaxDescriptorsSetCount = 256 * kMaxInflightFrames;

VkSamplerAddressMode GetVulkanSamplerAddressMode(TextureWrapping wrapping)
{
  switch (wrapping)
  {
  case TextureWrapping::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case TextureWrapping::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
  UNREACHABLE();
}

VkFilter GetVulkanFilter(TextureFilter filter)
{
  switch (filter)
  {
  case TextureFilter::Linear: return VK_FILTER_LINEAR;
  case TextureFilter::Nearest: return VK_FILTER_NEAREST;
  }
  UNREACHABLE();
}
}  // namespace

VulkanObjectManager::VulkanObjectManager(VkDevice device, VkPhysicalDeviceLimits const & deviceLimits,
                                         VkPhysicalDeviceMemoryProperties const & memoryProperties,
                                         uint32_t queueFamilyIndex)
  : m_device(device)
  , m_queueFamilyIndex(queueFamilyIndex)
  , m_memoryManager(device, deviceLimits, memoryProperties)
{
  size_t constexpr kAvgDestroyCount = 50;

  for (auto & q : m_queuesToDestroy[ThreadType::Frontend])
    q.reserve(kAvgDestroyCount);

  for (auto & descriptorsToDestroy : m_descriptorsToDestroy)
    descriptorsToDestroy.reserve(kAvgDestroyCount);
}

VulkanObjectManager::~VulkanObjectManager()
{
  for (auto & descriptorsToDestroy : m_descriptorsToDestroy)
    CollectDescriptorSetGroupsUnsafe(descriptorsToDestroy);

  for (size_t i = 0; i < ThreadType::Count; ++i)
  {
    for (auto & q : m_queuesToDestroy[i])
      CollectObjectsImpl(q);
  }

  for (auto const & s : m_samplers)
    vkDestroySampler(m_device, s.second, nullptr);
  m_samplers.clear();

  DestroyDescriptorPools();
}

void VulkanObjectManager::RegisterThread(ThreadType type)
{
  m_renderers[type] = std::this_thread::get_id();
}

void VulkanObjectManager::SetCurrentInflightFrameIndex(uint32_t index)
{
  CHECK(std::this_thread::get_id() == m_renderers[ThreadType::Frontend], ());
  CHECK_LESS(m_currentInflightFrameIndex, kMaxInflightFrames, ());
  m_currentInflightFrameIndex = index;
}

VulkanObject VulkanObjectManager::CreateBuffer(VulkanMemoryManager::ResourceType resourceType,
                                               uint32_t sizeInBytes, uint64_t batcherHash)
{
  VulkanObject result;
  VkBufferCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = 0;
  info.size = sizeInBytes;
  if (resourceType == VulkanMemoryManager::ResourceType::Geometry)
  {
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  else if (resourceType == VulkanMemoryManager::ResourceType::Uniform)
  {
    info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }
  else if (resourceType == VulkanMemoryManager::ResourceType::Staging)
  {
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }
  else
  {
    CHECK(false, ("Unsupported resource type."));
  }

  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.queueFamilyIndexCount = 1;
  info.pQueueFamilyIndices = &m_queueFamilyIndex;
  CHECK_VK_CALL(vkCreateBuffer(m_device, &info, nullptr, &result.m_buffer));
  
  SET_DEBUG_NAME_VK(VK_OBJECT_TYPE_BUFFER, result.m_buffer, 
    ((resourceType == VulkanMemoryManager::ResourceType::Geometry ? "B: Geometry (" :
    (resourceType == VulkanMemoryManager::ResourceType::Uniform ? "B: Uniform (" : 
     "B: Staging (")) + std::to_string(sizeInBytes) + " bytes)").c_str());

  VkMemoryRequirements memReqs = {};
  vkGetBufferMemoryRequirements(m_device, result.m_buffer, &memReqs);

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    result.m_allocation = m_memoryManager.Allocate(resourceType, memReqs, batcherHash);
    CHECK_VK_CALL(vkBindBufferMemory(m_device, result.m_buffer, result.GetMemory(),
                                     result.GetAlignedOffset()));
  }

#ifdef ENABLE_TRACE
  m_buffersCount++;
  TRACE_COUNTER("[drape][vulkan] Buffers", m_buffersCount);
#endif

  return result;
}

VulkanObject VulkanObjectManager::CreateImage(VkImageUsageFlags usageFlags, VkFormat format, VkImageTiling tiling,
                                              VkImageAspectFlags aspectFlags, uint32_t width, uint32_t height)
{
  VulkanObject result;
  VkImageCreateInfo imageCreateInfo = {};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.pNext = nullptr;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.format = format;
  imageCreateInfo.mipLevels = 1;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.tiling = tiling;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCreateInfo.extent = { width, height, 1 };
  imageCreateInfo.usage = usageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  CHECK_VK_CALL(vkCreateImage(m_device, &imageCreateInfo, nullptr, &result.m_image));

  VkMemoryRequirements memReqs = {};
  vkGetImageMemoryRequirements(m_device, result.m_image, &memReqs);

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    result.m_allocation = m_memoryManager.Allocate(VulkanMemoryManager::ResourceType::Image,
                                                   memReqs, 0 /* blockHash */);
    CHECK_VK_CALL(vkBindImageMemory(m_device, result.m_image,
                                    result.GetMemory(), result.GetAlignedOffset()));
  }

  VkImageViewCreateInfo viewCreateInfo = {};
  viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewCreateInfo.pNext = nullptr;
  viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewCreateInfo.format = format;
  if (usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
  {
    viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                 VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
  }
  else
  {
    viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                 VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
  }
  viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
  viewCreateInfo.subresourceRange.baseMipLevel = 0;
  viewCreateInfo.subresourceRange.levelCount = 1;
  viewCreateInfo.subresourceRange.baseArrayLayer = 0;
  viewCreateInfo.subresourceRange.layerCount = 1;
  viewCreateInfo.image = result.m_image;
  CHECK_VK_CALL(vkCreateImageView(m_device, &viewCreateInfo, nullptr, &result.m_imageView));

#ifdef ENABLE_TRACE
  m_imagesCount++;
  TRACE_COUNTER("[drape][vulkan] Images", m_imagesCount);
#endif

  return result;
}

DescriptorSetGroup VulkanObjectManager::CreateDescriptorSetGroup(ref_ptr<VulkanGpuProgram> program)
{
  CHECK(std::this_thread::get_id() == m_renderers[ThreadType::Frontend], ());

  DescriptorSetGroup s;
  VkDescriptorSetLayout layout = program->GetDescriptorSetLayout();
  VkDescriptorSetAllocateInfo allocInfo = {};

  // Find a pool with available sets.
  uint32_t poolIndex = 0;
  while (poolIndex < m_descriptorPools.size() && m_descriptorPools[poolIndex].m_availableSetsCount == 0)
    ++poolIndex;

  // No such a pool, create one.
  if (poolIndex == m_descriptorPools.size())
  {
    CreateDescriptorPool();
    poolIndex = m_descriptorPools.size() - 1;
  }

  // Allocate a descriptor set.
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_descriptorPools[poolIndex].m_pool;
  s.m_descriptorPoolIndex = poolIndex;
  allocInfo.pSetLayouts = &layout;
  allocInfo.descriptorSetCount = 1;

  // Decrease the available sets count.
  m_descriptorPools[poolIndex].m_availableSetsCount--;

  TRACE_COUNTER("[drape][vulkan] Descriptor pools", static_cast<int64_t>(m_descriptorPools.size()));
#ifdef ENABLE_TRACE
  int64_t usedDescriptorsSets = 0;
  for (auto const & pool : m_descriptorPools)
    usedDescriptorsSets += (kMaxDescriptorsSetCount - pool.m_availableSetsCount);
  TRACE_COUNTER("[drape][vulkan] Descriptor sets", usedDescriptorsSets);
#endif

  CHECK_VK_CALL(vkAllocateDescriptorSets(m_device, &allocInfo, &s.m_descriptorSet));
  return s;
}

void VulkanObjectManager::DestroyObject(VulkanObject object)
{
  auto const currentThreadId = std::this_thread::get_id();
  if (currentThreadId == m_renderers[ThreadType::Frontend])
  {
    m_queuesToDestroy[ThreadType::Frontend][m_currentInflightFrameIndex].push_back(std::move(object));
  }
  else if (currentThreadId == m_renderers[ThreadType::Backend])
  {
    m_queuesToDestroy[ThreadType::Backend][kBackendQueueIndex].push_back(std::move(object));
  }
  else
  {
    std::lock_guard<std::mutex> lock(m_destroyMutex);
    m_queuesToDestroy[ThreadType::Other][kOtherQueueIndex].push_back(std::move(object));
  }
}

void VulkanObjectManager::DestroyDescriptorSetGroup(DescriptorSetGroup group)
{
  CHECK(std::this_thread::get_id() == m_renderers[ThreadType::Frontend], ());
  m_descriptorsToDestroy[m_currentInflightFrameIndex].push_back(std::move(group));
}

void VulkanObjectManager::CollectDescriptorSetGroups(uint32_t inflightFrameIndex)
{
  CHECK(std::this_thread::get_id() == m_renderers[ThreadType::Frontend], ());
  CollectDescriptorSetGroupsUnsafe(m_descriptorsToDestroy[inflightFrameIndex]);
}

void VulkanObjectManager::CollectDescriptorSetGroupsUnsafe(DescriptorSetGroupArray & descriptors)
{
  for (auto const & d : descriptors)
  {
    CHECK_LESS(d.m_descriptorPoolIndex, m_descriptorPools.size(), ());
    CHECK_VK_CALL(vkFreeDescriptorSets(m_device, m_descriptorPools[d.m_descriptorPoolIndex].m_pool,
                                       1 /* count */, &d.m_descriptorSet));
    m_descriptorPools[d.m_descriptorPoolIndex].m_availableSetsCount++;
  }
  descriptors.clear();
}

void VulkanObjectManager::CollectObjects(uint32_t inflightFrameIndex)
{
  auto const currentThreadId = std::this_thread::get_id();
  if (currentThreadId == m_renderers[ThreadType::Frontend])
  {
    CollectObjectsForThread(m_queuesToDestroy[ThreadType::Frontend][inflightFrameIndex]);
  }
  else if (currentThreadId == m_renderers[ThreadType::Backend])
  {
    CollectObjectsForThread(m_queuesToDestroy[ThreadType::Backend][kBackendQueueIndex]);

    std::lock_guard<std::mutex> lock(m_destroyMutex);
    CollectObjectsForThread(m_queuesToDestroy[ThreadType::Other][kOtherQueueIndex]);
  }
}

void VulkanObjectManager::CollectObjectsForThread(VulkanObjectArray & objects)
{
  if (objects.empty())
    return;

  std::vector<VulkanObject> queueToDestroy;
  std::swap(objects, queueToDestroy);
  DrapeRoutine::Run([this, queueToDestroy = std::move(queueToDestroy)]()
  {
    CollectObjectsImpl(queueToDestroy);
  });
}

void VulkanObjectManager::CollectObjectsImpl(VulkanObjectArray const & objects)
{
  for (auto const & obj : objects)
  {
    if (obj.m_buffer != VK_NULL_HANDLE)
    {
      vkDestroyBuffer(m_device, obj.m_buffer, nullptr);
#ifdef ENABLE_TRACE
      m_buffersCount--;
      TRACE_COUNTER("[drape][vulkan] Buffers", m_buffersCount);
#endif
    }
    if (obj.m_imageView != VK_NULL_HANDLE)
    {
      vkDestroyImageView(m_device, obj.m_imageView, nullptr);
    }
    if (obj.m_image != VK_NULL_HANDLE)
    {
      vkDestroyImage(m_device, obj.m_image, nullptr);
#ifdef ENABLE_TRACE
      m_imagesCount--;
      TRACE_COUNTER("[drape][vulkan] Images", m_imagesCount);
#endif
    }
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  m_memoryManager.BeginDeallocationSession();
  for (auto const & obj : objects)
  {
    if (obj.m_allocation)
      m_memoryManager.Deallocate(obj.m_allocation);
  }
  m_memoryManager.EndDeallocationSession();
}

void VulkanObjectManager::DestroyObjectUnsafe(VulkanObject object)
{
  CollectObjectsImpl(VulkanObjectArray{object});
}

void VulkanObjectManager::SetMaxUniformBuffers(uint32_t maxUniformBuffers)
{
  m_maxUniformBuffers = maxUniformBuffers;
}

void VulkanObjectManager::SetMaxImageSamplers(uint32_t maxImageSamplers)
{
  m_maxImageSamplers = maxImageSamplers;
}

uint8_t * VulkanObjectManager::MapUnsafe(VulkanObject object)
{
  CHECK(!object.m_allocation->m_memoryBlock->m_isBlocked, ());

  CHECK(object.m_buffer != VK_NULL_HANDLE || object.m_image != VK_NULL_HANDLE, ());
  uint8_t * ptr = nullptr;
  CHECK_VK_CALL(vkMapMemory(m_device, object.GetMemory(), object.GetAlignedOffset(),
                            object.GetAlignedSize(), 0, reinterpret_cast<void **>(&ptr)));
  object.m_allocation->m_memoryBlock->m_isBlocked = true;
  return ptr;
}

void VulkanObjectManager::FlushUnsafe(VulkanObject object, uint32_t offset, uint32_t size)
{
  if (object.m_allocation->m_memoryBlock->m_isCoherent)
    return;

  CHECK(object.m_allocation->m_memoryBlock->m_isBlocked, ());

  VkMappedMemoryRange mappedRange = {};
  mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  mappedRange.memory = object.GetMemory();
  mappedRange.offset = object.GetAlignedOffset() + offset;
  if (size == 0)
    mappedRange.size = object.GetAlignedSize();
  else
    mappedRange.size = size;
  CHECK_VK_CALL(vkFlushMappedMemoryRanges(m_device, 1, &mappedRange));
}

void VulkanObjectManager::UnmapUnsafe(VulkanObject object)
{
  CHECK(object.m_allocation->m_memoryBlock->m_isBlocked, ());
  vkUnmapMemory(m_device, object.GetMemory());
  object.m_allocation->m_memoryBlock->m_isBlocked = false;
}

void VulkanObjectManager::Fill(VulkanObject object, void const * data, uint32_t sizeInBytes)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  void * gpuPtr = MapUnsafe(object);
  if (data != nullptr)
    memcpy(gpuPtr, data, sizeInBytes);
  else
    memset(gpuPtr, 0, sizeInBytes);
  FlushUnsafe(object);
  UnmapUnsafe(object);
}

void VulkanObjectManager::CreateDescriptorPool()
{
  CHECK_GREATER(m_maxUniformBuffers, 0, ());
  CHECK_GREATER(m_maxImageSamplers, 0, ());
  std::vector<VkDescriptorPoolSize> poolSizes =
  {
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_maxUniformBuffers * kMaxDescriptorsSetCount},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_maxImageSamplers * kMaxDescriptorsSetCount},
  };

  VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  descriptorPoolInfo.pPoolSizes = poolSizes.data();
  descriptorPoolInfo.maxSets = kMaxDescriptorsSetCount;

  DescriptorPool descriptorPool;
  CHECK_VK_CALL(vkCreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &descriptorPool.m_pool));
  descriptorPool.m_availableSetsCount = descriptorPoolInfo.maxSets;

  m_descriptorPools.push_back(descriptorPool);
}

void VulkanObjectManager::DestroyDescriptorPools()
{
  for (auto & pool : m_descriptorPools)
    vkDestroyDescriptorPool(m_device, pool.m_pool, nullptr);
}

VkSampler VulkanObjectManager::GetSampler(SamplerKey const & key)
{
  std::lock_guard<std::mutex> lock(m_samplerMutex);

  auto const it = m_samplers.find(key);
  if (it != m_samplers.end())
    return it->second;

  VkSamplerCreateInfo samplerCreateInfo = {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = samplerCreateInfo.minFilter = GetVulkanFilter(key.GetTextureFilter());
  samplerCreateInfo.addressModeU = GetVulkanSamplerAddressMode(key.GetWrapSMode());
  samplerCreateInfo.addressModeV = GetVulkanSamplerAddressMode(key.GetWrapTMode());

  VkSampler sampler;
  CHECK_VK_CALL(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &sampler));

  m_samplers.insert(std::make_pair(key, sampler));
  return sampler;
}
}  // namespace vulkan
}  // namespace dp
