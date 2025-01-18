#include "drape_frontend/drape_engine.hpp"

#include "drape_frontend/gui/drape_gui.hpp"
#include "drape_frontend/message_subclasses.hpp"
#include "drape_frontend/my_position_controller.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/drape_routine.hpp"
#include "drape/support_manager.hpp"

#include "platform/settings.hpp"

#include <unordered_map>

namespace df
{
using namespace std::placeholders;

namespace
{
std::string_view constexpr kLocationStateMode = "LastLocationStateMode";
std::string_view constexpr kLastEnterBackground = "LastEnterBackground";
}

DrapeEngine::DrapeEngine(Params && params)
  : m_myPositionModeChanged(std::move(params.m_myPositionModeChanged))
  , m_viewport(std::move(params.m_viewport))
{
  dp::DrapeRoutine::Init();

  VisualParams::Init(params.m_vs, df::CalculateTileSize(m_viewport.GetWidth(), m_viewport.GetHeight()));

  SetFontScaleFactor(params.m_fontsScaleFactor);

  gui::DrapeGui::Instance().SetSurfaceSize(m2::PointF(m_viewport.GetWidth(), m_viewport.GetHeight()));

  m_textureManager = make_unique_dp<dp::TextureManager>();
  m_threadCommutator = make_unique_dp<ThreadsCommutator>();
  m_requestedTiles = make_unique_dp<RequestedTiles>();

  using namespace location;
  EMyPositionMode mode = PendingPosition;
  if (settings::Get(kLocationStateMode, mode) && mode == FollowAndRotate)
  {
    // If the screen rect setting in follow and rotate mode is missing or invalid, it could cause
    // invalid animations, so the follow and rotate mode should be discarded.
    m2::AnyRectD rect;
    if (!(settings::Get("ScreenClipRect", rect) && df::GetWorldRect().IsRectInside(rect.GetGlobalRect())))
      mode = Follow;
  }

  if (!settings::Get(kLastEnterBackground, m_startBackgroundTime))
    m_startBackgroundTime = base::Timer::LocalTime();

  std::vector<PostprocessRenderer::Effect> effects;

//  bool enabledAntialiasing;
//  if (!settings::Get(dp::kSupportedAntialiasing, enabledAntialiasing))
//    enabledAntialiasing = false;

  // Turn off AA for a while by energy-saving issues.
  //if (enabledAntialiasing)
  //{
  //  LOG(LINFO, ("Antialiasing is enabled"));
  //  effects.push_back(PostprocessRenderer::Antialiasing);
  //}

  MyPositionController::Params mpParams(mode,
                                        base::Timer::LocalTime() - m_startBackgroundTime,
                                        params.m_hints,
                                        params.m_isRoutingActive,
                                        params.m_isAutozoomEnabled,
                                        std::bind(&DrapeEngine::MyPositionModeChanged, this, _1, _2));

  FrontendRenderer::Params frParams(params.m_apiVersion,
                                    make_ref(m_threadCommutator),
                                    params.m_factory,
                                    make_ref(m_textureManager),
                                    std::move(mpParams),
                                    m_viewport,
                                    std::bind(&DrapeEngine::ModelViewChanged, this, _1),
                                    std::bind(&DrapeEngine::TapEvent, this, _1),
                                    std::bind(&DrapeEngine::UserPositionChanged, this, _1, _2),
                                    make_ref(m_requestedTiles),
                                    std::move(params.m_overlaysShowStatsCallback),
                                    params.m_allow3dBuildings,
                                    params.m_trafficEnabled,
                                    params.m_blockTapEvents,
                                    std::move(effects),
                                    params.m_onGraphicsContextInitialized,
                                    std::move(params.m_renderInjectionHandler));

  BackendRenderer::Params brParams(
      params.m_apiVersion, frParams.m_commutator, frParams.m_oglContextFactory, frParams.m_texMng,
      params.m_model, params.m_model.UpdateCurrentCountryFn(), make_ref(m_requestedTiles),
      params.m_allow3dBuildings, params.m_trafficEnabled, params.m_isolinesEnabled,
      params.m_simplifiedTrafficColors, std::move(params.m_arrow3dCustomDecl),
      params.m_onGraphicsContextInitialized);

  m_backend = make_unique_dp<BackendRenderer>(std::move(brParams));
  m_frontend = make_unique_dp<FrontendRenderer>(std::move(frParams));

  m_widgetsInfo = std::move(params.m_info);

  RecacheGui(false);
  RecacheMapShapes();

  if (params.m_showChoosePositionMark)
    EnableChoosePositionMode(true, std::move(params.m_boundAreaTriangles), nullptr);

  ResizeImpl(m_viewport.GetWidth(), m_viewport.GetHeight());
}

DrapeEngine::~DrapeEngine()
{
  dp::DrapeRoutine::Shutdown();

  // Call Teardown explicitly! We must wait for threads completion.
  m_frontend->Teardown();
  m_backend->Teardown();

  // Reset thread commutator, it stores BaseRenderer pointers.
  m_threadCommutator.reset();

  // Reset pointers to FrontendRenderer and BackendRenderer.
  m_frontend.reset();
  m_backend.reset();

  gui::DrapeGui::Instance().Destroy();
  m_textureManager->Release();
}

void DrapeEngine::RecoverSurface(int w, int h, bool recreateContextDependentResources)
{
  if (m_choosePositionMode)
  {
    m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                    make_unique_dp<ShowChoosePositionMarkMessage>(),
                                    MessagePriority::Normal);
  }

