#pragma once
#import <MetalKit/MetalKit.h>

#include "drape/data_buffer.hpp"
#include "drape/data_buffer_impl.hpp"
#include "drape/metal/metal_base_context.hpp"
#include "drape/pointers.hpp"

#include "base/assert.hpp"

#include <cstdint>
#include <utility>

namespace dp
{
namespace metal
{
class MetalGPUBuffer : public BufferBase
{
public:
  MetalGPUBuffer(ref_ptr<GraphicsContext> context, void const * data,
                 uint8_t elementSize, uint32_t capacity);
  
  void UploadData(void const * data, uint32_t elementCount);
  
  void * Map(uint32_t elementOffset, uint32_t elementCount);
  
  void UpdateData(void * gpuPtr, void const * data,
                  uint32_t elementOffset, uint32_t elementCount);
  
  id<MTLBuffer> GetMetalBuffer() const { return m_metalBuffer; }
  
protected:
  void Resize(ref_ptr<MetalBaseContext> context, void const * data, uint32_t elementCount);
  
  id<MTLBuffer> m_metalBuffer;
};
  
class MetalGpuBufferImpl : public DataBufferImpl<MetalGPUBuffer>
{
public:
  template <typename... Args>
  MetalGpuBufferImpl(Args &&... params)
    : DataBufferImpl(std::forward<Args>(params)...)
  {}
  
  void const * Data() const override
  {
    ASSERT(false, ("Retrieving of raw data is unavailable for GPU buffer"));
    return nullptr;
  }
  
  void UploadData(ref_ptr<GraphicsContext> context,
                  void const * data, uint32_t elementCount) override
  {
    UNUSED_VALUE(context);
    m_buffer->UploadData(data, elementCount);
  }
  
  void UpdateData(void * destPtr, void const * srcPtr, uint32_t elementOffset,
                  uint32_t elementCount) override
  {
    m_buffer->UpdateData(destPtr, srcPtr, elementOffset, elementCount);
  }
  
  void * Map(ref_ptr<GraphicsContext> context,
             uint32_t elementOffset, uint32_t elementCount) override
  {
    UNUSED_VALUE(context);
    return m_buffer->Map(elementOffset, elementCount);
  }
  
  id<MTLBuffer> GetMetalBuffer() const { return m_buffer->GetMetalBuffer(); }
  
  void Bind() override {}
  void Unmap(ref_ptr<GraphicsContext>) override {}
};
}  // namespace metal
}  // namespace dp
