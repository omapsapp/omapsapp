#include "drape/mesh_object.hpp"
#include "drape/pointers.hpp"
#include "drape/vulkan/vulkan_base_context.hpp"
#include "drape/vulkan/vulkan_staging_buffer.hpp"
#include "drape/vulkan/vulkan_param_descriptor.hpp"
#include "drape/vulkan/vulkan_utils.hpp"

#include "base/assert.hpp"
#include "base/buffer_vector.hpp"

#include <cstdint>
#include <limits>
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

  void Build(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program) override
  {
    m_geometryBuffers.resize(m_mesh->m_buffers.size());
    m_bindingInfoCount = static_cast<uint8_t>(m_mesh->m_buffers.size());
    CHECK_LESS_OR_EQUAL(m_bindingInfoCount, kMaxBindingInfo, ());
    for (size_t i = 0; i < m_mesh->m_buffers.size(); i++)
    {
      auto const sizeInBytes = m_mesh->m_buffers[i]->GetSizeInBytes();
      if (sizeInBytes == 0)
        continue;

      m_geometryBuffers[i] = m_objectManager->CreateBuffer(VulkanMemoryManager::ResourceType::Geometry,
                                                           sizeInBytes, 0 /* batcherHash */);
      SET_DEBUG_NAME_VK(VK_OBJECT_TYPE_BUFFER, m_geometryBuffers[i].m_buffer, 
                        ("VB: Mesh (" + m_mesh->m_debugName + ") " + std::to_string(i)).c_str());

      m_objectManager->Fill(m_geometryBuffers[i], m_mesh->m_buffers[i]->GetData(), sizeInBytes);

      m_bindingInfo[i] = dp::BindingInfo(static_cast<uint8_t>(m_mesh->m_buffers[i]->m_attributes.size()),
                                         static_cast<uint8_t>(i));
      for (size_t j = 0; j < m_mesh->m_buffers[i]->m_attributes.size(); ++j)
      {
        auto const & attr = m_mesh->m_buffers[i]->m_attributes[j];
        auto & binding = m_bindingInfo[i].GetBindingDecl(static_cast<uint16_t>(j));
        binding.m_attributeName = attr.m_attributeName;
        binding.m_componentCount = static_cast<uint8_t>(attr.m_componentsCount);
        binding.m_componentType = gl_const::GLFloatType;
        binding.m_offset = static_cast<uint8_t>(attr.m_offset);
        CHECK_LESS_OR_EQUAL(m_mesh->m_buffers[i]->GetStrideInBytes(), 
                            static_cast<uint32_t>(std::numeric_limits<uint8_t>::max()), ());
        binding.m_stride = static_cast<uint8_t>(m_mesh->m_buffers[i]->GetStrideInBytes());
      }
    }

    if (!m_mesh->m_indices.empty())
    {
      auto const sizeInBytes = static_cast<uint32_t>(m_mesh->m_indices.size() * sizeof(uint16_t));
      m_indexBuffer = m_objectManager->CreateBuffer(VulkanMemoryManager::ResourceType::Geometry,
                                                    sizeInBytes, 0 /* batcherHash */);
      SET_DEBUG_NAME_VK(VK_OBJECT_TYPE_BUFFER, m_indexBuffer.m_buffer, 
                        ("IB: Mesh (" + m_mesh->m_debugName + ")").c_str());

      m_objectManager->Fill(m_indexBuffer, m_mesh->m_indices.data(), sizeInBytes);
    }
  }

  void Reset() override
  {
    m_descriptorUpdater.Destroy();
    for (auto const & b : m_geometryBuffers)
      m_objectManager->DestroyObject(b);
    m_geometryBuffers.clear();

    if (m_indexBuffer.m_buffer != VK_NULL_HANDLE)
      m_objectManager->DestroyObject(m_indexBuffer);
  }

  void UpdateBuffer(ref_ptr<dp::GraphicsContext> context, uint32_t bufferInd) override
  {
    CHECK_LESS(bufferInd, static_cast<uint32_t>(m_geometryBuffers.size()), ());
    auto & buffer = m_mesh->m_buffers[bufferInd];
    auto const sizeInBytes = buffer->GetSizeInBytes();
    CHECK(sizeInBytes != 0, ());

    UpdateBufferInternal(context, m_geometryBuffers[bufferInd].m_buffer, 
                         VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                         buffer->GetData(), sizeInBytes);
  }

  void UpdateIndexBuffer(ref_ptr<dp::GraphicsContext> context) override
  {
    CHECK(!m_mesh->m_indices.empty(), ());
    auto const sizeInBytes = m_mesh->m_indices.size() * sizeof(uint16_t);
    CHECK(m_indexBuffer.m_buffer != VK_NULL_HANDLE, ());
    
    UpdateBufferInternal(context, m_indexBuffer.m_buffer, 
                         VK_ACCESS_INDEX_READ_BIT,
                         m_mesh->m_indices.data(), sizeInBytes);
  }

  void DrawPrimitives(ref_ptr<dp::GraphicsContext> context, uint32_t vertexCount,
                      uint32_t startVertex) override
  {
    ref_ptr<dp::vulkan::VulkanBaseContext> vulkanContext = context;
    VkCommandBuffer commandBuffer = vulkanContext->GetCurrentRenderingCommandBuffer();
    CHECK(commandBuffer != nullptr, ());

    BindVertexBuffers(context, commandBuffer);

    vkCmdDraw(commandBuffer, vertexCount, 1, startVertex, 0);
  }

  void DrawPrimitivesIndexed(ref_ptr<dp::GraphicsContext> context, uint32_t indexCount, 
                             uint32_t startIndex) override
  {
    ref_ptr<dp::vulkan::VulkanBaseContext> vulkanContext = context;
    VkCommandBuffer commandBuffer = vulkanContext->GetCurrentRenderingCommandBuffer();
    CHECK(commandBuffer != nullptr, ());

    BindVertexBuffers(context, commandBuffer);

    CHECK(m_indexBuffer.m_buffer != VK_NULL_HANDLE, ());
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.m_buffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(commandBuffer, indexCount, 1, startIndex, 0, 0);
  }

  void Bind(ref_ptr<dp::GpuProgram> program) override {}
  void Unbind() override {}