  if (recreateContextDependentResources)
  {
    RecacheGui(false);
    RecacheMapShapes();
    m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                    make_unique_dp<RecoverContextDependentResourcesMessage>(),
                                    MessagePriority::Normal);
  }

  ResizeImpl(w, h);
}

void DrapeEngine::Resize(int w, int h)
{
  ASSERT_GREATER(w, 0, ());
  ASSERT_GREATER(h, 0, ());
  if (m_viewport.GetHeight() != static_cast<uint32_t>(h) ||
      m_viewport.GetWidth() != static_cast<uint32_t>(w))
    ResizeImpl(w, h);
}

void DrapeEngine::SetVisibleViewport(m2::RectD const & rect) const
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetVisibleViewportMessage>(rect),
                                  MessagePriority::Normal);
}

void DrapeEngine::Invalidate()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<InvalidateMessage>(),
                                  MessagePriority::High);
}

void DrapeEngine::AddTouchEvent(TouchEvent const & event)
{
  AddUserEvent(make_unique_dp<TouchEvent>(event));
}

void DrapeEngine::Scale(double factor, m2::PointD const & pxPoint, bool isAnim)
{
  AddUserEvent(make_unique_dp<ScaleEvent>(factor, pxPoint, isAnim));
}

void DrapeEngine::Move(double factorX, double factorY, bool isAnim)
{
  AddUserEvent(make_unique_dp<MoveEvent>(factorX, factorY, isAnim));
}

void DrapeEngine::Scroll(double distanceX, double distanceY)
{
  AddUserEvent(make_unique_dp<ScrollEvent>(distanceX, distanceY));
}

void DrapeEngine::Rotate(double azimuth, bool isAnim)
{
  AddUserEvent(make_unique_dp<RotateEvent>(azimuth, isAnim, nullptr /* parallelAnimCreator */));
}

void DrapeEngine::MakeFrameActive()
{
  AddUserEvent(make_unique_dp<ActiveFrameEvent>());
}

void DrapeEngine::ScaleAndSetCenter(m2::PointD const & centerPt, double scaleFactor, bool isAnim,
                                    bool trackVisibleViewport)
{
  PostUserEvent(make_unique_dp<SetCenterEvent>(scaleFactor, centerPt, isAnim,
                                               trackVisibleViewport,
                                               nullptr /* parallelAnimCreator */));
}

void DrapeEngine::SetModelViewCenter(m2::PointD const & centerPt, int zoom, bool isAnim,
                                     bool trackVisibleViewport)
{
  PostUserEvent(make_unique_dp<SetCenterEvent>(centerPt, zoom, isAnim, trackVisibleViewport,
                                               nullptr /* parallelAnimCreator */));
}

void DrapeEngine::SetModelViewRect(m2::RectD const & rect, bool applyRotation, int zoom, bool isAnim,
                                   bool useVisibleViewport)
{
  PostUserEvent(make_unique_dp<SetRectEvent>(rect, applyRotation, zoom, isAnim, useVisibleViewport,
                                             nullptr /* parallelAnimCreator */ ));
}

