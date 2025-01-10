#include "shaders/metal_program_params.hpp"

#include "drape/metal/metal_base_context.hpp"

namespace gpu
{
namespace metal
{
namespace
{
template<typename T>
void ApplyBytes(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
                T const & params)
{
  ref_ptr<dp::metal::MetalGpuProgram> p = program;
  ref_ptr<dp::metal::MetalBaseContext> metalContext = context;
  id<MTLRenderCommandEncoder> encoder = metalContext->GetCommandEncoder();
  
  auto const vsBindingIndex = p->GetVertexShaderUniformsBindingIndex();
  if (vsBindingIndex >= 0)
  {
    [encoder setVertexBytes:(void const *)&params length:sizeof(params)
                    atIndex:vsBindingIndex];
  }
  
  auto const fsBindingIndex = p->GetFragmentShaderUniformsBindingIndex();
  if (fsBindingIndex >= 0)
  {
    [encoder setFragmentBytes:(void const *)&params length:sizeof(params)
                      atIndex:fsBindingIndex];
  }
}
}  // namespace
  
void MetalProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                     ref_ptr<dp::GpuProgram> program,
                                     MapProgramParams const & params)
{
  ApplyBytes(context, program, params);
}

void MetalProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                     ref_ptr<dp::GpuProgram> program,
                                     RouteProgramParams const & params)
{
  ApplyBytes(context, program, params);
}

void MetalProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                     ref_ptr<dp::GpuProgram> program,
                                     TrafficProgramParams const & params)
{
  ApplyBytes(context, program, params);
}

void MetalProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                     ref_ptr<dp::GpuProgram> program,
                                     TransitProgramParams const & params)
{
  ApplyBytes(context, program, params);
}

void MetalProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                     ref_ptr<dp::GpuProgram> program,
                                     GuiProgramParams const & params)
{
  ApplyBytes(context, program, params);
}

void MetalProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                     ref_ptr<dp::GpuProgram> program,
                                     ShapesProgramParams const & params)
{
  ApplyBytes(context, program, params);
}

void MetalProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                     ref_ptr<dp::GpuProgram> program,
                                     Arrow3dProgramParams const & params)
{
  ApplyBytes(context, program, params);
}

void MetalProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                     ref_ptr<dp::GpuProgram> program,
                                     DebugRectProgramParams const & params)
{
  ApplyBytes(context, program, params);
}

void MetalProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                     ref_ptr<dp::GpuProgram> program,
                                     ScreenQuadProgramParams const & params)
{
  ApplyBytes(context, program, params);
}

void MetalProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                     ref_ptr<dp::GpuProgram> program,
                                     SMAAProgramParams const & params)
{
  ApplyBytes(context, program, params);
}

void MetalProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context, 
                                     ref_ptr<dp::GpuProgram> program,
                                     ImGuiProgramParams const & params)
{
  ApplyBytes(context, program, params);
}
}  // namespace metal
}  // namespace gpu
