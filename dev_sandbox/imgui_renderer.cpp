#include "imgui_renderer.hpp"

#include "base/logging.hpp"
#include "base/macros.hpp"

#include <drape_frontend/render_state_extension.hpp>

#include <shaders/program_manager.hpp>

#include <imgui/imgui.h>

#include <cstring>
#include <limits>

ImguiRenderer::ImguiRenderer()
  : m_state(df::CreateRenderState(gpu::Program::ImGui, df::DepthLayer::GuiLayer))
{
  m_state.SetDepthTestEnabled(false);
  m_state.SetBlending(dp::Blending(true));
}

void ImguiRenderer::Render(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::TextureManager> textureManager,
                           ref_ptr<gpu::ProgramManager> programManager)
{
  std::lock_guard<std::mutex> lock(m_bufferMutex);
  size_t renderDataIndex = (m_updateIndex + 1) % m_uiDataBuffer.size();
  UiDataBuffer & dataBuffer = m_uiDataBuffer[renderDataIndex];

  auto gpuProgram = programManager->GetProgram(m_state.GetProgram<gpu::Program>());

  bool needUpdate = true;
  if (!m_mesh || dataBuffer.m_vertices.size() > m_vertexCount || dataBuffer.m_indices.size() > m_indexCount)
  {
    while (dataBuffer.m_vertices.size() > m_vertexCount)
      m_vertexCount *= 2;
    while (dataBuffer.m_indices.size() > m_indexCount)
      m_indexCount *= 2;
    m_indexCount = std::min(m_indexCount, static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()));

    dataBuffer.m_vertices.resize(m_vertexCount);
    dataBuffer.m_indices.resize(m_indexCount);

    m_mesh = make_unique_dp<dp::MeshObject>(context, dp::MeshObject::DrawPrimitive::Triangles, "imGui");

    m_mesh->SetBuffer(0, std::move(dataBuffer.m_vertices));
    m_mesh->SetAttribute("a_position", 0, 0 /* offset */, 2);
    m_mesh->SetAttribute("a_texCoords", 0, 2 * sizeof(float) /* offset */, 2);
    m_mesh->SetAttribute("a_color", 0, 4 * sizeof(float) /* offset */, 4);
    m_mesh->SetIndexBuffer(std::move(dataBuffer.m_indices));
    m_mesh->Build(context, gpuProgram);

    dataBuffer.m_vertices.clear();
    dataBuffer.m_indices.clear();
    needUpdate = false;
  }

  if (!m_texture)
  {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    if (!m_textureData.empty())
    {
      m_texture = make_unique_dp<dp::StaticTexture>();
      m_texture->Create(context,
                        dp::Texture::Params{
                            .m_width = m_textureWidth,
                            .m_height = m_textureHeight,
                            .m_format = dp::TextureFormat::RGBA8,
                            .m_allocator = textureManager->GetTextureAllocator(),
                        },
                        m_textureData.data());
      m_textureData.clear();
      m_state.SetColorTexture(make_ref(m_texture));
    }
    else
    {
      // Can't render without texture.
      return;
    }
  }

  if (dataBuffer.m_drawCalls.empty())
    return;

  if (needUpdate && !dataBuffer.m_vertices.empty() && !dataBuffer.m_indices.empty())
  {
    m_mesh->UpdateBuffer(context, 0, dataBuffer.m_vertices);
    m_mesh->UpdateIndexBuffer(context, dataBuffer.m_indices);
    dataBuffer.m_vertices.clear();
    dataBuffer.m_indices.clear();
  }

  gpu::ImGuiProgramParams const params{.m_projection = m_projection};
  context->PushDebugLabel("ImGui Rendering");
  m_mesh->Render(context, gpuProgram, m_state, programManager->GetParamsSetter(), params,
                 [&, this]()
                 {
                   context->SetCullingEnabled(false);
                   for (auto const & drawCall : dataBuffer.m_drawCalls)
                   {
                     uint32_t y = drawCall.clipRect.y;
                     if (context->GetApiVersion() == dp::ApiVersion::OpenGLES3)
                       y = dataBuffer.m_height - y - drawCall.clipRect.w;
                     context->SetScissor(drawCall.clipRect.x, y, drawCall.clipRect.z, drawCall.clipRect.w);
                     m_mesh->DrawPrimitivesSubsetIndexed(context, drawCall.indexCount, drawCall.startIndex);
                   }
                   context->SetCullingEnabled(true);
                   context->SetScissor(0, 0, dataBuffer.m_width, dataBuffer.m_height);
                 });
  context->PopDebugLabel();
}

void ImguiRenderer::Update(std::function<void()> const & uiCallback)
{
  CHECK(uiCallback, ());
  ImGuiIO & io = ImGui::GetIO();
  if (!io.Fonts->IsBuilt())
    io.Fonts->Build();
  if (!m_texture)
    UpdateTexture();

  ImGui::NewFrame();
  uiCallback();
  ImGui::Render();
  UpdateBuffers();
}

void ImguiRenderer::Reset()
{
  {
    std::lock_guard<std::mutex> lock(m_textureMutex);
    m_texture.reset();
  }

  {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_mesh.reset();
  }
}

void ImguiRenderer::UpdateTexture()
{
  std::lock_guard<std::mutex> lock(m_textureMutex);
  unsigned char * pixels;
  int width, height;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  auto const sizeInBytes = width * height * sizeof(uint32_t);
  m_textureData.resize(sizeInBytes);
  memcpy(m_textureData.data(), pixels, sizeInBytes);
  m_textureWidth = static_cast<uint32_t>(width);
  m_textureHeight = static_cast<uint32_t>(height);
}

