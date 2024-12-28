#include "qt/draw_widget.hpp"

#include "qt/create_feature_dialog.hpp"
#include "qt/editor_dialog.hpp"
#include "qt/place_page_dialog_common.hpp"
#include "qt/place_page_dialog_developer.hpp"
#include "qt/place_page_dialog_user.hpp"
#include "qt/qt_common/helpers.hpp"
#include "qt/routing_settings_dialog.hpp"
#include "qt/screenshoter.hpp"

#include "generator/borders.hpp"

#include "map/framework.hpp"

#include "search/result.hpp"
#include "search/reverse_geocoder.hpp"

#include "routing/following_info.hpp"
#include "routing/routing_callbacks.hpp"

#include "storage/country_decl.hpp"
#include "storage/storage_defines.hpp"

#include "indexer/editable_map_object.hpp"

#include "platform/platform.hpp"

#include "coding/reader.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"

#include "defines.hpp"

#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QMenu>

#include <string>
#include <vector>

namespace qt
{
using namespace qt::common;

namespace
{
std::vector<dp::Color> colorList = {
    dp::Color(255, 0, 0, 255),   dp::Color(0, 255, 0, 255),   dp::Color(0, 0, 255, 255),
    dp::Color(255, 255, 0, 255), dp::Color(0, 255, 255, 255), dp::Color(255, 0, 255, 255),
    dp::Color(100, 0, 0, 255),   dp::Color(0, 100, 0, 255),   dp::Color(0, 0, 100, 255),
    dp::Color(100, 100, 0, 255), dp::Color(0, 100, 100, 255), dp::Color(100, 0, 100, 255)};

void DrawMwmBorder(df::DrapeApi & drapeApi, std::string const & mwmName,
                   std::vector<m2::RegionD> const & regions, bool withVertices)
{
  for (size_t i = 0; i < regions.size(); ++i)
  {
    auto const & region = regions[i];
    auto const & points = region.Data();
    if (points.empty())
      return;

    static uint32_t kColorCounter = 0;

    auto lineData = df::DrapeApiLineData(points, colorList[kColorCounter]).Width(4.0f).ShowId();
    if (withVertices)
      lineData.ShowPoints(true /* markPoints */);

    auto const & name = i == 0 ? mwmName : mwmName + "_" + std::to_string(i);
    drapeApi.AddLine(name, lineData);

    kColorCounter = (kColorCounter + 1) % colorList.size();
  }
}

#if defined(OMIM_OS_LINUX)
df::TouchEvent::ETouchType qtTouchEventTypeToDfTouchEventType(QEvent::Type qEventType)
{
  switch (qEventType)
  {
    case QEvent::TouchBegin: return df::TouchEvent::TOUCH_DOWN;
    case QEvent::TouchEnd: return df::TouchEvent::TOUCH_UP;
    case QEvent::TouchUpdate: return df::TouchEvent::TOUCH_MOVE;
    case QEvent::TouchCancel: return df::TouchEvent::TOUCH_CANCEL;
    default: return df::TouchEvent::TOUCH_NONE;
  }
}
#endif
}  // namespace

DrawWidget::DrawWidget(Framework & framework, std::unique_ptr<ScreenshotParams> && screenshotParams,
                       QWidget * parent)
  : TBase(framework, screenshotParams != nullptr, parent)
  , m_rubberBand(nullptr)
  , m_emulatingLocation(false)
{
  setFocusPolicy(Qt::StrongFocus);

  m_framework.SetPlacePageListeners([this]() { ShowPlacePage(); },
                                    {} /* onClose */, {} /* onUpdate */, {} /*onSwitchFullScreen */);

  auto & routingManager = m_framework.GetRoutingManager();

  routingManager.SetRouteBuildingListener(
      [&routingManager, this](routing::RouterResultCode, storage::CountriesSet const &)
      {
        auto & drapeApi = m_framework.GetDrapeApi();

        m_turnsVisualizer.ClearTurns(drapeApi);

        if (RoutingSettings::TurnsEnabled())
          m_turnsVisualizer.Visualize(routingManager, drapeApi);

        auto const routerType = routingManager.GetLastUsedRouter();
        if (routerType == routing::RouterType::Pedestrian || routerType == routing::RouterType::Bicycle)
        {
          RoutingManager::DistanceAltitude da;
          if (!routingManager.GetRouteAltitudesAndDistancesM(da))
            return;

          for (int iter = 0; iter < 2; ++iter)
          {
            LOG(LINFO, ("Altitudes", iter == 0 ? "before" : "after", "simplify:"));
            LOG_SHORT(LDEBUG, (da));

            uint32_t totalAscent, totalDescent;
            da.CalculateAscentDescent(totalAscent, totalDescent);
            LOG_SHORT(LINFO, ("Ascent:", totalAscent, "Descent:", totalDescent));

            da.Simplify();
          }
        }
      });

  routingManager.SetRouteRecommendationListener(
      [this](RoutingManager::Recommendation r) { OnRouteRecommendation(r); });

  if (screenshotParams != nullptr)
  {
    m_ratio = screenshotParams->m_dpiScale;
    m_screenshoter = std::make_unique<Screenshoter>(*screenshotParams, m_framework, this);
  }
}

DrawWidget::~DrawWidget()
{
  delete m_rubberBand;
}

void DrawWidget::PrepareShutdown()
{
  auto & routingManager = m_framework.GetRoutingManager();
  if (routingManager.IsRoutingActive() && routingManager.IsRoutingFollowing())
  {
    routingManager.SaveRoutePoints();

    auto style = m_framework.GetMapStyle();
    m_framework.MarkMapStyle(MapStyleIsDark(style) ? MapStyle::MapStyleDefaultDark : MapStyle::MapStyleDefaultLight);
  }
}

void DrawWidget::UpdateAfterSettingsChanged()
{
  m_framework.EnterForeground();
}

void DrawWidget::ShowAll()
{
  m_framework.ShowAll();
}

void DrawWidget::ChoosePositionModeEnable()
{
  m_framework.BlockTapEvents(true /* block */);
  m_framework.EnableChoosePositionMode(true /* enable */, false /* enableBounds */, nullptr /* optionalPosition */);
}

void DrawWidget::ChoosePositionModeDisable()
{
  m_framework.EnableChoosePositionMode(false /* enable */, false /* enableBounds */, nullptr /* optionalPosition */);
  m_framework.BlockTapEvents(false /* block */);
}

void DrawWidget::initializeGL()
{
  if (m_screenshotMode)
    m_framework.GetBookmarkManager().EnableTestMode(true);
  else
    m_framework.LoadBookmarks();

  MapWidget::initializeGL();

  m_framework.GetRoutingManager().LoadRoutePoints([this](bool success)
  {
    if (success)
      m_framework.GetRoutingManager().BuildRoute();
  });

  if (m_screenshotMode)
    m_screenshoter->Start();
}

bool DrawWidget::event(QEvent * event)
{
#if !defined(OMIM_OS_LINUX)
    return QOpenGLWidget::event(event);
#else
  // TouchScreen
  if (auto dfTouchEventType = qtTouchEventTypeToDfTouchEventType(event->type());
      dfTouchEventType != df::TouchEvent::TOUCH_NONE)
  {
    event->accept();
    QTouchEvent const * qtTouchEvent = dynamic_cast<QTouchEvent const *>(event);
    df::TouchEvent dfTouchEvent;
    // The SetTouchType hast to be set even if `qtTouchEvent->points()` is empty
    // which theoretically can happen in case of `QEvent::TouchCancel`
    dfTouchEvent.SetTouchType(dfTouchEventType);

    int64_t i = 0;
    for (auto it = qtTouchEvent->points().cbegin();
         it != qtTouchEvent->points().cend() && i < 2; /* For now drape_frontend can only handle max 2 touches */
         ++it, ++i)
    {
      df::Touch touch;
      touch.m_id = i;
      touch.m_location = m2::PointD(L2D(it->position().x()), L2D(it->position().y()));
      if (i == 0)
         dfTouchEvent.SetFirstTouch(touch);
      else
         dfTouchEvent.SetSecondTouch(touch);
    }
    m_framework.TouchEvent(dfTouchEvent);
    return true;
  }
  // TouchPad
  else if (event->type() == QEvent::NativeGesture)
  {
    event->accept();
    auto qNativeGestureEvent = dynamic_cast<QNativeGestureEvent*>(event);
    if (qNativeGestureEvent->gestureType() == Qt::ZoomNativeGesture)
    {
      QPointF const pos = qNativeGestureEvent->position();
      double const factor = qNativeGestureEvent->value();
      m_framework.Scale(exp(factor), m2::PointD(L2D(pos.x()), L2D(pos.y())), false);
      return true;
    }
  }
  // Everything else
  return QOpenGLWidget::event(event);
#endif
}

void DrawWidget::mousePressEvent(QMouseEvent * e)
{
  if (m_screenshotMode)
    return;

  QOpenGLWidget::mousePressEvent(e);

  m2::PointD const pt = GetDevicePoint(e);

  if (IsLeftButton(e))
  {
    if (IsShiftModifier(e))
      SubmitRoutingPoint(pt, false);
    else if (m_ruler.IsActive() && IsAltModifier(e))
      SubmitRulerPoint(pt);
    else if (IsAltModifier(e))
      SubmitFakeLocationPoint(pt);
    else
      m_framework.TouchEvent(GetDfTouchEventFromQMouseEvent(e, df::TouchEvent::TOUCH_DOWN));
  }
  else if (IsRightButton(e))
  {
    if (IsAltModifier(e))
    {
      SubmitBookmark(pt);
    }
    else if (!m_selectionMode || IsCommandModifier(e))
    {
      ShowInfoPopup(e, pt);
    }
    else
    {
      m_rubberBandOrigin = e->pos();
      if (m_rubberBand == nullptr)
        m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
      m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, QSize()));
      m_rubberBand->show();
    }
  }
}

