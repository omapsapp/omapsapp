#include "drape_frontend/text_shape.hpp"
#include "drape_frontend/render_state_extension.hpp"
#include "drape_frontend/text_handle.hpp"
#include "drape_frontend/text_layout.hpp"
#include "drape_frontend/visual_params.hpp"

#include "shaders/programs.hpp"

#include "drape/utils/vertex_decl.hpp"
#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"
#include "drape/overlay_handle.hpp"
#include "drape/texture_manager.hpp"

#include "base/string_utils.hpp"

#include <algorithm>
#include <utility>

namespace df
{
namespace
{
class StraightTextHandle : public TextHandle
{
  using TBase = TextHandle;

public:
  StraightTextHandle(dp::OverlayID const & id, strings::UniString const & text,
                     dp::Anchor anchor, glsl::vec2 const & pivot,
                     glsl::vec2 const & pxSize, glsl::vec2 const & offset,
                     uint64_t priority, ref_ptr<dp::TextureManager> textureManager, bool isOptional,
                     gpu::TTextDynamicVertexBuffer && normals, int minVisibleScale, bool isBillboard)
    : TextHandle(id, text, anchor, priority, textureManager, std::move(normals), minVisibleScale,
                 isBillboard)
    , m_pivot(glsl::ToPoint(pivot))
    , m_offset(glsl::ToPoint(offset))
    , m_size(glsl::ToPoint(pxSize))
    , m_isOptional(isOptional)
  {}

  void SetDynamicSymbolSizes(StraightTextLayout const & layout, std::vector<m2::PointF> const & symbolSizes,
                             dp::Anchor symbolAnchor)
  {
    m_layout = make_unique_dp<StraightTextLayout>(layout);
    m_symbolSizes = symbolSizes;
    m_symbolAnchor = symbolAnchor;
  }

  void GetAttributeMutation(ref_ptr<dp::AttributeBufferMutator> mutator) const override
  {
    SetForceUpdateNormals(IsVisible() && m_layout != nullptr);
    TextHandle::GetAttributeMutation(mutator);
  }

  bool Update(ScreenBase const & screen) override
  {
    if (!TBase::Update(screen))
      return false;

    if (m_layout != nullptr)
    {
      double zoom = 0.0;
      int index = 0;
      float lerpCoef = 0.0f;
      ExtractZoomFactors(screen, zoom, index, lerpCoef);
      m2::PointF symbolSize = InterpolateByZoomLevels(index, lerpCoef, m_symbolSizes);
      auto const offset = m_layout->GetTextOffset(symbolSize, m_anchor, m_symbolAnchor);
      m_buffer.clear();
      m_layout->CacheDynamicGeometry(offset, m_buffer);
      m_offset = glsl::ToPoint(offset);
    }
    return true;
  }

  m2::PointD GetPivot(ScreenBase const & screen, bool perspective) const override
  {
    m2::PointD pivot = TBase::GetPivot(screen, false);
    if (perspective)
      pivot = screen.PtoP3d(pivot - m2::PointD(m_offset), -m_pivotZ) + m2::PointD(m_offset);
    return pivot;
  }

  m2::RectD GetPixelRect(ScreenBase const & screen, bool perspective) const override
  {
    if (perspective)
    {
      if (IsBillboard())
      {
        m2::PointD const pxPivot(screen.GtoP(m2::PointD(m_pivot)));
        m2::PointD const pxPivotPerspective(screen.PtoP3d(pxPivot, -m_pivotZ));

        m2::RectD pxRectPerspective = GetPixelRect(screen, false);
        pxRectPerspective.Offset(-pxPivot);
        pxRectPerspective.Offset(pxPivotPerspective);

        return pxRectPerspective;
      }
      return GetPixelRectPerspective(screen);
    }

    m2::PointD pivot(screen.GtoP(m2::PointD(m_pivot)) + m2::PointD(m_offset));
    double x = pivot.x;
    double y = pivot.y;
    if (m_anchor & dp::Left)
      x += m_size.x;
    else if (m_anchor & dp::Right)
      x -= m_size.x;
    else
    {
      float halfWidth = m_size.x / 2.0f;
      x += halfWidth;
      pivot.x -= halfWidth;
    }

    if (m_anchor & dp::Top)
      y += m_size.y;
    else if (m_anchor & dp::Bottom)
      y -= m_size.y;
    else
    {
      float halfHeight = m_size.y / 2.0f;
      y += halfHeight;
      pivot.y -= halfHeight;
    }

    return m2::RectD(std::min(x, pivot.x), std::min(y, pivot.y),
                     std::max(x, pivot.x), std::max(y, pivot.y));
  }

