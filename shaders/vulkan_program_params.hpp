#pragma once

#include "shaders/program_params.hpp"

#include "drape/vulkan/vulkan_base_context.hpp"
#include "drape/vulkan/vulkan_gpu_program.hpp"
#include "drape/vulkan/vulkan_object_manager.hpp"
#include "drape/vulkan/vulkan_utils.hpp"

#include "base/thread_checker.hpp"

#include <cstdint>
#include <vector>

namespace gpu
{
namespace vulkan
{
class VulkanProgramPool;

class VulkanProgramParamsSetter : public ProgramParamsSetter
{
public:
  struct UniformBuffer
  {
    dp::vulkan::VulkanObject m_object;
    uint8_t * m_pointer = nullptr;
    uint32_t m_freeOffset = 0;
};

  VulkanProgramParamsSetter(ref_ptr<dp::vulkan::VulkanBaseContext> context, ref_ptr<VulkanProgramPool> programPool);
  ~VulkanProgramParamsSetter() override;

  void Destroy(ref_ptr<dp::vulkan::VulkanBaseContext> context);
  void Flush();
  void Finish(uint32_t inflightFrameIndex);

  void Apply(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
             MapProgramParams const & params) override;
  void Apply(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
             RouteProgramParams const & params) override;
  void Apply(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
             TrafficProgramParams const & params) override;
  void Apply(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
             TransitProgramParams const & params) override;
  void Apply(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
             GuiProgramParams const & params) override;
  void Apply(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
             ShapesProgramParams const & params) override;
  void Apply(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
             Arrow3dProgramParams const & params) override;
  void Apply(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
             DebugRectProgramParams const & params) override;
  void Apply(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
             ScreenQuadProgramParams const & params) override;
  void Apply(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
             SMAAProgramParams const & params) override;
  void Apply(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
             ImGuiProgramParams const & params) override;

private:
  template<typename T>
  void ApplyImpl(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
                 T const & params)
  {
    ASSERT_EQUAL(T::GetName(), ProgramParams::GetBoundParamsName(program),
                 ("Mismatched program and parameters", program->GetName()));
    ApplyBytes(context, reinterpret_cast<void const *>(&params), sizeof(params));
  }

  void ApplyBytes(ref_ptr<dp::vulkan::VulkanBaseContext> context,
                  void const * data, uint32_t sizeInBytes);

  ref_ptr<dp::vulkan::VulkanObjectManager> m_objectManager;
  std::array<std::vector<UniformBuffer>, dp::vulkan::kMaxInflightFrames> m_uniformBuffers;
  uint32_t m_offsetAlignment = 0;
  uint32_t m_sizeAlignment = 0;
  uint32_t m_flushHandlerId = 0;
  uint32_t m_finishHandlerId = 0;
  uint32_t m_updateInflightFrameId = 0;
  ThreadChecker m_threadChecker;

  uint32_t m_currentInflightFrameIndex = 0;
};
}  // namespace vulkan
}  // namespace gpu
