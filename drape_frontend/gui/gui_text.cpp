#include "drape_frontend/gui/gui_text.hpp"

#include "drape_frontend/batcher_bucket.hpp"
#include "drape_frontend/visual_params.hpp"

#include "shaders/programs.hpp"

#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include "drape/bidi.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <type_traits>

namespace gui
{
namespace
{
glsl::vec2 GetNormalsAndMask(dp::TextureManager::GlyphRegion const & glyph, float textRatio,
                             std::array<glsl::vec2, 4> & normals, std::array<glsl::vec2, 4> & maskTexCoord)
{
  m2::PointF const pixelSize = glyph.GetPixelSize() * textRatio;
  m2::RectF const & r = glyph.GetTexRect();

  float const xOffset = glyph.GetOffsetX() * textRatio;
  float const yOffset = glyph.GetOffsetY() * textRatio;

  float const upVector = -static_cast<int32_t>(pixelSize.y) - yOffset;
  float const bottomVector = -yOffset;

  normals[0] = glsl::vec2(xOffset, bottomVector);
  normals[1] = glsl::vec2(xOffset, upVector);
  normals[2] = glsl::vec2(pixelSize.x + xOffset, bottomVector);
  normals[3] = glsl::vec2(pixelSize.x + xOffset, upVector);

  maskTexCoord[0] = glsl::ToVec2(r.LeftTop());
  maskTexCoord[1] = glsl::ToVec2(r.LeftBottom());
  maskTexCoord[2] = glsl::ToVec2(r.RightTop());
  maskTexCoord[3] = glsl::ToVec2(r.RightBottom());

  return glsl::vec2(xOffset, yOffset);
}

void FillCommonDecl(dp::BindingDecl & decl, std::string const & name, uint8_t compCount,
                    uint8_t stride, uint8_t offset)
{
  decl.m_attributeName = name;
  decl.m_componentCount = compCount;
  decl.m_componentType = gl_const::GLFloatType;
  decl.m_stride = stride;
  decl.m_offset = offset;
}

void FillPositionDecl(dp::BindingDecl & decl, uint8_t stride, uint8_t offset)
{
  FillCommonDecl(decl, "a_position", 3, stride, offset);
}

void FillNormalDecl(dp::BindingDecl & decl, uint8_t stride, uint8_t offset)
{
  FillCommonDecl(decl, "a_normal", 2, stride, offset);
}

void FillColorDecl(dp::BindingDecl & decl, uint8_t stride, uint8_t offset)
{
  FillCommonDecl(decl, "a_colorTexCoord", 2, stride, offset);
}

void FillOutlineDecl(dp::BindingDecl & decl, uint8_t stride, uint8_t offset)
{
  FillCommonDecl(decl, "a_outlineColorTexCoord", 2, stride, offset);
}

void FillMaskDecl(dp::BindingDecl & decl, uint8_t stride, uint8_t offset)
{
  FillCommonDecl(decl, "a_maskTexCoord", 2, stride, offset);
}
}  // namespace

dp::BindingInfo const & StaticLabel::Vertex::GetBindingInfo()
{
  static std::unique_ptr<dp::BindingInfo> info;

  if (info == nullptr)
  {
    info.reset(new dp::BindingInfo(5));
    uint8_t stride = sizeof(Vertex);
    uint8_t offset = 0;

    FillPositionDecl(info->GetBindingDecl(0), stride, offset);
    offset += sizeof(glsl::vec3);
    FillColorDecl(info->GetBindingDecl(1), stride, offset);
    offset += sizeof(glsl::vec2);
    FillOutlineDecl(info->GetBindingDecl(2), stride, offset);
    offset += sizeof(glsl::vec2);
    FillNormalDecl(info->GetBindingDecl(3), stride, offset);
    offset += sizeof(glsl::vec2);
    FillMaskDecl(info->GetBindingDecl(4), stride, offset);
    ASSERT_EQUAL(offset + sizeof(glsl::vec2), stride, ());
  }

  return *info.get();
}

StaticLabel::LabelResult::LabelResult()
  : m_state(df::CreateRenderState(gpu::Program::TextStaticOutlinedGui, df::DepthLayer::GuiLayer))
{
  m_state.SetDepthTestEnabled(false);
}

char const * StaticLabel::DefaultDelim = "\n";

void StaticLabel::CacheStaticText(std::string const & text, char const * delim,
                                  dp::Anchor anchor, dp::FontDecl const & font,
                                  ref_ptr<dp::TextureManager> mng, LabelResult & result)
{
  ASSERT(!text.empty(), ());

  dp::TextureManager::TMultilineText textParts;
  strings::Tokenize(text, delim, [&textParts](std::string_view part)
  {
    textParts.push_back(bidi::log2vis(part));
  });

  ASSERT(!textParts.empty(), ());

  for (auto const & str : textParts)
    result.m_alphabet.insert(str.begin(), str.end());

  dp::TextureManager::TMultilineGlyphsBuffer buffers;
  mng->GetGlyphRegions(textParts, dp::GlyphManager::kDynamicGlyphSize, buffers);

#ifdef DEBUG
  ASSERT_EQUAL(textParts.size(), buffers.size(), ());
  for (size_t i = 0; i < textParts.size(); ++i)
  {
    ASSERT(!textParts[i].empty(), ());
    ASSERT_EQUAL(textParts[i].size(), buffers[i].size(), ());
  }

  ref_ptr<dp::Texture> texture = buffers[0][0].GetTexture();
  for (dp::TextureManager::TGlyphsBuffer const & b : buffers)
  {
    for (dp::TextureManager::GlyphRegion const & reg : b)
      ASSERT(texture == reg.GetTexture(), ());
  }
#endif

  dp::TextureManager::ColorRegion color;
  dp::TextureManager::ColorRegion outline;
  mng->GetColorRegion(font.m_color, color);
  mng->GetColorRegion(font.m_outlineColor, outline);
  ASSERT(color.GetTexture() == outline.GetTexture(), ());

  glsl::vec2 colorTex = glsl::ToVec2(color.GetTexRect().Center());
  glsl::vec2 outlineTex = glsl::ToVec2(outline.GetTexRect().Center());

  df::VisualParams const & vparams = df::VisualParams::Instance();
  auto const textRatio = font.m_size * static_cast<float>(vparams.GetVisualScale()) /
                         vparams.GetGlyphBaseSize();

  buffer_vector<float, 4> lineLengths;
  lineLengths.reserve(buffers.size());

  buffer_vector<size_t, 4> ranges;
  ranges.reserve(buffers.size());

  float fullHeight = 0.0;
  float prevLineHeight = 0.0;

  buffer_vector<Vertex, 128> & rb = result.m_buffer;
  for (int i = static_cast<int>(buffers.size()) - 1; i >= 0; --i)
  {
    dp::TextureManager::TGlyphsBuffer & regions = buffers[i];
    lineLengths.push_back(0.0f);
    float & currentLineLength = lineLengths.back();

    float depth = 0.0;
    glsl::vec2 pen(0.0, -fullHeight);
    prevLineHeight = 0.0;
    for (size_t j = 0; j < regions.size(); ++j)
    {
      std::array<glsl::vec2, 4> normals, maskTex;

      dp::TextureManager::GlyphRegion const & glyph = regions[j];
      glsl::vec2 offsets = GetNormalsAndMask(glyph, textRatio, normals, maskTex);

      glsl::vec3 position = glsl::vec3(0.0, 0.0, depth);

      for (size_t v = 0; v < normals.size(); ++v)
        rb.push_back(Vertex(position, colorTex, outlineTex, pen + normals[v], maskTex[v]));

      float const advance = glyph.GetAdvanceX() * textRatio;
      prevLineHeight = std::max(prevLineHeight, offsets.y + glyph.GetPixelHeight() * textRatio);
      pen += glsl::vec2(advance, glyph.GetAdvanceY() * textRatio);

      depth += 10.0f;
      if (j == 0)
        currentLineLength += (glyph.GetPixelSize().x * textRatio + offsets.x);
      else
        currentLineLength += advance;

      if (j == regions.size() - 1)
        currentLineLength += offsets.x;
    }

    ranges.push_back(rb.size());

    fullHeight += prevLineHeight;
  }

  float const halfHeight = 0.5f * fullHeight;

  float yOffset = halfHeight;
  if (anchor & dp::Top)
    yOffset = fullHeight;
  else if (anchor & dp::Bottom)
    yOffset = 0.0f;

  float maxLineLength = 0.0;
  size_t startIndex = 0;
  for (size_t i = 0; i < ranges.size(); ++i)
  {
    maxLineLength = std::max(lineLengths[i], maxLineLength);
    float xOffset = -lineLengths[i] / 2.0f;
    if (anchor & dp::Left)
      xOffset = 0;
    else if (anchor & dp::Right)
      xOffset += xOffset;

    size_t endIndex = ranges[i];
    for (size_t i = startIndex; i < endIndex; ++i)
    {
      rb[i].m_normal = rb[i].m_normal + glsl::vec2(xOffset, yOffset);
      result.m_boundRect.Add(glsl::ToPoint(rb[i].m_normal));
    }

    startIndex = endIndex;
  }

  result.m_state.SetColorTexture(color.GetTexture());
  result.m_state.SetMaskTexture(buffers[0][0].GetTexture());
}

dp::BindingInfo const & MutableLabel::StaticVertex::GetBindingInfo()
{
  static std::unique_ptr<dp::BindingInfo> info;

  if (info == nullptr)
  {
    info.reset(new dp::BindingInfo(3));

    uint8_t stride = sizeof(StaticVertex);
    uint8_t offset = 0;

    FillPositionDecl(info->GetBindingDecl(0), stride, offset);
    offset += sizeof(glsl::vec3);
    FillColorDecl(info->GetBindingDecl(1), stride, offset);
    offset += sizeof(glsl::vec2);
    FillOutlineDecl(info->GetBindingDecl(2), stride, offset);
    ASSERT_EQUAL(offset + sizeof(glsl::vec2), stride, ());
  }

  return *info.get();
}

dp::BindingInfo const & MutableLabel::DynamicVertex::GetBindingInfo()
{
  static std::unique_ptr<dp::BindingInfo> info;

  if (info == nullptr)
  {
    info.reset(new dp::BindingInfo(2, 1));
    uint8_t stride = sizeof(DynamicVertex);
    uint8_t offset = 0;

    FillNormalDecl(info->GetBindingDecl(0), stride, offset);
    offset += sizeof(glsl::vec2);
    FillMaskDecl(info->GetBindingDecl(1), stride, offset);
    ASSERT_EQUAL(offset + sizeof(glsl::vec2), stride, ());
  }

  return *info.get();
}

MutableLabel::PrecacheResult::PrecacheResult()
  : m_state(df::CreateRenderState(gpu::Program::TextOutlinedGui, df::DepthLayer::GuiLayer))
{
  m_state.SetDepthTestEnabled(false);
}

MutableLabel::MutableLabel(dp::Anchor anchor)
  : m_anchor(anchor)
{
}

void MutableLabel::SetMaxLength(uint16_t maxLength)
{
  m_maxLength = maxLength;
}

ref_ptr<dp::Texture> MutableLabel::SetAlphabet(std::string const & alphabet,
                                               ref_ptr<dp::TextureManager> mng)
{
  strings::UniString str = strings::MakeUniString(alphabet + ".");
  base::SortUnique(str);

  dp::TextureManager::TGlyphsBuffer buffer;
  mng->GetGlyphRegions(str, dp::GlyphManager::kDynamicGlyphSize, buffer);
  m_alphabet.reserve(buffer.size());

  ASSERT_EQUAL(str.size(), buffer.size(), ());
  m_alphabet.resize(str.size());
  std::transform(str.begin(), str.end(), buffer.begin(), m_alphabet.begin(),
                 [](strings::UniChar const & c, dp::TextureManager::GlyphRegion const & r)
  {
    return std::make_pair(c, r);
  });

  std::sort(m_alphabet.begin(), m_alphabet.end(),
            [](TAlphabetNode const & n1, TAlphabetNode const & n2)
  {
    return n1.first < n2.first;
  });

  return m_alphabet[0].second.GetTexture();
}

void MutableLabel::Precache(PrecacheParams const & params, PrecacheResult & result,
                            ref_ptr<dp::TextureManager> mng)
{
  SetMaxLength(static_cast<uint16_t>(params.m_maxLength));
  result.m_state.SetMaskTexture(SetAlphabet(params.m_alphabet, mng));
  df::VisualParams const & vparams = df::VisualParams::Instance();
  m_textRatio = params.m_font.m_size * static_cast<float>(vparams.GetVisualScale()) /
                vparams.GetGlyphBaseSize();

  dp::TextureManager::ColorRegion color;
  dp::TextureManager::ColorRegion outlineColor;

  mng->GetColorRegion(params.m_font.m_color, color);
  mng->GetColorRegion(params.m_font.m_outlineColor, outlineColor);
  result.m_state.SetColorTexture(color.GetTexture());

  glsl::vec2 colorTex = glsl::ToVec2(color.GetTexRect().Center());
  glsl::vec2 outlineTex = glsl::ToVec2(outlineColor.GetTexRect().Center());

  uint32_t maxGlyphWidth = 0;
  uint32_t maxGlyphHeight = 0;
  for (const auto & node : m_alphabet)
  {
    dp::TextureManager::GlyphRegion const & reg = node.second;
    m2::PointU pixelSize(reg.GetPixelSize());
    maxGlyphWidth = std::max(maxGlyphWidth, pixelSize.x);
    maxGlyphHeight = std::max(maxGlyphHeight, pixelSize.y);
  }

  float offsetWidth = 0;
  if (params.m_anchor == dp::Center)
  {
    offsetWidth -= maxGlyphWidth / (1.5f * vparams.GetVisualScale());
  }

  auto const vertexCount = static_cast<size_t>(m_maxLength) * 4;
  result.m_buffer.resize(vertexCount,
                         StaticVertex(glsl::vec3(offsetWidth, 0.0, 0.0), colorTex, outlineTex));

  float depth = 0.0f;
  for (size_t i = 0; i < vertexCount; i += 4)
  {
    result.m_buffer[i + 0].m_position.z = depth;
    result.m_buffer[i + 1].m_position.z = depth;
    result.m_buffer[i + 2].m_position.z = depth;
    result.m_buffer[i + 3].m_position.z = depth;
    depth += 10.0f;
  }

  result.m_maxPixelSize = m2::PointF(m_maxLength * maxGlyphWidth, maxGlyphHeight);
}

void MutableLabel::SetText(LabelResult & result, std::string text) const
{
  if (text.size() > m_maxLength)
    text = text.erase(static_cast<size_t>(m_maxLength - 3)) + "...";

  strings::UniString uniText = bidi::log2vis(text);

  float maxHeight = 0.0f;
  float length = 0.0f;
  glsl::vec2 pen = glsl::vec2(0.0, 0.0);

  for (size_t i = 0; i < uniText.size(); ++i)
  {
    strings::UniChar c = uniText[i];
    auto const it = std::find_if(m_alphabet.begin(), m_alphabet.end(),
                                 [&c](TAlphabetNode const & n)
    {
      return n.first == c;
    });

    ASSERT(it != m_alphabet.end(), ());
    if (it != m_alphabet.end())
    {
      std::array<glsl::vec2, 4> normals, maskTex;
      dp::TextureManager::GlyphRegion const & glyph = it->second;
      glsl::vec2 offsets = GetNormalsAndMask(glyph, m_textRatio, normals, maskTex);

      ASSERT_EQUAL(normals.size(), maskTex.size(), ());

      for (size_t i = 0; i < normals.size(); ++i)
        result.m_buffer.push_back(DynamicVertex(pen + normals[i], maskTex[i]));

      float const advance = glyph.GetAdvanceX() * m_textRatio;
      length += advance + offsets.x;
      pen += glsl::vec2(advance, glyph.GetAdvanceY() * m_textRatio);
      maxHeight = std::max(maxHeight, offsets.y + glyph.GetPixelHeight() * m_textRatio);
    }
  }

  glsl::vec2 anchorModifyer = glsl::vec2(-length / 2.0f, maxHeight / 2.0f);
  if (m_anchor & dp::Right)
    anchorModifyer.x = -length;
  else if (m_anchor & dp::Left)
    anchorModifyer.x = 0;

  if (m_anchor & dp::Top)
    anchorModifyer.y = maxHeight;
  else if (m_anchor & dp::Bottom)
    anchorModifyer.y = 0;

  for (DynamicVertex & v : result.m_buffer)
  {
    v.m_normal += anchorModifyer;
    result.m_boundRect.Add(m2::PointD(glsl::ToPoint(v.m_normal)));
  }

  for (size_t i = result.m_buffer.size(); i < 4 * m_maxLength; ++i)
    result.m_buffer.push_back(DynamicVertex(glsl::vec2(0.0, 0.0), glsl::vec2(0.0, 0.0)));
}

m2::PointF MutableLabel::GetAverageSize() const
{
  float h = 0, w = 0;
  for (auto const & node : m_alphabet)
  {
    dp::TextureManager::GlyphRegion const & reg = node.second;
    m2::PointF size = reg.GetPixelSize() * m_textRatio;
    w += size.x;
    h = std::max(h, size.y);
  }

  w /= m_alphabet.size();

  return m2::PointF(w, h);
}

MutableLabelHandle::MutableLabelHandle(uint32_t id, dp::Anchor anchor, m2::PointF const & pivot)
  : TBase(id, anchor, pivot, m2::PointF::Zero())
  , m_textView(make_unique_dp<MutableLabel>(anchor))
  , m_isContentDirty(true)
  , m_glyphsReady(false)
{}

MutableLabelHandle::MutableLabelHandle(uint32_t id, dp::Anchor anchor, m2::PointF const & pivot,
                                       ref_ptr<dp::TextureManager> textures)
  : TBase(id, anchor, pivot, m2::PointF::Zero())
  , m_textView(make_unique_dp<MutableLabel>(anchor))
  , m_isContentDirty(true)
  , m_textureManager(textures)
  , m_glyphsReady(false)
{}

void MutableLabelHandle::GetAttributeMutation(ref_ptr<dp::AttributeBufferMutator> mutator) const
{
  if (!m_isContentDirty)
    return;

  m_isContentDirty = false;
  MutableLabel::LabelResult result;
  m_textView->SetText(result, m_content);
  m_size = m2::PointF(static_cast<float>(result.m_boundRect.SizeX()),
                      static_cast<float>(result.m_boundRect.SizeY()));

  uint32_t byteCount =
      static_cast<uint32_t>(result.m_buffer.size()) * sizeof(MutableLabel::DynamicVertex);

  auto dataPointer =
      reinterpret_cast<MutableLabel::DynamicVertex *>(mutator->AllocateMutationBuffer(byteCount));
  std::copy(result.m_buffer.begin(), result.m_buffer.end(), dataPointer);

  dp::BindingInfo const & binding = MutableLabel::DynamicVertex::GetBindingInfo();
  dp::OverlayHandle::TOffsetNode offsetNode = GetOffsetNode(binding.GetID());

  dp::MutateNode mutateNode;
  mutateNode.m_data = make_ref(dataPointer);
  mutateNode.m_region = offsetNode.second;
  mutator->AddMutation(offsetNode.first, mutateNode);
}

bool MutableLabelHandle::Update(ScreenBase const & screen)
{
  if (!m_glyphsReady)
  {
    strings::UniString alphabetStr;
    for (auto const & node : m_textView->GetAlphabet())
      alphabetStr.push_back(node.first);

    m_glyphsReady = m_textureManager->AreGlyphsReady(alphabetStr,
      dp::GlyphManager::kDynamicGlyphSize);
  }

  if (!m_glyphsReady)
    return false;

  return TBase::Update(screen);
}

void MutableLabelHandle::SetTextureManager(ref_ptr<dp::TextureManager> textures)
{
  m_textureManager = textures;
}

ref_ptr<MutableLabel> MutableLabelHandle::GetTextView()
{
  return make_ref(m_textView);
}

void MutableLabelHandle::UpdateSize(m2::PointF const & size)
{
  m_size = size;
}

void MutableLabelHandle::SetContent(std::string && content)
{
  if (m_content != content)
  {
    m_isContentDirty = true;
    m_content = std::move(content);
  }
}

void MutableLabelHandle::SetContent(std::string const & content)
{
  if (m_content != content)
  {
    m_isContentDirty = true;
    m_content = content;
  }
}

m2::PointF MutableLabelDrawer::Draw(ref_ptr<dp::GraphicsContext> context, Params const & params,
                                    ref_ptr<dp::TextureManager> mng,
                                    dp::Batcher::TFlushFn && flushFn)
{
  uint32_t vertexCount = dp::Batcher::VertexPerQuad * params.m_maxLength;
  uint32_t indexCount = dp::Batcher::IndexPerQuad * params.m_maxLength;

  ASSERT(params.m_handleCreator != nullptr, ());
  drape_ptr<MutableLabelHandle> handle = params.m_handleCreator(params.m_anchor, params.m_pivot);

  MutableLabel::PrecacheParams preCacheP;
  preCacheP.m_anchor = params.m_anchor;
  preCacheP.m_alphabet = params.m_alphabet;
  preCacheP.m_font = params.m_font;
  preCacheP.m_maxLength = params.m_maxLength;

  MutableLabel::PrecacheResult staticData;

  handle->GetTextView()->Precache(preCacheP, staticData, mng);
  handle->UpdateSize(handle->GetTextView()->GetAverageSize());

  ASSERT_EQUAL(vertexCount, staticData.m_buffer.size(), ());
  buffer_vector<MutableLabel::DynamicVertex, 128> dynData;
  dynData.resize(staticData.m_buffer.size());

  dp::BindingInfo const & sBinding = MutableLabel::StaticVertex::GetBindingInfo();
  dp::BindingInfo const & dBinding = MutableLabel::DynamicVertex::GetBindingInfo();
  dp::AttributeProvider provider(2 /*stream count*/, static_cast<uint32_t>(staticData.m_buffer.size()));
  provider.InitStream(0 /*stream index*/, sBinding,
                      make_ref(staticData.m_buffer.data()));
  provider.InitStream(1 /*stream index*/, dBinding, make_ref(dynData.data()));

  {
    dp::Batcher batcher(indexCount, vertexCount);
    batcher.SetBatcherHash(static_cast<uint64_t>(df::BatcherBucket::Default));
    dp::SessionGuard guard(context, batcher, std::move(flushFn));
    batcher.InsertListOfStrip(context, staticData.m_state, make_ref(&provider),
                              std::move(handle), dp::Batcher::VertexPerQuad);
  }

  return staticData.m_maxPixelSize;
}

StaticLabelHandle::StaticLabelHandle(uint32_t id, ref_ptr<dp::TextureManager> textureManager,
                                     dp::Anchor anchor, m2::PointF const & pivot,
                                     m2::PointF const & size,
                                     TAlphabet const & alphabet)
  : TBase(id, anchor, pivot, size)
  , m_alphabet(alphabet.begin(), alphabet.end())
  , m_textureManager(textureManager)
  , m_glyphsReady(false)
{}

bool StaticLabelHandle::Update(ScreenBase const & screen)
{
  if (!m_glyphsReady)
    m_glyphsReady = m_textureManager->AreGlyphsReady(m_alphabet, dp::GlyphManager::kDynamicGlyphSize);

  if (!m_glyphsReady)
    return false;

  return TBase::Update(screen);
}
}  // namespace gui