  void GetPixelShape(ScreenBase const & screen, bool perspective, Rects & rects) const override
  {
    rects.emplace_back(GetPixelRect(screen, perspective));
  }

  bool IsBound() const override
  {
    return !m_isOptional;
  }

private:
  m2::PointF m_pivot;
  m2::PointF m_offset;
  m2::PointF m_size;
  bool m_isOptional;
  drape_ptr<StraightTextLayout> m_layout;
  std::vector<m2::PointF> m_symbolSizes;
  dp::Anchor m_symbolAnchor;
};
}  // namespace

TextShape::TextShape(m2::PointD const & basePoint, TextViewParams const & params,
                     TileKey const & tileKey,
                     m2::PointF const & symbolSize, m2::PointF const & symbolOffset, dp::Anchor symbolAnchor,
                     uint32_t textIndex)
  : m_basePoint(basePoint)
  , m_params(params)
  , m_tileCoords(tileKey.GetTileCoords())
  , m_symbolAnchor(symbolAnchor)
  , m_symbolOffset(symbolOffset)
  , m_textIndex(textIndex)
{
  m_symbolSizes.push_back(symbolSize);
}

TextShape::TextShape(m2::PointD const & basePoint, TextViewParams const & params,
                     TileKey const & tileKey, std::vector<m2::PointF> const & symbolSizes,
                     m2::PointF const & symbolOffset, dp::Anchor symbolAnchor, uint32_t textIndex)
  : m_basePoint(basePoint)
  , m_params(params)
  , m_tileCoords(tileKey.GetTileCoords())
  , m_symbolSizes(symbolSizes)
  , m_symbolAnchor(symbolAnchor)
  , m_symbolOffset(symbolOffset)
  , m_textIndex(textIndex)
{
  ASSERT_GREATER(m_symbolSizes.size(), 0, ());
}

void CalculateTextOffsets(dp::TitleDecl const titleDecl, m2::PointF const & primaryTextSize,
                          m2::PointF const & secondaryTextSize, glsl::vec2 & primaryResultOffset,
                          glsl::vec2 & secondaryResultOffset)
{
  primaryResultOffset = glsl::vec2(0.0f, 0.0f);
  secondaryResultOffset = glsl::vec2(0.0f, 0.0f);

  bool const hasSecondary = !titleDecl.m_secondaryText.empty();

  if (titleDecl.m_anchor & dp::Top)
  {
    // In the case when the anchor is dp::Top the value of primary offset y > 0,
    // the text shape is below the POI.
    primaryResultOffset.y = titleDecl.m_primaryOffset.y;
    if (hasSecondary)
      secondaryResultOffset.y = titleDecl.m_primaryOffset.y + primaryTextSize.y + titleDecl.m_secondaryOffset.y;
  }
  else if (titleDecl.m_anchor & dp::Bottom)
  {
    // In the case when the anchor is dp::Bottom the value of primary offset y < 0,
    // the text shape is above the POI.
    primaryResultOffset.y = titleDecl.m_primaryOffset.y;
    if (hasSecondary)
    {
      primaryResultOffset.y -= secondaryTextSize.y + titleDecl.m_secondaryOffset.y;
      secondaryResultOffset.y = titleDecl.m_primaryOffset.y;
    }
  }
  else if (hasSecondary)
  {
    // In the case when the anchor is dp::Center there isn't primary offset y.
    primaryResultOffset.y = -(primaryTextSize.y + titleDecl.m_secondaryOffset.y) / 2.0f;
    secondaryResultOffset.y = (secondaryTextSize.y + titleDecl.m_secondaryOffset.y) / 2.0f;
  }

  if (titleDecl.m_anchor & dp::Left)
  {
    // In the case when the anchor is dp::Left the value of primary offset x > 0,
    // the text shape is on the right from the POI.
    primaryResultOffset.x = titleDecl.m_primaryOffset.x;
    if (hasSecondary)
      secondaryResultOffset.x = primaryResultOffset.x;
  }
  else if (titleDecl.m_anchor & dp::Right)
  {
    // In the case when the anchor is dp::Right the value of primary offset x < 0,
    // the text shape is on the left from the POI.
    primaryResultOffset.x = titleDecl.m_primaryOffset.x;
    if (hasSecondary)
      secondaryResultOffset.x = primaryResultOffset.x;
  }
}

void TextShape::Draw(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::Batcher> batcher,
                     ref_ptr<dp::TextureManager> textures) const
{
  auto const & titleDecl = m_params.m_titleDecl;

  ASSERT(!titleDecl.m_primaryText.empty(), ());
  StraightTextLayout primaryLayout(
      strings::MakeUniString(titleDecl.m_primaryText), titleDecl.m_primaryTextFont.m_size,
      textures, titleDecl.m_anchor, titleDecl.m_forceNoWrap);


  if (m_params.m_limitedText)
  {
    if (auto const y = primaryLayout.GetPixelSize().y; y >= m_params.m_limits.y)
    {
      float const newFontSize = titleDecl.m_primaryTextFont.m_size * m_params.m_limits.y / y;
      primaryLayout = StraightTextLayout(strings::MakeUniString(titleDecl.m_primaryText), newFontSize,
                                         textures, titleDecl.m_anchor, titleDecl.m_forceNoWrap);
    }
  }

  glsl::vec2 primaryOffset;
  glsl::vec2 secondaryOffset;
  if (titleDecl.m_secondaryText.empty())
  {
    CalculateTextOffsets(titleDecl, primaryLayout.GetPixelSize(), m2::PointF{0.f, 0.f}, primaryOffset, secondaryOffset);
    primaryOffset += glsl::vec2{m_symbolOffset.x, m_symbolOffset.y};
  }
  else
  {
    StraightTextLayout secondaryLayout{strings::MakeUniString(titleDecl.m_secondaryText),
        titleDecl.m_secondaryTextFont.m_size, textures, titleDecl.m_anchor, titleDecl.m_forceNoWrap};

    if (secondaryLayout.GetGlyphCount() > 0)
    {
      CalculateTextOffsets(titleDecl, primaryLayout.GetPixelSize(), secondaryLayout.GetPixelSize(), primaryOffset, secondaryOffset);
      secondaryOffset += glsl::vec2(m_symbolOffset.x, m_symbolOffset.y);
      DrawSubString(context, secondaryLayout, titleDecl.m_secondaryTextFont, secondaryOffset, batcher,
                    textures, false /* isPrimary */, titleDecl.m_secondaryOptional);
    }
  }

  // The order of drawing secondary and primary texts has been changed after a minor refactoring for better performance.
  // If there are any issues caused by a swapped order, it should be changed back.
  if (primaryLayout.GetGlyphCount() > 0)
    DrawSubString(context, primaryLayout, titleDecl.m_primaryTextFont, primaryOffset, batcher,
                  textures, true /* isPrimary */, titleDecl.m_primaryOptional);
}

void TextShape::DrawSubString(ref_ptr<dp::GraphicsContext> context, StraightTextLayout & layout,
                              dp::FontDecl const & font, glm::vec2 const & baseOffset,
                              ref_ptr<dp::Batcher> batcher, ref_ptr<dp::TextureManager> textures,
                              bool isPrimary, bool isOptional) const
{
  glsl::vec2 const pt = glsl::ToVec2(ConvertToLocal(m_basePoint, m_params.m_tileCenter, kShapeCoordScalar));
  layout.SetBasePosition(glsl::vec4(pt, m_params.m_depth, -m_params.m_posZ), baseOffset);

  dp::Color outlineColor = isPrimary ? m_params.m_titleDecl.m_primaryTextFont.m_outlineColor
                                     : m_params.m_titleDecl.m_secondaryTextFont.m_outlineColor;

  if (outlineColor == dp::Color::Transparent())
    DrawSubStringPlain(context, layout, font, baseOffset, batcher, textures, isPrimary, isOptional);
  else
    DrawSubStringOutlined(context, layout, font, baseOffset, batcher, textures, isPrimary, isOptional);
}

void TextShape::DrawSubStringPlain(ref_ptr<dp::GraphicsContext> context, StraightTextLayout const & layout,
                                   dp::FontDecl const & font, glm::vec2 const &, ref_ptr<dp::Batcher> batcher,
                                   ref_ptr<dp::TextureManager> textures, bool isPrimary, bool isOptional) const
{
  gpu::TTextStaticVertexBuffer staticBuffer;
  gpu::TTextDynamicVertexBuffer dynamicBuffer;

  dp::TextureManager::ColorRegion color, outline;
  textures->GetColorRegion(font.m_color, color);
  textures->GetColorRegion(font.m_outlineColor, outline);

  auto const finalOffset = layout.GetTextOffset(m_symbolSizes.front(), m_params.m_titleDecl.m_anchor, m_symbolAnchor);
  layout.CacheDynamicGeometry(finalOffset, dynamicBuffer);

  layout.CacheStaticGeometry(color, staticBuffer);

  auto state = CreateRenderState(gpu::Program::Text, m_params.m_depthLayer);
  state.SetProgram3d(gpu::Program::TextBillboard);
  state.SetDepthTestEnabled(m_params.m_depthTestEnabled);

  ASSERT(color.GetTexture() == outline.GetTexture(), ());
  state.SetColorTexture(color.GetTexture());
  state.SetMaskTexture(layout.GetMaskTexture());

  gpu::TTextDynamicVertexBuffer initialDynBuffer(dynamicBuffer.size());

  m2::PointF const & pixelSize = layout.GetPixelSize();

  dp::OverlayID overlayId(m_params.m_featureId, m_params.m_markId, m_tileCoords, m_textIndex);
  drape_ptr<StraightTextHandle> handle = make_unique_dp<StraightTextHandle>(overlayId,
                                                                            layout.GetText(),
                                                                            m_params.m_titleDecl.m_anchor,
                                                                            glsl::ToVec2(m_basePoint),
                                                                            glsl::vec2(pixelSize.x, pixelSize.y),
                                                                            finalOffset,
                                                                            GetOverlayPriority(),
                                                                            textures,
                                                                            isOptional,
                                                                            std::move(dynamicBuffer),
                                                                            m_params.m_minVisibleScale,
                                                                            true);
  if (m_symbolSizes.size() > 1)
    handle->SetDynamicSymbolSizes(layout, m_symbolSizes, m_symbolAnchor);
  handle->SetPivotZ(m_params.m_posZ);

  ASSERT_LESS(m_params.m_startOverlayRank + 1, dp::OverlayRanksCount, ());
  handle->SetOverlayRank(isPrimary ? m_params.m_startOverlayRank : m_params.m_startOverlayRank + 1);

  handle->SetExtendingSize(m_params.m_extendingSize);
  if (m_params.m_specialDisplacement == SpecialDisplacement::UserMark ||
      m_params.m_specialDisplacement == SpecialDisplacement::SpecialModeUserMark)
  {
    handle->SetSpecialLayerOverlay(true);
  }

  dp::AttributeProvider provider(2, static_cast<uint32_t>(staticBuffer.size()));
  provider.InitStream(0, gpu::TextStaticVertex::GetBindingInfo(), make_ref(staticBuffer.data()));
  provider.InitStream(1, gpu::TextDynamicVertex::GetBindingInfo(), make_ref(initialDynBuffer.data()));
  batcher->InsertListOfStrip(context, state, make_ref(&provider), std::move(handle), 4);
}

void TextShape::DrawSubStringOutlined(ref_ptr<dp::GraphicsContext> context, StraightTextLayout const & layout,
                                      dp::FontDecl const & font, glm::vec2 const &, ref_ptr<dp::Batcher> batcher,
                                      ref_ptr<dp::TextureManager> textures, bool isPrimary, bool isOptional) const
{
  gpu::TTextOutlinedStaticVertexBuffer staticBuffer;
  gpu::TTextDynamicVertexBuffer dynamicBuffer;

  dp::TextureManager::ColorRegion color, outline;
  textures->GetColorRegion(font.m_color, color);
  textures->GetColorRegion(font.m_outlineColor, outline);

  auto const finalOffset = layout.GetTextOffset(m_symbolSizes.front(), m_params.m_titleDecl.m_anchor, m_symbolAnchor);
  layout.CacheDynamicGeometry(finalOffset, dynamicBuffer);

  layout.CacheStaticGeometry(color, outline, staticBuffer);

  auto state = CreateRenderState(gpu::Program::TextOutlined, m_params.m_depthLayer);
  state.SetProgram3d(gpu::Program::TextOutlinedBillboard);
  state.SetDepthTestEnabled(m_params.m_depthTestEnabled);
  ASSERT(color.GetTexture() == outline.GetTexture(), ());
  state.SetColorTexture(color.GetTexture());
  state.SetMaskTexture(layout.GetMaskTexture());

  gpu::TTextDynamicVertexBuffer initialDynBuffer(dynamicBuffer.size());

  m2::PointF const & pixelSize = layout.GetPixelSize();

  dp::OverlayID overlayId(m_params.m_featureId, m_params.m_markId, m_tileCoords, m_textIndex);
  drape_ptr<StraightTextHandle> handle = make_unique_dp<StraightTextHandle>(overlayId,
                                                                            layout.GetText(),
                                                                            m_params.m_titleDecl.m_anchor,
                                                                            glsl::ToVec2(m_basePoint),
                                                                            glsl::vec2(pixelSize.x, pixelSize.y),
                                                                            finalOffset,
                                                                            GetOverlayPriority(),
                                                                            textures,
                                                                            isOptional,
                                                                            std::move(dynamicBuffer),
                                                                            m_params.m_minVisibleScale,
                                                                            true);
  if (m_symbolSizes.size() > 1)
    handle->SetDynamicSymbolSizes(layout, m_symbolSizes, m_symbolAnchor);
  handle->SetPivotZ(m_params.m_posZ);

  ASSERT_LESS(m_params.m_startOverlayRank + 1, dp::OverlayRanksCount, ());
  handle->SetOverlayRank(isPrimary ? m_params.m_startOverlayRank : m_params.m_startOverlayRank + 1);

  handle->SetExtendingSize(m_params.m_extendingSize);
  if (m_params.m_specialDisplacement == SpecialDisplacement::UserMark ||
      m_params.m_specialDisplacement == SpecialDisplacement::SpecialModeUserMark)
  {
    handle->SetSpecialLayerOverlay(true);
  }

  dp::AttributeProvider provider(2, static_cast<uint32_t>(staticBuffer.size()));
  provider.InitStream(0, gpu::TextOutlinedStaticVertex::GetBindingInfo(), make_ref(staticBuffer.data()));
  provider.InitStream(1, gpu::TextDynamicVertex::GetBindingInfo(), make_ref(initialDynBuffer.data()));
  batcher->InsertListOfStrip(context, state, make_ref(&provider), std::move(handle), 4);
}

// TODO: *Shape classes are concerned with drawing themselves. Its strange they decide/manipulate overlays' priorities
// in the scene. It seems more logical to set priorities beforehand in the creators of *Shapes and pass on final values only.
// Check if such a refactoring makes sense.
uint64_t TextShape::GetOverlayPriority() const
{
  // Set up maximum priority for shapes which created by user in the editor and in case of disabling
  // displacement.
  if (m_params.m_createdByEditor || m_disableDisplacing)
    return dp::kPriorityMaskAll;

  if (m_params.m_specialDisplacement == SpecialDisplacement::SpecialModeUserMark)
    return dp::CalculateSpecialModeUserMarkPriority(m_params.m_specialPriority);

  if (m_params.m_specialDisplacement == SpecialDisplacement::UserMark)
    return dp::CalculateUserMarkPriority(m_params.m_minVisibleScale, m_params.m_specialPriority);

  // Overlay priority for text shapes considers length of the primary text
  // (the more text length, the more priority) and index of text.
  // [6 bytes - standard overlay priority][1 byte - length][1 byte - text index].
  static uint64_t constexpr kMask = ~static_cast<uint64_t>(0xFFFF);
  uint64_t priority = dp::CalculateOverlayPriority(m_params.m_rank, m_params.m_depth);
  priority &= kMask;
  priority |= (static_cast<uint8_t>(m_params.m_titleDecl.m_primaryText.size()) << 8);
  priority |= static_cast<uint8_t>(m_textIndex);

  return priority;
}
}  // namespace df
