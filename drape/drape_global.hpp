#pragma once

#include "drape/color.hpp"
#include "drape/pointers.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"

#include "std/target_os.hpp"

#include <cstdint>
#include <functional>

#if defined(__APPLE__)
#define OMIM_METAL_AVAILABLE
#endif

namespace gpu
{
class ProgramManager;
}  // namespace gpu

namespace dp
{
enum class ApiVersion
{
  Invalid = -1,
  OpenGLES2 = 0,
  OpenGLES3,
  Metal,
  Vulkan
};

/// @todo We have in code: if (anchor & dp::Center) which is not consistent with Center == 0.
/// Making Center == 1 breaks other defaults. Review this logic in future.
enum Anchor
{
  Center = 0,
  Left = 0x1,
  Right = Left << 1,
  Top = Right << 1,
  Bottom = Top << 1,
  LeftTop = Left | Top,
  RightTop = Right | Top,
  LeftBottom = Left | Bottom,
  RightBottom = Right | Bottom
};

enum LineCap
{
  SquareCap = -1,
  RoundCap = 0,
  ButtCap = 1,
};

enum LineJoin
{
  MiterJoin = -1,
  BevelJoin = 0,
  RoundJoin = 1,
};

using DrapeID = uint64_t;

struct FontDecl
{
  FontDecl() = default;
  FontDecl(Color const & color, float size, Color const & outlineColor = Color::Transparent())
    : m_color(color), m_outlineColor(outlineColor), m_size(size)
  {}

  Color m_color = Color::Transparent();
  Color m_outlineColor = Color::Transparent();
  float m_size = 0;
};

struct TitleDecl
{
  dp::FontDecl m_primaryTextFont;
  std::string m_primaryText;
  dp::FontDecl m_secondaryTextFont;
  std::string m_secondaryText;
  dp::Anchor m_anchor = dp::Anchor::Center;
  bool m_forceNoWrap = false;
  m2::PointF m_primaryOffset = m2::PointF(0.0f, 0.0f);
  m2::PointF m_secondaryOffset = m2::PointF(0.0f, 0.0f);
  bool m_primaryOptional = false;
  bool m_secondaryOptional = false;
};

class BaseFramebuffer
{
public:
  virtual ~BaseFramebuffer() = default;
  virtual void Bind() = 0;
};

inline std::string DebugPrint(dp::ApiVersion apiVersion)
{
  switch (apiVersion)
  {
  case dp::ApiVersion::Invalid: return "Invalid";
  case dp::ApiVersion::OpenGLES2: return "OpenGLES2";
  case dp::ApiVersion::OpenGLES3: return "OpenGLES3";
  case dp::ApiVersion::Metal: return "Metal";
  case dp::ApiVersion::Vulkan: return "Vulkan";
  }
  return "Unknown";
}

inline dp::ApiVersion ApiVersionFromString(std::string const & str)
{
#if defined(OMIM_METAL_AVAILABLE)
  if (str == "Metal")
    return dp::ApiVersion::Metal;
#endif

#if defined(OMIM_OS_ANDROID)
  if (str == "Vulkan")
    return dp::ApiVersion::Vulkan;
#endif

  if (str == "OpenGLES2")
    return dp::ApiVersion::OpenGLES2;

  if (str == "OpenGLES3")
    return dp::ApiVersion::OpenGLES3;

  // Default behavior for different OS. Appropriate fallback will be chosen
  // if default API is not supported.
#if defined(OMIM_METAL_AVAILABLE)
  return dp::ApiVersion::Metal;
#elif defined(OMIM_OS_ANDROID)
  return dp::ApiVersion::Vulkan;
#else
  return dp::ApiVersion::OpenGLES3;
#endif
}

class GraphicsContext;
class TextureManager;
using RenderInjectionHandler = std::function<void(ref_ptr<dp::GraphicsContext>, 
                                                  ref_ptr<TextureManager>, 
                                                  ref_ptr<gpu::ProgramManager>,
                                                  bool shutdown)>;
}  // namespace dp
