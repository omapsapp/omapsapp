#include "drape/mesh_object.hpp"

#include "drape/gl_constants.hpp"
#include "drape/gl_gpu_program.hpp"
#include "drape/gl_functions.hpp"
#include "drape/glsl_func.hpp"
#include "drape/glsl_types.hpp"
#include "drape/texture_manager.hpp"

#include "base/assert.hpp"

namespace
{
glConst GetGLDrawPrimitive(dp::MeshObject::DrawPrimitive drawPrimitive)
{
  switch (drawPrimitive)
  {
  case dp::MeshObject::DrawPrimitive::Triangles: return gl_const::GLTriangles;
  case dp::MeshObject::DrawPrimitive::TriangleStrip: return gl_const::GLTriangleStrip;
  case dp::MeshObject::DrawPrimitive::LineStrip: return gl_const::GLLineStrip;
  }
  UNREACHABLE();
}
}  // namespace

namespace dp
{
class GLMeshObjectImpl : public MeshObjectImpl
{
public:
  explicit GLMeshObjectImpl(ref_ptr<dp::MeshObject> mesh)
    : m_mesh(mesh)
  {}

  void Build(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program) override
  {
    UNUSED_VALUE(context);

    bool const isVAOSupported =
      GLFunctions::ExtensionsList.IsSupported(dp::GLExtensionsList::VertexArrayObject);
    if (isVAOSupported)
    {
      m_VAO = GLFunctions::glGenVertexArray();
      GLFunctions::glBindVertexArray(m_VAO);
    }

    for (auto & buffer : m_mesh->m_buffers)
    {
      buffer->m_bufferId = GLFunctions::glGenBuffer();
      GLFunctions::glBindBuffer(buffer->m_bufferId, gl_const::GLArrayBuffer);

      if (buffer->GetSizeInBytes() != 0)
      {
        GLFunctions::glBufferData(gl_const::GLArrayBuffer, buffer->GetSizeInBytes(),
                                  buffer->GetData(), gl_const::GLStaticDraw);
      }

      if (!m_mesh->m_indices.empty())
      {
        m_indexBuffer = GLFunctions::glGenBuffer();
        GLFunctions::glBindBuffer(m_indexBuffer, gl_const::GLElementArrayBuffer);
        GLFunctions::glBufferData(gl_const::GLElementArrayBuffer, 
                                  m_mesh->m_indices.size() * sizeof(uint16_t),
                                  m_mesh->m_indices.data(), gl_const::GLStaticDraw);
      }

      if (isVAOSupported)
      {
        ref_ptr<dp::GLGpuProgram> p = program;
        for (auto const & attribute : buffer->m_attributes)
        {
          int8_t const attributePosition = p->GetAttributeLocation(attribute.m_attributeName);
          ASSERT_NOT_EQUAL(attributePosition, -1, ());
          GLFunctions::glEnableVertexAttribute(attributePosition);
          GLFunctions::glVertexAttributePointer(attributePosition, attribute.m_componentsCount,
                                                attribute.m_type, false, buffer->GetStrideInBytes(), 
                                                attribute.m_offset);
        }
      }
    }

    if (isVAOSupported)
      GLFunctions::glBindVertexArray(0);
    GLFunctions::glBindBuffer(0, gl_const::GLArrayBuffer);
    if (!m_mesh->m_indices.empty())
      GLFunctions::glBindBuffer(0, gl_const::GLElementArrayBuffer);
  }

  void Reset() override
  {
    for (auto & buffer : m_mesh->m_buffers)
    {
      if (buffer->m_bufferId != 0)
      {
        GLFunctions::glDeleteBuffer(buffer->m_bufferId);
        buffer->m_bufferId = 0;
      }
    }

    if (m_indexBuffer != 0)
    {
      GLFunctions::glDeleteBuffer(m_indexBuffer);
      m_indexBuffer = 0;
    }

    if (m_VAO != 0)
    {
      GLFunctions::glDeleteVertexArray(m_VAO);
      m_VAO = 0;
    }
  }

  void UpdateBuffer(ref_ptr<dp::GraphicsContext> context, uint32_t bufferInd) override
  {
    UNUSED_VALUE(context);
    auto & buffer = m_mesh->m_buffers[bufferInd];
    GLFunctions::glBindBuffer(buffer->m_bufferId, gl_const::GLArrayBuffer);
    GLFunctions::glBufferData(gl_const::GLArrayBuffer, buffer->GetSizeInBytes(),
                              buffer->GetData(), gl_const::GLStaticDraw);
    GLFunctions::glBindBuffer(0, gl_const::GLArrayBuffer);
  }

