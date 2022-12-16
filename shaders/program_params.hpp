#pragma once

#include "shaders/programs.hpp"

#include "drape/drape_global.hpp"
#include "drape/glsl_types.hpp"
#include "drape/graphics_context.hpp"
#include "drape/gpu_program.hpp"
#include "drape/pointers.hpp"

#include "base/assert.hpp"

#include "std/target_os.hpp"

#include <map>
#include <vector>

namespace gpu
{
class ProgramParams
{
public:
  static void Init();
  static void Destroy();
  static std::string GetBoundParamsName(ref_ptr<dp::GpuProgram> program);

private:
  static std::map<std::string, std::string> m_boundParams;
};

#define BIND_PROGRAMS(ParamsType, ...) \
  static std::vector<gpu::Program> const & GetBoundPrograms() \
  { \
    static std::vector<gpu::Program> programs = {__VA_ARGS__}; \
    return programs; \
  } \
  static std::string GetName() { return std::string(#ParamsType); } \
  static void BindPrograms(std::map<std::string, std::string> & params) \
  { \
    for (auto const p : GetBoundPrograms()) \
    { \
      auto const programName = DebugPrint(p); \
      params[programName] = GetName(); \
    } \
  }

#define ALIGNMENT __attribute__ ((aligned(16)))

struct MapProgramParams
{
  glsl::mat4 m_modelView;
  glsl::mat4 m_projection;
  glsl::mat4 m_pivotTransform;
  float m_opacity = 1.0f;
  float m_zScale = 1.0f;
  float m_interpolation = 1.0f;
  float m_isOutlinePass = 1.0f;
  glsl::vec2 m_contrastGamma;

  BIND_PROGRAMS(MapProgramParams,
    Program::Area,
    Program::Area3d,
    Program::Area3dOutline,
    Program::AreaOutline,
    Program::Bookmark,
    Program::BookmarkAnim,
    Program::BookmarkAnimBillboard,
    Program::BookmarkBillboard,
    Program::CapJoin,
    Program::CirclePoint,
    Program::ColoredSymbol,
    Program::ColoredSymbolBillboard,
    Program::DashedLine,
    Program::HatchingArea,
    Program::Line,
    Program::MaskedTexturing,
    Program::MaskedTexturingBillboard,
    Program::PathSymbol,
    Program::Text,
    Program::TextBillboard,
    Program::TextFixed,
    Program::TextFixedBillboard,
    Program::TextOutlined,
    Program::TextOutlinedBillboard,
    Program::Texturing,
    Program::TexturingBillboard,
    Program::BookmarkAboveText,
    Program::BookmarkAnimAboveText,
    Program::BookmarkAnimAboveTextBillboard,
    Program::BookmarkAboveTextBillboard)
} ALIGNMENT;

struct RouteProgramParams
{
  glsl::mat4 m_modelView;
  glsl::mat4 m_projection;
  glsl::mat4 m_pivotTransform;
  glsl::vec4 m_routeParams;
  glsl::vec4 m_color;
  glsl::vec4 m_maskColor;
  glsl::vec4 m_outlineColor;
  glsl::vec2 m_pattern;
  glsl::vec2 m_angleCosSin;
  float m_arrowHalfWidth = 0.0f;
  float m_opacity = 1.0f;
  glsl::vec2 m_fakeBorders;
  glsl::vec4 m_fakeColor;
  glsl::vec4 m_fakeOutlineColor;

  BIND_PROGRAMS(RouteProgramParams,
    Program::Route,
    Program::RouteDash,
    Program::RouteArrow,
    Program::RouteMarker)
} ALIGNMENT;

struct TrafficProgramParams
{
  glsl::mat4 m_modelView;
  glsl::mat4 m_projection;
  glsl::mat4 m_pivotTransform;
  glsl::vec4 m_trafficParams;
  glsl::vec3 m_outlineColor;
  float m_outline = 0.0f;
  glsl::vec3 m_lightArrowColor;
  float m_opacity = 1.0f;
  glsl::vec3 m_darkArrowColor;

  BIND_PROGRAMS(TrafficProgramParams,
    Program::Traffic,
    Program::TrafficLine,
    Program::TrafficCircle)
} ALIGNMENT;

struct TransitProgramParams
{
  glsl::mat4 m_modelView;
  glsl::mat4 m_projection;
  glsl::mat4 m_pivotTransform;
  glsl::vec3 m_params;
  float m_lineHalfWidth = 0.0f;
  float m_maxRadius = 0.0f;

  BIND_PROGRAMS(TransitProgramParams,
    Program::Transit,
    Program::TransitCircle,
    Program::TransitMarker)
} ALIGNMENT;

struct GuiProgramParams
{
  glsl::mat4 m_modelView;
  glsl::mat4 m_projection;
  glsl::vec2 m_contrastGamma;
  glsl::vec2 m_position;
  float m_isOutlinePass = 1.0f;
  float m_opacity = 1.0f;
  float m_length = 0.0f;

  BIND_PROGRAMS(GuiProgramParams,
    Program::TextStaticOutlinedGui,
    Program::TextOutlinedGui,
    Program::TexturingGui,
    Program::Ruler)
} ALIGNMENT;

struct ShapesProgramParams
{
  glsl::mat4 m_modelView;
  glsl::mat4 m_projection;
  glsl::mat4 m_pivotTransform;
  glsl::vec3 m_position;
  float m_accuracy = 0.0;
  glsl::vec2 m_lineParams;
  float m_zScale = 1.0f;
  float m_opacity = 1.0f;
  float m_azimut = 0.0;

  BIND_PROGRAMS(ShapesProgramParams,
    Program::Accuracy,
    Program::MyPosition,
    Program::SelectionLine)
} ALIGNMENT;

struct Arrow3dProgramParams
{
  glsl::mat4 m_transform;
  glsl::vec4 m_color;

  BIND_PROGRAMS(Arrow3dProgramParams,
    Program::Arrow3d,
    Program::Arrow3dShadow,
    Program::Arrow3dOutline)
} ALIGNMENT;

struct DebugRectProgramParams
{
  glsl::vec4 m_color;

  BIND_PROGRAMS(DebugRectProgramParams, Program::DebugRect)
} ALIGNMENT;

struct ScreenQuadProgramParams
{
  float m_opacity = 1.0f;
  float m_invertV = 1.0f;

  BIND_PROGRAMS(ScreenQuadProgramParams, Program::ScreenQuad)
} ALIGNMENT;

struct SMAAProgramParams
{
  glsl::vec4 m_framebufferMetrics;

  BIND_PROGRAMS(SMAAProgramParams,
    Program::SmaaEdges,
    Program::SmaaBlendingWeight,
    Program::SmaaFinal)
} ALIGNMENT;

#undef ALIGNMENT

class ProgramParamsSetter
{
public:
  virtual ~ProgramParamsSetter() = default;
  virtual void Apply(ref_ptr<dp::GraphicsContext> context,
                     ref_ptr<dp::GpuProgram> program, MapProgramParams const & params) = 0;
  virtual void Apply(ref_ptr<dp::GraphicsContext> context,
                     ref_ptr<dp::GpuProgram> program, RouteProgramParams const & params) = 0;
  virtual void Apply(ref_ptr<dp::GraphicsContext> context,
                     ref_ptr<dp::GpuProgram> program, TrafficProgramParams const & params) = 0;
  virtual void Apply(ref_ptr<dp::GraphicsContext> context,
                     ref_ptr<dp::GpuProgram> program, TransitProgramParams const & params) = 0;
  virtual void Apply(ref_ptr<dp::GraphicsContext> context,
                     ref_ptr<dp::GpuProgram> program, GuiProgramParams const & params) = 0;
  virtual void Apply(ref_ptr<dp::GraphicsContext> context,
                     ref_ptr<dp::GpuProgram> program, ShapesProgramParams const & params) = 0;
  virtual void Apply(ref_ptr<dp::GraphicsContext> context,
                     ref_ptr<dp::GpuProgram> program, Arrow3dProgramParams const & params) = 0;
  virtual void Apply(ref_ptr<dp::GraphicsContext> context,
                     ref_ptr<dp::GpuProgram> program, DebugRectProgramParams const & params) = 0;
  virtual void Apply(ref_ptr<dp::GraphicsContext> context,
                     ref_ptr<dp::GpuProgram> program, ScreenQuadProgramParams const & params) = 0;
  virtual void Apply(ref_ptr<dp::GraphicsContext> context,
                     ref_ptr<dp::GpuProgram> program, SMAAProgramParams const & params) = 0;
};
}  // namespace gpu
