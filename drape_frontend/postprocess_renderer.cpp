#include "drape_frontend/postprocess_renderer.hpp"
#include "drape_frontend/render_state_extension.hpp"
#include "drape_frontend/screen_quad_renderer.hpp"

#include "shaders/program_manager.hpp"

#include "drape/glsl_types.hpp"
#include "drape/graphics_context.hpp"
#include "drape/mesh_object.hpp"
#include "drape/render_state.hpp"

#include "platform/trace.hpp"

#include "base/assert.hpp"

namespace df
{
namespace
{
class SMAABaseRenderParams
{
public:
  explicit SMAABaseRenderParams(gpu::Program program)
    : m_state(CreateRenderState(program, DepthLayer::GeometryLayer))
  {
    m_state.SetDepthTestEnabled(false);
    m_state.SetBlending(dp::Blending(false));
  }
  virtual ~SMAABaseRenderParams() = default;

  dp::RenderState const & GetRenderState() const { return m_state; }
  gpu::SMAAProgramParams const & GetProgramParams() const { return m_params; }

protected:
  dp::RenderState m_state;
  gpu::SMAAProgramParams m_params;
};

class EdgesRenderParams : public SMAABaseRenderParams
{
  using TBase = SMAABaseRenderParams;
public:
  EdgesRenderParams(): TBase(gpu::Program::SmaaEdges) {}

  void SetParams(ref_ptr<dp::Texture> texture, uint32_t width, uint32_t height)
  {
    m_state.SetTexture("u_colorTex", texture);

    m_params.m_framebufferMetrics = glsl::vec4(1.0f / width, 1.0f / height,
                                               static_cast<float>(width),
                                               static_cast<float>(height));
  }
};

class BlendingWeightRenderParams : public SMAABaseRenderParams
{
  using TBase = SMAABaseRenderParams;
public:
  BlendingWeightRenderParams() : TBase(gpu::Program::SmaaBlendingWeight) {}

  void SetParams(ref_ptr<dp::Texture> edgesTexture, ref_ptr<dp::Texture> areaTexture,
                 ref_ptr<dp::Texture> searchTexture, uint32_t width, uint32_t height)
  {
    m_state.SetTexture("u_colorTex", edgesTexture);
    m_state.SetTexture("u_smaaArea", areaTexture);
    m_state.SetTexture("u_smaaSearch", searchTexture);

    m_params.m_framebufferMetrics = glsl::vec4(1.0f / width, 1.0f / height,
                                               static_cast<float>(width),
                                               static_cast<float>(height));
  }
};

class SMAAFinalRenderParams : public SMAABaseRenderParams
{
  using TBase = SMAABaseRenderParams;
public:
  SMAAFinalRenderParams(): TBase(gpu::Program::SmaaFinal) {}

  void SetParams(ref_ptr<dp::Texture> colorTexture, ref_ptr<dp::Texture> blendingWeightTexture,
                 uint32_t width, uint32_t height)
  {
    m_state.SetTexture("u_colorTex", colorTexture);
    m_state.SetTexture("u_blendingWeightTex", blendingWeightTexture);

    m_params.m_framebufferMetrics = glsl::vec4(1.0f / width, 1.0f / height,
                                               static_cast<float>(width),
                                               static_cast<float>(height));
  }
};

class DefaultScreenQuadRenderParams
{
public:
  DefaultScreenQuadRenderParams()
    : m_state(CreateRenderState(gpu::Program::ScreenQuad, DepthLayer::GeometryLayer))
  {
    m_state.SetDepthTestEnabled(false);
    m_state.SetBlending(dp::Blending(false));
  }

  void SetParams(ref_ptr<dp::Texture> texture)
  {
    m_state.SetTexture("u_colorTex", texture);
  }

