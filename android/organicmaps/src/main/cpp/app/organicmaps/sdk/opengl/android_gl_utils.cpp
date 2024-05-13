#include "android_gl_utils.hpp"

#include "base/logging.hpp"
#include "base/src_point.hpp"
#include "base/string_utils.hpp"

namespace android
{

ConfigComparator::ConfigComparator(EGLDisplay display)
  : m_display(display)
{
}

bool ConfigComparator::operator()(EGLConfig const & l, EGLConfig const & r) const
{
  int const weight = configWeight(l) - configWeight(r);
  if (weight == 0)
    return configAlphaSize(l) < configAlphaSize(r);

  return weight < 0;
}

int ConfigComparator::configWeight(EGLConfig const & config) const
{
  int val = -1;
  eglGetConfigAttrib(m_display, config, EGL_CONFIG_CAVEAT, &val);

  switch (val)
  {
    case EGL_NONE:
      return 0;
    case EGL_SLOW_CONFIG:
      return 1;
    case EGL_NON_CONFORMANT_CONFIG:
      return 2;
    default:
      return 0;
  }
}

int ConfigComparator::configAlphaSize(EGLConfig const & config) const
{
  int val = 0;
  eglGetConfigAttrib(m_display, config, EGL_ALPHA_SIZE, &val);
  return val;
}

namespace
{

std::string GetEglError(EGLint error)
{
  switch (error)
  {
  case EGL_NOT_INITIALIZED :      return "EGL_NOT_INITIALIZED";
  case EGL_BAD_ACCESS :           return "EGL_BAD_ACCESS";
  case EGL_BAD_ALLOC :            return "EGL_BAD_ALLOC";
  case EGL_BAD_ATTRIBUTE :        return "EGL_BAD_ATTRIBUTE";
  case EGL_BAD_CONFIG :           return "EGL_BAD_CONFIG";
  case EGL_BAD_CONTEXT :          return "EGL_BAD_CONTEXT";
  case EGL_BAD_CURRENT_SURFACE :  return "EGL_BAD_CURRENT_SURFACE";
  case EGL_BAD_DISPLAY :          return "EGL_BAD_DISPLAY";
  case EGL_BAD_MATCH :            return "EGL_BAD_MATCH";
  case EGL_BAD_NATIVE_PIXMAP :    return "EGL_BAD_NATIVE_PIXMAP";
  case EGL_BAD_NATIVE_WINDOW :    return "EGL_BAD_NATIVE_WINDOW";
  case EGL_BAD_PARAMETER :        return "EGL_BAD_PARAMETER";
  case EGL_BAD_SURFACE :          return "EGL_BAD_SURFACE";
  case EGL_CONTEXT_LOST :         return "EGL_CONTEXT_LOST";
  default:                        return strings::to_string(error);
  }
}

}  // namespace

void CheckEGL(base::SrcPoint const & src)
{
  EGLint error = eglGetError();
  while (error != EGL_SUCCESS)
  {
    LOG(LERROR, ("SrcPoint : ", src, ". EGL error : ", GetEglError(error)));
    error = eglGetError();
  }
}

}  // namespace android
