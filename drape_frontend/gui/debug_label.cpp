#include "drape_frontend/gui/debug_label.hpp"

#include "drape_frontend/gui/drape_gui.hpp"

#include <functional>
#include <set>
#include <utility>

using namespace std::placeholders;

namespace gui
{
class DebugLabelHandle : public MutableLabelHandle
{
  using TBase = MutableLabelHandle;

public:
  DebugLabelHandle(uint32_t id, dp::Anchor anchor, m2::PointF const & pivot,
                   ref_ptr<dp::TextureManager> tex, TUpdateDebugLabelFn const & onUpdateFn)
    : MutableLabelHandle(id, anchor, pivot)
    , m_onUpdateFn(onUpdateFn)
  {
    SetTextureManager(tex);
  }

  bool Update(ScreenBase const & screen) override
  {
    std::string content;
    bool const isVisible = m_onUpdateFn(screen, content);

    SetIsVisible(isVisible);
    SetContent(content);

    return TBase::Update(screen);
  }

private:
  TUpdateDebugLabelFn m_onUpdateFn;
};

void AddSymbols(std::string const & str, std::set<char> & symbols)
{
  for (size_t i = 0, sz = str.length(); i < sz; ++i)
    symbols.insert(str[i]);
}

void DebugInfoLabels::AddLabel(ref_ptr<dp::TextureManager>, std::string const & caption,
                               TUpdateDebugLabelFn const &)
{
  std::string alphabet;
  std::set<char> symbols;
  AddSymbols(caption, symbols);
  AddSymbols("0123456789.-e", symbols);

  alphabet.reserve(symbols.size());
  for (auto const & ch : symbols)
    alphabet.push_back(ch);

  MutableLabelDrawer::Params params;
  params.m_anchor = dp::LeftTop;
  params.m_alphabet = alphabet;
  params.m_maxLength = 100;
  params.m_font = DrapeGui::GetGuiTextFont();
  params.m_font.m_color = dp::Color(0, 0, 255, 255);
  params.m_font.m_size *= 1.2;
  params.m_pivot = m_position.m_pixelPivot;

#ifdef RENDER_DEBUG_INFO_LABELS
  uint32_t const id = GuiHandleDebugLabel + m_labelsCount;

  params.m_handleCreator = [id, onUpdateFn, tex](dp::Anchor anchor, m2::PointF const & pivot) {
    return make_unique_dp<DebugLabelHandle>(id, anchor, pivot, tex, onUpdateFn);
  };
#endif
  m_labelsParams.push_back(params);
  ++m_labelsCount;
}

drape_ptr<ShapeRenderer> DebugInfoLabels::Draw(ref_ptr<dp::GraphicsContext> context,
                                               ref_ptr<dp::TextureManager> tex)
{
  drape_ptr<ShapeRenderer> renderer = make_unique_dp<ShapeRenderer>();

  m2::PointF pos = m_position.m_pixelPivot;

  for (auto & params : m_labelsParams)
  {
    params.m_pivot.y = pos.y;
    m2::PointF textSize = MutableLabelDrawer::Draw(context, params, tex,
      std::bind(&ShapeRenderer::AddShape, renderer.get(), _1, _2));
    pos.y += 2 * textSize.y;
  }

  m_labelsParams.clear();

  return renderer;
}
}  // namespace gui
