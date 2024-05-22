#include "drape_frontend/text_layout.hpp"
#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/bidi.hpp"
#include "drape/font_constants.hpp"

#include <algorithm>
#include <iterator>  // std::reverse_iterator
#include <numeric>

namespace df
{
namespace
{
float constexpr kValidSplineTurn = 0.96f;

class TextGeometryGenerator
{
public:
  TextGeometryGenerator(dp::TextureManager::ColorRegion const & color,
                        gpu::TTextStaticVertexBuffer & buffer)
    : m_colorCoord(glsl::ToVec2(color.GetTexRect().Center()))
    , m_buffer(buffer)
  {}

  void SetPenPosition(glsl::vec2 const & /* penOffset */) {}

  void operator() (dp::TextureManager::GlyphRegion const & glyph)
  {
    m2::RectF const & mask = glyph.GetTexRect();

    m_buffer.emplace_back(m_colorCoord, glsl::ToVec2(mask.LeftTop()));
    m_buffer.emplace_back(m_colorCoord, glsl::ToVec2(mask.LeftBottom()));
    m_buffer.emplace_back(m_colorCoord, glsl::ToVec2(mask.RightTop()));
    m_buffer.emplace_back(m_colorCoord, glsl::ToVec2(mask.RightBottom()));
  }

protected:
  glsl::vec2 m_colorCoord;
  gpu::TTextStaticVertexBuffer & m_buffer;
};

class StraightTextGeometryGenerator
{
public:
  StraightTextGeometryGenerator(glsl::vec4 const & pivot,
                                glsl::vec2 const & pixelOffset, float textRatio,
                                gpu::TTextDynamicVertexBuffer & dynBuffer)
    : m_pivot(pivot)
    , m_pixelOffset(pixelOffset)
    , m_buffer(dynBuffer)
    , m_textRatio(textRatio)
  {}

  void SetPenPosition(glsl::vec2 const & penPosition)
  {
    m_penPosition = penPosition;
    m_isFirstGlyph = true;
  }

  void operator()(dp::TextureManager::GlyphRegion const & glyph)
  {
    if (!glyph.IsValid())
      return;
    m2::PointF const pixelSize = glyph.GetPixelSize() * m_textRatio;

    float const xOffset = glyph.GetOffsetX() * m_textRatio;
    float const yOffset = glyph.GetOffsetY() * m_textRatio;

    float const upVector = -static_cast<int32_t>(pixelSize.y) - yOffset;
    float const bottomVector = -yOffset;

    if (m_isFirstGlyph)
    {
      m_isFirstGlyph = false;
      m_penPosition.x -= (xOffset + dp::kSdfBorder * m_textRatio);
    }

    auto const pixelPlusPen = m_pixelOffset + m_penPosition;
    m_buffer.emplace_back(m_pivot, pixelPlusPen + glsl::vec2(xOffset, bottomVector));
    m_buffer.emplace_back(m_pivot, pixelPlusPen + glsl::vec2(xOffset, upVector));
    m_buffer.emplace_back(m_pivot, pixelPlusPen + glsl::vec2(pixelSize.x + xOffset, bottomVector));
    m_buffer.emplace_back(m_pivot, pixelPlusPen + glsl::vec2(pixelSize.x + xOffset, upVector));
    m_penPosition += glsl::vec2(glyph.GetAdvanceX() * m_textRatio, glyph.GetAdvanceY() * m_textRatio);
  }

private:
  glsl::vec4 const & m_pivot;
  glsl::vec2 m_pixelOffset;
  glsl::vec2 m_penPosition;
  gpu::TTextDynamicVertexBuffer & m_buffer;
  float m_textRatio = 0.0f;
  bool m_isFirstGlyph = true;
};

class TextOutlinedGeometryGenerator
{
public:
  TextOutlinedGeometryGenerator(dp::TextureManager::ColorRegion const & color,
                                dp::TextureManager::ColorRegion const & outline,
                                gpu::TTextOutlinedStaticVertexBuffer & buffer)
    : m_colorCoord(glsl::ToVec2(color.GetTexRect().Center()))
    , m_outlineCoord(glsl::ToVec2(outline.GetTexRect().Center()))
    , m_buffer(buffer)
  {}

  void SetPenPosition(glsl::vec2 const & /* penOffset */) {}

