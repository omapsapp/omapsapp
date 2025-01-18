#pragma once

#include "drape/drape_global.hpp"
#include "drape/graphics_context.hpp"
#include "drape/pointers.hpp"
#include "drape/render_state.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace dp
{
class GpuProgram;
class GraphicsContext;
class MeshObjectImpl;

namespace metal
{
class MetalMeshObjectImpl;
}  // namespace metal

namespace vulkan
{
class VulkanMeshObjectImpl;
}  // namespace vulkan

// This class implements a simple mesh object which does not use an index buffer.
// Use this class only for simple geometry.
class MeshObject
{
  friend class MeshObjectImpl;
  friend class GLMeshObjectImpl;
  friend class metal::MetalMeshObjectImpl;
  friend class vulkan::VulkanMeshObjectImpl;

public:
  enum class DrawPrimitive: uint8_t
  {
    Triangles,
    TriangleStrip,
    LineStrip
  };

  MeshObject(ref_ptr<dp::GraphicsContext> context, DrawPrimitive drawPrimitive,
             std::string const & debugName = "");
  virtual ~MeshObject();

  template<typename T>
  void SetBuffer(uint32_t bufferInd, std::vector<T> && vertices, uint32_t stride = 0)
  {
    CHECK_LESS_OR_EQUAL(bufferInd, GetNextBufferIndex(), ());

    if (bufferInd == GetNextBufferIndex())
      m_buffers.emplace_back(make_unique_dp<VertexBuffer<T>>(std::move(vertices), stride));
    else
      m_buffers[bufferInd] = make_unique_dp<VertexBuffer<T>>(std::move(vertices), stride);

    Reset();
  }

  void SetAttribute(std::string const & attributeName, uint32_t bufferInd, uint32_t offset, 
                    uint32_t componentsCount, glConst type = gl_const::GLFloatType);

  template<typename T>
  void UpdateBuffer(ref_ptr<dp::GraphicsContext> context, uint32_t bufferInd, std::vector<T> const & vertices)
  {
    CHECK(m_initialized, ());
    CHECK_LESS(bufferInd, static_cast<uint32_t>(m_buffers.size()), ());
    CHECK(!vertices.empty(), ());

    auto & buffer = m_buffers[bufferInd];
    CHECK_LESS_OR_EQUAL(static_cast<uint32_t>(vertices.size() * sizeof(T)), buffer->GetSizeInBytes(), ());
    memcpy(buffer->GetData(), vertices.data(), vertices.size() * sizeof(T));

    UpdateImpl(context, bufferInd);
  }

  void SetIndexBuffer(std::vector<uint16_t> && indices);
  void UpdateIndexBuffer(ref_ptr<dp::GraphicsContext> context, std::vector<uint16_t> const & indices);

  template <typename TParamsSetter, typename TParams>
  void Render(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
              dp::RenderState const & state, ref_ptr<TParamsSetter> paramsSetter,
              TParams const & params)
  {
    Bind(context, program);

    ApplyState(context, program, state);
    paramsSetter->Apply(context, program, params);

    DrawPrimitives(context);

    Unbind(program);
  }

  template <typename TParamsSetter, typename TParams>
  void Render(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program,
              dp::RenderState const & state, ref_ptr<TParamsSetter> paramsSetter,
              TParams const & params, std::function<void()> && drawCallback)
  {
    Bind(context, program);

    ApplyState(context, program, state);
    paramsSetter->Apply(context, program, params);

    CHECK(drawCallback, ());
    drawCallback();

    Unbind(program);
  }
  
  uint32_t GetNextBufferIndex() const { return static_cast<uint32_t>(m_buffers.size()); }

  bool IsInitialized() const { return m_initialized; }
  void Build(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program);
  void Reset();

  // Should be called inside draw callback in Render() method
  void DrawPrimitivesSubset(ref_ptr<dp::GraphicsContext> context, uint32_t vertexCount,
                             uint32_t startVertex);
  void DrawPrimitivesSubsetIndexed(ref_ptr<dp::GraphicsContext> context, uint32_t indexCount, 
                                   uint32_t startIndex);

  static std::vector<float> GenerateNormalsForTriangles(std::vector<float> const & vertices, size_t componentsCount);

private:
  struct AttributeMapping
  {
    AttributeMapping() = default;

    AttributeMapping(std::string const & attributeName, uint32_t offset, uint32_t componentsCount, 
                     glConst type = gl_const::GLFloatType)
      : m_attributeName(attributeName)
      , m_offset(offset)
      , m_componentsCount(componentsCount)
      , m_type(type)
    {}

    std::string m_attributeName;
    uint32_t m_offset = 0;
    uint32_t m_componentsCount = 0;
    glConst m_type = gl_const::GLFloatType;
  };

  class VertexBufferBase {
  public:
    virtual ~VertexBufferBase() = default;
    virtual void * GetData() = 0;
    virtual uint32_t GetSizeInBytes() const = 0;
    virtual uint32_t GetStrideInBytes() const = 0;

    uint32_t m_bufferId = 0;
    std::vector<AttributeMapping> m_attributes;
  };

  template<typename T>
  class VertexBuffer : public VertexBufferBase
  {
  public:
    VertexBuffer() = default;

    VertexBuffer(std::vector<T> && data, uint32_t stride = 0)
      : m_data(std::move(data))
      , m_stride(stride == 0 ? sizeof(T) : stride)
    {
      CHECK_GREATER_OR_EQUAL(m_stride, sizeof(T), ());
    }

    void * GetData() override { return m_data.data(); }
    uint32_t GetSizeInBytes() const override { return static_cast<uint32_t>(m_data.size() * sizeof(T)); }
    uint32_t GetStrideInBytes() const override { return m_stride; }
    
  private:  
    std::vector<T> m_data;
    uint32_t m_stride = 0;
  };

  void InitForOpenGL();
  void InitForVulkan(ref_ptr<dp::GraphicsContext> context);

#if defined(OMIM_METAL_AVAILABLE)
  // Definition of this method is in a .mm-file.
  void InitForMetal();
#endif

  void UpdateImpl(ref_ptr<dp::GraphicsContext> context, uint32_t bufferInd);

  void Bind(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program);
  void Unbind(ref_ptr<dp::GpuProgram> program);
  void DrawPrimitives(ref_ptr<dp::GraphicsContext> context);

  std::vector<drape_ptr<VertexBufferBase>> m_buffers;
  std::vector<uint16_t> m_indices;
  DrawPrimitive m_drawPrimitive = DrawPrimitive::Triangles;

  drape_ptr<MeshObjectImpl> m_impl;
  bool m_initialized = false;

  std::string m_debugName;
};

class MeshObjectImpl
{
public:
  virtual ~MeshObjectImpl() = default;
  virtual void Build(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program) = 0;
  virtual void Reset() = 0;
  virtual void UpdateBuffer(ref_ptr<dp::GraphicsContext> context, uint32_t bufferInd) = 0;
  virtual void UpdateIndexBuffer(ref_ptr<dp::GraphicsContext> context) = 0;
  virtual void Bind(ref_ptr<dp::GpuProgram> program) = 0;
  virtual void Unbind() = 0;
  virtual void DrawPrimitives(ref_ptr<dp::GraphicsContext> context, uint32_t verticesCount,
                              uint32_t startVertex) = 0;
  virtual void DrawPrimitivesIndexed(ref_ptr<dp::GraphicsContext> context, uint32_t indexCount, 
                                     uint32_t startIndex) = 0;
};
}  // namespace dp
