#include "circle.hpp"

namespace
{
struct CircleVertex
{
  CircleVertex() = default;

  CircleVertex(glsl::vec2 position, glsl::vec3 color, glsl::vec3 outlineColor, float radius, float outlineWidthRatio)
    : m_position(position)
    , m_color(color)
    , m_outlineColor(outlineColor)
    , m_radius(radius)
    , m_outlineWidthRatio(outlineWidthRatio)
  {
  }

  static dp::BindingInfo GetBindingInfo()
  {
    dp::BindingFiller<CircleVertex> filler(5);
    filler.FillDecl<glsl::vec2>("a_position");
    filler.FillDecl<glsl::vec3>("a_color");
    filler.FillDecl<glsl::vec3>("a_outlineColor");
    filler.FillDecl<float>("a_radius");
    filler.FillDecl<float>("a_outlineWidthRatio");
    return filler.m_info;
  }

  glsl::vec2 m_position{};
  glsl::vec3 m_color{};
  glsl::vec3 m_outlineColor{};
  float m_radius{};
  float m_outlineWidthRatio{};
};

using CircleVertexData = buffer_vector<CircleVertex, dp::Batcher::VertexPerQuad>;

CircleVertexData createCircleVertexData(glsl::vec3 color, glsl::vec3 outlineColor, float radius,
                                        float outlineWidthRatio)
{
  CircleVertexData data;
  data.emplace_back(glsl::vec2(-1.0, 1.0), color, outlineColor, radius, outlineWidthRatio);
  data.emplace_back(glsl::vec2(-1.0, -1.0), color, outlineColor, radius, outlineWidthRatio);
  data.emplace_back(glsl::vec2(1.0, 1.0), color, outlineColor, radius, outlineWidthRatio);
  data.emplace_back(glsl::vec2(1.0, -1.0), color, outlineColor, radius, outlineWidthRatio);
  return data;
}
}  // namespace

namespace gui::elements
{
CircleHandle::CircleHandle(uint32_t id, dp::Anchor anchor, const m2::PointF & pivot, const m2::PointF & size)
  : Handle(id, anchor, pivot, size)
{
  SetIsVisible(true);
}

void Circle::SetHandleId(uint32_t handleId) { m_handleId = handleId; }

void Circle::SetPosition(const Position & position) { m_position = position; }

void Circle::SetRadius(float radius) { m_radius = radius; }

void Circle::SetOutlineWidthRatio(float widthRatio) { m_outlineWidthRatio = widthRatio; }

void Circle::SetColor(dp::Color const & color) { m_color = color; }

void Circle::SetOutlineColor(dp::Color const & color) { m_outlineColor = color; }

void Circle::SetHandleCreator(HandleCreator handleCreator)
{
  m_handleCreator = std::move(handleCreator);
}

void Circle::Draw(ref_ptr<dp::GraphicsContext> context, ShapeControl & control) const
{
  Validate();

  float const radiusInPixels = m_radius * df::VisualParams::Instance().GetVisualScale();
  CircleVertexData data =
      createCircleVertexData(glsl::ToVec3(m_color), glsl::ToVec3(m_outlineColor), radiusInPixels, m_outlineWidthRatio);

  auto state = df::CreateRenderState(gpu::Program::GuiCircle, df::DepthLayer::GuiLayer);
  state.SetDepthTestEnabled(false);

  dp::AttributeProvider provider(1, 4);
  provider.InitStream(0, CircleVertex::GetBindingInfo(), make_ref(data.data()));
  drape_ptr<dp::OverlayHandle> handle = m_handleCreator(
      m_handleId, m_position.m_anchor, m_position.m_pixelPivot, m2::PointF{radiusInPixels * 2, radiusInPixels * 2});

  dp::Batcher batcher(dp::Batcher::IndexPerQuad, dp::Batcher::VertexPerQuad);
  batcher.SetBatcherHash(static_cast<uint64_t>(df::BatcherBucket::Default));
  dp::SessionGuard guard(context, batcher, std::bind(&ShapeControl::AddShape, &control, _1, _2));
  batcher.InsertTriangleStrip(context, state, make_ref(&provider), std::move(handle));
}

void Circle::Validate() const
{
  ASSERT_NOT_EQUAL(m_handleId, 0, ("Handle id must be set."));
  ASSERT_EQUAL(m_position.m_anchor, dp::Center, ("Only dp::Center is supported for Circle."));
  ASSERT_NOT_EQUAL(m_radius, 0.0f, ("Radius must be set."));
  ASSERT_GREATER_OR_EQUAL(m_outlineWidthRatio, 0.0f, ("Outline width ratio must be in the range [0.0, 1.0]."));
  ASSERT_LESS_OR_EQUAL(m_outlineWidthRatio, 1.0f, ("Outline width ratio must be in the range [0.0, 1.0]."));
  ASSERT_NOT_EQUAL(m_color, dp::Color::Transparent(), ("Color must be set."));
  ASSERT(m_handleCreator, ("HandleCreator must be set."));
}
}  // namespace gui::elements
