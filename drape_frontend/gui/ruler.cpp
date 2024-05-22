#include "drape_frontend/gui/ruler.hpp"

#include "drape_frontend/animation/show_hide_animation.hpp"
#include "drape_frontend/batcher_bucket.hpp"
#include "drape_frontend/gui/drape_gui.hpp"
#include "drape_frontend/gui/gui_text.hpp"
#include "drape_frontend/gui/ruler_helper.hpp"

#include "shaders/programs.hpp"

#include "drape/glsl_types.hpp"

#include <functional>

using namespace std::placeholders;

namespace gui
{
namespace
{
struct RulerVertex
{
  RulerVertex() = default;
  RulerVertex(glsl::vec2 const & pos, glsl::vec2 const & normal, glsl::vec2 const & texCoord)
    : m_position(pos)
    , m_normal(normal)
    , m_texCoord(texCoord)
  {}

  glsl::vec2 m_position;
  glsl::vec2 m_normal;
  glsl::vec2 m_texCoord;
};

dp::BindingInfo GetBindingInfo()
{
  dp::BindingInfo info(3);
  uint8_t offset = 0;
  offset += dp::FillDecl<glsl::vec2, RulerVertex>(0, "a_position", info, offset);
  offset += dp::FillDecl<glsl::vec2, RulerVertex>(1, "a_normal", info, offset);
  /*offset += */dp::FillDecl<glsl::vec2, RulerVertex>(2, "a_colorTexCoords", info, offset);

  return info;
}

template<typename TBase>
class BaseRulerHandle : public TBase
{
public:
  BaseRulerHandle(uint32_t id, dp::Anchor anchor, m2::PointF const & pivot, bool isAppearing)
    : TBase(id, anchor, pivot)
    , m_isAppearing(isAppearing)
    , m_isVisibleAtEnd(true)
    , m_animation(false, 0.4)
  {}

  bool Update(ScreenBase const & screen)
  {
    RulerHelper & helper = DrapeGui::GetRulerHelper();

    TBase::SetIsVisible(true);
    bool isVisible = RulerHelper::IsVisible(screen);
    if (!isVisible)
    {
      m_animation.HideAnimated();
      m_isVisibleAtEnd = false;
    }
    else
    {
      if (helper.IsTextDirty())
      {
        m_isAppearing = !m_isAppearing;
        if (m_isAppearing)
        {
          m_animation.ShowAnimated();
          m_isVisibleAtEnd = true;
        }
        else
        {
          m_animation.HideAnimated();
          m_isVisibleAtEnd = false;
        }
      }
    }

    bool const result = TBase::Update(screen);
    UpdateImpl(screen, helper);

    if (m_animation.IsFinished())
      TBase::SetIsVisible(m_isVisibleAtEnd);

    return result;
  }

protected:
  virtual void UpdateImpl(ScreenBase const & /*screen*/, RulerHelper const & /*helper*/) {}

  bool IsAppearing() const { return m_isAppearing; }
  float GetOpacity() const { return static_cast<float>(m_animation.GetT()); }

private:
  bool m_isAppearing;
  bool m_isVisibleAtEnd;
  df::ShowHideAnimation m_animation;
};

class RulerHandle : public BaseRulerHandle<Handle>
{
  using TBase = BaseRulerHandle<Handle>;

public:
  RulerHandle(uint32_t id, dp::Anchor anchor, m2::PointF const & pivot, bool appearing)
    : TBase(id, anchor, pivot, appearing)
  {}

private:
  void UpdateImpl(ScreenBase const &, RulerHelper const & helper) override
  {
    if (!IsVisible())
      return;

    m_size = m2::PointF(helper.GetRulerPixelLength(), 2 * RulerHelper::GetRulerHalfHeight());
    if (IsAppearing())
      m_params.m_length = helper.GetRulerPixelLength();
    m_params.m_position = m_pivot;
    m_params.m_opacity = GetOpacity();
  }
};

class RulerTextHandle : public BaseRulerHandle<MutableLabelHandle>
{
  using TBase = BaseRulerHandle<MutableLabelHandle>;

public:
  RulerTextHandle(uint32_t id, dp::Anchor anchor, m2::PointF const & pivot,
                  bool isAppearing, ref_ptr<dp::TextureManager> textures)
    : TBase(id, anchor, pivot, isAppearing)
    , m_firstUpdate(true)
  {
    SetTextureManager(textures);
  }

  bool Update(ScreenBase const & screen) override
  {
    SetIsVisible(RulerHelper::IsVisible(screen));
    if (IsVisible() && (DrapeGui::GetRulerHelper().IsTextDirty() || m_firstUpdate))
    {
      SetContent(DrapeGui::GetRulerHelper().GetRulerText());
      m_firstUpdate = false;
    }

    return TBase::Update(screen);
  }

