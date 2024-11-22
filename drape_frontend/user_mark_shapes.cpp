#include "drape_frontend/user_mark_shapes.hpp"

#include "drape_frontend/colored_symbol_shape.hpp"
#include "drape_frontend/line_shape.hpp"
#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/poi_symbol_shape.hpp"
#include "drape_frontend/shape_view_params.hpp"
#include "drape_frontend/text_layout.hpp"
#include "drape_frontend/text_shape.hpp"
#include "drape_frontend/visual_params.hpp"

#include "shaders/programs.hpp"

#include "drape/utils/vertex_decl.hpp"
#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"

#include "indexer/feature_decl.hpp"
#include "indexer/scales.hpp"

#include "geometry/clipping.hpp"
#include "geometry/mercator.hpp"

#include <array>
#include <cmath>
#include <vector>

namespace df
{
namespace
{
std::array<double, 20> constexpr kLineWidthZoomFactor =
{
// 1   2    3    4    5    6    7    8    9    10   11   12   13   14   15   16   17   18   19   20
  0.3, 0.3, 0.3, 0.4, 0.5, 0.6, 0.7, 0.7, 0.7, 0.7, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
};

template <typename TCreateVector>
void AlignFormingNormals(TCreateVector const & fn, dp::Anchor anchor, dp::Anchor first,
                         dp::Anchor second, glsl::vec2 & firstNormal, glsl::vec2 & secondNormal)
{
  firstNormal = fn();
  secondNormal = -firstNormal;
  if ((anchor & second) != 0)
  {
    firstNormal *= 2;
    secondNormal = glsl::vec2(0.0, 0.0);
  }
  else if ((anchor & first) != 0)
  {
    firstNormal = glsl::vec2(0.0, 0.0);
    secondNormal *= 2;
  }
}

void AlignHorizontal(float halfWidth, dp::Anchor anchor, glsl::vec2 & left, glsl::vec2 & right)
{
  AlignFormingNormals([&halfWidth] { return glsl::vec2(-halfWidth, 0.0f); }, anchor, dp::Left,
                      dp::Right, left, right);
}

void AlignVertical(float halfHeight, dp::Anchor anchor, glsl::vec2 & up, glsl::vec2 & down)
{
  AlignFormingNormals([&halfHeight] { return glsl::vec2(0.0f, -halfHeight); }, anchor, dp::Top,
                      dp::Bottom, up, down);
}

struct UserPointVertex : public gpu::BaseVertex
{
  using TNormalAndAnimateOrZ = glsl::vec3;
  using TTexCoord = glsl::vec4;
  using TColor = glsl::vec4;
  using TAnimateOrZ = float;

  UserPointVertex() = default;
  UserPointVertex(TPosition const & pos, TNormalAndAnimateOrZ const & normalAndAnimateOrZ,
                  TTexCoord const & texCoord, TColor const & color)
    : m_position(pos)
    , m_normalAndAnimateOrZ(normalAndAnimateOrZ)
    , m_texCoord(texCoord)
    , m_color(color)
  {}

  static dp::BindingInfo GetBinding()
  {
    dp::BindingInfo info(4);
    uint8_t offset = 0;
    offset += dp::FillDecl<TPosition, UserPointVertex>(0, "a_position", info, offset);
    offset += dp::FillDecl<TNormalAndAnimateOrZ, UserPointVertex>(1, "a_normalAndAnimateOrZ", info,
                                                                  offset);
    offset += dp::FillDecl<TTexCoord, UserPointVertex>(2, "a_texCoords", info, offset);
    /*offset += */dp::FillDecl<TColor, UserPointVertex>(3, "a_color", info, offset);

    return info;
  }

