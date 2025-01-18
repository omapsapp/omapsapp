#include "drape/oglcontext.hpp"

#include "drape/gl_functions.hpp"

#include "base/logging.hpp"
#include "base/macros.hpp"

namespace dp
{
namespace
{
glConst DecodeTestFunction(TestFunction depthFunction)
{
  switch (depthFunction)
  {
  case TestFunction::Never: return gl_const::GLNever;
  case TestFunction::Less: return gl_const::GLLess;
  case TestFunction::Equal: return gl_const::GLEqual;
  case TestFunction::LessOrEqual: return gl_const::GLLessOrEqual;
  case TestFunction::Greater: return gl_const::GLGreat;
  case TestFunction::NotEqual: return gl_const::GLNotEqual;
  case TestFunction::GreaterOrEqual: return gl_const::GLGreatOrEqual;
  case TestFunction::Always: return gl_const::GLAlways;
  }
  UNREACHABLE();
}

glConst DecodeStencilFace(StencilFace stencilFace)
{
  switch (stencilFace)
  {
  case StencilFace::Front: return gl_const::GLFront;
  case StencilFace::Back: return gl_const::GLBack;
  case StencilFace::FrontAndBack: return gl_const::GLFrontAndBack;
  }
  UNREACHABLE();
}

glConst DecodeStencilAction(StencilAction stencilAction)
{
  switch (stencilAction)
  {
  case StencilAction::Keep: return gl_const::GLKeep;
  case StencilAction::Zero: return gl_const::GLZero;
  case StencilAction::Replace: return gl_const::GLReplace;
  case StencilAction::Increment: return gl_const::GLIncr;
  case StencilAction::IncrementWrap: return gl_const::GLIncrWrap;
  case StencilAction::Decrement: return gl_const::GLDecr;
  case StencilAction::DecrementWrap: return gl_const::GLDecrWrap;
  case StencilAction::Invert: return gl_const::GLInvert;
  }
  UNREACHABLE();
}

void OpenGLMessageCallback(glConst source, glConst type, uint32_t id, glConst severity,
                           int32_t length, char const * message, void * userData)
{
  UNUSED_VALUE(userData);

  std::string debugSource;
  if (source == gl_const::GLDebugSourceApi)
    debugSource = "DEBUG_SOURCE_API";
  else if (source == gl_const::GLDebugSourceShaderCompiler)
    debugSource = "DEBUG_SOURCE_SHADER_COMPILER";
  else if (source == gl_const::GLDebugSourceThirdParty)
    debugSource = "DEBUG_SOURCE_THIRD_PARTY";
  else if (source == gl_const::GLDebugSourceApplication)
    debugSource = "DEBUG_SOURCE_APPLICATION";
  else if (source == gl_const::GLDebugSourceOther)
    debugSource = "DEBUG_SOURCE_OTHER";

  std::string debugType;
  if (type == gl_const::GLDebugTypeError)
    debugType = "DEBUG_TYPE_ERROR";
  else if (type == gl_const::GLDebugDeprecatedBehavior)
    debugType = "DEBUG_TYPE_DEPRECATED_BEHAVIOR";
  else if (type == gl_const::GLDebugUndefinedBehavior)
    debugType = "DEBUG_TYPE_UNDEFINED_BEHAVIOR";
  else if (type == gl_const::GLDebugPortability)
    debugType = "DEBUG_TYPE_PORTABILITY";
  else if (type == gl_const::GLDebugPerformance)
    debugType = "DEBUG_TYPE_PERFORMANCE";
  else if (type == gl_const::GLDebugOther)
    debugType = "DEBUG_TYPE_OTHER";

  std::string debugSeverity;
  if (severity == gl_const::GLDebugSeverityLow)
    debugSeverity = "DEBUG_SEVERITY_LOW";
  else if (severity == gl_const::GLDebugSeverityMedium)
    debugSeverity = "DEBUG_SEVERITY_MEDIUM";
  else if (severity == gl_const::GLDebugSeverityHigh)
    debugSeverity = "DEBUG_SEVERITY_HIGH";
  else if (severity == gl_const::GLDebugSeverityNotification)
    debugSeverity = "DEBUG_SEVERITY_NOTIFICATION";

  LOG((type == gl_const::GLDebugTypeError) ? LERROR : LDEBUG,
      (std::string(message, static_cast<size_t>(length)), id, debugSource, debugType,
       debugSeverity));
}

static_assert(std::is_same_v<GLFunctions::TglDebugProc, decltype(&OpenGLMessageCallback)>,
              "Keep OpenGLMessageCallback type in sync with TglDebugProc type");
}  // namespace

void OGLContext::Init(ApiVersion apiVersion)
{
  GLFunctions::Init(apiVersion);

  GLFunctions::glPixelStore(gl_const::GLUnpackAlignment, 1);

  GLFunctions::glClearDepthValue(1.0);
  GLFunctions::glDepthFunc(gl_const::GLLessOrEqual);
  GLFunctions::glDepthMask(true);

  GLFunctions::glFrontFace(gl_const::GLClockwise);
  GLFunctions::glCullFace(gl_const::GLBack);
  GLFunctions::glEnable(gl_const::GLCullFace);
  GLFunctions::glEnable(gl_const::GLScissorTest);

  if (GLFunctions::CanEnableDebugMessages())
  {
    GLFunctions::glEnable(gl_const::GLDebugOutput);
    GLFunctions::glEnable(gl_const::GLDebugOutputSynchronous);
    GLFunctions::glDebugMessageCallback(&OpenGLMessageCallback, nullptr /* userData */);
    GLFunctions::glDebugMessageControl(gl_const::GLDontCare /* source */,
                                       gl_const::GLDebugTypeError, gl_const::GLDebugSeverityHigh,
                                       0 /* count */, nullptr /* ids */,
                                       gl_const::GLTrue /* enable */);
  }
}

ApiVersion OGLContext::GetApiVersion() const
{
  return GLFunctions::CurrentApiVersion;
}

std::string OGLContext::GetRendererName() const
{
  return GLFunctions::glGetString(gl_const::GLRenderer);
}

std::string OGLContext::GetRendererVersion() const
{
  return GLFunctions::glGetString(gl_const::GLVersion);
}

void OGLContext::DebugSynchronizeWithCPU()
{
  GLFunctions::glFinish();
}

void OGLContext::SetClearColor(dp::Color const & color)
{
  GLFunctions::glClearColor(color.GetRedF(), color.GetGreenF(), color.GetBlueF(), color.GetAlphaF());
}

void OGLContext::Clear(uint32_t clearBits, uint32_t storeBits)
{
  UNUSED_VALUE(storeBits);
  
  glConst glBits = 0;
  if (clearBits & ClearBits::ColorBit)
    glBits |= gl_const::GLColorBit;
  if (clearBits & ClearBits::DepthBit)
    glBits |= gl_const::GLDepthBit;
  if (clearBits & ClearBits::StencilBit)
    glBits |= gl_const::GLStencilBit;

  GLFunctions::glClear(glBits);
}

void OGLContext::Flush()
{
  GLFunctions::glFlush();
}

void OGLContext::SetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
  GLCHECK(GLFunctions::glViewport(x, y, w, h));
  GLCHECK(GLFunctions::glScissor(x, y, w, h));
}

