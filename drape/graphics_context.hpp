#pragma once

#include "drape/drape_global.hpp"
#include "drape/pointers.hpp"

#include <string>

namespace dp
{
enum ClearBits : uint32_t
{
  ColorBit = 1,
  DepthBit = 1 << 1,
  StencilBit = 1 << 2
};
  
uint32_t constexpr kClearBitsStoreAll = ClearBits::ColorBit | ClearBits::DepthBit | ClearBits::StencilBit;

enum class TestFunction : uint8_t
{
  Never = 0,
  Less,
  Equal,
  LessOrEqual,
  Greater,
  NotEqual,
  GreaterOrEqual,
  Always
};

enum class StencilFace : uint8_t
{
  Front = 0,
  Back,
  FrontAndBack
};

enum class StencilAction : uint8_t
{
  Keep = 0,
  Zero,
  Replace,
  Increment,
  IncrementWrap,
  Decrement,
  DecrementWrap,
  Invert
};

class GraphicsContext
{
public:
  virtual ~GraphicsContext() = default;
  virtual bool BeginRendering() { return true; }
  virtual void EndRendering() {}
  virtual void Present() = 0;
  virtual void MakeCurrent() = 0;
  virtual void DoneCurrent() {}
  // The value 'nullptr' means default(system) framebuffer.
  virtual void SetFramebuffer(ref_ptr<BaseFramebuffer> framebuffer) = 0;
  virtual void ForgetFramebuffer(ref_ptr<BaseFramebuffer> framebuffer) = 0;
  virtual void ApplyFramebuffer(std::string const & framebufferLabel) = 0;
  // w, h - pixel size of render target (logical size * visual scale).
  virtual void Resize(int /* w */, int /* h */) {}
  virtual void SetRenderingEnabled(bool /* enabled */) {}
  virtual void SetPresentAvailable(bool /* available */) {}
  virtual bool Validate() { return true; }
  virtual void CollectMemory() {}

  virtual void Init(ApiVersion apiVersion) = 0;
  virtual ApiVersion GetApiVersion() const = 0;
  virtual std::string GetRendererName() const = 0;
  virtual std::string GetRendererVersion() const = 0;
  virtual bool HasPartialTextureUpdates() const { return true; }

  virtual void DebugSynchronizeWithCPU() {}
  virtual void PushDebugLabel(std::string const & label) = 0;
  virtual void PopDebugLabel() = 0;

  virtual void SetClearColor(Color const & color) = 0;
  virtual void Clear(uint32_t clearBits, uint32_t storeBits) = 0;
  virtual void Flush() = 0;
  virtual void SetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) = 0;
  virtual void SetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h) = 0;
  virtual void SetDepthTestEnabled(bool enabled) = 0;
  virtual void SetDepthTestFunction(TestFunction depthFunction) = 0;
  virtual void SetStencilTestEnabled(bool enabled) = 0;
  virtual void SetStencilFunction(StencilFace face, TestFunction stencilFunction) = 0;
  virtual void SetStencilActions(StencilFace face, StencilAction stencilFailAction,
                                 StencilAction depthFailAction, StencilAction passAction) = 0;
  virtual void SetStencilReferenceValue(uint32_t stencilReferenceValue) = 0;
  virtual void SetCullingEnabled(bool enabled) = 0;
};
}  // namespace dp