void ImguiRenderer::UpdateBuffers()
{
  UiDataBuffer & dataBuffer = m_uiDataBuffer[m_updateIndex];
  dataBuffer.m_drawCalls.clear();

  ImDrawData * dd = ImGui::GetDrawData();
  auto const fbWidth = static_cast<int>(dd->DisplaySize.x * dd->FramebufferScale.x);
  auto const fbHeight = static_cast<int>(dd->DisplaySize.y * dd->FramebufferScale.y);
  if (fbWidth <= 0 || fbHeight <= 0 || dd->CmdListsCount == 0 || dd->TotalIdxCount == 0 || dd->TotalVtxCount == 0)
    return;
  dataBuffer.m_width = static_cast<uint32_t>(fbWidth);
  dataBuffer.m_height = static_cast<uint32_t>(fbHeight);

  CHECK(dd->TotalVtxCount <= std::numeric_limits<uint16_t>::max(),
        ("UI is so complex and now requires 32-bit indices. You need to improve dp::MeshObject or simplify UI"));

  CHECK((ImGui::GetIO().BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset) == 0, ());

  dataBuffer.m_vertices.resize(dd->TotalVtxCount);
  dataBuffer.m_indices.resize(dd->TotalIdxCount);

  int totalDrawCallsCount = 0;
  for (int i = 0; i < dd->CmdListsCount; ++i)
    totalDrawCallsCount += dd->CmdLists[i]->CmdBuffer.Size;
  dataBuffer.m_drawCalls.reserve(totalDrawCallsCount);

  ImVec2 const clipOff = dd->DisplayPos;
  ImVec2 const clipScale = dd->FramebufferScale;

  uint32_t vertexOffset = 0;
  uint32_t indexOffset = 0;
  for (int i = 0; i < dd->CmdListsCount; ++i)
  {
    ImDrawList const * cmdList = dd->CmdLists[i];
    for (int j = 0; j < cmdList->VtxBuffer.Size; ++j)
    {
      dp::Color color(cmdList->VtxBuffer.Data[j].col);
      dataBuffer.m_vertices[j + vertexOffset] = {
          .position = {cmdList->VtxBuffer.Data[j].pos.x, cmdList->VtxBuffer.Data[j].pos.y},
          .texCoords = {cmdList->VtxBuffer.Data[j].uv.x, cmdList->VtxBuffer.Data[j].uv.y},
          .color = {color.GetAlphaF(), color.GetBlueF(), color.GetGreenF(),
                    color.GetRedF()}  // Byte order is reversed in imGui
      };
    }

    static_assert(sizeof(uint16_t) == sizeof(ImDrawIdx));
    memcpy(dataBuffer.m_indices.data() + indexOffset, cmdList->IdxBuffer.Data,
           cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
    for (int j = 0; j < cmdList->IdxBuffer.Size; ++j)
    {
      uint32_t indexValue = dataBuffer.m_indices[j + indexOffset];
      indexValue += vertexOffset;
      CHECK(indexValue <= std::numeric_limits<uint16_t>::max(), ());
      dataBuffer.m_indices[j + indexOffset] = static_cast<uint16_t>(indexValue);
    }

    for (int cmdIndex = 0; cmdIndex < cmdList->CmdBuffer.Size; ++cmdIndex)
    {
      const ImDrawCmd cmd = cmdList->CmdBuffer[cmdIndex];
      ImVec2 clipMin((cmd.ClipRect.x - clipOff.x) * clipScale.x, (cmd.ClipRect.y - clipOff.y) * clipScale.y);
      ImVec2 clipMax((cmd.ClipRect.z - clipOff.x) * clipScale.x, (cmd.ClipRect.w - clipOff.y) * clipScale.y);
      if (clipMin.x < 0.0f)
        clipMin.x = 0.0f;
      if (clipMin.y < 0.0f)
        clipMin.y = 0.0f;
      if (clipMax.x > fbWidth)
        clipMax.x = static_cast<float>(fbWidth);
      if (clipMax.y > fbHeight)
        clipMax.y = static_cast<float>(fbHeight);
      if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
        continue;

      dataBuffer.m_drawCalls.emplace_back(DrawCall{
          .indexCount = static_cast<uint32_t>(cmd.ElemCount),
          .startIndex = static_cast<uint32_t>(indexOffset + cmd.IdxOffset),
          .clipRect = {static_cast<uint32_t>(clipMin.x), static_cast<uint32_t>(clipMin.y),
                       static_cast<uint32_t>(clipMax.x - clipMin.x), static_cast<uint32_t>(clipMax.y - clipMin.y)}});
    }

    vertexOffset += static_cast<uint32_t>(cmdList->VtxBuffer.Size);
    indexOffset += static_cast<uint32_t>(cmdList->IdxBuffer.Size);
  }
  CHECK(vertexOffset == dataBuffer.m_vertices.size(), ());
  CHECK(indexOffset == dataBuffer.m_indices.size(), ());

  {
    std::lock_guard<std::mutex> lock(m_bufferMutex);

    // Projection
    float const left = dd->DisplayPos.x;
    float const right = dd->DisplayPos.x + dd->DisplaySize.x;
    float const top = dd->DisplayPos.y;
    float const bottom = dd->DisplayPos.y + dd->DisplaySize.y;
    m_projection[0][0] = 2.0f / (right - left);
    m_projection[1][1] = 2.0f / (top - bottom);
    m_projection[2][2] = -1.0f;
    m_projection[3][3] = 1.0f;
    m_projection[0][3] = -(right + left) / (right - left);
    m_projection[1][3] = -(top + bottom) / (top - bottom);

    // Swap buffers
    m_updateIndex = (m_updateIndex + 1) % m_uiDataBuffer.size();
  }
}