void DrapeEngine::SetModelViewAnyRect(m2::AnyRectD const & rect, bool isAnim, bool useVisibleViewport)
{
  PostUserEvent(make_unique_dp<SetAnyRectEvent>(rect, isAnim, true /* fitInViewport */, useVisibleViewport));
}

void DrapeEngine::ClearUserMarksGroup(kml::MarkGroupId groupId)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<ClearUserMarkGroupMessage>(groupId),
                                  MessagePriority::Normal);
}

void DrapeEngine::ChangeVisibilityUserMarksGroup(kml::MarkGroupId groupId, bool isVisible)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<ChangeUserMarkGroupVisibilityMessage>(groupId, isVisible),
                                  MessagePriority::Normal);
}

void DrapeEngine::InvalidateUserMarks()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<InvalidateUserMarksMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::UpdateUserMarks(UserMarksProvider * provider, bool firstTime)
{
  auto const updatedGroupIds = firstTime ? provider->GetAllGroupIds()
                                         : provider->GetUpdatedGroupIds();
  if (updatedGroupIds.empty())
    return;

  auto marksRenderCollection = make_unique_dp<UserMarksRenderCollection>();
  auto linesRenderCollection = make_unique_dp<UserLinesRenderCollection>();
  auto justCreatedIdCollection = make_unique_dp<IDCollections>();
  auto removedIdCollection = make_unique_dp<IDCollections>();

  std::unordered_map<kml::MarkGroupId, drape_ptr<IDCollections>> groupsVisibleIds;

  auto const groupFilter = [&](kml::MarkGroupId groupId)
  {
    return provider->IsGroupVisible(groupId) &&
      (provider->GetBecameVisibleGroupIds().count(groupId) == 0);
  };

  using GroupFilter = std::function<bool(kml::MarkGroupId groupId)>;

  auto const collectIds = [&](kml::MarkIdSet const & markIds, kml::TrackIdSet const & lineIds,
                              GroupFilter const & filter, IDCollections & collection)
  {
    for (auto const markId : markIds)
    {
      if (filter == nullptr || filter(provider->GetUserPointMark(markId)->GetGroupId()))
        collection.m_markIds.push_back(markId);
    }

    for (auto const lineId : lineIds)
    {
      if (filter == nullptr || filter(provider->GetUserLineMark(lineId)->GetGroupId()))
        collection.m_lineIds.push_back(lineId);
    }
  };

  auto const collectRenderData = [&](kml::MarkIdSet const & markIds, kml::TrackIdSet const & lineIds,
                                     GroupFilter const & filter)
  {
    for (auto const markId : markIds)
    {
      auto const mark = provider->GetUserPointMark(markId);
      if (filter == nullptr || filter(mark->GetGroupId()))
        marksRenderCollection->emplace(markId, GenerateMarkRenderInfo(mark));
    }

    for (auto const lineId : lineIds)
    {
      auto const line = provider->GetUserLineMark(lineId);
      if (filter == nullptr || filter(line->GetGroupId()))
        linesRenderCollection->emplace(lineId, GenerateLineRenderInfo(line));
    }
  };

  if (firstTime)
  {
    for (auto groupId : provider->GetAllGroupIds())
    {
      auto visibleIdCollection = make_unique_dp<IDCollections>();

      if (provider->IsGroupVisible(groupId))
      {
        collectIds(provider->GetGroupPointIds(groupId), provider->GetGroupLineIds(groupId),
                   nullptr /* filter */, *visibleIdCollection);
        collectRenderData(provider->GetGroupPointIds(groupId), provider->GetGroupLineIds(groupId),
                          nullptr /* filter */);
      }
      groupsVisibleIds.emplace(groupId, std::move(visibleIdCollection));
    }
  }
  else
  {
    for (auto groupId : provider->GetUpdatedGroupIds())
    {
      auto visibleIdCollection = make_unique_dp<IDCollections>();
      if (provider->IsGroupVisible(groupId))
      {
        collectIds(provider->GetGroupPointIds(groupId), provider->GetGroupLineIds(groupId),
                   nullptr /* filter */, *visibleIdCollection);
      }
      groupsVisibleIds.emplace(groupId, std::move(visibleIdCollection));
    }

    collectIds(provider->GetCreatedMarkIds(), provider->GetCreatedLineIds(),
               groupFilter, *justCreatedIdCollection);
    collectIds(provider->GetRemovedMarkIds(), provider->GetRemovedLineIds(),
               nullptr /* filter */, *removedIdCollection);

    collectRenderData(provider->GetCreatedMarkIds(), provider->GetCreatedLineIds(), groupFilter);
    collectRenderData(provider->GetUpdatedMarkIds(), provider->GetUpdatedLineIds(), groupFilter);

    for (auto const groupId : provider->GetBecameVisibleGroupIds())
    {
      collectRenderData(provider->GetGroupPointIds(groupId), provider->GetGroupLineIds(groupId),
                        nullptr /* filter */);
    }

    for (auto const groupId : provider->GetBecameInvisibleGroupIds())
    {
      collectIds(provider->GetGroupPointIds(groupId), provider->GetGroupLineIds(groupId),
                 nullptr /* filter */, *removedIdCollection);
    }
  }

  if (!marksRenderCollection->empty() || !linesRenderCollection->empty() ||
      !removedIdCollection->IsEmpty() || !justCreatedIdCollection->IsEmpty())
  {
    m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                    make_unique_dp<UpdateUserMarksMessage>(
                                      std::move(justCreatedIdCollection),
                                      std::move(removedIdCollection),
                                      std::move(marksRenderCollection),
                                      std::move(linesRenderCollection)),
                                    MessagePriority::Normal);
  }

  for (auto & v : groupsVisibleIds)
  {
    m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                    make_unique_dp<UpdateUserMarkGroupMessage>(
                                      v.first,
                                      std::move(v.second)),
                                    MessagePriority::Normal);
  }
}