  TPosition m_position;
  TNormalAndAnimateOrZ m_normalAndAnimateOrZ;
  TTexCoord m_texCoord;
  TColor m_color;
};

std::string GetSymbolNameForZoomLevel(ref_ptr<UserPointMark::SymbolNameZoomInfo> symbolNames, TileKey const & tileKey)
{
  if (!symbolNames)
    return {};

  for (auto itName = symbolNames->crbegin(); itName != symbolNames->crend(); ++itName)
  {
    if (itName->first <= tileKey.m_zoomLevel)
      return itName->second;
  }
  return {};
}

m2::PointF GetSymbolOffsetForZoomLevel(ref_ptr<UserPointMark::SymbolOffsets> symbolOffsets, TileKey const & tileKey)
{
  if (!symbolOffsets)
    return m2::PointF::Zero();

  CHECK_GREATER(tileKey.m_zoomLevel, 0, ());
  CHECK_LESS_OR_EQUAL(tileKey.m_zoomLevel, scales::UPPER_STYLE_SCALE, ());

  auto const offsetIndex = static_cast<size_t>(tileKey.m_zoomLevel - 1);
  return symbolOffsets->operator[](offsetIndex);
}

void GenerateColoredSymbolShapes(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::TextureManager> textures,
                                 UserMarkRenderParams const & renderInfo, TileKey const & tileKey,
                                 m2::PointD const & tileCenter, m2::PointF const & symbolOffset,
                                 m2::PointF & symbolSize, dp::Batcher & batcher)
{
  m2::PointF sizeInc(0.0f, 0.0f);
  m2::PointF offset(0.0f, 0.0f);
  UserPointMark::SymbolSizes symbolSizesInc;

  auto const isTextBg = renderInfo.m_coloredSymbols->m_addTextSize;

  if (isTextBg)
  {
    CHECK(renderInfo.m_titleDecl, ());
    auto const & titleDecl = renderInfo.m_titleDecl->operator[](0);
    auto const textMetrics = textures->ShapeSingleTextLine(dp::kBaseFontSizePixels, titleDecl.m_primaryText, nullptr);
    auto const fontScale = static_cast<float>(VisualParams::Instance().GetFontScale());
    float const textRatio = titleDecl.m_primaryTextFont.m_size * fontScale / dp::kBaseFontSizePixels;

    sizeInc.x = textMetrics.m_lineWidthInPixels * textRatio;
    sizeInc.y = textMetrics.m_maxLineHeightInPixels * textRatio;

    if (renderInfo.m_symbolSizes != nullptr)
    {
      symbolSizesInc.reserve(renderInfo.m_symbolSizes->size());
      for (auto const & sz : *renderInfo.m_symbolSizes)
        symbolSizesInc.push_back(sz + sizeInc);
    }

    offset = StraightTextLayout::GetSymbolBasedTextOffset(symbolSize, titleDecl.m_anchor, renderInfo.m_anchor);
  }

  ColoredSymbolViewParams params;
  if (renderInfo.m_coloredSymbols->m_isSymbolStub)
  {
    params.m_anchor = renderInfo.m_anchor;
    params.m_color = dp::Color::Transparent();
    params.m_shape = ColoredSymbolViewParams::Shape::Rectangle;
    params.m_sizeInPixels = symbolSize;
    params.m_offset = symbolOffset;
  }
  else
  {
    for (auto const & e : renderInfo.m_coloredSymbols->m_zoomInfo)
    {
      if (e.first <= tileKey.m_zoomLevel)
      {
        params = e.second;
        break;
      }
    }
  }

  // Assign ids after fetching params from map above.
  params.m_featureId = renderInfo.m_featureId;
  params.m_markId = renderInfo.m_markId;

  m2::PointF coloredSize(0.0f, 0.0f);
  if (params.m_shape == ColoredSymbolViewParams::Shape::Circle)
  {
    params.m_radiusInPixels = params.m_radiusInPixels + std::max(sizeInc.x, sizeInc.y) / 2.0f;
    coloredSize = m2::PointF(params.m_radiusInPixels * 2.0f, params.m_radiusInPixels * 2.0f);
  }
  else
  {
    params.m_sizeInPixels = params.m_sizeInPixels + sizeInc;
    coloredSize = params.m_sizeInPixels;
  }
  if (!isTextBg)
    symbolSize = m2::PointF(std::max(coloredSize.x, symbolSize.x), std::max(coloredSize.y, symbolSize.y));

  params.m_tileCenter = tileCenter;
  params.m_depthTestEnabled = renderInfo.m_depthTestEnabled;
  params.m_depth = renderInfo.m_depth;
  params.m_depthLayer = renderInfo.m_depthLayer;
  params.m_minVisibleScale = renderInfo.m_minZoom;
  params.m_specialDisplacement = renderInfo.m_displacement;
  params.m_specialPriority = renderInfo.m_priority;
  params.m_offset += offset;
  if (renderInfo.m_symbolSizes != nullptr)
  {
    ColoredSymbolShape(renderInfo.m_pivot, params, tileKey,
                       kStartUserMarkOverlayIndex + renderInfo.m_index,
                       isTextBg ? symbolSizesInc : *renderInfo.m_symbolSizes.get())
        .Draw(context, &batcher, textures);
  }
  else
  {
    ColoredSymbolShape(renderInfo.m_pivot, params, tileKey,
                       kStartUserMarkOverlayIndex + renderInfo.m_index, renderInfo.m_coloredSymbols->m_needOverlay)
        .Draw(context, &batcher, textures);
  }
}

void GeneratePoiSymbolShape(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::TextureManager> textures,
                            UserMarkRenderParams const & renderInfo, TileKey const & tileKey,
                            m2::PointD const & tileCenter, std::string const & symbolName,
                            m2::PointF const & symbolOffset, dp::Batcher & batcher)
{
  PoiSymbolViewParams params;
  params.m_featureId = renderInfo.m_featureId;
  params.m_markId = renderInfo.m_markId;
  params.m_tileCenter = tileCenter;
  params.m_depthTestEnabled = renderInfo.m_depthTestEnabled;
  params.m_depth = renderInfo.m_depth;
  params.m_depthLayer = renderInfo.m_depthLayer;
  params.m_minVisibleScale = renderInfo.m_minZoom;
  params.m_specialDisplacement = renderInfo.m_displacement;
  params.m_specialPriority = renderInfo.m_priority;
  params.m_symbolName = symbolName;
  params.m_anchor = renderInfo.m_anchor;
  params.m_offset = symbolOffset;

  bool const hasColoredOverlay = renderInfo.m_coloredSymbols != nullptr && renderInfo.m_coloredSymbols->m_needOverlay;
  params.m_startOverlayRank = hasColoredOverlay ? dp::OverlayRank1 : dp::OverlayRank0;

  PoiSymbolShape(renderInfo.m_pivot, params, tileKey,
                 kStartUserMarkOverlayIndex + renderInfo.m_index)
      .Draw(context, &batcher, textures);
}

void GenerateTextShapes(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::TextureManager> textures,
                        UserMarkRenderParams const & renderInfo, TileKey const & tileKey,
                        m2::PointD const & tileCenter, m2::PointF const & symbolOffset,
                        m2::PointF const & symbolSize, dp::Batcher & batcher)
{
  if (renderInfo.m_minTitleZoom > tileKey.m_zoomLevel)
    return;

  auto const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());

