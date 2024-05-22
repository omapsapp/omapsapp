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
  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer>) override {}
  void ForgetFramebuffer(ref_ptr<dp::BaseFramebuffer>) override {}
  void ApplyFramebuffer(std::string const &) override {}

  void Init(dp::ApiVersion) override {}
  dp::ApiVersion GetApiVersion() const override { return m_apiVersion; }
  std::string GetRendererName() const override { return {}; }
  std::string GetRendererVersion() const override { return {}; }

  void PushDebugLabel(std::string const &) override {}
  void PopDebugLabel() override {}

  void SetClearColor(dp::Color const &) override {}
  void Clear(uint32_t, uint32_t) override {}
  void Flush() override {}
  void SetViewport(uint32_t, uint32_t, uint32_t, uint32_t) override {}
  void SetDepthTestEnabled(bool) override {}
  void SetDepthTestFunction(dp::TestFunction) override {}
  void SetStencilTestEnabled(bool) override {}
  void SetStencilFunction(dp::StencilFace, dp::TestFunction) override {}
  void SetStencilActions(dp::StencilFace, dp::StencilAction, dp::StencilAction, dp::StencilAction) override {}
  void SetStencilReferenceValue(uint32_t) override {}

private:
  dp::ApiVersion m_apiVersion = dp::ApiVersion::OpenGLES2;
};