void DrawWidget::mouseMoveEvent(QMouseEvent * e)
{
  if (m_screenshotMode)
    return;

  QOpenGLWidget::mouseMoveEvent(e);

  if (IsLeftButton(e) && !IsAltModifier(e))
  {
    m_framework.TouchEvent(GetDfTouchEventFromQMouseEvent(e, df::TouchEvent::TOUCH_MOVE));
    e->accept();
  }

  if (m_selectionMode && m_rubberBand != nullptr && m_rubberBand->isVisible())
    m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, e->pos()).normalized());
}

void DrawWidget::VisualizeMwmsBordersInRect(m2::RectD const & rect, bool withVertices,
                                            bool fromPackedPolygon, bool boundingBox)
{
  auto const getRegions = [&](std::string const & mwmName)
  {
    if (fromPackedPolygon)
    {
      std::vector<storage::CountryDef> countries;
      FilesContainerR reader(base::JoinPath(GetPlatform().ResourcesDir(), PACKED_POLYGONS_FILE));
      ReaderSource<ModelReaderPtr> src(reader.GetReader(PACKED_POLYGONS_INFO_TAG));
      rw::Read(src, countries);

      for (size_t id = 0; id < countries.size(); ++id)
      {
        if (countries[id].m_countryId != mwmName)
          continue;

        src = reader.GetReader(std::to_string(id));
        return borders::ReadPolygonsOfOneBorder(src);
      }

      UNREACHABLE();
    }
    else
    {
      std::string const bordersDir = base::JoinPath(GetPlatform().WritableDir(), BORDERS_DIR);
      std::string const path = base::JoinPath(bordersDir, mwmName + BORDERS_EXTENSION);

      std::vector<m2::RegionD> polygons;
      borders::LoadBorders(path, polygons);
      return polygons;
    }
  };

  auto mwmNames = m_framework.GetRegionsCountryIdByRect(rect, false /* rough */);

  for (auto & mwmName : mwmNames)
  {
    auto regions = getRegions(mwmName);
    mwmName += fromPackedPolygon ? ".bin" : ".poly";
    if (boundingBox)
    {
      std::vector<m2::RegionD> boxes;
      for (auto const & region : regions)
      {
        auto const r = region.GetRect();
        boxes.emplace_back(std::vector<m2::PointD>({r.LeftBottom(), r.LeftTop(), r.RightTop(),
                                                    r.RightBottom(), r.LeftBottom()}));
      }

      regions = std::move(boxes);
      mwmName += ".box";
    }
    DrawMwmBorder(m_framework.GetDrapeApi(), mwmName, regions, withVertices);
  }
}

