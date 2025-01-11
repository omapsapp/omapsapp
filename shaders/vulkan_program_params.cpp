#include "shaders/vulkan_program_params.hpp"

#include "shaders/vulkan_program_pool.hpp"

#include "drape/vulkan/vulkan_base_context.hpp"
#include "drape/vulkan/vulkan_gpu_program.hpp"
#include "drape/vulkan/vulkan_utils.hpp"

namespace gpu
{
namespace vulkan
{
namespace
{
uint32_t constexpr kUniformBufferSizeInBytes = 16 * 1024;

VulkanProgramParamsSetter::UniformBuffer CreateUniformBuffer(VkDevice device,
                                                             ref_ptr<dp::vulkan::VulkanObjectManager> objectManager,
                                                             uint32_t & offsetAlignment, uint32_t & sizeAlignment)
{
  using namespace dp::vulkan;
  auto constexpr kUniformBuffer = VulkanMemoryManager::ResourceType::Uniform;

  VulkanProgramParamsSetter::UniformBuffer result;
  result.m_object = objectManager->CreateBuffer(kUniformBuffer, kUniformBufferSizeInBytes, 0 /* batcherHash */);
  VkMemoryRequirements memReqs = {};
  vkGetBufferMemoryRequirements(device, result.m_object.m_buffer, &memReqs);
  sizeAlignment = objectManager->GetMemoryManager().GetSizeAlignment(memReqs);
  offsetAlignment = objectManager->GetMemoryManager().GetOffsetAlignment(kUniformBuffer);

  result.m_pointer = objectManager->MapUnsafe(result.m_object);
  return result;
}
}  // namespace

VulkanProgramParamsSetter::VulkanProgramParamsSetter(ref_ptr<dp::vulkan::VulkanBaseContext> context,
                                                     ref_ptr<VulkanProgramPool> programPool)
{
  using namespace dp::vulkan;
  m_objectManager = context->GetObjectManager();
  m_objectManager->SetMaxUniformBuffers(programPool->GetMaxUniformBuffers());
  m_objectManager->SetMaxImageSamplers(programPool->GetMaxImageSamplers());

  for (auto & ub : m_uniformBuffers)
  {
    ub.emplace_back(CreateUniformBuffer(context->GetDevice(), m_objectManager,
                                        m_sizeAlignment, m_offsetAlignment));
  }

  m_flushHandlerId = context->RegisterHandler(VulkanBaseContext::HandlerType::PrePresent,
    [this](uint32_t)
  {
    Flush();
  });

  m_finishHandlerId = context->RegisterHandler(VulkanBaseContext::HandlerType::PostPresent,
    [this](uint32_t inflightFrameIndex)
  {
    CHECK_LESS(inflightFrameIndex, dp::vulkan::kMaxInflightFrames, ());
    Finish(inflightFrameIndex);
  });

  m_updateInflightFrameId = context->RegisterHandler(VulkanBaseContext::HandlerType::UpdateInflightFrame,
    [this](uint32_t inflightFrameIndex)
  {
    CHECK_LESS(inflightFrameIndex, dp::vulkan::kMaxInflightFrames, ());
    m_currentInflightFrameIndex = inflightFrameIndex;
  });
}

VulkanProgramParamsSetter::~VulkanProgramParamsSetter()
{
  for (auto & ub : m_uniformBuffers)
    CHECK_EQUAL(ub.size(), 0, ());
}

void VulkanProgramParamsSetter::Destroy(ref_ptr<dp::vulkan::VulkanBaseContext> context)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  for (auto & ub : m_uniformBuffers)
  {
    for (auto & b : ub)
    {
      m_objectManager->UnmapUnsafe(b.m_object);
      m_objectManager->DestroyObject(b.m_object);
    }
    ub.clear();
  }

  context->UnregisterHandler(m_finishHandlerId);
  context->UnregisterHandler(m_flushHandlerId);
  context->UnregisterHandler(m_updateInflightFrameId);
}

void VulkanProgramParamsSetter::Flush()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  for (auto & b : m_uniformBuffers[m_currentInflightFrameIndex])
  {
    if (b.m_freeOffset == 0)
      continue;

    auto const size = b.m_freeOffset;
    m_objectManager->FlushUnsafe(b.m_object, 0 /* offset */, size);
  }
}

