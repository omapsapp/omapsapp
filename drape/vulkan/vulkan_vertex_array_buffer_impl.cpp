#include "drape/pointers.hpp"
#include "drape/vertex_array_buffer.hpp"
#include "drape/vulkan/vulkan_base_context.hpp"
#include "drape/vulkan/vulkan_gpu_buffer_impl.hpp"
#include "drape/vulkan/vulkan_param_descriptor.hpp"
#include "drape/vulkan/vulkan_utils.hpp"

#include "base/assert.hpp"
#include "base/macros.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace dp
{
namespace vulkan
{
class VulkanVertexArrayBufferImpl : public VertexArrayBufferImpl
{
public:
  VulkanVertexArrayBufferImpl(ref_ptr<VertexArrayBuffer> buffer,
                              ref_ptr<VulkanObjectManager> objectManager,
                              BindingInfoArray && bindingInfo,
                              uint8_t bindingInfoCount)
    : m_vertexArrayBuffer(std::move(buffer))
    , m_objectManager(objectManager)
    , m_bindingInfo(std::move(bindingInfo))
    , m_bindingInfoCount(bindingInfoCount)
    , m_descriptorUpdater(objectManager)
  {}

  ~VulkanVertexArrayBufferImpl() override
  {
    m_descriptorUpdater.Destroy();
  }

  bool Build(ref_ptr<GpuProgram> program) override
  {
    UNUSED_VALUE(program);
    return true;
  }
  
  bool Bind() override { return true; }
  void Unbind() override {}
  void BindBuffers(dp::BuffersMap const &) const override {}

  void RenderRange(ref_ptr<GraphicsContext> context, bool drawAsLine,
                   IndicesRange const & range) override
  {
    CHECK(m_vertexArrayBuffer->HasBuffers(), ());

    ref_ptr<dp::vulkan::VulkanBaseContext> vulkanContext = context;
    VkCommandBuffer commandBuffer = vulkanContext->GetCurrentRenderingCommandBuffer();
    CHECK(commandBuffer != nullptr, ());

    vulkanContext->SetPrimitiveTopology(drawAsLine ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST :
                                                     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    vulkanContext->SetBindingInfo(m_bindingInfo, m_bindingInfoCount);

    m_descriptorUpdater.Update(context);
    auto descriptorSet = m_descriptorUpdater.GetDescriptorSet();

    uint32_t dynamicOffset = vulkanContext->GetCurrentDynamicBufferOffset();
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vulkanContext->GetCurrentPipelineLayout(), 0, 1,
                            &descriptorSet, 1, &dynamicOffset);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      vulkanContext->GetCurrentPipeline());

    size_t constexpr kMaxBuffersCount = 4;
    std::array<VkBuffer, kMaxBuffersCount> buffers = {};
    std::array<VkDeviceSize, kMaxBuffersCount> offsets = {};

    uint32_t bufferIndex = 0;
    for (auto & buffer : m_vertexArrayBuffer->m_staticBuffers)
    {
      ref_ptr<VulkanGpuBufferImpl> b = buffer.second->GetBuffer();
      CHECK_LESS(bufferIndex, kMaxBuffersCount, ());
      buffers[bufferIndex++] = b->GetVulkanBuffer();
    }
    for (auto & buffer : m_vertexArrayBuffer->m_dynamicBuffers)
    {
      ref_ptr<VulkanGpuBufferImpl> b = buffer.second->GetBuffer();
      CHECK_LESS(bufferIndex, kMaxBuffersCount, ());
      buffers[bufferIndex++] = b->GetVulkanBuffer();
    }
    vkCmdBindVertexBuffers(commandBuffer, 0, bufferIndex, buffers.data(), offsets.data());

    ref_ptr<VulkanGpuBufferImpl> ib = m_vertexArrayBuffer->m_indexBuffer->GetBuffer();
    VkBuffer vulkanIndexBuffer = ib->GetVulkanBuffer();
    auto const indexType = dp::IndexStorage::IsSupported32bit() ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
    vkCmdBindIndexBuffer(commandBuffer, vulkanIndexBuffer, 0, indexType);

    CHECK_LESS_OR_EQUAL(range.m_idxStart + range.m_idxCount,
                        m_objectManager->GetMemoryManager().GetDeviceLimits().maxDrawIndexedIndexValue, ());

    vkCmdDrawIndexed(commandBuffer, range.m_idxCount, 1, range.m_idxStart, 0, 0);
  }
  
private:
  ref_ptr<VertexArrayBuffer> m_vertexArrayBuffer;
  ref_ptr<VulkanObjectManager> m_objectManager;
  BindingInfoArray m_bindingInfo;
  uint8_t m_bindingInfoCount = 0;
  ParamDescriptorUpdater m_descriptorUpdater;
};
}  // namespace vulkan
  
drape_ptr<VertexArrayBufferImpl> VertexArrayBuffer::CreateImplForVulkan(ref_ptr<GraphicsContext> context,
                                                                        ref_ptr<VertexArrayBuffer> buffer,
                                                                        BindingInfoArray && bindingInfo,
                                                                        uint8_t bindingInfoCount)
{
  ref_ptr<dp::vulkan::VulkanBaseContext> vulkanContext = context;
  return make_unique_dp<vulkan::VulkanVertexArrayBufferImpl>(buffer, vulkanContext->GetObjectManager(),
                                                             std::move(bindingInfo), bindingInfoCount);
}
}  // namespace dp
