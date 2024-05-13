#pragma once

#include "graphics/opengl/gl_render_context.hpp"

#include <memory>

namespace android
{
  class RenderContext : public graphics::gl::RenderContext
  {
  public:

    void makeCurrent();

    graphics::RenderContext * createShared();
  };
}
