#include "drape/mesh_object.hpp"
#include "drape/pointers.hpp"
#include "drape/vulkan/vulkan_base_context.hpp"
#include "drape/vulkan/vulkan_staging_buffer.hpp"
#include "drape/vulkan/vulkan_param_descriptor.hpp"
#include "drape/vulkan/vulkan_utils.hpp"

#include "base/assert.hpp"

#include <cstdint>
#include <vector>

namespace dp
{
namespace vulkan
{
namespace
{
VkPrimitiveTopology GetPrimitiveType(MeshObject::DrawPrimitive primitive)
{
  switch (primitive)
  {
    case MeshObject::DrawPrimitive::Triangles: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case MeshObject::DrawPrimitive::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case MeshObject::DrawPrimitive::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  }
  UNREACHABLE();
}
}  // namespace

class VulkanMeshObjectImpl : public MeshObjectImpl
{
public:
  VulkanMeshObjectImpl(ref_ptr<VulkanObjectManager> objectManager, ref_ptr<dp::MeshObject> mesh)
    : m_mesh(std::move(mesh))
    , m_objectManager(objectManager)
    , m_descriptorUpdater(objectManager)
  {}

  void Build(ref_ptr<dp::GraphicsContext>, ref_ptr<dp::GpuProgram>) override
  {
    m_geometryBuffers.resize(m_mesh->m_buffers.size());
    m_bindingInfoCount = static_cast<uint8_t>(m_mesh->m_buffers.size());
    CHECK_LESS_OR_EQUAL(m_bindingInfoCount, kMaxBindingInfo, ());
    for (size_t i = 0; i < m_mesh->m_buffers.size(); i++)
    {
      if (m_mesh->m_buffers[i].m_data.empty())
        continue;

      auto const sizeInBytes = static_cast<uint32_t>(m_mesh->m_buffers[i].m_data.size() *
                                                     sizeof(m_mesh->m_buffers[i].m_data[0]));
      m_geometryBuffers[i] = m_objectManager->CreateBuffer(VulkanMemoryManager::ResourceType::Geometry,
                                                           sizeInBytes, 0 /* batcherHash */);
      m_objectManager->Fill(m_geometryBuffers[i], m_mesh->m_buffers[i].m_data.data(), sizeInBytes);

      m_bindingInfo[i] = dp::BindingInfo(static_cast<uint8_t>(m_mesh->m_buffers[i].m_attributes.size()),
                                         static_cast<uint8_t>(i));
      for (size_t j = 0; j < m_mesh->m_buffers[i].m_attributes.size(); ++j)
      {
        auto const & attr = m_mesh->m_buffers[i].m_attributes[j];
        auto & binding = m_bindingInfo[i].GetBindingDecl(static_cast<uint16_t>(j));
        binding.m_attributeName = attr.m_attributeName;
        binding.m_componentCount = static_cast<uint8_t>(attr.m_componentsCount);
        binding.m_componentType = gl_const::GLFloatType;
        binding.m_offset = static_cast<uint8_t>(attr.m_offset);
        binding.m_stride = static_cast<uint8_t>(m_mesh->m_buffers[i].m_stride);
      }
    }
  }

  void Reset() override
  {
    m_descriptorUpdater.Destroy();
    for (auto const & b : m_geometryBuffers)
      m_objectManager->DestroyObject(b);
    m_geometryBuffers.clear();
  }