void DrapeEngine::SetRenderingEnabled(ref_ptr<dp::GraphicsContextFactory> contextFactory)
{
  m_backend->SetRenderingEnabled(contextFactory);
  m_frontend->SetRenderingEnabled(contextFactory);

  LOG(LDEBUG, ("Rendering enabled"));
}

void DrapeEngine::SetRenderingDisabled(bool const destroySurface)
{
  m_frontend->SetRenderingDisabled(destroySurface);
  m_backend->SetRenderingDisabled(destroySurface);

  LOG(LDEBUG, ("Rendering disabled"));
}

void DrapeEngine::InvalidateRect(m2::RectD const & rect)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<InvalidateRectMessage>(rect),
                                  MessagePriority::High);
}

void DrapeEngine::UpdateMapStyle()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<UpdateMapStyleMessage>(),
                                  MessagePriority::High);
}

void DrapeEngine::RecacheMapShapes()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<MapShapesRecacheMessage>(),
                                  MessagePriority::Normal);
}

dp::DrapeID DrapeEngine::GenerateDrapeID()
{
  return ++m_drapeIdGenerator;
}

void DrapeEngine::RecacheGui(bool needResetOldGui)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<GuiRecacheMessage>(m_widgetsInfo, needResetOldGui),
                                  MessagePriority::Normal);
}

void DrapeEngine::PostUserEvent(drape_ptr<UserEvent> && e)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<PostUserEventMessage>(std::move(e)),
                                  MessagePriority::Normal);
}

void DrapeEngine::AddUserEvent(drape_ptr<UserEvent> && e)
{
  m_frontend->AddUserEvent(std::move(e));
}

void DrapeEngine::ModelViewChanged(ScreenBase const & screen)
{
  if (m_modelViewChangedHandler != nullptr)
    m_modelViewChangedHandler(screen);
}

void DrapeEngine::MyPositionModeChanged(location::EMyPositionMode mode, bool routingActive)
{
  settings::Set(kLocationStateMode, mode);
  if (m_myPositionModeChanged)
    m_myPositionModeChanged(mode, routingActive);
}

location::EMyPositionMode DrapeEngine::GetMyPositionMode() const
{
  return m_frontend->GetMyPositionMode();
}

void DrapeEngine::TapEvent(TapInfo const & tapInfo)
{
  if (m_tapEventInfoHandler != nullptr)
    m_tapEventInfoHandler(tapInfo);
}

void DrapeEngine::UserPositionChanged(m2::PointD const & position, bool hasPosition)
{
  if (m_userPositionChangedHandler != nullptr)
    m_userPositionChangedHandler(position, hasPosition);
}