  for (auto const & titleDecl : *renderInfo.m_titleDecl)
  {
    if (titleDecl.m_primaryText.empty())
      continue;

    TextViewParams params;
    params.m_featureId = renderInfo.m_featureId;
    params.m_markId = renderInfo.m_markId;
    params.m_tileCenter = tileCenter;
    params.m_titleDecl = titleDecl;

    // Here we use visual scale to adapt texts sizes and offsets
    // to different screen resolutions and DPI.
    params.m_titleDecl.m_primaryTextFont.m_size *= vs;
    params.m_titleDecl.m_secondaryTextFont.m_size *= vs;
    params.m_titleDecl.m_primaryOffset *= vs;
    params.m_titleDecl.m_secondaryOffset *= vs;

    params.m_depthTestEnabled = renderInfo.m_depthTestEnabled;
    params.m_depth = renderInfo.m_depth;
    params.m_depthLayer = renderInfo.m_depthLayer;
    params.m_minVisibleScale = renderInfo.m_minZoom;

    uint32_t const overlayIndex = kStartUserMarkOverlayIndex + renderInfo.m_index;
    if (renderInfo.m_hasTitlePriority)
    {
      params.m_specialDisplacement = renderInfo.m_displacement;
      params.m_specialPriority = renderInfo.m_priority;
      params.m_startOverlayRank = dp::OverlayRank0;

      if (renderInfo.m_symbolNames != nullptr && renderInfo.m_symbolIsPOI)
        params.m_startOverlayRank++;

      if (renderInfo.m_coloredSymbols != nullptr && renderInfo.m_coloredSymbols->m_needOverlay)
        params.m_startOverlayRank++;

      ASSERT_LESS(params.m_startOverlayRank, dp::OverlayRanksCount, ());
    }

    if (renderInfo.m_symbolSizes != nullptr)
    {
      TextShape(renderInfo.m_pivot, params, tileKey, *renderInfo.m_symbolSizes,
                m2::PointF(0.0f, 0.0f) /* symbolOffset */, renderInfo.m_anchor, overlayIndex)
          .Draw(context, &batcher, textures);
    }
    else
    {
      TextShape(renderInfo.m_pivot, params, tileKey, symbolSize, symbolOffset, renderInfo.m_anchor,
                overlayIndex)
          .Draw(context, &batcher, textures);
    }
  }
}

m2::SharedSpline SimplifySpline(m2::SharedSpline const & in, double minSqrLength)
{
  m2::SharedSpline spline;
  spline.Reset(new m2::Spline(in->GetSize()));

  m2::PointD lastAddedPoint;
  for (auto const & point : in->GetPath())
  {
    if (spline->GetSize() > 1 && point.SquaredLength(lastAddedPoint) < minSqrLength)
    {
      spline->ReplacePoint(point);
    }
    else
    {
      spline->AddPoint(point);
      lastAddedPoint = point;
    }
  }
  return spline;
}

std::string GetBackgroundSymbolName(std::string const & symbolName)
{
  char const * kDelimiter = "-";
  auto const tokens = strings::Tokenize(symbolName, kDelimiter);
  if (tokens.size() < 2 || tokens.size() > 3)
    return {};

  std::string res;
  res.append(tokens[0]).append(kDelimiter).append("bg");
  if (tokens.size() == 3)
    res.append(kDelimiter).append(tokens[2]);
  return res;
}

drape_ptr<dp::OverlayHandle> CreateSymbolOverlayHandle(UserMarkRenderParams const & renderInfo,
                                                       TileKey const & tileKey,
                                                       m2::PointF const & symbolOffset,
                                                       m2::RectD const & pixelRect)
{
  if (!renderInfo.m_isSymbolSelectable || !renderInfo.m_isNonDisplaceable)
    return nullptr;

  dp::OverlayID overlayId(renderInfo.m_featureId, renderInfo.m_markId, tileKey.GetTileCoords(),
                          kStartUserMarkOverlayIndex + renderInfo.m_index);
  drape_ptr<dp::OverlayHandle> handle = make_unique_dp<dp::SquareHandle>(
      overlayId, renderInfo.m_anchor, renderInfo.m_pivot,
      pixelRect.RightTop() - pixelRect.LeftBottom(), m2::PointD(symbolOffset), 0 /*priority*/,
      true /* isBound */, renderInfo.m_minZoom, true /* isBillboard */);
  return handle;
}
}  // namespace

