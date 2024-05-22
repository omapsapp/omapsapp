#pragma once

#include "drape/attribute_buffer_mutator.hpp"
#include "drape/binding_info.hpp"
#include "drape/data_buffer.hpp"
#include "drape/gpu_program.hpp"
#include "drape/index_buffer.hpp"
#include "drape/index_buffer_mutator.hpp"
#include "drape/pointers.hpp"

#include <cstdint>
#include <map>
#include <vector>

namespace dp
{
struct IndicesRange
{
  uint32_t m_idxStart;
  uint32_t m_idxCount;

  IndicesRange() : m_idxStart(0), m_idxCount(0) {}
  IndicesRange(uint32_t idxStart, uint32_t idxCount) : m_idxStart(idxStart), m_idxCount(idxCount) {}
  bool IsValid() const { return m_idxCount != 0; }
};

using BuffersMap = std::map<BindingInfo, drape_ptr<DataBuffer>>;

class VertexArrayBufferImpl
{
public:
  virtual ~VertexArrayBufferImpl() = default;
  virtual bool Build(ref_ptr<GpuProgram> program) = 0;
  virtual bool Bind() = 0;
  virtual void Unbind() = 0;
  virtual void BindBuffers(BuffersMap const & buffers) const = 0;
  virtual void RenderRange(ref_ptr<GraphicsContext> context,
                           bool drawAsLine, IndicesRange const & range) = 0;

  virtual void AddBindingInfo(dp::BindingInfo const & /* bindingInfo */) {}
};

namespace metal
{
class MetalVertexArrayBufferImpl;
}  // namespace metal

namespace vulkan
{
class VulkanVertexArrayBufferImpl;
}  // namespace vulkan

class VertexArrayBuffer
{
  friend class metal::MetalVertexArrayBufferImpl;
  friend class vulkan::VulkanVertexArrayBufferImpl;
public:
  VertexArrayBuffer(uint32_t indexBufferSize, uint32_t dataBufferSize, uint64_t batcherHash);
  ~VertexArrayBuffer();

  // This method must be called on a reading thread, before VAO will be transferred to the render thread.
  void Preflush(ref_ptr<GraphicsContext> context);

  // OES_vertex_array_object create OpenGL resource that belong only one GL context (which was
  // created by). By this reason Build/Bind and Render must be called only on FrontendRenderer
  // thread.
  void Render(ref_ptr<GraphicsContext> context, bool drawAsLine);
  void RenderRange(ref_ptr<GraphicsContext> context, bool drawAsLine, IndicesRange const & range);
  void Build(ref_ptr<GraphicsContext> context, ref_ptr<GpuProgram> program);

  uint32_t GetAvailableVertexCount() const;
  uint32_t GetAvailableIndexCount() const;
  uint32_t GetStartIndexValue() const;
  uint32_t GetDynamicBufferOffset(BindingInfo const & bindingInfo);
  uint32_t GetIndexCount() const;

  void UploadData(ref_ptr<GraphicsContext> context, BindingInfo const & bindingInfo,
                  void const * data, uint32_t count);
  void UploadIndices(ref_ptr<GraphicsContext> context, void const * data, uint32_t count);

  void ApplyMutation(ref_ptr<GraphicsContext> context,
                     ref_ptr<IndexBufferMutator> indexMutator,
                     ref_ptr<AttributeBufferMutator> attrMutator);

  void ResetChangingTracking() { m_isChanged = false; }
  bool IsChanged() const { return m_isChanged; }
  bool HasBuffers() const { return !m_staticBuffers.empty() || !m_dynamicBuffers.empty(); }

private:
  ref_ptr<DataBuffer> GetOrCreateStaticBuffer(BindingInfo const & bindingInfo);
  ref_ptr<DataBuffer> GetOrCreateDynamicBuffer(BindingInfo const & bindingInfo);
  ref_ptr<DataBuffer> GetDynamicBuffer(BindingInfo const & bindingInfo) const;

  ref_ptr<DataBuffer> GetOrCreateBuffer(BindingInfo const & bindingInfo, bool isDynamic);
  ref_ptr<DataBuffer> GetBuffer(BindingInfo const & bindingInfo, bool isDynamic) const;
  bool Bind() const;
  void Unbind() const;
  void BindStaticBuffers() const;
  void BindDynamicBuffers() const;
  void BindBuffers(BuffersMap const & buffers) const;

  ref_ptr<DataBufferBase> GetIndexBuffer() const;

  void PreflushImpl(ref_ptr<GraphicsContext> context);

  void CollectBindingInfo(dp::BindingInfo const & bindingInfo);

  // Definition of this method is in a .mm-file.
  drape_ptr<VertexArrayBufferImpl> CreateImplForMetal(ref_ptr<VertexArrayBuffer> buffer);

  // Definition of this method is in a separate .cpp-file.
  drape_ptr<VertexArrayBufferImpl> CreateImplForVulkan(ref_ptr<GraphicsContext> context,
                                                       ref_ptr<VertexArrayBuffer> buffer,
                                                       BindingInfoArray && bindingInfo,
                                                       uint8_t bindingInfoCount);

  uint32_t const m_dataBufferSize;
  uint64_t const m_batcherHash;

  drape_ptr<VertexArrayBufferImpl> m_impl;
  BuffersMap m_staticBuffers;
  BuffersMap m_dynamicBuffers;

  drape_ptr<IndexBuffer> m_indexBuffer;

  bool m_isPreflushed = false;
  bool m_moveToGpuOnBuild = false;
  bool m_isChanged = false;
  BindingInfoArray m_bindingInfo;
  uint8_t m_bindingInfoCount = 0;
};
}  // namespace dp