  void operator() (dp::TextureManager::GlyphRegion const & glyph)
  {
    m2::RectF const & mask = glyph.GetTexRect();
    m_buffer.emplace_back(m_colorCoord, m_outlineCoord, glsl::ToVec2(mask.LeftTop()));
    m_buffer.emplace_back(m_colorCoord, m_outlineCoord, glsl::ToVec2(mask.LeftBottom()));
    m_buffer.emplace_back(m_colorCoord, m_outlineCoord, glsl::ToVec2(mask.RightTop()));
    m_buffer.emplace_back(m_colorCoord, m_outlineCoord, glsl::ToVec2(mask.RightBottom()));
  }

protected:
  glsl::vec2 m_colorCoord;
  glsl::vec2 m_outlineCoord;
  gpu::TTextOutlinedStaticVertexBuffer & m_buffer;
};

void SplitText(strings::UniString & visText, buffer_vector<size_t, 2> & delimIndexes)
{
  size_t const count = visText.size();
  if (count > 15)
  {
    // split on two parts
    typedef strings::UniString::iterator TIter;
    auto const iMiddle = visText.begin() + count / 2;

    char constexpr delims[] = " \n\t";
    size_t constexpr delimsSize = sizeof(delims)/sizeof(delims[0]) - 1;  // Do not count trailing string's zero.

    // find next delimiter after middle [m, e)
    auto iNext = std::find_first_of(iMiddle,
                                    visText.end(),
                                    delims, delims + delimsSize);

    // find last delimiter before middle [b, m)
    auto iPrev = std::find_first_of(std::reverse_iterator<TIter>(iMiddle),
                                    std::reverse_iterator<TIter>(visText.begin()),
                                    delims, delims + delimsSize).base();
    // don't do split like this:
    //     xxxx
    // xxxxxxxxxxxx
    if (4 * std::distance(visText.begin(), iPrev) <= static_cast<long>(count))
      iPrev = visText.end();
    else
      --iPrev;

    // get the closest delimiter to the middle
    if (iNext == visText.end() ||
        (iPrev != visText.end() && std::distance(iPrev, iMiddle) < std::distance(iMiddle, iNext)))
    {
      iNext = iPrev;
    }

    // split string on 2 parts
    if (iNext != visText.end())
    {
      ASSERT(iNext != visText.begin(), ());
      TIter delimSymbol = iNext;
      TIter secondPart = iNext + 1;

      delimIndexes.push_back(static_cast<size_t>(std::distance(visText.begin(), delimSymbol)));

      if (secondPart != visText.end())
      {
        strings::UniString result(visText.begin(), delimSymbol);
        result.append(secondPart, visText.end());
        visText = result;
        delimIndexes.push_back(visText.size());
      }
      return;
    }
  }

  delimIndexes.push_back(count);
}

class XLayouter
{
public:
  explicit XLayouter(dp::Anchor anchor)
    : m_anchor(anchor)
  {}

  float operator()(float currentLength, float maxLength) const
  {
    ASSERT_GREATER_OR_EQUAL(maxLength, currentLength, ());

    if (m_anchor & dp::Left)
      return 0.0;
    else if (m_anchor & dp::Right)
      return -currentLength;
    else
      return -(currentLength / 2.0f);
  }

private:
  dp::Anchor m_anchor;
};

class YLayouter
{
public:
  YLayouter(dp::Anchor anchor, float summaryHeight)
  {
    if (anchor & dp::Top)
      m_penOffset = 0.0f;
    else if (anchor & dp::Bottom)
      m_penOffset = -summaryHeight;
    else
      m_penOffset = -(summaryHeight / 2.0f);
  }