  void UpdateIndexBuffer(ref_ptr<dp::GraphicsContext> context) override
  {
    UNUSED_VALUE(context);
    CHECK(!m_mesh->m_indices.empty(), ());
    CHECK(m_indexBuffer, ("Index buffer was not created"));
    GLFunctions::glBindBuffer(m_indexBuffer, gl_const::GLElementArrayBuffer);
    GLFunctions::glBufferData(gl_const::GLElementArrayBuffer, 
                              m_mesh->m_indices.size() * sizeof(uint16_t),
                              m_mesh->m_indices.data(), gl_const::GLStaticDraw);
    GLFunctions::glBindBuffer(0, gl_const::GLElementArrayBuffer);
  }

  void Bind(ref_ptr<dp::GpuProgram> program) override
  {
    if (GLFunctions::ExtensionsList.IsSupported(dp::GLExtensionsList::VertexArrayObject))
    {
      GLFunctions::glBindVertexArray(m_VAO);
      return;
    }

    ref_ptr<dp::GLGpuProgram> p = program;
    for (auto const & buffer : m_mesh->m_buffers)
    {
      GLFunctions::glBindBuffer(buffer->m_bufferId, gl_const::GLArrayBuffer);
      if (m_indexBuffer != 0)
        GLFunctions::glBindBuffer(m_indexBuffer, gl_const::GLElementArrayBuffer);
      for (auto const & attribute : buffer->m_attributes)
      {
        int8_t const attributePosition = p->GetAttributeLocation(attribute.m_attributeName);
        ASSERT_NOT_EQUAL(attributePosition, -1, ());
        GLFunctions::glEnableVertexAttribute(attributePosition);
        GLFunctions::glVertexAttributePointer(attributePosition, attribute.m_componentsCount,
                                              attribute.m_type, false, buffer->GetStrideInBytes(), 
                                              attribute.m_offset);
      }
    }
  }

  void Unbind() override
  {
    if (GLFunctions::ExtensionsList.IsSupported(dp::GLExtensionsList::VertexArrayObject))
      GLFunctions::glBindVertexArray(0);
    GLFunctions::glBindBuffer(0, gl_const::GLArrayBuffer);
    if (m_indexBuffer != 0)
      GLFunctions::glBindBuffer(0, gl_const::GLElementArrayBuffer);
  }

  void DrawPrimitives(ref_ptr<dp::GraphicsContext> context, uint32_t verticesCount, 
                      uint32_t startVertex) override
  {
    UNUSED_VALUE(context);
    GLFunctions::glDrawArrays(GetGLDrawPrimitive(m_mesh->m_drawPrimitive), 
                              static_cast<int32_t>(startVertex), 
                              verticesCount);
  }

