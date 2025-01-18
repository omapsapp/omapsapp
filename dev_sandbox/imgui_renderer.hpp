#pragma once

#include "drape/glsl_types.hpp"
#include "drape/graphics_context.hpp"
#include "drape/mesh_object.hpp"
#include "drape/pointers.hpp"
#include "drape/render_state.hpp"
#include "drape/static_texture.hpp"
#include "drape/texture_manager.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

class ImguiRenderer
{
public:
  ImguiRenderer();
  void Render(ref_ptr<dp::GraphicsContext> context, 
              ref_ptr<dp::TextureManager> textureManager,
              ref_ptr<gpu::ProgramManager> programManager);
  void Update(std::function<void()> const & uiCallback);
  void Reset();

private:
  void UpdateTexture();
  void UpdateBuffers();

  struct ImguiVertex
  {
    glsl::vec2 position;
    glsl::vec2 texCoords;
    glsl::vec4 color;
  };
  static_assert(sizeof(ImguiVertex) == 2 * sizeof(glsl::vec4));

  struct DrawCall
  {
    uint32_t indexCount = 0;
    uint32_t startIndex = 0;
    glsl::uvec4 clipRect{};
  };

  drape_ptr<dp::MeshObject> m_mesh;
  uint32_t m_vertexCount = 2000;
  uint32_t m_indexCount = 3000;

  drape_ptr<dp::StaticTexture> m_texture;
  std::vector<unsigned char> m_textureData;
  uint32_t m_textureWidth = 0;
  uint32_t m_textureHeight = 0;

  dp::RenderState m_state;

  struct UiDataBuffer
  {
    std::vector<ImguiVertex> m_vertices;
    std::vector<uint16_t> m_indices;
    std::vector<DrawCall> m_drawCalls;
    uint32_t m_width;
    uint32_t m_height;
  };
  std::array<UiDataBuffer, 2> m_uiDataBuffer;
  size_t m_updateIndex = 0;

  glsl::mat4 m_projection;

  std::mutex m_bufferMutex;
  std::mutex m_textureMutex;
};
