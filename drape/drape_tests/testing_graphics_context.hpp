#pragma once

#include "drape/graphics_context.hpp"

// Testing context simulates OpenGLES2 API version.
class TestingGraphicsContext : public dp::GraphicsContext
{
public:
  TestingGraphicsContext() = default;
  explicit TestingGraphicsContext(dp::ApiVersion apiVersion) : m_apiVersion(apiVersion) {}

  void Present() override {}
  void MakeCurrent() override {}
  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override {}
  void ForgetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override {}
  void ApplyFramebuffer(std::string const & framebufferLabel) override {}

  void Init(dp::ApiVersion apiVersion) override {}
  dp::ApiVersion GetApiVersion() const override { return m_apiVersion; }
  std::string GetRendererName() const override { return {}; }
  std::string GetRendererVersion() const override { return {}; }

  void PushDebugLabel(std::string const & label) override {}
  void PopDebugLabel() override {}

  void SetClearColor(dp::Color const & color) override {}
  void Clear(uint32_t clearBits, uint32_t storeBits) override {}
  void Flush() override {}
  void SetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override {}
  void SetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override {}
  void SetDepthTestEnabled(bool enabled) override {}
  void SetDepthTestFunction(dp::TestFunction depthFunction) override {}
  void SetStencilTestEnabled(bool enabled) override {}
  void SetStencilFunction(dp::StencilFace face, dp::TestFunction stencilFunction) override {}
  void SetStencilActions(dp::StencilFace face, dp::StencilAction stencilFailAction,
                         dp::StencilAction depthFailAction, dp::StencilAction passAction) override {}
  void SetStencilReferenceValue(uint32_t stencilReferenceValue) override {}
  void SetCullingEnabled(bool enabled) override {}

private:
  dp::ApiVersion m_apiVersion = dp::ApiVersion::OpenGLES2;
};