  void DrawPrimitivesIndexed(ref_ptr<dp::GraphicsContext> context, uint32_t indexCount, 
                             uint32_t startIndex) override
  {
    UNUSED_VALUE(context);
    CHECK(m_indexBuffer != 0, ());
    GLFunctions::glDrawElements(GetGLDrawPrimitive(m_mesh->m_drawPrimitive), sizeof(uint16_t), 
                                indexCount, startIndex);
  }

private:
  ref_ptr<dp::MeshObject> m_mesh;
  uint32_t m_VAO = 0;
  uint32_t m_indexBuffer = 0;
};

MeshObject::MeshObject(ref_ptr<dp::GraphicsContext> context, DrawPrimitive drawPrimitive,
                       std::string const & debugName)
  : m_drawPrimitive(drawPrimitive)
  , m_debugName(debugName)
{
  auto const apiVersion = context->GetApiVersion();
  if (apiVersion == dp::ApiVersion::OpenGLES2 || apiVersion == dp::ApiVersion::OpenGLES3)
  {
    InitForOpenGL();
  }
  else if (apiVersion == dp::ApiVersion::Metal)
  {
#if defined(OMIM_METAL_AVAILABLE)
    InitForMetal();
#endif
  }
  else if (apiVersion == dp::ApiVersion::Vulkan)
  {
    InitForVulkan(context);
  }
  CHECK(m_impl != nullptr, ());
}

MeshObject::~MeshObject()
{
  Reset();
}

void MeshObject::InitForOpenGL()
{
  m_impl = make_unique_dp<GLMeshObjectImpl>(make_ref(this));
}

void MeshObject::SetAttribute(std::string const & attributeName, uint32_t bufferInd, uint32_t offset,
                              uint32_t componentsCount, glConst type)
{
  CHECK_LESS(bufferInd, m_buffers.size(), ());
  CHECK(m_buffers[bufferInd], ());
  m_buffers[bufferInd]->m_attributes.emplace_back(attributeName, offset, componentsCount, type);

  Reset();
}

void MeshObject::Reset()
{
  CHECK(m_impl != nullptr, ());
  m_impl->Reset();

  m_initialized = false;
}

void MeshObject::Build(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program)
{
  Reset();

  CHECK(m_impl != nullptr, ());
  m_impl->Build(context, program);

  m_initialized = true;
}

void MeshObject::Bind(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::GpuProgram> program)
{
  program->Bind();

  if (!m_initialized)
    Build(context, program);

  CHECK(m_impl != nullptr, ());
  m_impl->Bind(program);
}

void MeshObject::SetIndexBuffer(std::vector<uint16_t> && indices)
{
  m_indices = std::move(indices);
  Reset();
}

void MeshObject::UpdateIndexBuffer(ref_ptr<dp::GraphicsContext> context, std::vector<uint16_t> const & indices)
{
  CHECK(!indices.empty(), ("Use SetIndexBuffer() to reset index buffer"));
  CHECK_LESS_OR_EQUAL(indices.size(), m_indices.size(), ());
  memcpy(m_indices.data(), indices.data(), indices.size() * sizeof(uint16_t));

  CHECK(m_impl != nullptr, ());
  m_impl->UpdateIndexBuffer(context);
}

void MeshObject::DrawPrimitivesSubset(ref_ptr<dp::GraphicsContext> context, uint32_t vertexCount,
                                      uint32_t startVertex)
{
  CHECK(m_impl != nullptr, ());
  CHECK(!m_buffers.empty(), ());
  auto const & buffer = m_buffers[0];
  auto const vertexNum = buffer->GetSizeInBytes() / buffer->GetStrideInBytes();
  CHECK_LESS(startVertex, vertexNum, ());
  CHECK_LESS_OR_EQUAL(startVertex + vertexCount, vertexNum, ());

  m_impl->DrawPrimitives(context, vertexCount, startVertex);
}

void MeshObject::DrawPrimitivesSubsetIndexed(ref_ptr<dp::GraphicsContext> context, uint32_t indexCount, 
                                             uint32_t startIndex)
{
  CHECK(m_impl != nullptr, ());
  CHECK(!m_indices.empty(), ());
  CHECK_LESS(startIndex, m_indices.size(), ());
  CHECK_LESS_OR_EQUAL(startIndex + indexCount, m_indices.size(), ());

  m_impl->DrawPrimitivesIndexed(context, indexCount, startIndex);
}

void MeshObject::DrawPrimitives(ref_ptr<dp::GraphicsContext> context)
{
  if (m_buffers.empty())
    return;

  auto const & buffer = m_buffers[0];
  auto const vertexNum = buffer->GetSizeInBytes() / buffer->GetStrideInBytes();
#ifdef DEBUG
  for (size_t i = 1; i < m_buffers.size(); i++)
  {
    ASSERT_EQUAL(m_buffers[i]->GetSizeInBytes() / m_buffers[i]->GetStrideInBytes(), vertexNum, 
                 ("All buffers in a mesh must contain the same vertex number"));
  }
#endif
  
  CHECK(m_impl != nullptr, ());
  if (m_indices.empty())
    m_impl->DrawPrimitives(context, vertexNum, 0);
  else
    m_impl->DrawPrimitivesIndexed(context, static_cast<uint32_t>(m_indices.size()), 0);
}

void MeshObject::Unbind(ref_ptr<dp::GpuProgram> program)
{
  program->Unbind();

  CHECK(m_impl != nullptr, ());
  m_impl->Unbind();
}

void MeshObject::UpdateImpl(ref_ptr<dp::GraphicsContext> context, uint32_t bufferInd)
{
  CHECK(m_impl != nullptr, ());
  m_impl->UpdateBuffer(context, bufferInd);
}

// static
std::vector<float> MeshObject::GenerateNormalsForTriangles(std::vector<float> const & vertices,
                                                           size_t componentsCount)
{
  auto const trianglesCount = vertices.size() / (3 * componentsCount);
  std::vector<float> normals;
  normals.reserve(trianglesCount * 9);
  for (size_t triangle = 0; triangle < trianglesCount; ++triangle)
  {
    glsl::vec3 v[3];
    for (size_t vertex = 0; vertex < 3; ++vertex)
    {
      size_t const offset = triangle * componentsCount * 3 + vertex * componentsCount;
      v[vertex] = glsl::vec3(vertices[offset], vertices[offset + 1], vertices[offset + 2]);
    }

    glsl::vec3 normal = glsl::cross(glsl::vec3(v[1].x - v[0].x, v[1].y - v[0].y, v[1].z - v[0].z),
                                    glsl::vec3(v[2].x - v[0].x, v[2].y - v[0].y, v[2].z - v[0].z));
    normal = glsl::normalize(normal);

    for (size_t vertex = 0; vertex < 3; ++vertex)
    {
      normals.push_back(normal.x);
      normals.push_back(normal.y);
      normals.push_back(normal.z);
    }
  }
  return normals;
}
}  // namespace dp

