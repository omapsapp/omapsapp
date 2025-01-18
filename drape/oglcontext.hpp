#pragma once

#include "drape/graphics_context.hpp"

namespace dp
{
class OGLContext : public GraphicsContext
{
public:
  void Init(ApiVersion apiVersion) override;
  ApiVersion GetApiVersion() const override;
  std::string GetRendererName() const override;
  std::string GetRendererVersion() const override;
  void ForgetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override {}
  void ApplyFramebuffer(std::string const & framebufferLabel) override {}

  void DebugSynchronizeWithCPU() override;
  void PushDebugLabel(std::string const & label) override {}
  void PopDebugLabel() override {}

  void SetClearColor(dp::Color const & color) override;
  void Clear(uint32_t clearBits, uint32_t storeBits) override;
  void Flush() override;
  void SetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;
  void SetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;
  void SetDepthTestEnabled(bool enabled) override;
  void SetDepthTestFunction(TestFunction depthFunction) override;
  void SetStencilTestEnabled(bool enabled) override;
  void SetStencilFunction(StencilFace face, TestFunction stencilFunction) override;
  void SetStencilActions(StencilFace face, StencilAction stencilFailAction, StencilAction depthFailAction,
                         StencilAction passAction) override;
  
  // Do not use custom stencil reference value in OpenGL rendering.
  void SetStencilReferenceValue(uint32_t stencilReferenceValue) override {}

  void SetCullingEnabled(bool enabled) override;
};
}  // namespace dp