void DrawWidget::mouseReleaseEvent(QMouseEvent * e)
{
  if (m_screenshotMode)
    return;

  QOpenGLWidget::mouseReleaseEvent(e);
  if (IsLeftButton(e) && !IsAltModifier(e))
  {
    m_framework.TouchEvent(GetDfTouchEventFromQMouseEvent(e, df::TouchEvent::TOUCH_UP));
  }
  else if (m_selectionMode && IsRightButton(e) &&
           m_rubberBand != nullptr && m_rubberBand->isVisible())
  {
    ProcessSelectionMode();
  }
}

void DrawWidget::ProcessSelectionMode()
{
  QPoint const lt = m_rubberBand->geometry().topLeft();
  QPoint const rb = m_rubberBand->geometry().bottomRight();
  m2::RectD rect;
  rect.Add(m_framework.PtoG(m2::PointD(L2D(lt.x()), L2D(lt.y()))));
  rect.Add(m_framework.PtoG(m2::PointD(L2D(rb.x()), L2D(rb.y()))));

  switch (*m_selectionMode)
  {
  case SelectionMode::Features:
    m_framework.VisualizeRoadsInRect(rect);
    break;

  case SelectionMode::CityBoundaries:
    m_framework.VisualizeCityBoundariesInRect(rect);
    break;

  case SelectionMode::CityRoads:
    m_framework.VisualizeCityRoadsInRect(rect);
    break;

  case SelectionMode::CrossMwmSegments:
    m_framework.VisualizeCrossMwmTransitionsInRect(rect);
    break;

  case SelectionMode::MwmsBordersByPolyFiles:
    VisualizeMwmsBordersInRect(rect, false /* withVertices */, false /* fromPackedPolygon */,
                               false /* boundingBox */);
    break;

  case SelectionMode::MwmsBordersWithVerticesByPolyFiles:
    VisualizeMwmsBordersInRect(rect, true /* withVertices */, false /* fromPackedPolygon */,
                               false /* boundingBox */);
    break;

  case SelectionMode::MwmsBordersByPackedPolygon:
    VisualizeMwmsBordersInRect(rect, false /* withVertices */, true /* fromPackedPolygon */,
                               false /* boundingBox */);
    break;

  case SelectionMode::MwmsBordersWithVerticesByPackedPolygon:
    VisualizeMwmsBordersInRect(rect, true /* withVertices */, true /* fromPackedPolygon */,
                               false /* boundingBox */);
    break;

  case SelectionMode::BoundingBoxByPolyFiles:
    VisualizeMwmsBordersInRect(rect, true /* withVertices */, false /* fromPackedPolygon */,
                               true /* boundingBox */);
    break;

  case SelectionMode::BoundingBoxByPackedPolygon:
    VisualizeMwmsBordersInRect(rect, true /* withVertices */, true /* fromPackedPolygon */,
                               true /* boundingBox */);
    break;

  default:
    UNREACHABLE();
  }

  m_rubberBand->hide();
}