void DrapeEngine::ResizeImpl(int w, int h)
{
  gui::DrapeGui::Instance().SetSurfaceSize(m2::PointF(w, h));
  m_viewport.SetViewport(0, 0, w, h);
  PostUserEvent(make_unique_dp<ResizeEvent>(w, h));
}

void DrapeEngine::SetCompassInfo(location::CompassInfo const & info)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<CompassInfoMessage>(info),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetGpsInfo(location::GpsInfo const & info, bool isNavigable,
                             location::RouteMatchingInfo const & routeInfo)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<GpsInfoMessage>(info, isNavigable, routeInfo),
                                  MessagePriority::Normal);
}

void DrapeEngine::SwitchMyPositionNextMode()
{
  using Mode = ChangeMyPositionModeMessage::EChangeType;
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ChangeMyPositionModeMessage>(Mode::SwitchNextMode),
                                  MessagePriority::Normal);
}

void DrapeEngine::LoseLocation()
{
  using Mode = ChangeMyPositionModeMessage::EChangeType;
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ChangeMyPositionModeMessage>(Mode::LoseLocation),
                                  MessagePriority::Normal);
}

void DrapeEngine::StopLocationFollow()
{
  using Mode = ChangeMyPositionModeMessage::EChangeType;
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ChangeMyPositionModeMessage>(Mode::StopFollowing),
                                  MessagePriority::Normal);
}

void DrapeEngine::FollowRoute(int preferredZoomLevel, int preferredZoomLevel3d, bool enableAutoZoom,
                              bool isArrowGlued)
{
  m_threadCommutator->PostMessage(
      ThreadsCommutator::RenderThread,
      make_unique_dp<FollowRouteMessage>(preferredZoomLevel, preferredZoomLevel3d, enableAutoZoom,
                                         isArrowGlued),
      MessagePriority::Normal);
}

void DrapeEngine::SetModelViewListener(ModelViewChangedHandler && fn)
{
  m_modelViewChangedHandler = std::move(fn);
}

#if defined(OMIM_OS_DESKTOP)
void DrapeEngine::NotifyGraphicsReady(GraphicsReadyHandler const & fn, bool needInvalidate)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<NotifyGraphicsReadyMessage>(fn, needInvalidate),
                                  MessagePriority::Normal);
}
#endif

void DrapeEngine::SetTapEventInfoListener(TapEventInfoHandler && fn)
{
  m_tapEventInfoHandler = std::move(fn);
}

void DrapeEngine::SetUserPositionListener(UserPositionChangedHandler && fn)
{
  m_userPositionChangedHandler = std::move(fn);
}

void DrapeEngine::SelectObject(SelectionShape::ESelectedObject obj, m2::PointD const & pt,
                               FeatureID const & featureId, bool isAnim,
                               bool isGeometrySelectionAllowed, bool isSelectionShapeVisible)
{
  m_threadCommutator->PostMessage(
      ThreadsCommutator::RenderThread,
      make_unique_dp<SelectObjectMessage>(obj, pt, featureId, isAnim, isGeometrySelectionAllowed,
                                          isSelectionShapeVisible),
      MessagePriority::Normal);
}

void DrapeEngine::DeselectObject()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SelectObjectMessage>(SelectObjectMessage::DismissTag()),
                                  MessagePriority::Normal);
}

dp::DrapeID DrapeEngine::AddSubroute(SubrouteConstPtr subroute)
{
  dp::DrapeID const id = GenerateDrapeID();
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<AddSubrouteMessage>(id, subroute),
                                  MessagePriority::Normal);
  return id;
}

void DrapeEngine::RemoveSubroute(dp::DrapeID subrouteId, bool deactivateFollowing)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<RemoveSubrouteMessage>(subrouteId, deactivateFollowing),
                                  MessagePriority::Normal);
}

void DrapeEngine::DeactivateRouteFollowing()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<DeactivateRouteFollowingMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetSubrouteVisibility(dp::DrapeID subrouteId, bool isVisible)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetSubrouteVisibilityMessage>(subrouteId, isVisible),
                                  MessagePriority::Normal);
}