  dp::RenderState const & GetRenderState() const { return m_state; }
  gpu::ScreenQuadProgramParams const & GetProgramParams() const { return m_params; }

private:
  dp::RenderState m_state;
  gpu::ScreenQuadProgramParams m_params;
};

void InitFramebuffer(ref_ptr<dp::GraphicsContext> context,
                     drape_ptr<dp::Framebuffer> & framebuffer,
                     uint32_t width, uint32_t height,
                     bool depthEnabled, bool stencilEnabled)
{
  if (framebuffer == nullptr)
  {
    framebuffer = make_unique_dp<dp::Framebuffer>(dp::TextureFormat::RGBA8, depthEnabled,
                                                  stencilEnabled);
  }
  framebuffer->SetSize(context, width, height);
}

void InitFramebuffer(ref_ptr<dp::GraphicsContext> context,
                     drape_ptr<dp::Framebuffer> & framebuffer,
                     dp::TextureFormat colorFormat,
                     ref_ptr<dp::Framebuffer::DepthStencil> depthStencilRef,
                     uint32_t width, uint32_t height)
{
  if (framebuffer == nullptr)
    framebuffer = make_unique_dp<dp::Framebuffer>(colorFormat);
  framebuffer->SetDepthStencilRef(std::move(depthStencilRef));
  framebuffer->SetSize(context, width, height);
}

bool IsSupported(drape_ptr<dp::Framebuffer> const & framebuffer)
{
  return framebuffer != nullptr && framebuffer->IsSupported();
}
}  // namespace

PostprocessRenderer::~PostprocessRenderer()
{
  ClearContextDependentResources();
}

void PostprocessRenderer::Init(ref_ptr<dp::GraphicsContext> context, dp::FramebufferFallback && fallback,
                               PrerenderFrame && prerenderFrame)
{
  TRACE_SECTION("[drape] PostprocessRenderer::Init");
  m_apiVersion = context->GetApiVersion();
  m_screenQuadRenderer = make_unique_dp<ScreenQuadRenderer>(context);
  m_framebufferFallback = std::move(fallback);
  ASSERT(m_framebufferFallback != nullptr, ());
  m_prerenderFrame = std::move(prerenderFrame);
  ASSERT(m_prerenderFrame != nullptr, ());
}

void PostprocessRenderer::ClearContextDependentResources()
{
  m_screenQuadRenderer.reset();
  m_framebufferFallback = nullptr;
  m_prerenderFrame = nullptr;
  m_staticTextures.reset();

  m_mainFramebuffer.reset();
  m_isMainFramebufferRendered = false;
  m_edgesFramebuffer.reset();
  m_blendingWeightFramebuffer.reset();
  m_smaaFramebuffer.reset();
  m_isSmaaFramebufferRendered = false;
}

void PostprocessRenderer::Resize(ref_ptr<dp::GraphicsContext> context, uint32_t width, uint32_t height)
{
  m_width = width;
  m_height = height;

  UpdateFramebuffers(context, m_width, m_height);
}

void PostprocessRenderer::SetStaticTextures(drape_ptr<PostprocessStaticTextures> && textures)
{
  m_staticTextures = std::move(textures);
}

bool PostprocessRenderer::IsEnabled() const
{
  // Do not use post processing in routing following mode by energy-saving reasons.
  // For Metal rendering to the texture is more efficient,
  // since nextDrawable will be requested later.

  if (m_apiVersion != dp::ApiVersion::Metal && m_isRouteFollowingActive)
    return false;

  return IsSupported(m_mainFramebuffer);
}

void PostprocessRenderer::SetEffectEnabled(ref_ptr<dp::GraphicsContext> context,
                                           Effect effect, bool enabled)
{
  // Do not support AA for OpenGLES 2.0.
  if (m_apiVersion == dp::ApiVersion::OpenGLES2 && effect == Effect::Antialiasing)
    return;

  auto const oldValue = m_effects;
  auto const effectMask = static_cast<uint32_t>(effect);
  m_effects = (m_effects & ~effectMask) | (enabled ? effectMask : 0);

  if (m_width != 0 && m_height != 0 && oldValue != m_effects)
    UpdateFramebuffers(context, m_width, m_height);
}

bool PostprocessRenderer::IsEffectEnabled(Effect effect) const
{
  return (m_effects & static_cast<uint32_t>(effect)) > 0;
}

bool PostprocessRenderer::CanRenderAntialiasing() const
{
  if (!IsEffectEnabled(Effect::Antialiasing))
    return false;

  if (!IsSupported(m_edgesFramebuffer) || !IsSupported(m_blendingWeightFramebuffer) ||
      !IsSupported(m_smaaFramebuffer))
  {
    return false;
  }

  if (m_staticTextures == nullptr ||
      m_staticTextures->m_smaaSearchTexture == nullptr ||
      m_staticTextures->m_smaaAreaTexture == nullptr)
  {
    return false;
  }

  if (m_apiVersion == dp::ApiVersion::OpenGLES2 || m_apiVersion == dp::ApiVersion::OpenGLES3)
  {
    return m_staticTextures->m_smaaAreaTexture->GetID() != 0 &&
           m_staticTextures->m_smaaSearchTexture->GetID() != 0;
  }
  return true;
}

bool PostprocessRenderer::BeginFrame(ref_ptr<dp::GraphicsContext> context, ScreenBase const & modelView,
                                     bool activeFrame)
{
  TRACE_SECTION("[drape] PostprocessRenderer::BeginFrame");
  if (!IsEnabled())
  {
    CHECK(m_prerenderFrame != nullptr, ());
    m_prerenderFrame(modelView);

    CHECK(m_framebufferFallback != nullptr, ());
    return m_framebufferFallback();
  }

  m_frameStarted = activeFrame || !m_isMainFramebufferRendered;
  if (m_frameStarted)
  {
    CHECK(m_prerenderFrame != nullptr, ());
    m_prerenderFrame(modelView);

    context->SetFramebuffer(make_ref(m_mainFramebuffer));
  }

  if (m_frameStarted && CanRenderAntialiasing())
    context->SetStencilTestEnabled(false);

  m_isMainFramebufferRendered = true;
  return m_frameStarted;
}

bool PostprocessRenderer::EndFrame(ref_ptr<dp::GraphicsContext> context,
                                   ref_ptr<gpu::ProgramManager> gpuProgramManager,
                                   dp::Viewport const & viewport)
{
  TRACE_SECTION("[drape] PostprocessRenderer::EndFrame");
  if (!IsEnabled())
    return true;

  // Subpixel Morphological Antialiasing (SMAA).
  if (m_frameStarted && CanRenderAntialiasing())
  {
    ASSERT(m_staticTextures->m_smaaAreaTexture != nullptr, ());
    ASSERT(m_staticTextures->m_smaaSearchTexture != nullptr, ());

    if (m_apiVersion == dp::ApiVersion::OpenGLES2 || m_apiVersion == dp::ApiVersion::OpenGLES3)
    {
      ASSERT_GREATER(m_staticTextures->m_smaaAreaTexture->GetID(), 0, ());
      ASSERT_GREATER(m_staticTextures->m_smaaSearchTexture->GetID(), 0, ());
    }

    context->SetClearColor(dp::Color::Transparent());
    context->SetStencilTestEnabled(true);

    // Render edges to texture.
    {
      TRACE_SECTION("[drape][SMAA] Edges rendering");
      context->SetFramebuffer(make_ref(m_edgesFramebuffer));
      context->Clear(dp::ClearBits::ColorBit, dp::ClearBits::ColorBit | dp::ClearBits::StencilBit /* storeBits */);
      if (m_apiVersion == dp::ApiVersion::Metal || m_apiVersion == dp::ApiVersion::Vulkan)
        context->SetStencilFunction(dp::StencilFace::FrontAndBack, dp::TestFunction::Greater);
      else
        context->SetStencilFunction(dp::StencilFace::FrontAndBack, dp::TestFunction::NotEqual);
      context->SetStencilActions(dp::StencilFace::FrontAndBack, dp::StencilAction::Zero,
                                 dp::StencilAction::Zero, dp::StencilAction::Replace);
      context->SetStencilReferenceValue(1);
      context->ApplyFramebuffer("SMAA edges");
      viewport.Apply(context);

      EdgesRenderParams params;
      params.SetParams(m_mainFramebuffer->GetTexture(), m_width, m_height);

      auto program = gpuProgramManager->GetProgram(params.GetRenderState().GetProgram<gpu::Program>());
      m_screenQuadRenderer->Render(context, program, params.GetRenderState(), gpuProgramManager->GetParamsSetter(),
                                   params.GetProgramParams());
    }

    // Render blending weight to texture.
    {
      TRACE_SECTION("[drape][SMAA] Blending weight rendering");
      context->SetFramebuffer(make_ref(m_blendingWeightFramebuffer));
      context->Clear(dp::ClearBits::ColorBit, dp::ClearBits::ColorBit | dp::ClearBits::StencilBit /* storeBits */);
      context->SetStencilFunction(dp::StencilFace::FrontAndBack, dp::TestFunction::Equal);
      context->SetStencilActions(dp::StencilFace::FrontAndBack, dp::StencilAction::Keep,
                                 dp::StencilAction::Keep, dp::StencilAction::Keep);
      context->ApplyFramebuffer("SMAA blending");
      viewport.Apply(context);

      BlendingWeightRenderParams params;
      params.SetParams(m_edgesFramebuffer->GetTexture(),
                       m_staticTextures->m_smaaAreaTexture,
                       m_staticTextures->m_smaaSearchTexture,
                       m_width, m_height);

      auto program = gpuProgramManager->GetProgram(params.GetRenderState().GetProgram<gpu::Program>());
      m_screenQuadRenderer->Render(context, program, params.GetRenderState(), gpuProgramManager->GetParamsSetter(),
                                   params.GetProgramParams());
    }

    // SMAA final pass.
    context->SetStencilTestEnabled(false);
    {
      TRACE_SECTION("[drape][SMAA] Final pass rendering");
      context->SetFramebuffer(make_ref(m_smaaFramebuffer));
      context->Clear(dp::ClearBits::ColorBit, dp::ClearBits::ColorBit /* storeBits */);
      context->ApplyFramebuffer("SMAA final");
      viewport.Apply(context);

      SMAAFinalRenderParams params;
      params.SetParams(m_mainFramebuffer->GetTexture(),
                       m_blendingWeightFramebuffer->GetTexture(),
                       m_width, m_height);

      auto program = gpuProgramManager->GetProgram(params.GetRenderState().GetProgram<gpu::Program>());
      m_screenQuadRenderer->Render(context, program, params.GetRenderState(), gpuProgramManager->GetParamsSetter(),
                                   params.GetProgramParams());

      m_isSmaaFramebufferRendered = true;
    }
  }

  ref_ptr<dp::Framebuffer> finalFramebuffer;
  if (m_isSmaaFramebufferRendered)
    finalFramebuffer = make_ref(m_smaaFramebuffer);
  else
    finalFramebuffer = make_ref(m_mainFramebuffer);

  CHECK(m_framebufferFallback != nullptr, ());
  bool m_wasRendered = false;
  if (m_framebufferFallback())
  {
    TRACE_SECTION("[drape] Postprocessing composition");
    context->Clear(dp::ClearBits::ColorBit, dp::ClearBits::ColorBit /* storeBits */);
    context->ApplyFramebuffer("Dynamic frame");
    viewport.Apply(context);

    DefaultScreenQuadRenderParams params;
    params.SetParams(finalFramebuffer->GetTexture());

    auto program = gpuProgramManager->GetProgram(params.GetRenderState().GetProgram<gpu::Program>());
    m_screenQuadRenderer->Render(context, program, params.GetRenderState(), gpuProgramManager->GetParamsSetter(),
                                 params.GetProgramParams());

    m_wasRendered = true;
  }
  m_frameStarted = false;
  return m_wasRendered;
}

void PostprocessRenderer::EnableWritingToStencil(ref_ptr<dp::GraphicsContext> context) const
{
  if (!m_frameStarted || !CanRenderAntialiasing())
    return;
  context->SetStencilTestEnabled(true);
  context->SetStencilFunction(dp::StencilFace::FrontAndBack, dp::TestFunction::Always);
  context->SetStencilActions(dp::StencilFace::FrontAndBack, dp::StencilAction::Keep,
                             dp::StencilAction::Keep, dp::StencilAction::Replace);
}

void PostprocessRenderer::DisableWritingToStencil(ref_ptr<dp::GraphicsContext> context) const
{
  if (!m_frameStarted || !CanRenderAntialiasing())
    return;
  context->SetStencilTestEnabled(false);
}

void PostprocessRenderer::UpdateFramebuffers(ref_ptr<dp::GraphicsContext> context,
                                             uint32_t width, uint32_t height)
{
  TRACE_SECTION("[drape] PostprocessRenderer::UpdateFramebuffers");
  ASSERT_NOT_EQUAL(width, 0, ());
  ASSERT_NOT_EQUAL(height, 0, ());

  CHECK_EQUAL(m_apiVersion, context->GetApiVersion(), ());
  InitFramebuffer(context, m_mainFramebuffer, width, height, true /* depthEnabled */,
                  m_apiVersion != dp::ApiVersion::OpenGLES2 /* stencilEnabled */);
  m_isMainFramebufferRendered = false;

  m_isSmaaFramebufferRendered = false;
  if (!m_isRouteFollowingActive && IsEffectEnabled(Effect::Antialiasing))
  {
    CHECK_NOT_EQUAL(m_apiVersion, dp::ApiVersion::OpenGLES2, ());

    InitFramebuffer(context, m_edgesFramebuffer, dp::TextureFormat::RedGreen,
                    m_mainFramebuffer->GetDepthStencilRef(),
                    width, height);
    InitFramebuffer(context, m_blendingWeightFramebuffer, dp::TextureFormat::RGBA8,
                    m_mainFramebuffer->GetDepthStencilRef(),
                    width, height);
    InitFramebuffer(context, m_smaaFramebuffer, dp::TextureFormat::RGBA8,
                    m_mainFramebuffer->GetDepthStencilRef(),
                    width, height);
  }
  else
  {
    context->ForgetFramebuffer(make_ref(m_edgesFramebuffer));
    context->ForgetFramebuffer(make_ref(m_blendingWeightFramebuffer));
    context->ForgetFramebuffer(make_ref(m_smaaFramebuffer));

    m_edgesFramebuffer.reset();
    m_blendingWeightFramebuffer.reset();
    m_smaaFramebuffer.reset();
  }
}

bool PostprocessRenderer::OnFramebufferFallback(ref_ptr<dp::GraphicsContext> context)
{
  if (m_frameStarted)
  {
    context->SetFramebuffer(make_ref(m_mainFramebuffer));
    return true;
  }

  return m_framebufferFallback();
}

void PostprocessRenderer::OnChangedRouteFollowingMode(ref_ptr<dp::GraphicsContext> context,
                                                      bool isRouteFollowingActive)
{
  if (m_isRouteFollowingActive == isRouteFollowingActive)
    return;

  m_isRouteFollowingActive = isRouteFollowingActive;
  if (m_width != 0 && m_height != 0)
    UpdateFramebuffers(context, m_width, m_height);
}

StencilWriterGuard::StencilWriterGuard(ref_ptr<PostprocessRenderer> renderer,
                                       ref_ptr<dp::GraphicsContext> context)
  : m_renderer(std::move(renderer))
  , m_context(std::move(context))
{
  ASSERT(m_renderer != nullptr, ());
  m_renderer->EnableWritingToStencil(m_context);
}

StencilWriterGuard::~StencilWriterGuard()
{
  m_renderer->DisableWritingToStencil(m_context);
}
}  // namespace df