void VulkanProgramParamsSetter::Finish(uint32_t inflightFrameIndex)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  for (auto & b : m_uniformBuffers[inflightFrameIndex])
    b.m_freeOffset = 0;
}

void VulkanProgramParamsSetter::ApplyBytes(ref_ptr<dp::vulkan::VulkanBaseContext> context,
                                           void const * data, uint32_t sizeInBytes)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  auto const & mm = m_objectManager->GetMemoryManager();
  auto const alignedSize = mm.GetAligned(sizeInBytes, m_sizeAlignment);

  auto & ub = m_uniformBuffers[m_currentInflightFrameIndex];

  int index = -1;
  for (int i = 0; i < static_cast<int>(ub.size()); ++i)
  {
    if (ub[i].m_freeOffset + alignedSize <= kUniformBufferSizeInBytes)
    {
      index = i;
      break;
    }
  }
  if (index < 0)
  {
    uint32_t sizeAlignment, offsetAlignment;
    ub.emplace_back(CreateUniformBuffer(context->GetDevice(), m_objectManager,
                                        sizeAlignment, offsetAlignment));
    CHECK_EQUAL(m_sizeAlignment, sizeAlignment, ());
    CHECK_EQUAL(m_offsetAlignment, offsetAlignment, ());
    index = static_cast<int>(ub.size()) - 1;
  }
  CHECK_LESS_OR_EQUAL(ub[index].m_freeOffset + alignedSize, kUniformBufferSizeInBytes, ());

  auto const alignedOffset = ub[index].m_freeOffset;
  uint8_t * ptr = ub[index].m_pointer + alignedOffset;

  // Update offset and align it.
  ub[index].m_freeOffset += alignedSize;
  ub[index].m_freeOffset = std::min(mm.GetAligned(ub[index].m_freeOffset, m_offsetAlignment),
                                    ub[index].m_object.GetAlignedSize());

  memcpy(ptr, data, sizeInBytes);

  dp::vulkan::ParamDescriptor descriptor;
  descriptor.m_type = dp::vulkan::ParamDescriptor::Type::DynamicUniformBuffer;
  descriptor.m_bufferDescriptor.buffer = ub[index].m_object.m_buffer;
  descriptor.m_bufferDescriptor.range = alignedSize;
  descriptor.m_bufferDynamicOffset = alignedOffset;
  descriptor.m_id = static_cast<uint32_t>(index);
  context->ApplyParamDescriptor(std::move(descriptor));
}
  
void VulkanProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                      ref_ptr<dp::GpuProgram> program,
                                      MapProgramParams const & params)
{
  ApplyImpl(context, program, params);
}

void VulkanProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                      ref_ptr<dp::GpuProgram> program,
                                      RouteProgramParams const & params)
{
  ApplyImpl(context, program, params);
}

void VulkanProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                      ref_ptr<dp::GpuProgram> program,
                                      TrafficProgramParams const & params)
{
  ApplyImpl(context, program, params);
}

void VulkanProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                      ref_ptr<dp::GpuProgram> program,
                                      TransitProgramParams const & params)
{
  ApplyImpl(context, program, params);
}

void VulkanProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                      ref_ptr<dp::GpuProgram> program,
                                      GuiProgramParams const & params)
{
  ApplyImpl(context, program, params);
}

void VulkanProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                      ref_ptr<dp::GpuProgram> program,
                                      ShapesProgramParams const & params)
{
  ApplyImpl(context, program, params);
}

void VulkanProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                      ref_ptr<dp::GpuProgram> program,
                                      Arrow3dProgramParams const & params)
{
  ApplyImpl(context, program, params);
}

void VulkanProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                      ref_ptr<dp::GpuProgram> program,
                                      DebugRectProgramParams const & params)
{
  ApplyImpl(context, program, params);
}

void VulkanProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                      ref_ptr<dp::GpuProgram> program,
                                      ScreenQuadProgramParams const & params)
{
  ApplyImpl(context, program, params);
}

void VulkanProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                      ref_ptr<dp::GpuProgram> program,
                                      SMAAProgramParams const & params)
{
  ApplyImpl(context, program, params);
}

void VulkanProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                      ref_ptr<dp::GpuProgram> program, 
                                      ImGuiProgramParams const & params)
{
  ApplyImpl(context, program, params);
}
}  // namespace vulkan
}  // namespace gpu
