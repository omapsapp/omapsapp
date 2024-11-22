#include "drape_frontend/gui/compass.hpp"

#include "drape_frontend/animation/show_hide_animation.hpp"
#include "drape_frontend/batcher_bucket.hpp"
#include "drape_frontend/gui/drape_gui.hpp"

#include "shaders/programs.hpp"

#include "drape/glsl_func.hpp"
#include "drape/glsl_types.hpp"

#include "drape/utils/vertex_decl.hpp"

#include <functional>
#include <utility>

using namespace std::placeholders;

namespace gui
{
namespace
{
struct CompassVertex
{
  CompassVertex(glsl::vec2 const & position, glsl::vec2 const & texCoord)
    : m_position(position)
    , m_texCoord(texCoord)
  {}

  glsl::vec2 m_position;
  glsl::vec2 m_texCoord;
};

class CompassHandle : public TappableHandle
{
  using TBase = TappableHandle;

public:
  CompassHandle(uint32_t id, m2::PointF const & pivot, m2::PointF const & size,
                Shape::TTapHandler const & tapHandler)
    : TappableHandle(id, dp::Center, pivot, size)
    , m_tapHandler(tapHandler)
    , m_animation(false, 0.25)
  {}

  void OnTap() override
  {
    if (m_tapHandler != nullptr)
      m_tapHandler();
  }

  bool Update(ScreenBase const & screen) override
  {
    static double const kVisibleStartAngle = math::DegToRad(5.0);
    static double const kVisibleEndAngle = math::DegToRad(355.0);

    auto const angle = static_cast<float>(ang::AngleIn2PI(screen.GetAngle()));

    bool isVisiblePrev = IsVisible();
    bool isVisibleAngle = angle > kVisibleStartAngle && angle < kVisibleEndAngle;

    bool isVisible = isVisibleAngle || (isVisiblePrev && DrapeGui::Instance().IsInUserAction());

    if (isVisible)
    {
      m_animation.ShowAnimated();
      SetIsVisible(true);
    }
    else
      m_animation.HideAnimated();

    if (IsVisible())
    {
      TBase::Update(screen);

      glsl::mat4 r = glsl::rotate(glsl::mat4(), angle, glsl::vec3(0.0, 0.0, 1.0));
      glsl::mat4 m = glsl::translate(glsl::mat4(), glsl::vec3(m_pivot, 0.0));
      m_params.m_modelView = glsl::transpose(m * r);
      m_params.m_opacity = static_cast<float>(m_animation.GetT());
    }

    if (m_animation.IsFinished())
      SetIsVisible(isVisible);

    return true;
  }

private:
  Shape::TTapHandler m_tapHandler;
  df::ShowHideAnimation m_animation;
};
}  // namespace

drape_ptr<ShapeRenderer> Compass::Draw(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::TextureManager> tex,
                                       TTapHandler const & tapHandler) const
{
  dp::TextureManager::SymbolRegion region;
  tex->GetSymbolRegion("compass-image", region);
  auto const halfSize = glsl::ToVec2(region.GetPixelSize() * 0.5f);
  auto const texRect = region.GetTexRect();

  ASSERT_EQUAL(m_position.m_anchor, dp::Center, ());
  CompassVertex vertexes[] =
  {
    CompassVertex(glsl::vec2(-halfSize.x, halfSize.y), glsl::ToVec2(texRect.LeftTop())),
    CompassVertex(glsl::vec2(-halfSize.x, -halfSize.y), glsl::ToVec2(texRect.LeftBottom())),
    CompassVertex(glsl::vec2(halfSize.x, halfSize.y), glsl::ToVec2(texRect.RightTop())),
    CompassVertex(glsl::vec2(halfSize.x, -halfSize.y), glsl::ToVec2(texRect.RightBottom()))
  };

  auto state = df::CreateRenderState(gpu::Program::TexturingGui, df::DepthLayer::GuiLayer);
  state.SetColorTexture(region.GetTexture());
  state.SetDepthTestEnabled(false);
  state.SetTextureIndex(region.GetTextureIndex());

  dp::AttributeProvider provider(1, 4);
  dp::BindingInfo info(2);

  dp::BindingDecl & posDecl = info.GetBindingDecl(0);
  posDecl.m_attributeName = "a_position";
  posDecl.m_componentCount = 2;
  posDecl.m_componentType = gl_const::GLFloatType;
  posDecl.m_offset = 0;
  posDecl.m_stride = sizeof(CompassVertex);

  dp::BindingDecl & texDecl = info.GetBindingDecl(1);
  texDecl.m_attributeName = "a_colorTexCoords";
  texDecl.m_componentCount = 2;
  texDecl.m_componentType = gl_const::GLFloatType;
  texDecl.m_offset = sizeof(glsl::vec2);
  texDecl.m_stride = posDecl.m_stride;

  provider.InitStream(0, info, make_ref(&vertexes));

  drape_ptr<dp::OverlayHandle> handle = make_unique_dp<CompassHandle>(EGuiHandle::GuiHandleCompass,
                                                                      m_position.m_pixelPivot,
                                                                      region.GetPixelSize(), tapHandler);

  drape_ptr<ShapeRenderer> renderer = make_unique_dp<ShapeRenderer>();
  dp::Batcher batcher(dp::Batcher::IndexPerQuad, dp::Batcher::VertexPerQuad);
  batcher.SetBatcherHash(static_cast<uint64_t>(df::BatcherBucket::Default));
  dp::SessionGuard guard(context, batcher, std::bind(&ShapeRenderer::AddShape, renderer.get(), _1, _2));
  batcher.InsertTriangleStrip(context, state, make_ref(&provider), std::move(handle));

  return renderer;
}
}  // namespace gui