void CacheUserMarks(ref_ptr<dp::GraphicsContext> context, TileKey const & tileKey,
                    ref_ptr<dp::TextureManager> textures, kml::MarkIdCollection const & marksId,
                    UserMarksRenderCollection const & renderParams, dp::Batcher & batcher)
{
  using UPV = UserPointVertex;
  buffer_vector<UPV, dp::Batcher::VertexPerQuad> buffer;

  for (auto const id : marksId)
  {
    auto const it = renderParams.find(id);
    if (it == renderParams.end())
      continue;

    UserMarkRenderParams const & renderInfo = *it->second;
    if (!renderInfo.m_isVisible)
      continue;

    m2::PointD const tileCenter = tileKey.GetGlobalRect().Center();

    m2::PointF symbolSize(0.0f, 0.0f);
    dp::TextureManager::SymbolRegion symbolRegion;
    auto const symbolName = GetSymbolNameForZoomLevel(make_ref(renderInfo.m_symbolNames), tileKey);
    if (!symbolName.empty())
    {
      textures->GetSymbolRegion(symbolName, symbolRegion);
      symbolSize = symbolRegion.GetPixelSize();
    }

    m2::PointF symbolOffset = m2::PointF::Zero();
    if (renderInfo.m_symbolIsPOI)
      symbolOffset = GetSymbolOffsetForZoomLevel(make_ref(renderInfo.m_symbolOffsets), tileKey);

    if (renderInfo.m_coloredSymbols != nullptr)
    {
      GenerateColoredSymbolShapes(context, textures, renderInfo, tileKey, tileCenter, symbolOffset, symbolSize,
                                  batcher);
    }

    if (!symbolName.empty())
    {
      if (renderInfo.m_symbolIsPOI)
      {
        GeneratePoiSymbolShape(context, textures, renderInfo, tileKey, tileCenter, symbolName, symbolOffset, batcher);
      }
      else
      {
        buffer.clear();

        dp::TextureManager::SymbolRegion backgroundRegion;
        if (auto const background = GetBackgroundSymbolName(symbolName); !background.empty())
        {
          if (textures->GetSymbolRegionSafe(background, backgroundRegion))
            CHECK_EQUAL(symbolRegion.GetTextureIndex(), backgroundRegion.GetTextureIndex(), ());
        }

        m2::RectF const & texRect = symbolRegion.GetTexRect();
        m2::RectF const & bgTexRect = backgroundRegion.GetTexRect();
        m2::PointF const pxSize = symbolRegion.GetPixelSize();
        dp::Anchor const anchor = renderInfo.m_anchor;
        m2::PointD const pt = MapShape::ConvertToLocal(renderInfo.m_pivot, tileCenter,
                                                       kShapeCoordScalar);
        glsl::vec3 const pos = glsl::vec3(glsl::ToVec2(pt), renderInfo.m_depth);
        bool const runAnim = renderInfo.m_hasCreationAnimation && renderInfo.m_justCreated;

        glsl::vec2 left, right, up, down;
        AlignHorizontal(pxSize.x * 0.5f, anchor, left, right);
        AlignVertical(pxSize.y * 0.5f, anchor, up, down);

        m2::PointD const pixelOffset = renderInfo.m_pixelOffset;
        glsl::vec2 const offset(pixelOffset.x, pixelOffset.y);

        dp::Color color = dp::Color::White();
        if (!renderInfo.m_color.empty())
          color = df::GetColorConstant(renderInfo.m_color);

        glsl::vec4 maskColor(color.GetRedF(), color.GetGreenF(), color.GetBlueF(),
                             renderInfo.m_symbolOpacity);
        float animateOrZ = 0.0f;
        if (!renderInfo.m_customDepth)
          animateOrZ = runAnim ? 1.0f : -1.0f;

        buffer.emplace_back(
            pos, glsl::vec3(left + down + offset, animateOrZ),
            glsl::ToVec4(m2::PointD(texRect.LeftTop()), m2::PointD(bgTexRect.LeftTop())),
            maskColor);
        buffer.emplace_back(
            pos, glsl::vec3(left + up + offset, animateOrZ),
            glsl::ToVec4(m2::PointD(texRect.LeftBottom()), m2::PointD(bgTexRect.LeftBottom())),
            maskColor);
        buffer.emplace_back(
            pos, glsl::vec3(right + down + offset, animateOrZ),
            glsl::ToVec4(m2::PointD(texRect.RightTop()), m2::PointD(bgTexRect.RightTop())),
            maskColor);
        buffer.emplace_back(
            pos, glsl::vec3(right + up + offset, animateOrZ),
            glsl::ToVec4(m2::PointD(texRect.RightBottom()), m2::PointD(bgTexRect.RightBottom())),
            maskColor);

        m2::RectD rect;
        for (auto const & vertex : buffer)
          rect.Add(glsl::FromVec2(glsl::vec2(vertex.m_normalAndAnimateOrZ)));

        drape_ptr<dp::OverlayHandle> overlayHandle =
            CreateSymbolOverlayHandle(renderInfo, tileKey, symbolOffset, rect);

        gpu::Program program;
        gpu::Program program3d;
        if (renderInfo.m_isMarkAboveText)
        {
          program = runAnim ? gpu::Program::BookmarkAnimAboveText
                            : gpu::Program::BookmarkAboveText;
          program3d = runAnim ? gpu::Program::BookmarkAnimAboveTextBillboard
                              : gpu::Program::BookmarkAboveTextBillboard;
        }
        else
        {
          program = runAnim ? gpu::Program::BookmarkAnim
                            : gpu::Program::Bookmark;
          program3d = runAnim ? gpu::Program::BookmarkAnimBillboard
                              : gpu::Program::BookmarkBillboard;
        }
        auto state = CreateRenderState(program, renderInfo.m_depthLayer);
        state.SetProgram3d(program3d);
        state.SetColorTexture(symbolRegion.GetTexture());
        state.SetTextureFilter(dp::TextureFilter::Nearest);
        state.SetDepthTestEnabled(renderInfo.m_depthTestEnabled);
        state.SetTextureIndex(symbolRegion.GetTextureIndex());

        dp::AttributeProvider attribProvider(1, static_cast<uint32_t>(buffer.size()));
        attribProvider.InitStream(0, UPV::GetBinding(), make_ref(buffer.data()));

        batcher.InsertListOfStrip(context, state, make_ref(&attribProvider),
                                  std::move(overlayHandle), dp::Batcher::VertexPerQuad);
      }
    }

    if (renderInfo.m_titleDecl != nullptr)
      GenerateTextShapes(context, textures, renderInfo, tileKey, tileCenter, symbolOffset, symbolSize, batcher);

    renderInfo.m_justCreated = false;
  }
}