dp::DrapeID DrapeEngine::AddRoutePreviewSegment(m2::PointD const & startPt, m2::PointD const & finishPt)
{
  dp::DrapeID const id = GenerateDrapeID();
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<AddRoutePreviewSegmentMessage>(id, startPt, finishPt),
                                  MessagePriority::Normal);
  return id;
}

void DrapeEngine::RemoveRoutePreviewSegment(dp::DrapeID segmentId)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<RemoveRoutePreviewSegmentMessage>(segmentId),
                                  MessagePriority::Normal);
}

void DrapeEngine::RemoveAllRoutePreviewSegments()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<RemoveRoutePreviewSegmentMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetWidgetLayout(gui::TWidgetsLayoutInfo && info)
{
  m_widgetsLayout = std::move(info);
  for (auto const & layout : m_widgetsLayout)
  {
    auto const itInfo = m_widgetsInfo.find(layout.first);
    if (itInfo != m_widgetsInfo.end())
      itInfo->second.m_pixelPivot = layout.second;
  }
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<GuiLayerLayoutMessage>(m_widgetsLayout),
                                  MessagePriority::Normal);
}

void DrapeEngine::AllowAutoZoom(bool allowAutoZoom)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<AllowAutoZoomMessage>(allowAutoZoom),
                                  MessagePriority::Normal);
}

void DrapeEngine::Allow3dMode(bool allowPerspectiveInNavigation, bool allow3dBuildings)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<Allow3dBuildingsMessage>(allow3dBuildings),
                                  MessagePriority::Normal);

  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<Allow3dModeMessage>(allowPerspectiveInNavigation,
                                                                     allow3dBuildings),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetMapLangIndex(int8_t mapLangIndex)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<SetMapLangIndexMessage>(mapLangIndex),
                                  MessagePriority::Normal);

  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetMapLangIndexMessage>(mapLangIndex),
                                  MessagePriority::Normal);
}

void DrapeEngine::EnablePerspective()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<EnablePerspectiveMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::UpdateGpsTrackPoints(std::vector<df::GpsTrackPoint> && toAdd,
                                       std::vector<uint32_t> && toRemove)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<UpdateGpsTrackPointsMessage>(std::move(toAdd),
                                                                              std::move(toRemove)),
                                  MessagePriority::Normal);
}

void DrapeEngine::ClearGpsTrackPoints()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ClearGpsTrackPointsMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::EnableChoosePositionMode(bool enable, std::vector<m2::TriangleD> && boundAreaTriangles,
                                           m2::PointD const * optionalPosition)
{
  m_choosePositionMode = enable;
  bool kineticScroll = m_kineticScrollEnabled;
  if (enable)
  {
    StopLocationFollow();
    m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                    make_unique_dp<ShowChoosePositionMarkMessage>(),
                                    MessagePriority::Normal);
    kineticScroll = false;
  }
  else
  {
    RecacheGui(true);
  }
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetAddNewPlaceModeMessage>(enable, std::move(boundAreaTriangles),
                                                                            kineticScroll, optionalPosition),
                                  MessagePriority::Normal);
}

void DrapeEngine::BlockTapEvents(bool block)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<BlockTapEventsMessage>(block),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetKineticScrollEnabled(bool enabled)
{
  m_kineticScrollEnabled = enabled;
  if (m_choosePositionMode)
    return;

  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetKineticScrollEnabledMessage>(m_kineticScrollEnabled),
                                  MessagePriority::Normal);
}

void DrapeEngine::OnEnterForeground()
{
  double const backgroundTime = base::Timer::LocalTime() - m_startBackgroundTime;
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<OnEnterForegroundMessage>(backgroundTime),
                                  MessagePriority::High);
}

void DrapeEngine::OnEnterBackground()
{
  m_startBackgroundTime = base::Timer::LocalTime();
  settings::Set(kLastEnterBackground, m_startBackgroundTime);

  /// @todo By VNG: Make direct call to FR, because logic with PostMessage is not working now.
  /// Rendering engine becomes disabled first and posted message won't be processed in a correct timing
  /// and will remain pending in queue, waiting until rendering queue will became active.
  /// As a result, we will get OnEnterBackground notification when we already entered foreground (sic!).
  /// One minus with direct call is that we are not in FR rendering thread, but I don't see a problem here now.
  /// To make it works as expected with PostMessage, we should refactor platform notifications,
  /// especially Android with its AppBackgroundTracker.
  m_frontend->OnEnterBackground();

//  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
//                                  make_unique_dp<OnEnterBackgroundMessage>(),
//                                  MessagePriority::High);
}

