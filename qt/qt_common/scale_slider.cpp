#include "qt/qt_common/scale_slider.hpp"

#include "qt/qt_common/map_widget.hpp"
#include "qt/qt_common/proxy_style.hpp"

#include "indexer/scales.hpp"

#include "base/math.hpp"

#include <QtWidgets/QToolBar>

#include <memory>

namespace qt
{
namespace common
{
namespace
{
class MyProxyStyle : public ProxyStyle
{
public:
  explicit MyProxyStyle(QStyle * parent) : ProxyStyle(parent) {}

  int styleHint(StyleHint hint, const QStyleOption * option, const QWidget * widget,
                QStyleHintReturn * returnData) const override
  {
    if (hint == SH_Slider_AbsoluteSetButtons)
      return Qt::LeftButton;
    return ProxyStyle::styleHint(hint, option, widget, returnData);
  }
};
}  // namespace

ScaleSlider::ScaleSlider(Qt::Orientation orient, QWidget * parent)
  : QSlider(orient, parent), m_factor(20)
{
  setStyle(new MyProxyStyle(style()));
  SetRange(2, scales::GetUpperScale());
  setTickPosition(QSlider::TicksRight);
}

// static
void ScaleSlider::Embed(Qt::Orientation orient, QToolBar & toolBar, MapWidget & mapWidget)
{
  toolBar.addAction(QIcon(":/common/plus.png"), tr("Scale +"), &mapWidget, SLOT(ScalePlus()));
  {
    auto slider = std::make_unique<ScaleSlider>(orient, &toolBar);
    mapWidget.BindSlider(*slider);
    toolBar.addWidget(slider.release());
  }
  toolBar.addAction(QIcon(":/common/minus.png"), tr("Scale -"), &mapWidget, SLOT(ScaleMinus()));
}

double ScaleSlider::GetScaleFactor() const
{
  double const oldValue = value();
  double const newValue = sliderPosition();

  if (oldValue == newValue)
    return 1.0;
  double const f = pow(2, fabs(oldValue - newValue) / m_factor);
  return (newValue > oldValue ? f : 1.0 / f);
}

void ScaleSlider::SetPosWithBlockedSignals(double pos)
{
  bool const blocked = signalsBlocked();
  blockSignals(true);
  setSliderPosition(math::SignedRound(pos * m_factor));
  blockSignals(blocked);
}

void ScaleSlider::SetRange(int low, int up) { setRange(low * m_factor, up * m_factor); }
}  // namespace common
}  // namespace qt
