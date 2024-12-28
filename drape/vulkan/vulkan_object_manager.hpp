#pragma once

#include "drape/pointers.hpp"
#include "drape/vulkan/vulkan_gpu_program.hpp"
#include "drape/vulkan/vulkan_memory_manager.hpp"
#include "drape/vulkan/vulkan_param_descriptor.hpp"
#include "drape/vulkan/vulkan_utils.hpp"

#include "base/assert.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace dp
{
namespace vulkan
{
struct VulkanObject
{
  VkBuffer m_buffer = {};
  VkImage m_image = {};
  VkImageView m_imageView = {};
  VulkanMemoryManager::AllocationPtr m_allocation;

  VkDeviceMemory GetMemory() const
  {
    ASSERT(m_allocation != nullptr, ());
    ASSERT(m_allocation->m_memoryBlock != nullptr, ());
    return m_allocation->m_memoryBlock->m_memory;
  }

  uint32_t GetAlignedOffset() const
  {
    ASSERT(m_allocation != nullptr, ());
    return m_allocation->m_alignedOffset;
  }

  uint32_t GetAlignedSize() const
  {
    ASSERT(m_allocation != nullptr, ());
    return m_allocation->m_alignedSize;
  }
};

class VulkanStagingBuffer;

class VulkanObjectManager
{
public:
  VulkanObjectManager(VkDevice device, VkPhysicalDeviceLimits const & deviceLimits,
                      VkPhysicalDeviceMemoryProperties const & memoryProperties,
                      uint32_t queueFamilyIndex);
  ~VulkanObjectManager();

  enum ThreadType
  {
    Frontend = 0,
    Backend,
    Other,
    Count
  };
  void RegisterThread(ThreadType type);

  void SetCurrentInflightFrameIndex(uint32_t index);

  VulkanObject CreateBuffer(VulkanMemoryManager::ResourceType resourceType,
                            uint32_t sizeInBytes, uint64_t batcherHash);
  VulkanObject CreateImage(VkImageUsageFlags usageFlags, VkFormat format, VkImageTiling tiling,
                           VkImageAspectFlags aspectFlags, uint32_t width, uint32_t height);
  DescriptorSetGroup CreateDescriptorSetGroup(ref_ptr<VulkanGpuProgram> program);

  // Use unsafe function ONLY if an object exists on the only thread, otherwise
  // use safe Fill function.
  uint8_t * MapUnsafe(VulkanObject object);
  void FlushUnsafe(VulkanObject object, uint32_t offset = 0, uint32_t size = 0);
  void UnmapUnsafe(VulkanObject object);
  void Fill(VulkanObject object, void const * data, uint32_t sizeInBytes);

  void DestroyObject(VulkanObject object);
  void DestroyDescriptorSetGroup(DescriptorSetGroup group);
  void CollectDescriptorSetGroups(uint32_t inflightFrameIndex);
  void CollectObjects(uint32_t inflightFrameIndex);

  // Use unsafe function ONLY if an object has been just created.
  void DestroyObjectUnsafe(VulkanObject object);

  VkDevice GetDevice() const { return m_device; }
  VulkanMemoryManager const & GetMemoryManager() const { return m_memoryManager; };
  VkSampler GetSampler(SamplerKey const & key);

  void SetMaxUniformBuffers(uint32_t maxUniformBuffers);
  void SetMaxImageSamplers(uint32_t maxImageSamplers);

private:
  using DescriptorSetGroupArray = std::vector<DescriptorSetGroup>;
  using VulkanObjectArray = std::vector<VulkanObject>;

  void CreateDescriptorPool();
  void DestroyDescriptorPools();
  void CollectObjectsForThread(VulkanObjectArray & objects);
  void CollectObjectsImpl(VulkanObjectArray const & objects);
  void CollectDescriptorSetGroupsUnsafe(DescriptorSetGroupArray & descriptors);

  VkDevice const m_device;
  uint32_t const m_queueFamilyIndex;
  VulkanMemoryManager m_memoryManager;

  std::array<std::thread::id, ThreadType::Count> m_renderers = {};
  std::array<std::array<VulkanObjectArray, kMaxInflightFrames>, ThreadType::Count> m_queuesToDestroy = {};

  struct DescriptorPool
  {
    VkDescriptorPool m_pool;
    uint32_t m_availableSetsCount = 0;
  };
  std::vector<DescriptorPool> m_descriptorPools;

  std::array<DescriptorSetGroupArray, kMaxInflightFrames> m_descriptorsToDestroy;

  std::map<SamplerKey, VkSampler> m_samplers;

  uint32_t m_currentInflightFrameIndex = 0;

  uint32_t m_maxUniformBuffers = 0;
  uint32_t m_maxImageSamplers = 0;

  std::mutex m_mutex;
  std::mutex m_samplerMutex;
  std::mutex m_destroyMutex;

#ifdef ENABLE_TRACE
  int64_t m_buffersCount = 0;
  int64_t m_imagesCount = 0;
#endif
};
}  // namespace vulkan
}  // namespace dp
