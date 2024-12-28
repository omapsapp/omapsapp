#include "drape_frontend/circles_pack_shape.hpp"
#include "drape_frontend/batcher_bucket.hpp"

#include "shaders/programs.hpp"

#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"
#include "drape/glsl_func.hpp"
#include "drape/glsl_types.hpp"
#include "drape/graphics_context.hpp"
#include "drape/texture_manager.hpp"

#include <memory>

namespace df
{
namespace
{
uint32_t const kDynamicStreamID = 0x7F;

struct CirclesPackStaticVertex
{
  using TNormal = glsl::vec3;

  CirclesPackStaticVertex() = default;
  explicit CirclesPackStaticVertex(TNormal const & normal) : m_normal(normal) {}

  TNormal m_normal;
};

dp::RenderState GetCirclesPackState(ref_ptr<dp::TextureManager> texMng)
{
  auto state = CreateRenderState(gpu::Program::CirclePoint, DepthLayer::OverlayLayer);
  state.SetColorTexture(texMng->GetSymbolsTexture());
  state.SetDepthTestEnabled(false);
  return state;
}

dp::BindingInfo const & GetCirclesPackStaticBindingInfo()
{
  static std::unique_ptr<dp::BindingInfo> s_info;
  if (s_info == nullptr)
  {
    dp::BindingFiller<CirclesPackStaticVertex> filler(1);
    filler.FillDecl<CirclesPackStaticVertex::TNormal>("a_normal");
    s_info.reset(new dp::BindingInfo(filler.m_info));
  }
  return *s_info;
}

dp::BindingInfo const & GetCirclesPackDynamicBindingInfo()
{
  static std::unique_ptr<dp::BindingInfo> s_info;
  if (s_info == nullptr)
  {
    dp::BindingFiller<CirclesPackDynamicVertex> filler(2, kDynamicStreamID);
    filler.FillDecl<CirclesPackDynamicVertex::TPosition>("a_position");
    filler.FillDecl<CirclesPackDynamicVertex::TColor>("a_color");
    s_info.reset(new dp::BindingInfo(filler.m_info));
  }
  return *s_info;
}
}  // namespace

CirclesPackHandle::CirclesPackHandle(size_t pointsCount)
  : OverlayHandle(dp::OverlayID{}, dp::Anchor::Center, 0 /* priority */, 1 /* minVisibleScale */,
                  false /* isBillboard */)
  , m_needUpdate(false)
{
  m_buffer.resize(pointsCount * dp::Batcher::VertexPerQuad);
}

void CirclesPackHandle::GetAttributeMutation(ref_ptr<dp::AttributeBufferMutator> mutator) const
{
  if (!m_needUpdate)
    return;

  TOffsetNode const & node = GetOffsetNode(kDynamicStreamID);
  ASSERT_EQUAL(node.first.GetElementSize(), sizeof(CirclesPackDynamicVertex), ());
  ASSERT_EQUAL(node.second.m_count, m_buffer.size(), ());

  uint32_t const bytesCount =
      static_cast<uint32_t>(m_buffer.size()) * sizeof(CirclesPackDynamicVertex);
  void * buffer = mutator->AllocateMutationBuffer(bytesCount);
  memcpy(buffer, m_buffer.data(), bytesCount);

  dp::MutateNode mutateNode;
  mutateNode.m_region = node.second;
  mutateNode.m_data = make_ref(buffer);
  mutator->AddMutation(node.first, mutateNode);

  m_needUpdate = false;
}

bool CirclesPackHandle::Update(ScreenBase const & screen)
{
  UNUSED_VALUE(screen);
  return true;
}

bool CirclesPackHandle::IndexesRequired() const { return false; }

m2::RectD CirclesPackHandle::GetPixelRect(ScreenBase const & screen, bool perspective) const
{
  UNUSED_VALUE(screen);
  UNUSED_VALUE(perspective);
  return m2::RectD();
}

void CirclesPackHandle::GetPixelShape(ScreenBase const & screen, bool perspective,
                                      Rects & rects) const
{
  UNUSED_VALUE(screen);
  UNUSED_VALUE(perspective);
}

void CirclesPackHandle::SetPoint(size_t index, m2::PointD const & position, float radius,
                                 dp::Color const & color)
{
  size_t const bufferIndex = index * dp::Batcher::VertexPerQuad;
  ASSERT_LESS_OR_EQUAL(bufferIndex + dp::Batcher::VertexPerQuad, m_buffer.size(), ());

  for (size_t i = 0; i < dp::Batcher::VertexPerQuad; ++i)
  {
    m_buffer[bufferIndex + i].m_position = glsl::vec3(position.x, position.y, radius);
    m_buffer[bufferIndex + i].m_color = glsl::ToVec4(color);
  }
  m_needUpdate = true;
}

void CirclesPackHandle::Clear()
{
  fill(begin(m_buffer), end(m_buffer), CirclesPackDynamicVertex(glsl::vec3(0.0f), glsl::vec4(0.0f)));
  m_needUpdate = true;
}

size_t CirclesPackHandle::GetPointsCount() const
{
  return m_buffer.size() / dp::Batcher::VertexPerQuad;
}

void CirclesPackShape::Draw(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::TextureManager> texMng,
                            CirclesPackRenderData & data)
{
  ASSERT_NOT_EQUAL(data.m_pointsCount, 0, ());

  uint32_t constexpr kVerticesInPoint = dp::Batcher::VertexPerQuad;
  uint32_t constexpr kIndicesInPoint = dp::Batcher::IndexPerQuad;

  std::vector<CirclesPackStaticVertex> staticVertexData;
  staticVertexData.reserve(data.m_pointsCount * kVerticesInPoint);
  static_assert(kVerticesInPoint == 4, "According to the loop below");
  for (size_t i = 0; i < data.m_pointsCount; ++i)
  {
    staticVertexData.emplace_back(CirclesPackStaticVertex::TNormal(-1.0f, 1.0f, 1.0f));
    staticVertexData.emplace_back(CirclesPackStaticVertex::TNormal(-1.0f, -1.0f, 1.0f));
    staticVertexData.emplace_back(CirclesPackStaticVertex::TNormal(1.0f, 1.0f, 1.0f));
    staticVertexData.emplace_back(CirclesPackStaticVertex::TNormal(1.0f, -1.0f, 1.0f));
  }

  std::vector<CirclesPackDynamicVertex> dynamicVertexData;
  dynamicVertexData.resize(data.m_pointsCount * kVerticesInPoint);

  dp::Batcher batcher(data.m_pointsCount * kIndicesInPoint, data.m_pointsCount * kVerticesInPoint);
  batcher.SetBatcherHash(static_cast<uint64_t>(BatcherBucket::Overlay));
  dp::SessionGuard guard(context, batcher,
                         [&data](dp::RenderState const & state, drape_ptr<dp::RenderBucket> && b)
  {
    data.m_bucket = std::move(b);
    data.m_state = state;
  });

  drape_ptr<dp::OverlayHandle> handle = make_unique_dp<CirclesPackHandle>(data.m_pointsCount);

  dp::AttributeProvider provider(2 /* stream count */,
                                 static_cast<uint32_t>(staticVertexData.size()));
  provider.InitStream(0 /* stream index */, GetCirclesPackStaticBindingInfo(),
                      make_ref(staticVertexData.data()));
  provider.InitStream(1 /* stream index */, GetCirclesPackDynamicBindingInfo(),
                      make_ref(dynamicVertexData.data()));
  batcher.InsertListOfStrip(context, GetCirclesPackState(texMng), make_ref(&provider),
                            std::move(handle), kVerticesInPoint);

  context->Flush();
}
}  // namespace df