  void UpdateBuffer(ref_ptr<dp::GraphicsContext> context, uint32_t bufferInd) override
  {
    CHECK_LESS(bufferInd, static_cast<uint32_t>(m_geometryBuffers.size()), ());

    ref_ptr<dp::vulkan::VulkanBaseContext> vulkanContext = context;
    VkCommandBuffer commandBuffer = vulkanContext->GetCurrentMemoryCommandBuffer();
    CHECK(commandBuffer != nullptr, ());

    auto & buffer = m_mesh->m_buffers[bufferInd];
    CHECK(!buffer.m_data.empty(), ());

    // Copy to default or temporary staging buffer.
    auto const sizeInBytes = static_cast<uint32_t>(buffer.m_data.size() * sizeof(buffer.m_data[0]));
    auto stagingBuffer = vulkanContext->GetDefaultStagingBuffer();
    if (stagingBuffer->HasEnoughSpace(sizeInBytes))
    {
      auto staging = stagingBuffer->Reserve(sizeInBytes);
      memcpy(staging.m_pointer, buffer.m_data.data(), sizeInBytes);

      // Schedule command to copy from the staging buffer to our geometry buffer.
      VkBufferCopy copyRegion = {};
      copyRegion.dstOffset = 0;
      copyRegion.srcOffset = staging.m_offset;
      copyRegion.size = sizeInBytes;
      vkCmdCopyBuffer(commandBuffer, staging.m_stagingBuffer, m_geometryBuffers[bufferInd].m_buffer,
                      1, &copyRegion);
    }
    else
    {
      // Here we use temporary staging object, which will be destroyed after the nearest
      // command queue submitting.
      VulkanStagingBuffer tempStagingBuffer(m_objectManager, sizeInBytes);
      CHECK(tempStagingBuffer.HasEnoughSpace(sizeInBytes), ());
      auto staging = tempStagingBuffer.Reserve(sizeInBytes);
      memcpy(staging.m_pointer, buffer.m_data.data(), sizeInBytes);
      tempStagingBuffer.Flush();

      // Schedule command to copy from the staging buffer to our geometry buffer.
      VkBufferCopy copyRegion = {};
      copyRegion.dstOffset = 0;
      copyRegion.srcOffset = staging.m_offset;
      copyRegion.size = sizeInBytes;
      vkCmdCopyBuffer(commandBuffer, staging.m_stagingBuffer, m_geometryBuffers[bufferInd].m_buffer,
                      1, &copyRegion);
    }

    // Set up a barrier to prevent data collisions.
    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = m_geometryBuffers[bufferInd].m_buffer;
    barrier.offset = 0;
    barrier.size = sizeInBytes;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr,
                         1, &barrier, 0, nullptr);
  }

  void DrawPrimitives(ref_ptr<dp::GraphicsContext> context, uint32_t verticesCount) override
  {
    ref_ptr<dp::vulkan::VulkanBaseContext> vulkanContext = context;
    VkCommandBuffer commandBuffer = vulkanContext->GetCurrentRenderingCommandBuffer();
    CHECK(commandBuffer != nullptr, ());

    vulkanContext->SetPrimitiveTopology(GetPrimitiveType(m_mesh->m_drawPrimitive));
    vulkanContext->SetBindingInfo(m_bindingInfo, m_bindingInfoCount);

    m_descriptorUpdater.Update(context);
    auto descriptorSet = m_descriptorUpdater.GetDescriptorSet();

    uint32_t dynamicOffset = vulkanContext->GetCurrentDynamicBufferOffset();
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vulkanContext->GetCurrentPipelineLayout(), 0, 1,
                            &descriptorSet, 1, &dynamicOffset);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      vulkanContext->GetCurrentPipeline());

    VkDeviceSize offsets[1] = {0};
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_geometryBuffers.size()); ++i)
      vkCmdBindVertexBuffers(commandBuffer, i, 1, &m_geometryBuffers[i].m_buffer, offsets);

    vkCmdDraw(commandBuffer, verticesCount, 1, 0, 0);
  }

  void Bind(ref_ptr<dp::GpuProgram>) override {}
  void Unbind() override {}

private:
  ref_ptr<dp::MeshObject> m_mesh;
  ref_ptr<VulkanObjectManager> m_objectManager;
  std::vector<VulkanObject> m_geometryBuffers;
  BindingInfoArray m_bindingInfo;
  uint8_t m_bindingInfoCount = 0;
  ParamDescriptorUpdater m_descriptorUpdater;
};
}  // namespace vulkan

void MeshObject::InitForVulkan(ref_ptr<dp::GraphicsContext> context)
{
  ref_ptr<dp::vulkan::VulkanBaseContext> vulkanContext = context;
  m_impl = make_unique_dp<vulkan::VulkanMeshObjectImpl>(vulkanContext->GetObjectManager(),
                                                        make_ref(this));
}
}  // namespace dp