void DrawWidget::keyPressEvent(QKeyEvent * e)
{
  if (m_screenshotMode)
    return;

  if (IsLeftButton(QGuiApplication::mouseButtons()) &&
      e->key() == Qt::Key_Control)
  {
    df::TouchEvent event;
    event.SetTouchType(df::TouchEvent::TOUCH_DOWN);
    df::Touch touch;
    touch.m_id = 0;
    touch.m_location = m2::PointD(L2D(QCursor::pos().x()), L2D(QCursor::pos().y()));
    event.SetFirstTouch(touch);
    event.SetSecondTouch(GetSymmetrical(touch));

    m_framework.TouchEvent(event);
  }
}

void DrawWidget::keyReleaseEvent(QKeyEvent * e)
{
  if (m_screenshotMode)
    return;

  if (IsLeftButton(QGuiApplication::mouseButtons()) &&
      e->key() == Qt::Key_Control)
  {
    df::TouchEvent event;
    event.SetTouchType(df::TouchEvent::TOUCH_UP);
    df::Touch touch;
    touch.m_id = 0;
    touch.m_location = m2::PointD(L2D(QCursor::pos().x()), L2D(QCursor::pos().y()));
    event.SetFirstTouch(touch);
    event.SetSecondTouch(GetSymmetrical(touch));

    m_framework.TouchEvent(event);
  }
  else if (e->key() == Qt::Key_Alt)
    m_emulatingLocation = false;
}