void OGLContext::SetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
  GLCHECK(GLFunctions::glScissor(x, y, w, h));
}

void OGLContext::SetDepthTestEnabled(bool enabled)
{
  if (enabled)
    GLFunctions::glEnable(gl_const::GLDepthTest);
  else
    GLFunctions::glDisable(gl_const::GLDepthTest);
}

void OGLContext::SetDepthTestFunction(TestFunction depthFunction)
{
  GLFunctions::glDepthFunc(DecodeTestFunction(depthFunction));
}

void OGLContext::SetStencilTestEnabled(bool enabled)
{
  if (enabled)
    GLFunctions::glEnable(gl_const::GLStencilTest);
  else
    GLFunctions::glDisable(gl_const::GLStencilTest);
}

void OGLContext::SetStencilFunction(StencilFace face, TestFunction stencilFunction)
{
  GLFunctions::glStencilFuncSeparate(DecodeStencilFace(face), DecodeTestFunction(stencilFunction), 1, 1);
}

void OGLContext::SetStencilActions(StencilFace face, StencilAction stencilFailAction, StencilAction depthFailAction,
                                   StencilAction passAction)
{
  GLFunctions::glStencilOpSeparate(DecodeStencilFace(face),
                                   DecodeStencilAction(stencilFailAction),
                                   DecodeStencilAction(depthFailAction),
                                   DecodeStencilAction(passAction));
}

void OGLContext::SetCullingEnabled(bool enabled)
{
  if (enabled)
    GLFunctions::glEnable(gl_const::GLCullFace);
  else
    GLFunctions::glDisable(gl_const::GLCullFace);
}
}  // namespace dp
