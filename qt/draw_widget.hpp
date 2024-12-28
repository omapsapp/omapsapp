#pragma once

#include "qt/qt_common/map_widget.hpp"
#include "qt/routing_turns_visualizer.hpp"
#include "qt/ruler.hpp"
#include "qt/selection.hpp"

#include "map/routing_manager.hpp"

#include "search/result.hpp"

#include "indexer/map_style.hpp"

#include <QtWidgets/QRubberBand>

#include <memory>
#include <optional>
#include <string>

class Framework;

namespace qt
{
namespace common
{
class ScaleSlider;
}

class Screenshoter;
struct ScreenshotParams;

class DrawWidget : public qt::common::MapWidget
{
  using TBase = MapWidget;

  Q_OBJECT

public Q_SLOTS:
  void ShowAll();

  void ChoosePositionModeEnable();
  void ChoosePositionModeDisable();

public:
  DrawWidget(Framework & framework, std::unique_ptr<ScreenshotParams> && screenshotParams,
             QWidget * parent);
  ~DrawWidget() override;

  std::string GetDistance(search::Result const & res) const;

  void CreateFeature();

  void OnLocationUpdate(location::GpsInfo const & info);

  void UpdateAfterSettingsChanged();

  void PrepareShutdown();

  Framework & GetFramework() { return m_framework; }

  void SetMapStyle(MapStyle mapStyle);

  void SetRuler(bool enabled);

  RouteMarkType GetRoutePointAddMode() const { return m_routePointAddMode; }
  void SetRoutePointAddMode(RouteMarkType mode) { m_routePointAddMode = mode; }
  void FollowRoute();
  void ClearRoute();
  void OnRouteRecommendation(RoutingManager::Recommendation recommendation);

  void RefreshDrawingRules();
  void SetMapStyleToDefault();
  void SetMapStyleToVehicle();
  void SetMapStyleToOutdoors();

protected:
  /// @name Overriden from MapWidget.
  //@{
  void initializeGL() override;

  // Touch events
  bool event(QEvent * event) override;

  // Non-touch events
  void mousePressEvent(QMouseEvent * e) override;
  void mouseMoveEvent(QMouseEvent * e) override;
  void mouseReleaseEvent(QMouseEvent * e) override;
  //@}

  void keyPressEvent(QKeyEvent * e) override;
  void keyReleaseEvent(QKeyEvent * e) override;

private:
  void SubmitFakeLocationPoint(m2::PointD const & pt);
  void SubmitRulerPoint(m2::PointD const & pt);
  void SubmitRoutingPoint(m2::PointD const & pt, bool pointIsMercator);
  void SubmitBookmark(m2::PointD const & pt);
  void ShowPlacePage();

  void VisualizeMwmsBordersInRect(m2::RectD const & rect, bool withVertices,
                                  bool fromPackedPolygon, bool boundingBox);

  m2::PointD P2G(m2::PointD const & pt) const;
  m2::PointD GetCoordsFromSettingsIfExists(bool start, m2::PointD const & pt, bool pointIsMercator) const;

  QRubberBand * m_rubberBand;
  QPoint m_rubberBandOrigin;

  bool m_emulatingLocation;

public:
  /// Pass empty \a mode to drop selection.
  void SetSelectionMode(std::optional<SelectionMode> mode) { m_selectionMode = mode; }

  void DropSelectionIfMWMBordersMode()
  {
    static_assert(SelectionMode::MWMBorders < SelectionMode::Cancelled, "");
    if (m_selectionMode && *m_selectionMode > SelectionMode::MWMBorders && *m_selectionMode < SelectionMode::Cancelled)
      m_selectionMode = {};
  }

private:
  void ProcessSelectionMode();
  std::optional<SelectionMode> m_selectionMode;
  RouteMarkType m_routePointAddMode = RouteMarkType::Finish;

  std::unique_ptr<Screenshoter> m_screenshoter;
  Ruler m_ruler;
  RoutingTurnsVisualizer m_turnsVisualizer;
};
}  // namespace qt