std::string DrawWidget::GetDistance(search::Result const & res) const
{
  platform::Distance dist;
  if (auto const position = m_framework.GetCurrentPosition())
  {
    auto const ll = mercator::ToLatLon(*position);
    double dummy;
    (void)m_framework.GetDistanceAndAzimut(res.GetFeatureCenter(), ll.m_lat, ll.m_lon, -1.0, dist,
                                           dummy);
  }
  return dist.ToString();
}

void DrawWidget::CreateFeature()
{
  auto cats = m_framework.GetEditorCategories();
  CreateFeatureDialog dlg(this, cats);
  if (dlg.exec() == QDialog::Accepted)
  {
    osm::EditableMapObject emo;
    if (m_framework.CreateMapObject(m_framework.GetViewportCenter(), dlg.GetSelectedType(), emo))
    {
      EditorDialog dlg(this, emo);
      int const result = dlg.exec();
      if (result == QDialog::Accepted)
        m_framework.SaveEditedMapObject(emo);
    }
    else
    {
      LOG(LWARNING, ("Error creating new map object."));
    }
  }
}

void DrawWidget::OnLocationUpdate(location::GpsInfo const & info)
{
  if (!m_emulatingLocation)
    m_framework.OnLocationUpdate(info);
}

void DrawWidget::SetMapStyle(MapStyle mapStyle)
{
  m_framework.SetMapStyle(mapStyle);
}

void DrawWidget::SubmitFakeLocationPoint(m2::PointD const & pt)
{
  m_emulatingLocation = true;

  m2::PointD const point = GetCoordsFromSettingsIfExists(true /* start */, pt, false /* pointIsMercator */);

  m_framework.OnLocationUpdate(qt::common::MakeGpsInfo(point));

  auto & routingManager = m_framework.GetRoutingManager();
  if (routingManager.IsRoutingActive())
  {
    /// Immediate update of the position in Route to get updated FollowingInfo state for visual debugging.
    /// m_framework.OnLocationUpdate calls RoutingSession::OnLocationPositionChanged
    /// with delay several times according to interpolation.
    /// @todo Write log when the final point will be reached and
    /// RoutingSession::OnLocationPositionChanged will be called the last time.
    routingManager.RoutingSession().OnLocationPositionChanged(qt::common::MakeGpsInfo(point));

    routing::FollowingInfo loc;
    routingManager.GetRouteFollowingInfo(loc);
    if (routingManager.GetCurrentRouterType() == routing::RouterType::Pedestrian)
    {
      LOG(LDEBUG, ("Distance:", loc.m_distToTarget, "Time:", loc.m_time,
                   DebugPrint(loc.m_pedestrianTurn),
                   "in", loc.m_distToTurn.ToString(),
                   loc.m_nextStreetName.empty() ? "" : "to " + loc.m_nextStreetName ));
    }
    else
    {
      std::string speed;
      if (loc.m_speedLimitMps > 0)
        speed = "SpeedLimit: " + measurement_utils::FormatSpeedNumeric(loc.m_speedLimitMps, measurement_utils::Units::Metric);

      LOG(LDEBUG, ("Distance:", loc.m_distToTarget, "Time:", loc.m_time, speed,
                   GetTurnString(loc.m_turn), (loc.m_exitNum != 0 ? ":" + std::to_string(loc.m_exitNum) : ""),
                   "in", loc.m_distToTurn.ToString(),
                   loc.m_nextStreetName.empty() ? "" : "to " + loc.m_nextStreetName ));
    }
  }
}

