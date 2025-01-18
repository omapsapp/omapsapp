#import <MetalKit/MetalKit.h>

#include "drape/metal/metal_base_context.hpp"
#include "drape/mesh_object.hpp"
#include "drape/pointers.hpp"

#include "base/assert.hpp"

#include <cstdint>
#include <cstring>
#include <sstream>

namespace dp
{
namespace metal
{
namespace
{
MTLPrimitiveType GetPrimitiveType(MeshObject::DrawPrimitive primitive)
{
  switch (primitive)
  {
  case MeshObject::DrawPrimitive::Triangles: return MTLPrimitiveTypeTriangle;
  case MeshObject::DrawPrimitive::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
  case MeshObject::DrawPrimitive::LineStrip: return MTLPrimitiveTypeLineStrip;
  }
  CHECK(false, ("Unsupported type"));
}
}  // namespace

class MetalMeshObjectImpl : public MeshObjectImpl
{
public:
  MetalMeshObjectImpl(ref_ptr<dp::MeshObject> mesh)
    : m_mesh(std::move(mesh))
  {}
  
  void Build(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program) override
  {
    ref_ptr<dp::metal::MetalBaseContext> metalContext = context;
    id<MTLDevice> device = metalContext->GetMetalDevice();
    
    m_geometryBuffers.resize(m_mesh->m_buffers.size());
    for (size_t i = 0; i < m_mesh->m_buffers.size(); i++)
    { 
      auto const sizeInBytes = m_mesh->m_buffers[i]->GetSizeInBytes();
      if (sizeInBytes == 0)
        continue;
      
      m_geometryBuffers[i] = [device newBufferWithBytes:m_mesh->m_buffers[i]->GetData()
                                                 length:sizeInBytes
                                                options:MTLResourceCPUCacheModeWriteCombined];
      std::ostringstream ss;
      ss << "MeshVB:";
      for (size_t j = 0; j < m_mesh->m_buffers[i]->m_attributes.size(); j++)
      {
        ss << m_mesh->m_buffers[i]->m_attributes[j].m_attributeName;
        if (j + 1 < m_mesh->m_buffers[i]->m_attributes.size())
          ss << "+";
      }
      m_geometryBuffers[i].label = @(ss.str().c_str());
    }

    if (!m_mesh->m_indices.empty())
    {
      m_indexBuffer = [device newBufferWithBytes:m_mesh->m_indices.data()
                                          length:m_mesh->m_indices.size() * sizeof(uint16_t)
                                         options:MTLResourceCPUCacheModeWriteCombined];
      m_indexBuffer.label = @"MeshIB";
    }
  }
  
  void Reset() override
  {
    m_geometryBuffers.clear();
    m_indexBuffer = nil;
  }

  void UpdateBuffer(ref_ptr<dp::GraphicsContext> context, uint32_t bufferInd) override
  {
    UNUSED_VALUE(context);
    CHECK_LESS(bufferInd, static_cast<uint32_t>(m_geometryBuffers.size()), ());
    
    auto & buffer = m_mesh->m_buffers[bufferInd];
    auto const sizeInBytes = buffer->GetSizeInBytes();
    CHECK(sizeInBytes != 0, ());
    
    uint8_t * bufferPointer = (uint8_t *)[m_geometryBuffers[bufferInd] contents];
    memcpy(bufferPointer, buffer->GetData(), sizeInBytes);
  }

  void UpdateIndexBuffer(ref_ptr<dp::GraphicsContext> context) override
  {
    UNUSED_VALUE(context);
    CHECK(m_indexBuffer != nil, ());
    
    auto const sizeInBytes = m_mesh->m_indices.size() * sizeof(uint16_t);
    CHECK(sizeInBytes != 0, ());

    uint8_t * bufferPointer = (uint8_t *)[m_indexBuffer contents];
    memcpy(bufferPointer, m_mesh->m_indices.data(), sizeInBytes);
  }
  
  void Bind(ref_ptr<dp::GpuProgram> program) override {}
  
  void Unbind() override {}
  
  void DrawPrimitives(ref_ptr<dp::GraphicsContext> context, uint32_t vertexCount,
                      uint32_t startVertex) override
  {
    ref_ptr<dp::metal::MetalBaseContext> metalContext = context;
    if (!metalContext->HasAppliedPipelineState())
      return;
    
    id<MTLRenderCommandEncoder> encoder = metalContext->GetCommandEncoder();
    for (size_t i = 0; i < m_geometryBuffers.size(); i++)
      [encoder setVertexBuffer:m_geometryBuffers[i] offset:0 atIndex:i];
    
    [encoder drawPrimitives:GetPrimitiveType(m_mesh->m_drawPrimitive) 
                vertexStart:startVertex
                vertexCount:vertexCount];
  }

  void DrawPrimitivesIndexed(ref_ptr<dp::GraphicsContext> context, uint32_t indexCount, 
                            uint32_t startIndex) override
  {
    ref_ptr<dp::metal::MetalBaseContext> metalContext = context;
    if (!metalContext->HasAppliedPipelineState())
      return;
    
    id<MTLRenderCommandEncoder> encoder = metalContext->GetCommandEncoder();
    for (size_t i = 0; i < m_geometryBuffers.size(); i++)
      [encoder setVertexBuffer:m_geometryBuffers[i] offset:0 atIndex:i];

    CHECK(m_indexBuffer != nil, ());
    [encoder drawIndexedPrimitives:GetPrimitiveType(m_mesh->m_drawPrimitive)
                        indexCount:indexCount
                         indexType:MTLIndexTypeUInt16
                       indexBuffer:m_indexBuffer
                 indexBufferOffset:startIndex * sizeof(uint16_t)];
  }
  
private:
  ref_ptr<dp::MeshObject> m_mesh;
  std::vector<id<MTLBuffer>> m_geometryBuffers;
  id<MTLBuffer> m_indexBuffer;
};
}  // namespace metal

#ifdef OMIM_METAL_AVAILABLE
void MeshObject::InitForMetal()
{
  m_impl = make_unique_dp<metal::MetalMeshObjectImpl>(make_ref(this));
}
#endif  // OMIM_METAL_AVAILABLE
}  // namespace dp
