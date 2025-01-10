#include "shaders/gl_program_params.hpp"

#include "drape/gl_gpu_program.hpp"
#include "drape/uniform_value.hpp"

#include "base/assert.hpp"
#include "base/macros.hpp"

#include <string>

namespace gpu
{
namespace
{
struct UniformsGuard
{
  template <typename ParamsType>
  UniformsGuard(ref_ptr<dp::GLGpuProgram> program, ParamsType const &)
    : m_program(program)
    , m_paramsName(ParamsType::GetName())
  {
    ASSERT_EQUAL(m_paramsName, ProgramParams::GetBoundParamsName(program),
                 ("Mismatched program and parameters", m_program->GetName()));
  }

  ~UniformsGuard()
  {
    auto const uniformsCount = m_program->GetNumericUniformsCount();
    CHECK_EQUAL(m_counter, uniformsCount, ("Not all numeric uniforms are set up",
                m_program->GetName(), m_paramsName));
  }

  ref_ptr<dp::GLGpuProgram> m_program;
  std::string const m_paramsName;
  uint32_t m_counter = 0;
};

template <typename ParamType> class GLTypeWrapper;

#define BIND_GL_TYPE(DataType, GLType) \
  template <> class GLTypeWrapper<DataType> \
  { \
  public: \
    static glConst Value() { return GLType; } \
  };

BIND_GL_TYPE(float, gl_const::GLFloatType)
BIND_GL_TYPE(glsl::vec2, gl_const::GLFloatVec2)
BIND_GL_TYPE(glsl::vec3, gl_const::GLFloatVec3)
BIND_GL_TYPE(glsl::vec4, gl_const::GLFloatVec4)
BIND_GL_TYPE(glsl::mat4, gl_const::GLFloatMat4)
BIND_GL_TYPE(int, gl_const::GLIntType)
BIND_GL_TYPE(glsl::ivec2, gl_const::GLIntVec2)
BIND_GL_TYPE(glsl::ivec3, gl_const::GLIntVec3)
BIND_GL_TYPE(glsl::ivec4, gl_const::GLIntVec4)

class Parameter
{
public:
  template<typename ParamType>
  static void CheckApply(UniformsGuard & guard, std::string const & name,
                         ParamType const & t)
  {
    if (Apply<ParamType>(guard.m_program, name, t))
      guard.m_counter++;
  }

private:
  template<typename ParamType>
  static bool Apply(ref_ptr<dp::GLGpuProgram> program, std::string const & name,
                    ParamType const & p)
  {
    auto const location = program->GetUniformLocation(name);
    if (location < 0)
      return false;

    ASSERT_EQUAL(program->GetUniformType(name), GLTypeWrapper<ParamType>::Value(), ());
    dp::UniformValue::ApplyRaw(location, p);
    return true;
  }
};
}  // namespace

void GLProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::GpuProgram> program,
                                  MapProgramParams const & params)
{
  UNUSED_VALUE(context);
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_pivotTransform", params.m_pivotTransform);
  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
  Parameter::CheckApply(guard, "u_zScale", params.m_zScale);
  Parameter::CheckApply(guard, "u_interpolation", params.m_interpolation);
  Parameter::CheckApply(guard, "u_isOutlinePass", params.m_isOutlinePass);
  Parameter::CheckApply(guard, "u_contrastGamma", params.m_contrastGamma);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::GpuProgram> program,
                                  RouteProgramParams const & params)
{
  UNUSED_VALUE(context);
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_pivotTransform", params.m_pivotTransform);
  Parameter::CheckApply(guard, "u_routeParams", params.m_routeParams);
  Parameter::CheckApply(guard, "u_color", params.m_color);
  Parameter::CheckApply(guard, "u_maskColor", params.m_maskColor);
  Parameter::CheckApply(guard, "u_outlineColor", params.m_outlineColor);
  Parameter::CheckApply(guard, "u_pattern", params.m_pattern);
  Parameter::CheckApply(guard, "u_angleCosSin", params.m_angleCosSin);
  Parameter::CheckApply(guard, "u_arrowHalfWidth", params.m_arrowHalfWidth);
  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
  Parameter::CheckApply(guard, "u_fakeBorders", params.m_fakeBorders);
  Parameter::CheckApply(guard, "u_fakeColor", params.m_fakeColor);
  Parameter::CheckApply(guard, "u_fakeOutlineColor", params.m_fakeOutlineColor);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::GpuProgram> program,
                                  TrafficProgramParams const & params)
{
  UNUSED_VALUE(context);
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_pivotTransform", params.m_pivotTransform);
  Parameter::CheckApply(guard, "u_trafficParams", params.m_trafficParams);
  Parameter::CheckApply(guard, "u_outlineColor", params.m_outlineColor);
  Parameter::CheckApply(guard, "u_outline", params.m_outline);
  Parameter::CheckApply(guard, "u_lightArrowColor", params.m_lightArrowColor);
  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
  Parameter::CheckApply(guard, "u_darkArrowColor", params.m_darkArrowColor);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::GpuProgram> program,
                                  TransitProgramParams const & params)
{
  UNUSED_VALUE(context);
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_pivotTransform", params.m_pivotTransform);
  Parameter::CheckApply(guard, "u_params", params.m_params);
  Parameter::CheckApply(guard, "u_lineHalfWidth", params.m_lineHalfWidth);
  Parameter::CheckApply(guard, "u_maxRadius", params.m_maxRadius);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::GpuProgram> program,
                                  GuiProgramParams const & params)
{
  UNUSED_VALUE(context);
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_contrastGamma", params.m_contrastGamma);
  Parameter::CheckApply(guard, "u_position", params.m_position);
  Parameter::CheckApply(guard, "u_isOutlinePass", params.m_isOutlinePass);
  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
  Parameter::CheckApply(guard, "u_length", params.m_length);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::GpuProgram> program,
                                  ShapesProgramParams const & params)
{
  UNUSED_VALUE(context);
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_pivotTransform", params.m_pivotTransform);
  Parameter::CheckApply(guard, "u_position", params.m_position);
  Parameter::CheckApply(guard, "u_accuracy", params.m_accuracy);
  Parameter::CheckApply(guard, "u_lineParams", params.m_lineParams);
  Parameter::CheckApply(guard, "u_zScale", params.m_zScale);
  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
  Parameter::CheckApply(guard, "u_azimut", params.m_azimut);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::GpuProgram> program,
                                  Arrow3dProgramParams const & params)
{
  UNUSED_VALUE(context);
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_transform", params.m_transform);
  Parameter::CheckApply(guard, "u_normalTransform", params.m_normalTransform);
  Parameter::CheckApply(guard, "u_color", params.m_color);
  Parameter::CheckApply(guard, "u_texCoordFlipping", params.m_texCoordFlipping);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::GpuProgram> program,
                                  DebugRectProgramParams const & params)
{
  UNUSED_VALUE(context);
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_color", params.m_color);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::GpuProgram> program,
                                  ScreenQuadProgramParams const & params)
{
  UNUSED_VALUE(context);
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::GpuProgram> program,
                                  SMAAProgramParams const & params)
{
  UNUSED_VALUE(context);
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_framebufferMetrics", params.m_framebufferMetrics);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::GpuProgram> program, 
                                  ImGuiProgramParams const & params)
{
  UNUSED_VALUE(context);
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_projection", params.m_projection);
}
}  // namespace gpu