  float operator()(float currentHeight)
  {
    m_penOffset += currentHeight;
    return m_penOffset;
  }

private:
    float m_penOffset;
};

void CalculateOffsets(dp::Anchor anchor, float textRatio,
                      dp::TextureManager::TGlyphsBuffer const & glyphs,
                      buffer_vector<size_t, 2> const & delimIndexes,
                      buffer_vector<std::pair<size_t, glsl::vec2>, 2> & result,
                      m2::PointF & pixelSize, size_t & rowsCount)
{
  typedef std::pair<float, float> TLengthAndHeight;
  buffer_vector<TLengthAndHeight, 2> lengthAndHeight;
  float maxLength = 0;
  float summaryHeight = 0;
  rowsCount = 0;

  size_t start = 0;
  for (size_t index = 0; index < delimIndexes.size(); ++index)
  {
    size_t const end = delimIndexes[index];
    ASSERT_NOT_EQUAL(start, end, ());
    lengthAndHeight.emplace_back(0, 0);
    auto & [length, height] = lengthAndHeight.back();
    for (size_t glyphIndex = start; glyphIndex < end && glyphIndex < glyphs.size(); ++glyphIndex)
    {
      dp::TextureManager::GlyphRegion const & glyph = glyphs[glyphIndex];
      if (!glyph.IsValid())
        continue;

      if (glyphIndex == start)
        length -= glyph.GetOffsetX() * textRatio;

      length += glyph.GetAdvanceX() * textRatio;

      float yAdvance = glyph.GetAdvanceY();
      if (glyph.GetOffsetY() < 0)
        yAdvance += glyph.GetOffsetY();

      height = std::max(height, (glyph.GetPixelHeight() + yAdvance) * textRatio);
    }
    maxLength = std::max(maxLength, length);
    summaryHeight += height;
    if (height > 0.0f)
      ++rowsCount;
    start = end;
  }

  ASSERT_EQUAL(delimIndexes.size(), lengthAndHeight.size(), ());

  XLayouter const xL(anchor);
  YLayouter yL(anchor, summaryHeight);
  for (size_t index = 0; index < delimIndexes.size(); ++index)
  {
    auto const & [length, height] = lengthAndHeight[index];
    result.emplace_back(delimIndexes[index], glsl::vec2(xL(length, maxLength), yL(height)));
  }

  pixelSize = m2::PointF(maxLength, summaryHeight);
}

double GetTextMinPeriod(double pixelTextLength)
{
  double const vs = df::VisualParams::Instance().GetVisualScale();
  double const etalonEmpty = std::max(300.0 * vs, pixelTextLength);
  return etalonEmpty + pixelTextLength;
}
}  // namespace

void TextLayout::Init(strings::UniString && text, float fontSize, ref_ptr<dp::TextureManager> textures)
{
  m_text = std::move(text);
  auto const & vpi = VisualParams::Instance();
  float const fontScale = static_cast<float>(vpi.GetFontScale());
  m_textSizeRatio = fontSize * fontScale / dp::kBaseFontSizePixels;
  textures->GetGlyphRegions(m_text, m_metrics);
}

ref_ptr<dp::Texture> TextLayout::GetMaskTexture() const
{
  ASSERT(!m_metrics.empty(), ());
#ifdef DEBUG
  ref_ptr<dp::Texture> tex = m_metrics[0].GetTexture();
  for (GlyphRegion const & g : m_metrics)
  {
    ASSERT(g.GetTexture() == tex, ());
  }
#endif

  return m_metrics[0].GetTexture();
}

uint32_t TextLayout::GetGlyphCount() const
{
  return static_cast<uint32_t>(m_metrics.size());
}

float TextLayout::GetPixelLength() const
{
  return m_textSizeRatio * std::accumulate(m_metrics.begin(), m_metrics.end(), 0.0f,
                                           [](double const & v, GlyphRegion const & glyph) -> float
  {
    return static_cast<float>(v) + glyph.GetAdvanceX();
  });
}

float TextLayout::GetPixelHeight() const
{
  return m_textSizeRatio * dp::kBaseFontSizePixels;
}

strings::UniString const & TextLayout::GetText() const
{
  return m_text;
}

StraightTextLayout::StraightTextLayout(strings::UniString const & text, float fontSize,
                                       ref_ptr<dp::TextureManager> textures, dp::Anchor anchor, bool forceNoWrap)
{
  strings::UniString visibleText = bidi::log2vis(text);
  // Possible if name has strange symbols only.
  if (visibleText.empty())
    return;

  buffer_vector<size_t, 2> delimIndexes;
  if (visibleText == text && !forceNoWrap)
    SplitText(visibleText, delimIndexes);
  else
    delimIndexes.push_back(visibleText.size());

  TBase::Init(std::move(visibleText), fontSize, textures);
  CalculateOffsets(anchor, m_textSizeRatio, m_metrics, delimIndexes, m_offsets, m_pixelSize, m_rowsCount);
}

m2::PointF StraightTextLayout::GetSymbolBasedTextOffset(m2::PointF const & symbolSize, dp::Anchor textAnchor,
                                                        dp::Anchor symbolAnchor)
{
  m2::PointF offset(0.0f, 0.0f);

  float const halfSymbolW = symbolSize.x / 2.0f;
  float const halfSymbolH = symbolSize.y / 2.0f;

  auto const adjustOffset = [&](dp::Anchor anchor)
  {
    if (anchor & dp::Top)
      offset.y += halfSymbolH;
    else if (anchor & dp::Bottom)
      offset.y -= halfSymbolH;

    if (anchor & dp::Left)
      offset.x += halfSymbolW;
    else if (anchor & dp::Right)
      offset.x -= halfSymbolW;
  };

  adjustOffset(textAnchor);
  adjustOffset(symbolAnchor);

  return offset;
}

glsl::vec2 StraightTextLayout::GetTextOffset(m2::PointF const & symbolSize, dp::Anchor textAnchor,
                                             dp::Anchor symbolAnchor) const
{
  auto const symbolBasedOffset = GetSymbolBasedTextOffset(symbolSize, textAnchor, symbolAnchor);
  return m_baseOffset + glsl::ToVec2(symbolBasedOffset);
}

void StraightTextLayout::CacheStaticGeometry(dp::TextureManager::ColorRegion const & colorRegion,
                                             gpu::TTextStaticVertexBuffer & staticBuffer) const
{
  TextGeometryGenerator staticGenerator(colorRegion, staticBuffer);
  staticBuffer.reserve(4 * m_metrics.size());
  Cache(staticGenerator);
}

void StraightTextLayout::CacheStaticGeometry(dp::TextureManager::ColorRegion const & colorRegion,
                                             dp::TextureManager::ColorRegion const & outlineRegion,
                                             gpu::TTextOutlinedStaticVertexBuffer & staticBuffer) const
{
  TextOutlinedGeometryGenerator outlinedGenerator(colorRegion, outlineRegion, staticBuffer);
  staticBuffer.reserve(4 * m_metrics.size());
  Cache(outlinedGenerator);
}

void StraightTextLayout::SetBasePosition(glm::vec4 const & pivot, glm::vec2 const & baseOffset)
{
  m_pivot = pivot;
  m_baseOffset = baseOffset;
}

void StraightTextLayout::CacheDynamicGeometry(glsl::vec2 const & pixelOffset,
                                              gpu::TTextDynamicVertexBuffer & dynamicBuffer) const
{
  StraightTextGeometryGenerator generator(m_pivot, pixelOffset, m_textSizeRatio, dynamicBuffer);
  dynamicBuffer.reserve(4 * m_metrics.size());
  Cache(generator);
}

PathTextLayout::PathTextLayout(m2::PointD const & tileCenter, strings::UniString const & text,
                               float fontSize, ref_ptr<dp::TextureManager> textures)
  : m_tileCenter(tileCenter)
{
  Init(bidi::log2vis(text), fontSize, textures);
}

void PathTextLayout::CacheStaticGeometry(dp::TextureManager::ColorRegion const & colorRegion,
                                         dp::TextureManager::ColorRegion const & outlineRegion,
                                         gpu::TTextOutlinedStaticVertexBuffer & staticBuffer) const
{
  TextOutlinedGeometryGenerator gen(colorRegion, outlineRegion, staticBuffer);
  staticBuffer.reserve(4 * m_metrics.size());
  std::for_each(m_metrics.begin(), m_metrics.end(), gen);
}

void PathTextLayout::CacheStaticGeometry(dp::TextureManager::ColorRegion const & colorRegion,
                                         gpu::TTextStaticVertexBuffer & staticBuffer) const
{
  TextGeometryGenerator gen(colorRegion, staticBuffer);
  staticBuffer.reserve(4 * m_metrics.size());
  std::for_each(m_metrics.begin(), m_metrics.end(), gen);
}

bool PathTextLayout::CacheDynamicGeometry(m2::Spline::iterator const & iter, float depth,
                                          m2::PointD const & globalPivot,
                                          gpu::TTextDynamicVertexBuffer & buffer) const
{
  float const halfLength = 0.5f * GetPixelLength();

  m2::Spline::iterator beginIter = iter;
  beginIter.Advance(-halfLength);
  m2::Spline::iterator endIter = iter;
  endIter.Advance(halfLength);
  if (beginIter.BeginAgain() || endIter.BeginAgain())
    return false;

  float const halfFontSize = 0.5f * GetPixelHeight();
  float advanceSign = 1.0f;
  m2::Spline::iterator penIter = beginIter;
  if (beginIter.m_pos.x > endIter.m_pos.x)
  {
    advanceSign = -advanceSign;
    penIter = endIter;
  }

  m2::PointD const pxPivot = iter.m_pos;
  buffer.resize(4 * m_metrics.size());

  glsl::vec4 const pivot(glsl::ToVec2(MapShape::ConvertToLocal(globalPivot, m_tileCenter,
                                                               kShapeCoordScalar)), depth, 0.0f);
  static float const kEps = 1e-5f;
  for (size_t i = 0; i < m_metrics.size(); ++i)
  {
    GlyphRegion const & g = m_metrics[i];
    m2::PointF const pxSize = g.GetPixelSize() * m_textSizeRatio;
    float const xAdvance = g.GetAdvanceX() * m_textSizeRatio;

    m2::PointD const baseVector = penIter.m_pos - pxPivot;
    m2::PointD const currentTangent = penIter.m_avrDir.Normalize();

    if (fabs(xAdvance) > kEps)
      penIter.Advance(advanceSign * xAdvance);
    m2::PointD const newTangent = penIter.m_avrDir.Normalize();

    glsl::vec2 const tangent = glsl::ToVec2(newTangent);
    glsl::vec2 const normal = glsl::vec2(-tangent.y, tangent.x);
    glsl::vec2 const formingVector = glsl::ToVec2(baseVector) + halfFontSize * normal;

    float const xOffset = g.GetOffsetX() * m_textSizeRatio;
    float const yOffset = g.GetOffsetY() * m_textSizeRatio;

    float const upVector = - (pxSize.y + yOffset);
    float const bottomVector = - yOffset;

    size_t const baseIndex = 4 * i;

    buffer[baseIndex + 0] = {pivot, formingVector + normal * bottomVector + tangent * xOffset};
    buffer[baseIndex + 1] = {pivot, formingVector + normal * upVector + tangent * xOffset};
    buffer[baseIndex + 2] = {pivot, formingVector + normal * bottomVector + tangent * (pxSize.x + xOffset)};
    buffer[baseIndex + 3] = {pivot, formingVector + normal * upVector + tangent * (pxSize.x + xOffset)};

    if (i > 0)
    {
      auto const dotProduct = static_cast<float>(m2::DotProduct(currentTangent, newTangent));
      if (dotProduct < kValidSplineTurn)
        return false;
    }
  }
  return true;
}

double PathTextLayout::CalculateTextLength(double textPixelLength)
{
  // We leave a little space on each side of the text.
  double const kTextBorder = 4.0;
  return kTextBorder + textPixelLength;
}

void PathTextLayout::CalculatePositions(double splineLength, double splineScaleToPixel,
                                        double textPixelLength, std::vector<double> & offsets)
{
  double const textLength = CalculateTextLength(textPixelLength);

  // On the next scale m_scaleGtoP will be twice.
  if (textLength > splineLength * 2.0f * splineScaleToPixel)
    return;

  double const kPathLengthScalar = 0.75;
  double const pathLength = kPathLengthScalar * splineScaleToPixel * splineLength;
  double const minPeriodSize = GetTextMinPeriod(textLength);
  double const twoTextsAndEmpty = minPeriodSize + textLength;

  if (pathLength < twoTextsAndEmpty)
  {
    // If we can't place 2 texts and empty part along the path,
    // we place only one text at the center of the path.
    offsets.push_back(splineLength * 0.5f);
  }
  else
  {
    double const textCount = std::max(floor(pathLength / minPeriodSize), 1.0);
    double const glbTextLen = splineLength / textCount;
    for (double offset = 0.5 * glbTextLen; offset < splineLength; offset += glbTextLen)
      offsets.push_back(offset);
  }
}

}  // namespace df