private:
  void BindVertexBuffers(ref_ptr<dp::GraphicsContext> context, VkCommandBuffer commandBuffer)
  {
    ref_ptr<dp::vulkan::VulkanBaseContext> vulkanContext = context;

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

    buffer_vector<VkBuffer, 8> buffers;
    buffer_vector<VkDeviceSize, 8> offsets;
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_geometryBuffers.size()); ++i)
    {
      buffers.emplace_back(m_geometryBuffers[i].m_buffer);
      offsets.emplace_back(0);
    }
    vkCmdBindVertexBuffers(commandBuffer, 0, m_geometryBuffers.size(), buffers.data(), 
                           offsets.data());
  }

  void UpdateBufferInternal(ref_ptr<dp::GraphicsContext> context, VkBuffer buffer, 
                            VkAccessFlagBits bufferAccessMask,
                            void const * data, uint32_t sizeInBytes)
  {
    ref_ptr<dp::vulkan::VulkanBaseContext> vulkanContext = context;
    VkCommandBuffer commandBuffer = vulkanContext->GetCurrentMemoryCommandBuffer();
    CHECK(commandBuffer != nullptr, ());

    // Set up a barrier to prevent data collisions (write-after-write, write-after-read).
    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | bufferAccessMask;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = sizeInBytes;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                         1, &barrier, 0, nullptr);

    // Copy to default or temporary staging buffer.
    auto stagingBuffer = vulkanContext->GetDefaultStagingBuffer();
    if (stagingBuffer->HasEnoughSpace(sizeInBytes))
    {
      auto staging = stagingBuffer->Reserve(sizeInBytes);
      memcpy(staging.m_pointer, data, sizeInBytes);

      // Schedule command to copy from the staging buffer to our geometry buffer.
      VkBufferCopy copyRegion = {};
      copyRegion.dstOffset = 0;
      copyRegion.srcOffset = staging.m_offset;
      copyRegion.size = sizeInBytes;
      vkCmdCopyBuffer(commandBuffer, staging.m_stagingBuffer, buffer, 1, &copyRegion);
    }
    else
    {
      // Here we use temporary staging object, which will be destroyed after the nearest
      // command queue submitting.
      VulkanStagingBuffer tempStagingBuffer(m_objectManager, sizeInBytes);
      CHECK(tempStagingBuffer.HasEnoughSpace(sizeInBytes), ());
      auto staging = tempStagingBuffer.Reserve(sizeInBytes);
      memcpy(staging.m_pointer, data, sizeInBytes);
      tempStagingBuffer.Flush();

      // Schedule command to copy from the staging buffer to our geometry buffer.
      VkBufferCopy copyRegion = {};
      copyRegion.dstOffset = 0;
      copyRegion.srcOffset = staging.m_offset;
      copyRegion.size = sizeInBytes;
      vkCmdCopyBuffer(commandBuffer, staging.m_stagingBuffer, buffer, 1, &copyRegion);
    }

    // Set up a barrier to prevent data collisions (read-after-write).
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = bufferAccessMask;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr,
                         1, &barrier, 0, nullptr);
  }

  ref_ptr<dp::MeshObject> m_mesh;
  ref_ptr<VulkanObjectManager> m_objectManager;
  std::vector<VulkanObject> m_geometryBuffers;
  VulkanObject m_indexBuffer;
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