void DrapeEngine::RequestSymbolsSize(std::vector<std::string> const & symbols,
                                     TRequestSymbolsSizeCallback const & callback)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<RequestSymbolsSizeMessage>(symbols, callback),
                                  MessagePriority::Normal);
}

void DrapeEngine::EnableTraffic(bool trafficEnabled)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<EnableTrafficMessage>(trafficEnabled),
                                  MessagePriority::Normal);
}

void DrapeEngine::UpdateTraffic(traffic::TrafficInfo const & info)
{
  if (info.GetColoring().empty())
    return;

#ifdef DEBUG
  for (auto const & segmentPair : info.GetColoring())
    ASSERT_NOT_EQUAL(segmentPair.second, traffic::SpeedGroup::Unknown, ());
#endif

  df::TrafficSegmentsColoring segmentsColoring;
  segmentsColoring.emplace(info.GetMwmId(), info.GetColoring());

  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<UpdateTrafficMessage>(std::move(segmentsColoring)),
                                  MessagePriority::Normal);
}

void DrapeEngine::ClearTrafficCache(MwmSet::MwmId const & mwmId)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<ClearTrafficDataMessage>(mwmId),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetSimplifiedTrafficColors(bool simplified)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<SetSimplifiedTrafficColorsMessage>(simplified),
                                  MessagePriority::Normal);
}

void DrapeEngine::EnableTransitScheme(bool enable)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<EnableTransitSchemeMessage>(enable),
                                  MessagePriority::Normal);
}

void DrapeEngine::ClearTransitSchemeCache(MwmSet::MwmId const & mwmId)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<ClearTransitSchemeDataMessage>(mwmId),
                                  MessagePriority::Normal);
}

void DrapeEngine::ClearAllTransitSchemeCache()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<ClearAllTransitSchemeDataMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::UpdateTransitScheme(TransitDisplayInfos && transitDisplayInfos)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<UpdateTransitSchemeMessage>(std::move(transitDisplayInfos)),
                                  MessagePriority::Normal);
}

void DrapeEngine::EnableIsolines(bool enable)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<EnableIsolinesMessage>(enable),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetFontScaleFactor(double scaleFactor)
{
  VisualParams::Instance().SetFontScale(scaleFactor);
}

void DrapeEngine::RunScenario(ScenarioManager::ScenarioData && scenarioData,
                              ScenarioManager::ScenarioCallback const & onStartFn,
                              ScenarioManager::ScenarioCallback const & onFinishFn)
{
  auto const & manager = m_frontend->GetScenarioManager();
  if (manager != nullptr)
    manager->RunScenario(std::move(scenarioData), onStartFn, onFinishFn);
}

void DrapeEngine::SetCustomFeatures(df::CustomFeatures && ids)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<SetCustomFeaturesMessage>(std::move(ids)),
                                  MessagePriority::Normal);
}

void DrapeEngine::RemoveCustomFeatures(MwmSet::MwmId const & mwmId)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<RemoveCustomFeaturesMessage>(mwmId),
                                  MessagePriority::Normal);
}

void DrapeEngine::RemoveAllCustomFeatures()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<RemoveCustomFeaturesMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetPosteffectEnabled(PostprocessRenderer::Effect effect, bool enabled)
{
  if (effect == df::PostprocessRenderer::Antialiasing)
  {
    LOG(LINFO, ("Antialiasing is", (enabled ? "enabled" : "disabled")));
    settings::Set(dp::kSupportedAntialiasing, enabled);
  }

  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<SetPosteffectEnabledMessage>(effect, enabled),
                                  MessagePriority::Normal);
}

void DrapeEngine::RunFirstLaunchAnimation()
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<RunFirstLaunchAnimationMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::ShowDebugInfo(bool shown)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<ShowDebugInfoMessage>(shown),
                                  MessagePriority::Normal);
}

void DrapeEngine::EnableDebugRectRendering(bool enabled)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<EnableDebugRectRenderingMessage>(enabled),
                                  MessagePriority::Normal);
}

