#pragma once
#import <MetalKit/MetalKit.h>

#include "drape/graphics_context.hpp"
#include "drape/gpu_program.hpp"
#include "drape/metal/metal_cleaner.hpp"
#include "drape/metal/metal_states.hpp"
#include "drape/metal/metal_texture.hpp"
#include "drape/pointers.hpp"
#include "drape/texture_types.hpp"

#include "geometry/point2d.hpp"

#include <cstdint>
#include <functional>

namespace dp
{
namespace metal
{
class MetalBaseContext : public dp::GraphicsContext
{
public:
  using DrawableRequest = std::function<id<CAMetalDrawable>()>;
  
  MetalBaseContext(id<MTLDevice> device, m2::PointU const & screenSize,
                   DrawableRequest && drawableRequest);
  
  bool BeginRendering() override;
  void EndRendering() override;
  void Present() override;
  void MakeCurrent() override {}
  void DoneCurrent() override {}
  bool Validate() override { return true; }
  void Resize(int w, int h) override;
  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override;
  void ForgetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override {}
  void ApplyFramebuffer(std::string const & framebufferLabel) override;
  void Init(ApiVersion apiVersion) override;
  ApiVersion GetApiVersion() const override;
  std::string GetRendererName() const override;
  std::string GetRendererVersion() const override;

  void DebugSynchronizeWithCPU() override;
  void PushDebugLabel(std::string const & label) override;
  void PopDebugLabel() override;
  
  void SetClearColor(Color const & color) override;
  void Clear(uint32_t clearBits, uint32_t storeBits) override;
  void Flush() override {}
  void SetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;
  void SetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;
  void SetDepthTestEnabled(bool enabled) override;
  void SetDepthTestFunction(TestFunction depthFunction) override;
  void SetStencilTestEnabled(bool enabled) override;
  void SetStencilFunction(StencilFace face, TestFunction stencilFunction) override;
  void SetStencilActions(StencilFace face, StencilAction stencilFailAction,
                         StencilAction depthFailAction, StencilAction passAction) override;
  void SetStencilReferenceValue(uint32_t stencilReferenceValue) override { m_stencilReferenceValue = stencilReferenceValue; }
  void SetCullingEnabled(bool enabled) override;

  id<MTLDevice> GetMetalDevice() const;
  id<MTLCommandBuffer> GetCommandBuffer() const;
  id<MTLRenderCommandEncoder> GetCommandEncoder() const;
  id<MTLDepthStencilState> GetDepthStencilState();
  id<MTLRenderPipelineState> GetPipelineState(ref_ptr<GpuProgram> program, bool blendingEnabled);
  id<MTLSamplerState> GetSamplerState(TextureFilter filter, TextureWrapping wrapSMode,
                                      TextureWrapping wrapTMode);
  
  void SetSystemPrograms(drape_ptr<GpuProgram> && programClearColor,
                         drape_ptr<GpuProgram> && programClearDepth,
                         drape_ptr<GpuProgram> && programClearColorAndDepth);
  
  void ApplyPipelineState(id<MTLRenderPipelineState> state);
  bool HasAppliedPipelineState() const;
  void ResetPipelineStatesCache();

  MTLRenderPassDescriptor * GetRenderPassDescriptor() const;
  
protected:
  void RecreateDepthTexture(m2::PointU const & screenSize);
  void RequestFrameDrawable();
  void ResetFrameDrawable();
  void FinishCurrentEncoding();

  id<MTLDevice> m_device;
  DrawableRequest m_drawableRequest;
  drape_ptr<MetalTexture> m_depthTexture;
  MTLRenderPassDescriptor * m_renderPassDescriptor;
  id<MTLCommandQueue> m_commandQueue;
  ref_ptr<dp::BaseFramebuffer> m_currentFramebuffer;
  MetalStates::DepthStencilKey m_currentDepthStencilKey;
  MetalStates m_metalStates;
  
  // These objects are recreated each frame. They MUST NOT be stored anywhere.
  id<CAMetalDrawable> m_frameDrawable;
  id<MTLCommandBuffer> m_frameCommandBuffer;
  id<MTLRenderCommandEncoder> m_currentCommandEncoder;
  id<MTLRenderPipelineState> m_lastPipelineState;
  
  MetalCleaner m_cleaner;
  
  uint32_t m_stencilReferenceValue = 1;
};
}  // namespace metal
}  // namespace dp