void ProcessSplineSegmentRects(m2::SharedSpline const & spline, double maxSegmentLength,
                               const std::function<bool(const m2::RectD & segmentRect)> & func)
{
  double const splineFullLength = spline->GetLength();
  double length = 0;
  while (length < splineFullLength)
  {
    m2::RectD splineRect;

    auto const itBegin = spline->GetPoint(length);
    auto itEnd = spline->GetPoint(length + maxSegmentLength);
    if (itEnd.BeginAgain())
    {
      double const lastSegmentLength = spline->GetLastLength();
      itEnd = spline->GetPoint(splineFullLength - lastSegmentLength / 2.0);
      splineRect.Add(spline->GetPath().back());
    }

    spline->ForEachNode(itBegin, itEnd, [&splineRect](m2::PointD const & pt)
    {
      splineRect.Add(pt);
    });

    length += maxSegmentLength;

    if (!func(splineRect))
      return;
  }
}

void CacheUserLines(ref_ptr<dp::GraphicsContext> context, TileKey const & tileKey,
                    ref_ptr<dp::TextureManager> textures, kml::TrackIdCollection const & linesId,
                    UserLinesRenderCollection const & renderParams, dp::Batcher & batcher)
{
  CHECK_GREATER(tileKey.m_zoomLevel, 0, ());
  CHECK_LESS(tileKey.m_zoomLevel - 1, static_cast<int>(kLineWidthZoomFactor.size()), ());

  double const vs = df::VisualParams::Instance().GetVisualScale();
  bool const simplify = tileKey.m_zoomLevel <= 15;

  // This var is used only if simplify == true.
  double minSegmentSqrLength = 1.0;
  if (simplify)
    minSegmentSqrLength = math::Pow2(4.0 * vs * GetScreenScale(tileKey.m_zoomLevel));

  m2::RectD const tileRect = tileKey.GetGlobalRect();

  // Process spline by segments that are no longer than tile size.
  //double const maxLength = mercator::Bounds::kRangeX / (1 << (tileKey.m_zoomLevel - 1));

  for (auto const & id : linesId)
  {
    auto const it = renderParams.find(id);
    if (it == renderParams.end())
      continue;

    UserLineRenderParams const & renderInfo = *it->second;

    // Spline is a shared_ptr here, can reassign later.
    for (auto spline : renderInfo.m_splines)
    {
      // This check is redundant, because we already made rough check while covering tracks by tiles
      // (see UserMarkGenerator::UpdateIndex).
      // Also looks like ClipSplineByRect works faster than Spline iterating in ProcessSplineSegmentRects
      // by |maxLength| segments on high zoom levels.
      /*
      bool intersected = false;
      ProcessSplineSegmentRects(spline, maxLength, [&tileRect, &intersected](m2::RectD const & segmentRect)
      {
        if (segmentRect.IsIntersect(tileRect))
          intersected = true;
        return !intersected;
      });

      if (!intersected)
        continue;
      */

      if (simplify)
        spline = SimplifySpline(spline, minSegmentSqrLength);

      if (spline->GetSize() < 2)
        continue;

      for (auto const & clippedSpline : m2::ClipSplineByRect(tileRect, spline))
      {
        for (auto const & layer : renderInfo.m_layers)
        {
          LineViewParams params;
          params.m_tileCenter = tileRect.Center();
          params.m_baseGtoPScale = 1.0f;
          params.m_cap = dp::RoundCap;
          params.m_join = dp::RoundJoin;
          params.m_color = layer.m_color;
          params.m_depthTestEnabled = true;
          params.m_depth = layer.m_depth;
          params.m_depthLayer = renderInfo.m_depthLayer;
          params.m_width = static_cast<float>(layer.m_width * vs * kLineWidthZoomFactor[tileKey.m_zoomLevel - 1]);
          params.m_minVisibleScale = 1;
          params.m_rank = 0;

          LineShape(clippedSpline, params).Draw(context, make_ref(&batcher), textures);
        }
      }
    }
  }
}
} // namespace df