void DrawWidget::SubmitRulerPoint(m2::PointD const & pt)
{
  m_ruler.AddPoint(P2G(pt));
  m_ruler.DrawLine(m_framework.GetDrapeApi());
}

void DrawWidget::SubmitRoutingPoint(m2::PointD const & pt, bool pointIsMercator)
{
  auto & routingManager = m_framework.GetRoutingManager();

  // Check if limit of intermediate points is reached.
  bool const isIntermediate = m_routePointAddMode == RouteMarkType::Intermediate;
  if (isIntermediate && !routingManager.CouldAddIntermediatePoint())
    routingManager.RemoveRoutePoint(RouteMarkType::Intermediate, 0);

  // Insert implicit start point.
  if (m_routePointAddMode == RouteMarkType::Finish && routingManager.GetRoutePoints().empty())
  {
    RouteMarkData startPoint;
    startPoint.m_pointType = RouteMarkType::Start;
    startPoint.m_isMyPosition = true;
    routingManager.AddRoutePoint(std::move(startPoint));
  }

  RouteMarkData point;
  point.m_pointType = m_routePointAddMode;
  point.m_isMyPosition = false;
  if (!isIntermediate)
    point.m_position = GetCoordsFromSettingsIfExists(false /* start */, pt, pointIsMercator);
 else
    point.m_position = pointIsMercator ? pt : P2G(pt);

  routingManager.AddRoutePoint(std::move(point));

  if (routingManager.GetRoutePoints().size() >= 2)
  {
    if (RoutingSettings::UseDebugGuideTrack())
    {
      // Like in guides_tests.cpp, GetTestGuides().
      routing::GuidesTracks guides;
      guides[10] = {{{mercator::FromLatLon(48.13999, 11.56873), 10},
                     {mercator::FromLatLon(48.14096, 11.57246), 10},
                     {mercator::FromLatLon(48.14487, 11.57259), 10}}};
      routingManager.RoutingSession().SetGuidesForTests(std::move(guides));
    }
    else
      routingManager.RoutingSession().SetGuidesForTests({});

    routingManager.BuildRoute();
  }
}

void DrawWidget::SubmitBookmark(m2::PointD const & pt)
{
  auto & manager = m_framework.GetBookmarkManager();

  kml::BookmarkData data;
  data.m_color.m_predefinedColor = kml::PredefinedColor::Red;
  data.m_point = m_framework.P3dtoG(pt);
  manager.GetEditSession().CreateBookmark(std::move(data), manager.LastEditedBMCategory());
}

void DrawWidget::FollowRoute()
{
  auto & routingManager = m_framework.GetRoutingManager();

  auto const points = routingManager.GetRoutePoints();
  if (points.size() < 2)
    return;
  if (!points.front().m_isMyPosition && !points.back().m_isMyPosition)
    return;
  if (routingManager.IsRoutingActive() && !routingManager.IsRoutingFollowing())
  {
    routingManager.FollowRoute();
    SetMapStyleToVehicle();
  }
}

void DrawWidget::ClearRoute()
{
  auto & routingManager = m_framework.GetRoutingManager();

  bool const wasActive = routingManager.IsRoutingActive() && routingManager.IsRoutingFollowing();
  routingManager.CloseRouting(true /* remove route points */);

  if (wasActive)
    SetMapStyleToDefault();

  m_turnsVisualizer.ClearTurns(m_framework.GetDrapeApi());
}

void DrawWidget::OnRouteRecommendation(RoutingManager::Recommendation recommendation)
{
  if (recommendation == RoutingManager::Recommendation::RebuildAfterPointsLoading)
  {
    auto & routingManager = m_framework.GetRoutingManager();

    RouteMarkData startPoint;
    startPoint.m_pointType = RouteMarkType::Start;
    startPoint.m_isMyPosition = true;
    routingManager.AddRoutePoint(std::move(startPoint));

    if (routingManager.GetRoutePoints().size() >= 2)
      routingManager.BuildRoute();
  }
}

