#include "drape_frontend/text_handle.hpp"

#include "drape/texture_manager.hpp"

#include "base/logging.hpp"

#include <ios>
#include <sstream>
#include <utility>

namespace df
{
TextHandle::TextHandle(dp::OverlayID const & id, strings::UniString const & text, dp::Anchor anchor, uint64_t priority,
                       ref_ptr<dp::TextureManager> textureManager, int minVisibleScale, bool isBillboard)
  : OverlayHandle(id, anchor, priority, minVisibleScale, isBillboard)
  , m_forceUpdateNormals(false)
  , m_isLastVisible(false)
  , m_text(text)
  , m_textureManager(textureManager)
  , m_glyphsReady(false)
{}

TextHandle::TextHandle(dp::OverlayID const & id, strings::UniString const & text, dp::Anchor anchor,
                       uint64_t priority, ref_ptr<dp::TextureManager> textureManager,
                       gpu::TTextDynamicVertexBuffer && normals, int minVisibleScale, bool isBillboard)
  : OverlayHandle(id, anchor, priority, minVisibleScale, isBillboard)
  , m_buffer(std::move(normals))
  , m_forceUpdateNormals(false)
  , m_isLastVisible(false)
  , m_text(text)
  , m_textureManager(textureManager)
  , m_glyphsReady(false)
{}

void TextHandle::GetAttributeMutation(ref_ptr<dp::AttributeBufferMutator> mutator) const
{
  bool const isVisible = IsVisible();
  if (!m_forceUpdateNormals && m_isLastVisible == isVisible)
    return;

  TOffsetNode const & node = GetOffsetNode(gpu::TextDynamicVertex::GetDynamicStreamID());
  ASSERT(node.first.GetElementSize() == sizeof(gpu::TextDynamicVertex), ());
  ASSERT(node.second.m_count == m_buffer.size(), ());

  uint32_t const byteCount = static_cast<uint32_t>(m_buffer.size()) * sizeof(gpu::TextDynamicVertex);
  void * buffer = mutator->AllocateMutationBuffer(byteCount);
  if (isVisible)
    memcpy(buffer, m_buffer.data(), byteCount);
  else
    memset(buffer, 0, byteCount);

  dp::MutateNode mutateNode;
  mutateNode.m_region = node.second;
  mutateNode.m_data = make_ref(buffer);
  mutator->AddMutation(node.first, mutateNode);

  m_isLastVisible = isVisible;
}

bool TextHandle::Update(ScreenBase const &)
{
  if (!m_glyphsReady)
    m_glyphsReady = m_textureManager->AreGlyphsReady(m_text);

  return m_glyphsReady;
}

bool TextHandle::IndexesRequired() const
{
  // Disable indices usage for text handles.
  return false;
}

void TextHandle::SetForceUpdateNormals(bool forceUpdate) const
{
  m_forceUpdateNormals = forceUpdate;
}

#ifdef DEBUG_OVERLAYS_OUTPUT
std::string TextHandle::GetOverlayDebugInfo()
{
  std::ostringstream out;
  out << "Text Priority(" << std::hex << std::setw(16) << std::setfill('0') << GetPriority()
      << ") " << std::dec << DebugPrint(GetOverlayID()) << " " << strings::ToUtf8(m_text);
  return out.str();
}
#endif
}  // namespace df
