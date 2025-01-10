#pragma once

#include "shaders/program_params.hpp"

#include <cstdint>

namespace gpu
{
namespace metal
{
class MetalProgramParamsSetter : public ProgramParamsSetter
{
public:
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
};
}  // namespace metal
}  // namespace gpu