drape_ptr<UserMarkRenderParams> DrapeEngine::GenerateMarkRenderInfo(UserPointMark const * mark)
{
  auto renderInfo = make_unique_dp<UserMarkRenderParams>();
  renderInfo->m_markId = mark->GetId();
  renderInfo->m_anchor = mark->GetAnchor();
  renderInfo->m_depthTestEnabled = mark->GetDepthTestEnabled();
  if (mark->GetDepth() != UserPointMark::kInvalidDepth)
  {
    renderInfo->m_depth = mark->GetDepth();
    renderInfo->m_customDepth = true;
  }
  renderInfo->m_depthLayer = mark->GetDepthLayer();
  renderInfo->m_minZoom = mark->GetMinZoom();
  renderInfo->m_minTitleZoom = mark->GetMinTitleZoom();
  renderInfo->m_isVisible = mark->IsVisible();
  renderInfo->m_pivot = mark->GetPivot();
  renderInfo->m_pixelOffset = mark->GetPixelOffset();
  renderInfo->m_titleDecl = mark->GetTitleDecl();
  renderInfo->m_symbolNames = mark->GetSymbolNames();
  renderInfo->m_coloredSymbols = mark->GetColoredSymbols();
  renderInfo->m_symbolSizes = mark->GetSymbolSizes();
  renderInfo->m_symbolOffsets = mark->GetSymbolOffsets();
  renderInfo->m_color = mark->GetColorConstant();
  renderInfo->m_symbolIsPOI = mark->SymbolIsPOI();
  renderInfo->m_hasTitlePriority = mark->HasTitlePriority();
  renderInfo->m_priority = mark->GetPriority();
  renderInfo->m_displacement = mark->GetDisplacement();
  renderInfo->m_index = mark->GetIndex();
  renderInfo->m_featureId = mark->GetFeatureID();
  renderInfo->m_hasCreationAnimation = mark->HasCreationAnimation();
  renderInfo->m_isMarkAboveText = mark->IsMarkAboveText();
  renderInfo->m_symbolOpacity = mark->GetSymbolOpacity();
  renderInfo->m_isSymbolSelectable = mark->IsSymbolSelectable();
  renderInfo->m_isNonDisplaceable = mark->IsNonDisplaceable();
  return renderInfo;
}

drape_ptr<UserLineRenderParams> DrapeEngine::GenerateLineRenderInfo(UserLineMark const * mark)
{
  auto renderInfo = make_unique_dp<UserLineRenderParams>();
  renderInfo->m_minZoom = mark->GetMinZoom();
  renderInfo->m_depthLayer = mark->GetDepthLayer();

  mark->ForEachGeometry([&renderInfo](std::vector<m2::PointD> && points)
  {
    renderInfo->m_splines.emplace_back(std::move(points));
  });

  renderInfo->m_layers.reserve(mark->GetLayerCount());
  for (size_t layerIndex = 0, layersCount = mark->GetLayerCount(); layerIndex < layersCount; ++layerIndex)
  {
    renderInfo->m_layers.emplace_back(mark->GetColor(layerIndex),
                                      mark->GetWidth(layerIndex),
                                      mark->GetDepth(layerIndex));
  }
  return renderInfo;
}

void DrapeEngine::UpdateVisualScale(double vs, bool needStopRendering)
{
  if (needStopRendering)
    SetRenderingDisabled(true /* destroySurface */);

  VisualParams::Instance().SetVisualScale(vs);

  if (needStopRendering)
    SetRenderingEnabled();

  RecacheGui(false);
  RecacheMapShapes();
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<RecoverContextDependentResourcesMessage>(),
                                  MessagePriority::Normal);
}

void DrapeEngine::UpdateMyPositionRoutingOffset(bool useDefault, int offsetY)
{
  m_threadCommutator->PostMessage(ThreadsCommutator::RenderThread,
                                  make_unique_dp<UpdateMyPositionRoutingOffsetMessage>(useDefault, offsetY),
                                  MessagePriority::Normal);
}

void DrapeEngine::SetCustomArrow3d(std::optional<Arrow3dCustomDecl> arrow3dCustomDecl)
{
  m_threadCommutator->PostMessage(
      ThreadsCommutator::ResourceUploadThread,
      make_unique_dp<Arrow3dRecacheMessage>(std::move(arrow3dCustomDecl)), MessagePriority::High);
}
}  // namespace df