void DrawWidget::ShowPlacePage()
{
  place_page::Info const & info = m_framework.GetCurrentPlacePageInfo();
  search::ReverseGeocoder::Address address;
  if (info.IsFeature())
  {
    search::ReverseGeocoder const coder(m_framework.GetDataSource());
    coder.GetExactAddress(info.GetID(), address);
  }
  else
  {
    address = m_framework.GetAddressAtPoint(info.GetMercator());
  }

  std::unique_ptr<QDialog> placePageDialog = nullptr;
  bool developerMode;
  if (settings::Get(settings::kDeveloperMode, developerMode) && developerMode)
    placePageDialog = std::make_unique<PlacePageDialogDeveloper>(this, info, address);
  else
    placePageDialog = std::make_unique<PlacePageDialogUser>(this, info, address);

  switch (placePageDialog->exec())
  {
  case place_page_dialog::EditPlace:
  {
    osm::EditableMapObject emo;
    if (m_framework.GetEditableMapObject(info.GetID(), emo))
    {
      EditorDialog dlg(this, emo);
      int const result = dlg.exec();
      if (result == QDialog::Accepted)
      {
        m_framework.SaveEditedMapObject(emo);
        m_framework.UpdatePlacePageInfoForCurrentSelection();
      }
      else if (result == QDialogButtonBox::DestructiveRole)
      {
        m_framework.DeleteFeature(info.GetID());
      }
    }
    else
    {
      LOG(LERROR, ("Error while trying to edit feature."));
    }
  }
  break;
  case place_page_dialog::RouteFrom:
  {
    SetRoutePointAddMode(RouteMarkType::Start);
    SubmitRoutingPoint(info.GetMercator(), true);
  }
  break;
  case place_page_dialog::AddStop:
  {
    SetRoutePointAddMode(RouteMarkType::Intermediate);
    SubmitRoutingPoint(info.GetMercator(), true);
  }
  break;
  case place_page_dialog::RouteTo:
  {
    SetRoutePointAddMode(RouteMarkType::Finish);
    SubmitRoutingPoint(info.GetMercator(), true);
  }
  break;
  default: break;
  }
  m_framework.DeactivateMapSelection();
}

void DrawWidget::SetRuler(bool enabled)
{
  if (!enabled)
    m_ruler.EraseLine(m_framework.GetDrapeApi());
  m_ruler.SetActive(enabled);
}

// static
void DrawWidget::RefreshDrawingRules()
{
  SetMapStyle(MapStyleDefaultLight);
}

void DrawWidget::SetMapStyleToDefault()
{
  auto const style = m_framework.GetMapStyle();
  SetMapStyle(MapStyleIsDark(style) ? MapStyle::MapStyleDefaultDark : MapStyle::MapStyleDefaultLight);
}

void DrawWidget::SetMapStyleToVehicle()
{
  auto const style = m_framework.GetMapStyle();
  SetMapStyle(MapStyleIsDark(style) ? MapStyle::MapStyleVehicleDark : MapStyle::MapStyleVehicleLight);
}

void DrawWidget::SetMapStyleToOutdoors()
{
  auto const style = m_framework.GetMapStyle();
  SetMapStyle(MapStyleIsDark(style) ? MapStyle::MapStyleOutdoorsDark : MapStyle::MapStyleOutdoorsLight);
}

m2::PointD DrawWidget::P2G(m2::PointD const & pt) const
{
  return m_framework.P3dtoG(pt);
}

m2::PointD DrawWidget::GetCoordsFromSettingsIfExists(bool start, m2::PointD const & pt,  bool pointIsMercator) const
{
  if (auto optional = RoutingSettings::GetCoords(start))
    return mercator::FromLatLon(*optional);

  return pointIsMercator ? pt : P2G(pt);
}
}  // namespace qt