  void SetPivot(glsl::vec2 const & pivot) override
  {
    TBase::SetPivot(pivot + glsl::vec2(0.0, RulerHelper::GetVerticalTextOffset() - RulerHelper::GetRulerHalfHeight()));
  }

protected:
  void UpdateImpl(ScreenBase const & /*screen*/, RulerHelper const & /*helper*/) override
  {
    if (IsVisible())
      m_params.m_opacity = GetOpacity();
  }

private:
  bool m_firstUpdate;
};
}  // namespace

drape_ptr<ShapeRenderer> Ruler::Draw(ref_ptr<dp::GraphicsContext> context, m2::PointF & size,
                                     ref_ptr<dp::TextureManager> tex) const
{
  ShapeControl control;
  size = m2::PointF::Zero();
  DrawRuler(context, size, control, tex, true);
  DrawRuler(context, size, control, tex, false);
  DrawText(context, size, control, tex, true);
  DrawText(context, size, control, tex, false);

  drape_ptr<ShapeRenderer> renderer = make_unique_dp<ShapeRenderer>();
  renderer->AddShapeControl(std::move(control));
  return renderer;
}

void Ruler::DrawRuler(ref_ptr<dp::GraphicsContext> context, m2::PointF & size,
                      ShapeControl & control, ref_ptr<dp::TextureManager> tex,
                      bool isAppearing) const
{
  buffer_vector<RulerVertex, 4> data;

  dp::TextureManager::ColorRegion reg;
  tex->GetColorRegion(DrapeGui::GetGuiTextFont().m_color, reg);

  glsl::vec2 texCoord = glsl::ToVec2(reg.GetTexRect().Center());
  float const h = RulerHelper::GetRulerHalfHeight();
  size += m2::PointF(RulerHelper::GetMaxRulerPixelLength(), 2.0f * h);

  glsl::vec2 normals[] =
  {
    glsl::vec2(-1.0, 0.0),
    glsl::vec2(1.0, 0.0),
  };

  dp::Anchor anchor = m_position.m_anchor;
  if (anchor & dp::Left)
    normals[0] = glsl::vec2(0.0, 0.0);
  else if (anchor & dp::Right)
    normals[1] = glsl::vec2(0.0, 0.0);

  data.push_back(RulerVertex(glsl::vec2(0.0, h), normals[0], texCoord));
  data.push_back(RulerVertex(glsl::vec2(0.0, -h), normals[0], texCoord));
  data.push_back(RulerVertex(glsl::vec2(0.0, h), normals[1], texCoord));
  data.push_back(RulerVertex(glsl::vec2(0.0, -h), normals[1], texCoord));

  auto state = df::CreateRenderState(gpu::Program::Ruler, df::DepthLayer::GuiLayer);
  state.SetColorTexture(reg.GetTexture());
  state.SetDepthTestEnabled(false);

  dp::AttributeProvider provider(1, 4);
  provider.InitStream(0, GetBindingInfo(), make_ref(data.data()));

  {
    dp::Batcher batcher(dp::Batcher::IndexPerQuad, dp::Batcher::VertexPerQuad);
    batcher.SetBatcherHash(static_cast<uint64_t>(df::BatcherBucket::Default));
    dp::SessionGuard guard(context, batcher, std::bind(&ShapeControl::AddShape, &control, _1, _2));
    batcher.InsertTriangleStrip(context, state, make_ref(&provider),
                                make_unique_dp<RulerHandle>(EGuiHandle::GuiHandleRuler,
                                  m_position.m_anchor, m_position.m_pixelPivot, isAppearing));
  }
}

void Ruler::DrawText(ref_ptr<dp::GraphicsContext> context, m2::PointF & size,
                     ShapeControl & control, ref_ptr<dp::TextureManager> tex,
                     bool isAppearing) const
{
  std::string alphabet;
  uint32_t maxTextLength;
  RulerHelper::GetTextInitInfo(alphabet, maxTextLength);

  MutableLabelDrawer::Params params;
  params.m_anchor = static_cast<dp::Anchor>((m_position.m_anchor & (dp::Right | dp::Left)) | dp::Bottom);
  params.m_alphabet = alphabet;
  params.m_maxLength = maxTextLength;
  params.m_font = DrapeGui::GetGuiTextFont();
  params.m_pivot = m_position.m_pixelPivot + m2::PointF(0.0f, RulerHelper::GetVerticalTextOffset());
  params.m_handleCreator = [isAppearing, tex](dp::Anchor anchor, m2::PointF const & pivot) {
    return make_unique_dp<RulerTextHandle>(EGuiHandle::GuiHandleRulerLabel, anchor, pivot,
                                           isAppearing, tex);
  };

  m2::PointF const textSize = MutableLabelDrawer::Draw(context, params, tex,
    std::bind(&ShapeControl::AddShape, &control, _1, _2));
  size.y += (textSize.y + std::abs(RulerHelper::GetVerticalTextOffset()));
}
}  // namespace gui
