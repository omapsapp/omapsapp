#include "drape_frontend/frontend_renderer.hpp"
#include "drape_frontend/animation/interpolation_holder.hpp"
#include "drape_frontend/animation_system.hpp"
#include "drape_frontend/debug_rect_renderer.hpp"
#include "drape_frontend/drape_measurer.hpp"
#include "drape_frontend/drape_notifier.hpp"
#include "drape_frontend/gui/drape_gui.hpp"
#include "drape_frontend/gui/ruler_helper.hpp"
#include "drape_frontend/message_subclasses.hpp"
#include "drape_frontend/postprocess_renderer.hpp"
#include "drape_frontend/scenario_manager.hpp"
#include "drape_frontend/screen_operations.hpp"
#include "drape_frontend/screen_quad_renderer.hpp"
#include "drape_frontend/user_mark_shapes.hpp"
#include "drape_frontend/visual_params.hpp"

#include "shaders/programs.hpp"

#include "drape/constants.hpp"
#include "drape/drape_global.hpp"
#include "drape/framebuffer.hpp"
#include "drape/support_manager.hpp"
#include "drape/utils/projection.hpp"

#include "indexer/drawing_rules.hpp"
#include "indexer/scales.hpp"

#include "geometry/any_rect2d.hpp"

#include "platform/trace.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"
#include "base/timer.hpp"

#include "std/target_os.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <thread>
#include <utility>


namespace df
{
using namespace std::placeholders;

namespace
{
float constexpr kIsometryAngle = static_cast<float>(math::pi) * 76.0f / 180.0f;
double constexpr kVSyncInterval = 0.06;
// Metal/Vulkan rendering is fast, so we can decrease sync inverval.
double constexpr kVSyncIntervalMetalVulkan = 0.03;

std::string const kTransitBackgroundColor = "TransitBackground";

template <typename ToDo>
bool RemoveGroups(ToDo && filter, std::vector<drape_ptr<RenderGroup>> & groups,
                  ref_ptr<dp::OverlayTree> tree)
{
  auto const itEnd = groups.end();
  auto it = std::remove_if(groups.begin(), itEnd, [&filter, tree](drape_ptr<RenderGroup> & group)
  {
    if (filter(group))
    {
      group->RemoveOverlay(tree);
      return true;
    }
    return false;
  });

  if (it != itEnd)
  {
    groups.erase(it, itEnd);
    return true;
  }
  return false;
}

class DebugLabelGuard
{
public:
  DebugLabelGuard(ref_ptr<dp::GraphicsContext> context, std::string const & label)
    : m_context(context)
  {
    m_context->PushDebugLabel(label);
  }

  ~DebugLabelGuard()
  {
    m_context->PopDebugLabel();
  }

private:
  ref_ptr<dp::GraphicsContext> m_context;
};

#if defined(DEBUG) || defined(DEBUG_DRAPE_XCODE)
#define DEBUG_LABEL(context, labelText) DebugLabelGuard __debugLabel(context, labelText);
#else
#define DEBUG_LABEL(context, labelText)
#endif

class DrapeMeasurerGuard
{
public:
  DrapeMeasurerGuard()
  {
    DrapeMeasurer::Instance().BeforeRenderFrame();
  }

  ~DrapeMeasurerGuard()
  {
    DrapeMeasurer::Instance().AfterRenderFrame(m_isActiveFrame, m_viewportCenter);
  }

  void SetProperties(bool isActiveFrame, m2::PointD const & viewportCenter)
  {
    m_isActiveFrame = isActiveFrame;
    m_viewportCenter = viewportCenter;
  }

private:
  bool m_isActiveFrame = false;
  m2::PointD m_viewportCenter = m2::PointD::Zero();
};

#if defined(DRAPE_MEASURER_BENCHMARK) && (defined(RENDER_STATISTIC) || defined(TRACK_GPU_MEM))
class DrapeImmediateRenderingMeasurerGuard
{
public:
  DrapeImmediateRenderingMeasurerGuard(ref_ptr<dp::GraphicsContext> context)
    : m_context(context)
  {
    DrapeMeasurer::Instance().BeforeImmediateRendering();
  }

  ~DrapeImmediateRenderingMeasurerGuard()
  {
    DrapeMeasurer::Instance().AfterImmediateRendering();
#ifdef DISABLE_SCREEN_PRESENTATION
    m_context->DebugSynchronizeWithCPU();
#endif
  }

private:
  ref_ptr<dp::GraphicsContext> m_context;
};
#endif
}  // namespace

FrontendRenderer::FrontendRenderer(Params && params)
  : BaseRenderer(ThreadsCommutator::RenderThread, params)
  , m_trafficRenderer(new TrafficRenderer())
  , m_transitSchemeRenderer(new TransitSchemeRenderer())
  , m_drapeApiRenderer(new DrapeApiRenderer())
  , m_overlayTree(new dp::OverlayTree(VisualParams::Instance().GetVisualScale()))
  , m_enablePerspectiveInNavigation(false)
  , m_enable3dBuildings(params.m_allow3dBuildings)
  , m_isIsometry(false)
  , m_blockTapEvents(params.m_blockTapEvents)
  , m_choosePositionMode(false)
  , m_screenshotMode(params.m_myPositionParams.m_hints.m_screenshotMode)
  , m_mapLangIndex(StringUtf8Multilang::kDefaultCode)
  , m_viewport(params.m_viewport)
  , m_modelViewChangedHandler(std::move(params.m_modelViewChangedHandler))
  , m_tapEventInfoHandler(std::move(params.m_tapEventHandler))
  , m_userPositionChangedHandler(std::move(params.m_positionChangedHandler))
  , m_requestedTiles(params.m_requestedTiles)
  , m_maxGeneration(0)
  , m_maxUserMarksGeneration(0)
  , m_needRestoreSize(false)
  , m_trafficEnabled(params.m_trafficEnabled)
  , m_overlaysTracker(new OverlaysTracker())
  , m_overlaysShowStatsCallback(std::move(params.m_overlaysShowStatsCallback))
  , m_forceUpdateScene(false)
  , m_forceUpdateUserMarks(false)
  , m_postprocessRenderer(new PostprocessRenderer())
  , m_enabledOnStartEffects(params.m_enabledEffects)
#ifdef SCENARIO_ENABLE
  , m_scenarioManager(new ScenarioManager(this))
#endif
  , m_notifier(make_unique_dp<DrapeNotifier>(params.m_commutator))
  , m_renderInjectionHandler(std::move(params.m_renderInjectionHandler))
{
#ifdef DEBUG
  m_isTeardowned = false;
#endif

  ASSERT(m_modelViewChangedHandler, ());
  ASSERT(m_tapEventInfoHandler, ());
  ASSERT(m_userPositionChangedHandler, ());

  m_gpsTrackRenderer = make_unique_dp<GpsTrackRenderer>([this](uint32_t pointsCount)
  {
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                              make_unique_dp<CacheCirclesPackMessage>(
                                pointsCount, CacheCirclesPackMessage::GpsTrack),
                              MessagePriority::Normal);
  });

  m_routeRenderer = make_unique_dp<RouteRenderer>([this](uint32_t pointsCount)
  {
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                              make_unique_dp<CacheCirclesPackMessage>(
                                pointsCount, CacheCirclesPackMessage::RoutePreview),
                              MessagePriority::Normal);
  });

  m_myPositionController = make_unique_dp<MyPositionController>(
      std::move(params.m_myPositionParams), make_ref(m_notifier));

  StartThread();
}

FrontendRenderer::~FrontendRenderer()
{
  ASSERT(m_isTeardowned, ());
}

void FrontendRenderer::Teardown()
{
  m_scenarioManager.reset();
  StopThread();
#ifdef DEBUG
  m_isTeardowned = true;
#endif
}

void FrontendRenderer::UpdateCanBeDeletedStatus()
{
  m2::RectD const & screenRect = m_userEventStream.GetCurrentScreen().ClipRect();

  std::vector<m2::RectD> notFinishedTileRects;
  notFinishedTileRects.reserve(m_notFinishedTiles.size());
  for (auto const & tileKey : m_notFinishedTiles)
    notFinishedTileRects.push_back(tileKey.GetGlobalRect());

  for (RenderLayer & layer : m_layers)
  {
    for (auto & group : layer.m_renderGroups)
    {
      if (!group->IsPendingOnDelete())
        continue;

      bool canBeDeleted = true;
      if (!notFinishedTileRects.empty())
      {
        m2::RectD const tileRect = group->GetTileKey().GetGlobalRect();
        if (tileRect.IsIntersect(screenRect))
          canBeDeleted = !HasIntersection(tileRect, notFinishedTileRects);
      }
      layer.m_isDirty |= group->UpdateCanBeDeletedStatus(canBeDeleted, GetCurrentZoom(),
                                                         make_ref(m_overlayTree));
    }
  }
}

void FrontendRenderer::AcceptMessage(ref_ptr<Message> message)
{
  switch (message->GetType())
  {
  case Message::Type::FlushTile:
    {
      ref_ptr<FlushRenderBucketMessage> msg = message;
      dp::RenderState const & state = msg->GetState();
      TileKey const & key = msg->GetKey();
      drape_ptr<dp::RenderBucket> bucket = msg->AcceptBuffer();
      if (key.m_zoomLevel == GetCurrentZoom() && CheckTileGenerations(key))
      {
        PrepareBucket(state, bucket);
        AddToRenderGroup<RenderGroup>(state, std::move(bucket), key);
      }
      break;
    }

  case Message::Type::FlushOverlays:
    {
      ref_ptr<FlushOverlaysMessage> msg = message;
      TOverlaysRenderData renderData = msg->AcceptRenderData();
      for (auto & overlayRenderData : renderData)
      {
        if (overlayRenderData.m_tileKey.m_zoomLevel == GetCurrentZoom() &&
            CheckTileGenerations(overlayRenderData.m_tileKey))
        {
          PrepareBucket(overlayRenderData.m_state, overlayRenderData.m_bucket);
          AddToRenderGroup<RenderGroup>(overlayRenderData.m_state, std::move(overlayRenderData.m_bucket),
                                        overlayRenderData.m_tileKey);
        }
      }
      UpdateCanBeDeletedStatus();

      m_firstTilesReady = true;
      if (m_firstLaunchAnimationTriggered)
      {
        CheckAndRunFirstLaunchAnimation();
        m_firstLaunchAnimationTriggered = false;
      }
      break;
    }

  case Message::Type::FinishTileRead:
    {
      ref_ptr<FinishTileReadMessage> msg = message;
      bool changed = false;
      for (auto const & tileKey : msg->GetTiles())
      {
        if (CheckTileGenerations(tileKey))
        {
          auto it = m_notFinishedTiles.find(tileKey);
          if (it != m_notFinishedTiles.end())
          {
            m_notFinishedTiles.erase(it);
            changed = true;
          }
        }
      }
      if (changed || m_notFinishedTiles.empty())
        UpdateCanBeDeletedStatus();

      if (m_notFinishedTiles.empty())
      {
#if defined(DRAPE_MEASURER_BENCHMARK) && defined(GENERATING_STATISTIC)
        DrapeMeasurer::Instance().EndScenePreparing();
#endif
        m_trafficRenderer->OnGeometryReady(GetCurrentZoom());

#if defined(OMIM_OS_DESKTOP)
        if (m_graphicsStage == GraphicsStage::WaitReady)
          m_graphicsStage = GraphicsStage::WaitRendering;
#endif
      }
      break;
    }

  case Message::Type::InvalidateRect:
    {
      ref_ptr<InvalidateRectMessage> m = message;
      InvalidateRect(m->GetRect());
      break;
    }

  case Message::Type::FlushUserMarks:
    {
      ref_ptr<FlushUserMarksMessage> msg = message;
      TUserMarksRenderData marksRenderData = msg->AcceptRenderData();
      for (auto & renderData : marksRenderData)
      {
        if (renderData.m_tileKey.m_zoomLevel == GetCurrentZoom() &&
            CheckTileGenerations(renderData.m_tileKey))
        {
          PrepareBucket(renderData.m_state, renderData.m_bucket);
          AddToRenderGroup<UserMarkRenderGroup>(renderData.m_state, std::move(renderData.m_bucket),
                                                renderData.m_tileKey);
        }
      }
      break;
    }

  case Message::Type::GuiLayerRecached:
    {
      ref_ptr<GuiLayerRecachedMessage> msg = message;
      CHECK(m_context != nullptr, ());
      drape_ptr<gui::LayerRenderer> renderer = std::move(msg->AcceptRenderer());
      renderer->Build(m_context, make_ref(m_gpuProgramManager));
      if (msg->NeedResetOldGui())
        m_guiRenderer.reset();
      if (m_guiRenderer == nullptr)
        m_guiRenderer = std::move(renderer);
      else
        m_guiRenderer->Merge(make_ref(renderer));

      CHECK(m_guiRenderer != nullptr, ());
      m_guiRenderer->SetLayout(m_lastWidgetsLayout);

      bool oldMode = m_choosePositionMode;
      m_choosePositionMode = m_guiRenderer->HasWidget(gui::WIDGET_CHOOSE_POSITION_MARK);
      if (oldMode != m_choosePositionMode)
      {
        ScreenBase const & screen = m_userEventStream.GetCurrentScreen();
        CheckIsometryMinScale(screen);
        UpdateDisplacementEnabled();
        m_forceUpdateScene = true;
      }

      if (m_guiRenderer->HasWidget(gui::WIDGET_RULER))
        gui::DrapeGui::GetRulerHelper().Invalidate();
      break;
    }

  case Message::Type::GuiLayerLayout:
    {
      m_lastWidgetsLayout = ref_ptr<GuiLayerLayoutMessage>(message)->GetLayoutInfo();
      if (m_guiRenderer != nullptr)
        m_guiRenderer->SetLayout(m_lastWidgetsLayout);
      break;
    }

  case Message::Type::UpdateMyPositionRoutingOffset:
    {
      ref_ptr<UpdateMyPositionRoutingOffsetMessage> msg = message;
      m_myPositionController->UpdateRoutingOffsetY(msg->UseDefault(), msg->GetOffsetY());
      m_myPositionController->UpdatePosition();
      break;
    }

  case Message::Type::MapShapes:
    {
      ref_ptr<MapShapesMessage> msg = message;
      CHECK(m_context != nullptr, ());
      m_texMng->ApplyInvalidatedStaticTextures();
      m_myPositionController->SetRenderShape(m_context, m_texMng, msg->AcceptShape(),
                                             msg->AcceptPeloadedArrow3dData());
      m_selectionShape = msg->AcceptSelection();
      if (m_selectObjectMessage != nullptr)
      {
        ProcessSelection(make_ref(m_selectObjectMessage));
        m_selectObjectMessage.reset();
        AddUserEvent(make_unique_dp<SetVisibleViewportEvent>(m_userEventStream.GetVisibleViewport()));
      }
      break;
    }

  case Message::Type::ChangeMyPositionMode:
    {
      ref_ptr<ChangeMyPositionModeMessage> msg = message;
      switch (msg->GetChangeType())
      {
      case ChangeMyPositionModeMessage::SwitchNextMode:
        m_myPositionController->NextMode(m_userEventStream.GetCurrentScreen());
        break;
      case ChangeMyPositionModeMessage::StopFollowing:
        m_myPositionController->StopLocationFollow();
        break;
      case ChangeMyPositionModeMessage::LoseLocation:
        m_myPositionController->LoseLocation();
        break;
      default:
        ASSERT(false, ("Unknown change type:", static_cast<int>(msg->GetChangeType())));
        break;
      }
      break;
    }

  case Message::Type::CompassInfo:
    {
      ref_ptr<CompassInfoMessage> msg = message;
      m_myPositionController->OnCompassUpdate(msg->GetInfo(), m_userEventStream.GetCurrentScreen());
      break;
    }

  case Message::Type::GpsInfo:
    {
#ifdef SCENARIO_ENABLE
      if (m_scenarioManager->IsRunning())
        break;
#endif
      ref_ptr<GpsInfoMessage> msg = message;
      m_myPositionController->OnLocationUpdate(msg->GetInfo(), msg->IsNavigable(),
                                               m_userEventStream.GetCurrentScreen());

      location::RouteMatchingInfo const & info = msg->GetRouteInfo();
      if (info.HasDistanceFromBegin())
      {
        m_routeRenderer->UpdateDistanceFromBegin(info.GetDistanceFromBegin());
        // Here we have to recache route arrows.
        m_routeRenderer->PrepareRouteArrows(m_userEventStream.GetCurrentScreen(),
                                            std::bind(&FrontendRenderer::OnPrepareRouteArrows, this, _1, _2));
      }

      break;
    }

  case Message::Type::SelectObject:
    {
      ref_ptr<SelectObjectMessage> msg = message;
      m_overlayTree->SetSelectedFeature(msg->IsDismiss() ? FeatureID() : msg->GetFeatureID());
      if (m_selectionShape == nullptr)
      {
        m_selectObjectMessage = make_unique_dp<SelectObjectMessage>(
            msg->GetSelectedObject(), msg->GetPosition(), msg->GetFeatureID(), msg->IsAnim(),
            msg->IsGeometrySelectionAllowed(), true /* isSelectionShapeVisible */);
      }
      else
      {
        ProcessSelection(msg);
        AddUserEvent(make_unique_dp<SetVisibleViewportEvent>(m_userEventStream.GetVisibleViewport()));
      }
      break;
    }

  case Message::Type::FlushSelectionGeometry:
    {
      ref_ptr<FlushSelectionGeometryMessage> msg = message;
      if (m_selectionShape != nullptr)
      {
        m_selectionShape->AddSelectionGeometry(msg->AcceptRenderData(), msg->GetRecacheId());
        if (m_selectionShape->HasSelectionGeometry())
        {
          auto bbox = m_selectionShape->GetSelectionGeometryBoundingBox();
          CHECK(bbox.IsValid(), ());
          bbox.Scale(kBoundingBoxScale);
          AddUserEvent(make_unique_dp<SetRectEvent>(bbox, true /* rotate */, kDoNotChangeZoom,
                                                    true /* isAnim */, true /* useVisibleViewport */,
                                                    nullptr /* parallelAnimCreator */));
        }
      }
      break;
    }

  case Message::Type::FlushSubroute:
    {
      ref_ptr<FlushSubrouteMessage> msg = message;
      auto subrouteData = msg->AcceptRenderData();

      if (subrouteData->m_recacheId < 0)
        subrouteData->m_recacheId = m_lastRecacheRouteId;

      if (!CheckRouteRecaching(make_ref(subrouteData)))
        break;

      m_routeRenderer->ClearObsoleteData(m_lastRecacheRouteId);

      CHECK(m_context != nullptr, ());
      m_routeRenderer->AddSubrouteData(m_context, std::move(subrouteData), make_ref(m_gpuProgramManager));

      // Here we have to recache route arrows.
      m_routeRenderer->PrepareRouteArrows(m_userEventStream.GetCurrentScreen(),
                                          std::bind(&FrontendRenderer::OnPrepareRouteArrows, this, _1, _2));

      if (m_pendingFollowRoute != nullptr)
      {
        FollowRoute(m_pendingFollowRoute->m_preferredZoomLevel,
                    m_pendingFollowRoute->m_preferredZoomLevelIn3d,
                    m_pendingFollowRoute->m_enableAutoZoom, m_pendingFollowRoute->m_isArrowGlued);
        m_pendingFollowRoute.reset();
      }
      break;
    }

  case Message::Type::FlushTransitScheme:
    {
      ref_ptr<FlushTransitSchemeMessage > msg = message;
      auto renderData = msg->AcceptRenderData();
      CHECK(m_context != nullptr, ());
      m_transitSchemeRenderer->AddRenderData(m_context, make_ref(m_gpuProgramManager),
                                             make_ref(m_overlayTree), std::move(renderData));
      break;
    }

  case Message::Type::FlushSubrouteArrows:
    {
      ref_ptr<FlushSubrouteArrowsMessage> msg = message;
      drape_ptr<SubrouteArrowsData> routeArrowsData = msg->AcceptRenderData();
      if (CheckRouteRecaching(make_ref(routeArrowsData)))
      {
        CHECK(m_context != nullptr, ());
        m_routeRenderer->AddSubrouteArrowsData(m_context, std::move(routeArrowsData),
                                               make_ref(m_gpuProgramManager));
      }
      break;
    }

  case Message::Type::FlushSubrouteMarkers:
    {
      ref_ptr<FlushSubrouteMarkersMessage> msg = message;
      drape_ptr<SubrouteMarkersData> markersData = msg->AcceptRenderData();
      if (markersData->m_recacheId < 0)
        markersData->m_recacheId = m_lastRecacheRouteId;

      if (CheckRouteRecaching(make_ref(markersData)))
      {
        CHECK(m_context != nullptr, ());
        m_routeRenderer->AddSubrouteMarkersData(m_context, std::move(markersData),
                                                make_ref(m_gpuProgramManager));
      }
      break;
    }

  case Message::Type::RemoveSubroute:
    {
      ref_ptr<RemoveSubrouteMessage> msg = message;
      if (msg->NeedDeactivateFollowing())
      {
        CHECK(m_context != nullptr, ());
        m_routeRenderer->SetFollowingEnabled(false);
        m_routeRenderer->Clear();
        ++m_lastRecacheRouteId;
        m_myPositionController->DeactivateRouting();
        m_postprocessRenderer->OnChangedRouteFollowingMode(m_context, false /* active */);
        if (m_enablePerspectiveInNavigation)
          DisablePerspective();
      }
      else
      {
        m_routeRenderer->RemoveSubrouteData(msg->GetSegmentId());
      }
      break;
    }

  case Message::Type::FollowRoute:
    {
      ref_ptr<FollowRouteMessage> const msg = message;

      // After night style switching or drape engine reinitialization FrontendRenderer can
      // receive FollowRoute message before FlushSubroute message, so we need to postpone its processing.
      if (m_routeRenderer->GetSubroutes().empty())
      {
        m_pendingFollowRoute = std::make_unique<FollowRouteData>(
            msg->GetPreferredZoomLevel(), msg->GetPreferredZoomLevelIn3d(), msg->EnableAutoZoom(),
            msg->IsArrowGlued());
      }
      else
      {
        FollowRoute(msg->GetPreferredZoomLevel(), msg->GetPreferredZoomLevelIn3d(),
                    msg->EnableAutoZoom(), msg->IsArrowGlued());
      }
      break;
    }

  case Message::Type::DeactivateRouteFollowing:
    {
      m_myPositionController->DeactivateRouting();
      CHECK(m_context != nullptr, ());
      m_postprocessRenderer->OnChangedRouteFollowingMode(m_context, false /* active */);
      if (m_enablePerspectiveInNavigation)
        DisablePerspective();
      break;
    }

  case Message::Type::AddRoutePreviewSegment:
    {
      ref_ptr<AddRoutePreviewSegmentMessage> const msg = message;
      RouteRenderer::PreviewInfo info;
      info.m_startPoint = msg->GetStartPoint();
      info.m_finishPoint = msg->GetFinishPoint();
      m_routeRenderer->AddPreviewSegment(msg->GetSegmentId(), std::move(info));
      break;
    }

  case Message::Type::RemoveRoutePreviewSegment:
    {
      ref_ptr<RemoveRoutePreviewSegmentMessage> const msg = message;
      if (msg->NeedRemoveAll())
        m_routeRenderer->RemoveAllPreviewSegments();
      else
        m_routeRenderer->RemovePreviewSegment(msg->GetSegmentId());
      break;
    }

  case Message::Type::SetSubrouteVisibility:
    {
      ref_ptr<SetSubrouteVisibilityMessage> const msg = message;
      m_routeRenderer->SetSubrouteVisibility(msg->GetSubrouteId(), msg->IsVisible());
      break;
    }

  case Message::Type::PrepareSubrouteArrows:
    {
      ref_ptr<PrepareSubrouteArrowsMessage> msg = message;
      m_routeRenderer->CacheRouteArrows(m_userEventStream.GetCurrentScreen(),
                                        msg->GetSubrouteId(), msg->AcceptBorders(),
                                        std::bind(&FrontendRenderer::OnCacheRouteArrows, this, _1, _2));
      break;
    }

  case Message::Type::RecoverContextDependentResources:
    {
      UpdateContextDependentResources();
      break;
    }

  case Message::Type::UpdateMapStyle:
    {
#ifdef BUILD_DESIGNER
      classificator::Load();
#endif // BUILD_DESIGNER

      // Clear all graphics.
      for (RenderLayer & layer : m_layers)
      {
        layer.m_renderGroups.clear();
        layer.m_isDirty = false;
      }

      // Must be recreated on map style changing.
      CHECK(m_context != nullptr, ());
      m_transitBackground = make_unique_dp<ScreenQuadRenderer>(m_context);

      // Invalidate read manager.
      {
        BaseBlockingMessage::Blocker blocker;
        m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<InvalidateReadManagerRectMessage>(blocker),
                                  MessagePriority::Normal);
        blocker.Wait();
      }

      // Delete all messages which can contain render states (and textures references inside).
      auto f = [this]()
      {
        InstantMessageFilter([](ref_ptr<Message> msg) { return msg->ContainsRenderState(); });
      };

      // Notify backend renderer and wait for completion.
      {
        BaseBlockingMessage::Blocker blocker;
        m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                  make_unique_dp<SwitchMapStyleMessage>(blocker, std::move(f)),
                                  MessagePriority::Normal);
        blocker.Wait();
      }

      UpdateContextDependentResources();
      break;
    }

  case Message::Type::AllowAutoZoom:
    {
      ref_ptr<AllowAutoZoomMessage> const msg = message;
      m_myPositionController->EnableAutoZoomInRouting(msg->AllowAutoZoom());
      break;
    }

  case Message::Type::EnablePerspective:
    {
      AddUserEvent(make_unique_dp<SetAutoPerspectiveEvent>(true /* isAutoPerspective */));
      break;
    }

  case Message::Type::Allow3dMode:
    {
      ref_ptr<Allow3dModeMessage> const msg = message;
      ScreenBase const & screen = m_userEventStream.GetCurrentScreen();

#ifdef OMIM_OS_DESKTOP
      if (m_enablePerspectiveInNavigation == msg->AllowPerspective() &&
          m_enablePerspectiveInNavigation != screen.isPerspective())
      {
        AddUserEvent(make_unique_dp<SetAutoPerspectiveEvent>(m_enablePerspectiveInNavigation));
      }
#endif

      if (m_enable3dBuildings != msg->Allow3dBuildings())
      {
        m_enable3dBuildings = msg->Allow3dBuildings();
        CheckIsometryMinScale(screen);
        m_forceUpdateScene = true;
      }

      if (m_enablePerspectiveInNavigation != msg->AllowPerspective())
      {
        m_enablePerspectiveInNavigation = msg->AllowPerspective();
        m_myPositionController->EnablePerspectiveInRouting(m_enablePerspectiveInNavigation);
        if (m_myPositionController->IsInRouting())
        {
          AddUserEvent(make_unique_dp<SetAutoPerspectiveEvent>(m_enablePerspectiveInNavigation));
        }
      }
      break;
    }
    case Message::Type::SetMapLangIndex:
    {
      ref_ptr<SetMapLangIndexMessage> const msg = message;

      if (m_mapLangIndex != msg->MapLangIndex())
      {
        m_mapLangIndex = msg->MapLangIndex();
        m_forceUpdateScene = true;
      }
      break;
    }
  case Message::Type::FlushCirclesPack:
    {
      ref_ptr<FlushCirclesPackMessage> msg = message;
      CHECK(m_context != nullptr, ());
      switch (msg->GetDestination())
      {
      case CacheCirclesPackMessage::GpsTrack:
        m_gpsTrackRenderer->AddRenderData(m_context, make_ref(m_gpuProgramManager), msg->AcceptRenderData());
        break;
      case CacheCirclesPackMessage::RoutePreview:
        m_routeRenderer->AddPreviewRenderData(m_context, msg->AcceptRenderData(), make_ref(m_gpuProgramManager));
        break;
      }
      break;
    }

  case Message::Type::UpdateGpsTrackPoints:
    {
      ref_ptr<UpdateGpsTrackPointsMessage> msg = message;
      m_gpsTrackRenderer->UpdatePoints(msg->GetPointsToAdd(), msg->GetPointsToRemove());
      break;
    }

  case Message::Type::ClearGpsTrackPoints:
    {
      m_gpsTrackRenderer->Clear();
      break;
    }

  case Message::Type::BlockTapEvents:
    {
      ref_ptr<BlockTapEventsMessage> msg = message;
      m_blockTapEvents = msg->NeedBlock();
      break;
    }

  case Message::Type::SetKineticScrollEnabled:
    {
      ref_ptr<SetKineticScrollEnabledMessage> msg = message;
      m_userEventStream.SetKineticScrollEnabled(msg->IsEnabled());
      break;
    }

  case Message::Type::OnEnterForeground:
    {
      ref_ptr<OnEnterForegroundMessage> msg = message;
      m_myPositionController->OnEnterForeground(msg->GetTime());
      break;
    }

//  case Message::Type::OnEnterBackground:
//    {
//      m_myPositionController->OnEnterBackground();
//      break;
//    }

  case Message::Type::SetAddNewPlaceMode:
    {
      ref_ptr<SetAddNewPlaceModeMessage> msg = message;
      m_userEventStream.SetKineticScrollEnabled(msg->IsKineticScrollEnabled());
      m_dragBoundArea = msg->AcceptBoundArea();
      if (msg->IsEnabled())
      {
        if (!m_dragBoundArea.empty())
        {
          PullToBoundArea(true /* randomPlace */, true /* applyZoom */);
        }
        else
        {
          // Exact position for POI or screen's center for Add place on map.
          int zoom = kDoNotChangeZoom;
          if (GetCurrentZoom() < scales::GetAddNewPlaceScale())
            zoom = scales::GetAddNewPlaceScale();

          auto const pt = msg->GetOptionalPosition();
          AddUserEvent(make_unique_dp<SetCenterEvent>(pt ? *pt : m_userEventStream.GetCurrentScreen().GlobalRect().Center(),
                                                      zoom, true /* isAnim */, false /* trackVisibleViewport */,
                                                      nullptr /* parallelAnimCreator */));
        }
      }
      break;
    }

  case Message::Type::SetVisibleViewport:
    {
      ref_ptr<SetVisibleViewportMessage> msg = message;
      AddUserEvent(make_unique_dp<SetVisibleViewportEvent>(msg->GetRect()));
      m_myPositionController->SetVisibleViewport(msg->GetRect());
      m_myPositionController->UpdatePosition();
      PullToBoundArea(false /* randomPlace */, false /* applyZoom */);
      break;
    }

  case Message::Type::Invalidate:
    {
      m_myPositionController->ResetRoutingNotFollowTimer();
      m_myPositionController->ResetBlockAutoZoomTimer();
      break;
    }

  case Message::Type::EnableTransitScheme:
    {
      ref_ptr<EnableTransitSchemeMessage > msg = message;
      m_transitSchemeEnabled = msg->IsEnabled();
      // Enabling anti aliasing destroys performance on some Android devices
      // Jagged lines on subway lines are only visible on low density screens
      // so we don't enable it here
      if (!msg->IsEnabled())
        m_transitSchemeRenderer->ClearContextDependentResources(make_ref(m_overlayTree));
      break;
    }

  case Message::Type::ClearTransitSchemeData:
    {
      ref_ptr<ClearTransitSchemeDataMessage> msg = message;
      m_transitSchemeRenderer->Clear(msg->GetMwmId(), make_ref(m_overlayTree));
      break;
    }

  case Message::Type::ClearAllTransitSchemeData:
    {
      m_transitSchemeRenderer->ClearContextDependentResources(make_ref(m_overlayTree));
      break;
    }

  case Message::Type::EnableTraffic:
    {
      ref_ptr<EnableTrafficMessage> msg = message;
      m_trafficEnabled = msg->IsTrafficEnabled();
      if (msg->IsTrafficEnabled())
        m_forceUpdateScene = true;
      else
        m_trafficRenderer->ClearContextDependentResources();
      break;
    }

  case Message::Type::RegenerateTraffic:
  case Message::Type::SetSimplifiedTrafficColors:
  case Message::Type::UpdateMetalines:
  case Message::Type::EnableIsolines:
    {
      m_forceUpdateScene = true;
      break;
    }

  case Message::Type::EnableDebugRectRendering:
    {
      ref_ptr<EnableDebugRectRenderingMessage> msg = message;
      m_isDebugRectRenderingEnabled = msg->IsEnabled();
      m_debugRectRenderer->SetEnabled(msg->IsEnabled());
    }
    break;

  case Message::Type::InvalidateUserMarks:
    {
      m_forceUpdateUserMarks = true;
      break;
    }

  case Message::Type::FlushTrafficData:
    {
      if (!m_trafficEnabled)
        break;
      ref_ptr<FlushTrafficDataMessage> msg = message;
      CHECK(m_context != nullptr, ());
      m_trafficRenderer->AddRenderData(m_context, make_ref(m_gpuProgramManager),
                                       msg->AcceptRenderData());
      break;
    }

  case Message::Type::ClearTrafficData:
    {
      ref_ptr<ClearTrafficDataMessage> msg = message;
      m_trafficRenderer->Clear(msg->GetMwmId());
      break;
    }

  case Message::Type::DrapeApiFlush:
    {
      ref_ptr<DrapeApiFlushMessage> msg = message;
      CHECK(m_context != nullptr, ());
      m_drapeApiRenderer->AddRenderProperties(m_context, make_ref(m_gpuProgramManager),
                                              msg->AcceptProperties());
      break;
    }

  case Message::Type::DrapeApiRemove:
    {
      ref_ptr<DrapeApiRemoveMessage> msg = message;
      if (msg->NeedRemoveAll())
        m_drapeApiRenderer->Clear();
      else
        m_drapeApiRenderer->RemoveRenderProperty(msg->GetId());
      break;
    }

  case Message::Type::SetTrackedFeatures:
    {
      ref_ptr<SetTrackedFeaturesMessage> msg = message;
      m_overlaysTracker->SetTrackedOverlaysFeatures(msg->AcceptFeatures());
      m_forceUpdateScene = true;
      break;
    }

  case Message::Type::SetPostprocessStaticTextures:
    {
      ref_ptr<SetPostprocessStaticTexturesMessage> msg = message;
      m_postprocessRenderer->SetStaticTextures(msg->AcceptTextures());
      break;
    }

  case Message::Type::SetPosteffectEnabled:
    {
      ref_ptr<SetPosteffectEnabledMessage> msg = message;
      // Enabling anti aliasing destroys performance on some Android devices
#ifndef OMIM_OS_IPHONE_SIMULATOR
      CHECK(m_context != nullptr, ());
      m_postprocessRenderer->SetEffectEnabled(m_context, msg->GetEffect(), msg->IsEnabled());
#endif
      break;
    }

  case Message::Type::RunFirstLaunchAnimation:
    {
      ref_ptr<RunFirstLaunchAnimationMessage> msg = message;
      m_firstLaunchAnimationTriggered = true;
      CheckAndRunFirstLaunchAnimation();
      break;
    }

  case Message::Type::PostUserEvent:
    {
      ref_ptr<PostUserEventMessage> msg = message;
      AddUserEvent(msg->AcceptEvent());
      break;
    }

  case Message::Type::FinishTexturesInitialization:
    {
      ref_ptr<FinishTexturesInitializationMessage> msg = message;
      m_finishTexturesInitialization = true;
      break;
    }

  case Message::Type::CleanupTextures:
    {
      // Do nothing, just destroy message with textures.
      break;
    }

  case Message::Type::ShowDebugInfo:
    {
      ref_ptr<ShowDebugInfoMessage> msg = message;
      gui::DrapeGui::Instance().GetScaleFpsHelper().SetVisible(msg->IsShown());
      break;
    }

  case Message::Type::NotifyRenderThread:
    {
      ref_ptr<NotifyRenderThreadMessage> msg = message;
      msg->InvokeFunctor();
      break;
    }

#if defined(OMIM_OS_DESKTOP)
  case Message::Type::NotifyGraphicsReady:
    {
      ref_ptr<NotifyGraphicsReadyMessage> msg = message;
      m_graphicsReadyFn = msg->GetCallback();
      if (m_graphicsReadyFn)
      {
        m_graphicsStage = GraphicsStage::WaitReady;
        if (m_notFinishedTiles.empty())
        {
          if (msg->NeedInvalidate())
            InvalidateRect(m_userEventStream.GetCurrentScreen().ClipRect());
          else
            m_graphicsStage = GraphicsStage::WaitRendering;
        }
      }
      break;
    }
#endif

  default:
    ASSERT(false, ());
  }
}

std::unique_ptr<threads::IRoutine> FrontendRenderer::CreateRoutine()
{
  return std::make_unique<Routine>(*this);
}

void FrontendRenderer::UpdateContextDependentResources()
{
  ++m_lastRecacheRouteId;

  for (auto const & subroute : m_routeRenderer->GetSubroutes())
  {
    auto msg = make_unique_dp<AddSubrouteMessage>(subroute.m_subrouteId,
                                                  subroute.m_subroute,
                                                  m_lastRecacheRouteId);
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread, std::move(msg),
                              MessagePriority::Normal);
  }

  m_trafficRenderer->ClearContextDependentResources();

  if (IsValidCurrentZoom())
  {
    // Request new tiles.
    ScreenBase const & screen = m_userEventStream.GetCurrentScreen();
    m_lastReadedModelView = screen;
    m_requestedTiles->Set(screen, m_isIsometry || screen.isPerspective(),
                          m_forceUpdateScene, m_forceUpdateUserMarks,
                          ResolveTileKeys(screen));
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                              make_unique_dp<UpdateReadManagerMessage>(),
                              MessagePriority::UberHighSingleton);
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                              make_unique_dp<RegenerateTransitMessage>(),
                              MessagePriority::Normal);
  }

  m_gpsTrackRenderer->Update();
}

void FrontendRenderer::FollowRoute(int preferredZoomLevel, int preferredZoomLevelIn3d,
                                   bool enableAutoZoom, bool isArrowGlued)
{
  m_myPositionController->ActivateRouting(
      !m_enablePerspectiveInNavigation ? preferredZoomLevel : preferredZoomLevelIn3d,
      enableAutoZoom, isArrowGlued);

  if (m_enablePerspectiveInNavigation)
    AddUserEvent(make_unique_dp<SetAutoPerspectiveEvent>(true /* isAutoPerspective */));

  m_routeRenderer->SetFollowingEnabled(true);
  CHECK(m_context != nullptr, ());
  m_postprocessRenderer->OnChangedRouteFollowingMode(m_context, true /* active */);
}

bool FrontendRenderer::CheckRouteRecaching(ref_ptr<BaseSubrouteData> subrouteData)
{
  return subrouteData->m_recacheId >= m_lastRecacheRouteId;
}

void FrontendRenderer::InvalidateRect(m2::RectD const & gRect)
{
  ScreenBase const screen = m_userEventStream.GetCurrentScreen();
  m2::RectD rect = gRect;
  if (rect.Intersect(screen.ClipRect()))
  {
    // Find tiles to invalidate.
    TTilesCollection tiles;
    int const dataZoomLevel = ClipTileZoomByMaxDataZoom(GetCurrentZoom());
    CalcTilesCoverage(rect, dataZoomLevel, [this, &rect, &tiles](int tileX, int tileY)
    {
      TileKey const key(tileX, tileY, GetCurrentZoom());
      if (rect.IsIntersect(key.GetGlobalRect()))
        tiles.insert(key);
    });

    // Remove tiles to invalidate from screen.
    auto eraseFunction = [&tiles](drape_ptr<RenderGroup> const & group)
    {
      return tiles.find(group->GetTileKey()) != tiles.end();
    };
    for (RenderLayer & layer : m_layers)
    {
      RemoveGroups(eraseFunction, layer.m_renderGroups, make_ref(m_overlayTree));
      layer.m_isDirty = true;
    }

    // Remove tiles to invalidate from backend renderer.
    BaseBlockingMessage::Blocker blocker;
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                              make_unique_dp<InvalidateReadManagerRectMessage>(blocker, tiles),
                              MessagePriority::High);
    blocker.Wait();

    // Request new tiles.
    m_lastReadedModelView = screen;
    m_requestedTiles->Set(screen, m_isIsometry || screen.isPerspective(),
                          m_forceUpdateScene, m_forceUpdateUserMarks,
                          ResolveTileKeys(screen));
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                              make_unique_dp<UpdateReadManagerMessage>(),
                              MessagePriority::UberHighSingleton);
  }
}

void FrontendRenderer::OnResize(ScreenBase const & screen)
{
  m2::RectD const viewportRect = screen.PixelRectIn3d();
  double constexpr kEps = 1e-5;
  bool const viewportChanged = !m2::IsEqualSize(m_lastReadedModelView.PixelRectIn3d(), viewportRect, kEps, kEps);

  m_myPositionController->OnUpdateScreen(screen);

  if (viewportChanged)
    m_myPositionController->UpdatePosition();

  auto const sx = static_cast<uint32_t>(viewportRect.SizeX());
  auto const sy = static_cast<uint32_t>(viewportRect.SizeY());
  m_viewport.SetViewport(0, 0, sx, sy);

  CHECK(m_context != nullptr, ());
  // All resize methods must be protected from setting up the same size.
  m_context->Resize(sx, sy);
  m_buildingsFramebuffer->SetSize(m_context, sx, sy);
  m_postprocessRenderer->Resize(m_context, sx, sy);
  m_needRestoreSize = false;

  RefreshProjection(screen);
  RefreshPivotTransform(screen);
}

template<typename TRenderGroup>
void FrontendRenderer::AddToRenderGroup(dp::RenderState const & state,
                                        drape_ptr<dp::RenderBucket> && renderBucket,
                                        TileKey const & newTile)
{
  RenderLayer & layer = m_layers[static_cast<size_t>(GetDepthLayer(state))];
  for (auto const & g : layer.m_renderGroups)
  {
    if (!g->IsPendingOnDelete() && g->GetState() == state && g->GetTileKey().EqualStrict(newTile))
    {
      g->AddBucket(std::move(renderBucket));
      layer.m_isDirty = true;
      return;
    }
  }

  drape_ptr<TRenderGroup> group = make_unique_dp<TRenderGroup>(state, newTile);
  group->AddBucket(std::move(renderBucket));

  layer.m_renderGroups.push_back(std::move(group));
  layer.m_isDirty = true;
}

void FrontendRenderer::RemoveRenderGroupsLater(TRenderGroupRemovePredicate const & predicate)
{
  for (RenderLayer & layer : m_layers)
  {
    RemoveGroups([&predicate, &layer](drape_ptr<RenderGroup> const & group)
    {
      if (predicate(group))
      {
        group->DeleteLater();
        layer.m_isDirty = true;
        return group->CanBeDeleted();
      }
      return false;
    }, layer.m_renderGroups, make_ref(m_overlayTree));
  }
}

bool FrontendRenderer::CheckTileGenerations(TileKey const & tileKey)
{
  bool const result = (tileKey.m_generation >= m_maxGeneration);

  if (tileKey.m_generation > m_maxGeneration)
    m_maxGeneration = tileKey.m_generation;

  if (tileKey.m_userMarksGeneration > m_maxUserMarksGeneration)
    m_maxUserMarksGeneration = tileKey.m_userMarksGeneration;

  RemoveRenderGroupsLater([&tileKey](drape_ptr<RenderGroup> const & group)
  {
    auto const & groupKey = group->GetTileKey();
    return groupKey == tileKey &&
          (groupKey.m_generation < tileKey.m_generation ||
          (group->IsUserMark() && groupKey.m_userMarksGeneration < tileKey.m_userMarksGeneration));
  });

  return result;
}

void FrontendRenderer::OnCompassTapped()
{
  m_myPositionController->OnCompassTapped();
  m_selectionTrackInfo.reset();
}

std::pair<FeatureID, kml::MarkId> FrontendRenderer::GetVisiblePOI(m2::PointD const & pixelPoint)
{
  double halfSize = VisualParams::Instance().GetTouchRectRadius();
  m2::PointD sizePoint(halfSize, halfSize);
  m2::RectD selectRect(pixelPoint - sizePoint, pixelPoint + sizePoint);
  return GetVisiblePOI(selectRect);
}

std::pair<FeatureID, kml::MarkId> FrontendRenderer::GetVisiblePOI(m2::RectD const & pixelRect)
{
  m2::PointD pt = pixelRect.Center();
  ScreenBase const & screen = m_userEventStream.GetCurrentScreen();
  if (m_overlayTree->IsNeedUpdate())
    BuildOverlayTree(screen);

  dp::TOverlayContainer selectResult;

  auto selectionRect = pixelRect;
  constexpr double kTapRectFactor = 0.25;  // make tap rect size smaller for extended objects
  selectionRect.Scale(kTapRectFactor);
  SearchInNonDisplaceableUserMarksLayer(screen, DepthLayer::SearchMarkLayer, selectionRect,
                                        selectResult);

  if (selectResult.empty())
    m_overlayTree->Select(pixelRect, selectResult);

  if (selectResult.empty())
    return {FeatureID(), kml::kInvalidMarkId};

  double minSquaredDist = std::numeric_limits<double>::max();
  ref_ptr<dp::OverlayHandle> closestOverlayHandle;
  for (ref_ptr<dp::OverlayHandle> const & handle : selectResult)
  {
    double const curSquaredDist = pt.SquaredLength(handle->GetPivot(screen, screen.isPerspective()));
    if (curSquaredDist < minSquaredDist)
    {
      minSquaredDist = curSquaredDist;
      closestOverlayHandle = handle;
    }
  }
  CHECK(closestOverlayHandle, ());

  auto const & overlayId = closestOverlayHandle->GetOverlayID();
  return {overlayId.m_featureId, overlayId.m_markId};
}

void FrontendRenderer::PullToBoundArea(bool randomPlace, bool applyZoom)
{
  if (m_dragBoundArea.empty())
    return;

  ScreenBase const & screen = m_userEventStream.GetCurrentScreen();
  m2::PointD const center = screen.GlobalRect().Center();
  if (!m2::IsPointInsideTriangles(center, m_dragBoundArea))
  {
    m2::PointD const dest = randomPlace ? m2::GetRandomPointInsideTriangles(m_dragBoundArea) :
                                          m2::ProjectPointToTriangles(center, m_dragBoundArea);
    int zoom = kDoNotChangeZoom;
    if (applyZoom && GetCurrentZoom() < scales::GetAddNewPlaceScale())
      zoom = scales::GetAddNewPlaceScale();
    AddUserEvent(make_unique_dp<SetCenterEvent>(dest, zoom, true /* isAnim */, false /* trackVisibleViewport */,
                                                nullptr /* parallelAnimCreator */));
  }
}

void FrontendRenderer::ProcessSelection(ref_ptr<SelectObjectMessage> msg)
{
  if (msg->IsDismiss())
  {
    m_selectionShape->Hide();
    if (!m_myPositionController->IsModeChangeViewport() && m_selectionTrackInfo)
    {
      AddUserEvent(make_unique_dp<SetAnyRectEvent>(m_selectionTrackInfo->m_startRect,
                                                   true /* isAnim */, false /* fitInViewport */,
                                                   false /* useVisibleViewport */));
    }
    m_selectionTrackInfo.reset();
  }
  else
  {
    double offsetZ = 0.0;
    ScreenBase modelView;
    m_userEventStream.GetTargetScreen(modelView);

    if (modelView.isPerspective() && msg->GetSelectedObject() != SelectionShape::ESelectedObject::OBJECT_TRACK)
    {
      dp::TOverlayContainer selectResult;
      if (m_overlayTree->IsNeedUpdate())
        BuildOverlayTree(modelView);
      m_overlayTree->Select(msg->GetPosition(), selectResult);
      for (ref_ptr<dp::OverlayHandle> const & handle : selectResult)
        offsetZ = std::max(offsetZ, handle->GetPivotZ());
    }

    if (msg->IsSelectionShapeVisible())
      m_selectionShape->Show(msg->GetSelectedObject(), msg->GetPosition(), offsetZ, msg->IsAnim());
    else
      m_selectionShape->Hide();

    if (!m_myPositionController->IsModeChangeViewport())
    {
      if (auto const startPosition = m_selectionShape->GetPixelPosition(modelView, GetCurrentZoom()))
        m_selectionTrackInfo = SelectionTrackInfo(modelView.GlobalRect(), *startPosition);
    }

    if (msg->IsGeometrySelectionAllowed())
    {
      m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                make_unique_dp<CheckSelectionGeometryMessage>(
                                  msg->GetFeatureID(), m_selectionShape->GetRecacheId()),
                                MessagePriority::Normal);
    }
  }
}

void FrontendRenderer::BeginUpdateOverlayTree(ScreenBase const & modelView)
{
  if (m_overlayTree->Frame())
    m_overlayTree->StartOverlayPlacing(modelView, GetCurrentZoom());
}

void FrontendRenderer::UpdateOverlayTree(ScreenBase const & modelView, drape_ptr<RenderGroup> & renderGroup)
{
  if (m_overlayTree->IsNeedUpdate())
    renderGroup->CollectOverlay(make_ref(m_overlayTree));
  else
    renderGroup->Update(modelView);
}

void FrontendRenderer::EndUpdateOverlayTree()
{
  if (m_overlayTree->IsNeedUpdate())
  {
    m_overlayTree->EndOverlayPlacing();

    // Track overlays.
    if (m_overlaysTracker->StartTracking(GetCurrentZoom(),
                                         m_myPositionController->IsModeHasPosition(),
                                         m_myPositionController->GetDrawablePosition(),
                                         m_myPositionController->GetHorizontalAccuracy()))
    {
      for (auto const & handle : m_overlayTree->GetHandlesCache())
      {
        if (handle->IsVisible())
          m_overlaysTracker->Track(handle->GetOverlayID().m_featureId);
      }
      m_overlaysTracker->FinishTracking();
    }
  }
}

void FrontendRenderer::RenderScene(ScreenBase const & modelView, bool activeFrame)
{
  TRACE_SECTION("[drape] RenderScene");
  CHECK(m_context != nullptr, ());
#if defined(DRAPE_MEASURER_BENCHMARK) && (defined(RENDER_STATISTIC) || defined(TRACK_GPU_MEM))
  DrapeImmediateRenderingMeasurerGuard drapeMeasurerGuard(m_context);
#endif

  if (m_postprocessRenderer->BeginFrame(m_context, modelView, activeFrame))
  {
    RefreshBgColor();

    uint32_t clearBits = dp::ClearBits::ColorBit | dp::ClearBits::DepthBit;
    if (m_apiVersion == dp::ApiVersion::OpenGLES2 || m_apiVersion == dp::ApiVersion::OpenGLES3)
      clearBits |= dp::ClearBits::StencilBit;

    uint32_t storeBits = dp::ClearBits::ColorBit;
    if (m_postprocessRenderer->IsEffectEnabled(PostprocessRenderer::Effect::Antialiasing))
    {
      clearBits |= dp::ClearBits::StencilBit;
      storeBits |= dp::ClearBits::StencilBit;
      m_context->SetStencilReferenceValue(2 /* write this value to the stencil buffer */);
    }

    m_context->Clear(clearBits, storeBits);
    m_context->ApplyFramebuffer("Static frame");
    m_viewport.Apply(m_context);

    Render2dLayer(modelView);
    RenderUserMarksLayer(modelView, DepthLayer::UserLineLayer);

    bool const hasTransitRouteData = HasTransitRouteData();
    if (m_buildingsFramebuffer->IsSupported() && !m_routeRenderer->IsRulerRoute())
    {
      RenderTrafficLayer(modelView);
      if (!hasTransitRouteData)
        RenderRouteLayer(modelView);
      Render3dLayer(modelView);
    }
    else
    {
      Render3dLayer(modelView);
      RenderTrafficLayer(modelView);
      if (!hasTransitRouteData)
        RenderRouteLayer(modelView);
    }

    m_context->Clear(dp::ClearBits::DepthBit, dp::kClearBitsStoreAll);

    if (m_selectionShape && IsValidCurrentZoom())
    {
      SelectionShape::ESelectedObject selectedObject = m_selectionShape->GetSelectedObject();
      if (selectedObject == SelectionShape::OBJECT_MY_POSITION)
      {
        ASSERT(m_myPositionController->IsModeHasPosition(), ());
        m_selectionShape->SetPosition(m_myPositionController->Position());
        m_selectionShape->Render(m_context, make_ref(m_gpuProgramManager), modelView,
                                 GetCurrentZoom(), m_frameValues);
      }
      else if (selectedObject == SelectionShape::OBJECT_POI)
      {
        m_selectionShape->Render(m_context, make_ref(m_gpuProgramManager), modelView,
                                 GetCurrentZoom(), m_frameValues);
      }
    }

    {
      StencilWriterGuard guard(make_ref(m_postprocessRenderer), m_context);
      RenderOverlayLayer(modelView);
    }

    if (IsValidCurrentZoom())
    {
      m_gpsTrackRenderer->RenderTrack(m_context, make_ref(m_gpuProgramManager), modelView, GetCurrentZoom(),
                                      m_frameValues);

      if (m_selectionShape &&
          (m_selectionShape->GetSelectedObject() == SelectionShape::OBJECT_USER_MARK ||
           m_selectionShape->GetSelectedObject() == SelectionShape::OBJECT_TRACK))
      {
        m_selectionShape->Render(m_context, make_ref(m_gpuProgramManager), modelView, GetCurrentZoom(),
                                 m_frameValues);
      }
    }

    if (hasTransitRouteData)
      RenderRouteLayer(modelView);

    {
      StencilWriterGuard guard(make_ref(m_postprocessRenderer), m_context);
      RenderUserMarksLayer(modelView, DepthLayer::UserMarkLayer);
      RenderUserMarksLayer(modelView, DepthLayer::RoutingBottomMarkLayer);
      RenderUserMarksLayer(modelView, DepthLayer::RoutingMarkLayer);
      RenderNonDisplaceableUserMarksLayer(modelView, DepthLayer::SearchMarkLayer);
    }

    if (!HasRouteData())
      RenderTransitSchemeLayer(modelView);

    m_drapeApiRenderer->Render(m_context, make_ref(m_gpuProgramManager), modelView, m_frameValues);

    for (auto const & arrow : m_overlayTree->GetDisplacementInfo())
      m_debugRectRenderer->DrawArrow(m_context, modelView, arrow);
  }

  if (!m_postprocessRenderer->EndFrame(m_context, make_ref(m_gpuProgramManager), m_viewport))
    return;

  if (IsValidCurrentZoom())
  {
    uint32_t clearBits = dp::ClearBits::DepthBit;
    if (m_apiVersion == dp::ApiVersion::OpenGLES2 || m_apiVersion == dp::ApiVersion::OpenGLES3)
      clearBits |= dp::ClearBits::StencilBit;
    m_context->Clear(clearBits, dp::kClearBitsStoreAll);

    m_myPositionController->Render(m_context, make_ref(m_gpuProgramManager), modelView,
                                   GetCurrentZoom(), m_frameValues);
  }

  if (m_guiRenderer && !m_screenshotMode)
  {
    m_guiRenderer->Render(m_context, make_ref(m_gpuProgramManager), m_myPositionController->IsInRouting(),
                          modelView);
  }

#if defined(OMIM_OS_DESKTOP)
  if (m_graphicsStage == GraphicsStage::WaitRendering)
    m_graphicsStage = GraphicsStage::Rendered;
#endif
}

void FrontendRenderer::Render2dLayer(ScreenBase const & modelView)
{
  TRACE_SECTION("[drape] Render2dLayer");
  RenderLayer & layer2d = m_layers[static_cast<size_t>(DepthLayer::GeometryLayer)];
  layer2d.Sort(make_ref(m_overlayTree));

  CHECK(m_context != nullptr, ());
  DEBUG_LABEL(m_context, "2D Layer");
  for (drape_ptr<RenderGroup> const & group : layer2d.m_renderGroups)
    RenderSingleGroup(m_context, modelView, make_ref(group));
}

void FrontendRenderer::PreRender3dLayer(ScreenBase const & modelView)
{
  TRACE_SECTION("[drape] PreRender3dLayer");
  if (!m_buildingsFramebuffer->IsSupported())
    return;

  RenderLayer & layer = m_layers[static_cast<size_t>(DepthLayer::Geometry3dLayer)];
  if (layer.m_renderGroups.empty())
    return;

  m_context->SetFramebuffer(make_ref(m_buildingsFramebuffer));
  m_context->SetClearColor(dp::Color::Transparent());
  m_context->Clear(dp::ClearBits::ColorBit | dp::ClearBits::DepthBit, dp::ClearBits::ColorBit /* storeBits */);
  m_context->ApplyFramebuffer("Buildings");
  m_viewport.Apply(m_context);

  layer.Sort(make_ref(m_overlayTree));
  for (drape_ptr<RenderGroup> const & group : layer.m_renderGroups)
    RenderSingleGroup(m_context, modelView, make_ref(group));
}

void FrontendRenderer::Render3dLayer(ScreenBase const & modelView)
{
  TRACE_SECTION("[drape] Render3dLayer");
  RenderLayer & layer = m_layers[static_cast<size_t>(DepthLayer::Geometry3dLayer)];
  if (layer.m_renderGroups.empty())
    return;

  DEBUG_LABEL(m_context, "3D Layer");
  if (m_buildingsFramebuffer->IsSupported())
  {
    float const kOpacity = 0.7f;
    m_screenQuadRenderer->RenderTexture(m_context, make_ref(m_gpuProgramManager),
                                        m_buildingsFramebuffer->GetTexture(),
                                        kOpacity);
  }
  else
  {
    // Fallback for devices which do not support rendering to the texture.
    CHECK(m_context != nullptr, ());
    m_context->Clear(dp::ClearBits::DepthBit, dp::kClearBitsStoreAll);

    layer.Sort(make_ref(m_overlayTree));
    for (drape_ptr<RenderGroup> const & group : layer.m_renderGroups)
      RenderSingleGroup(m_context, modelView, make_ref(group));
  }
}

void FrontendRenderer::RenderOverlayLayer(ScreenBase const & modelView)
{
  TRACE_SECTION("[drape] RenderOverlayLayer");
  CHECK(m_context != nullptr, ());
  DEBUG_LABEL(m_context, "Overlay Layer");
  RenderLayer & overlay = m_layers[static_cast<size_t>(DepthLayer::OverlayLayer)];
  BuildOverlayTree(modelView);
  for (drape_ptr<RenderGroup> & group : overlay.m_renderGroups)
    RenderSingleGroup(m_context, modelView, make_ref(group));
}

bool FrontendRenderer::HasTransitRouteData() const
{
  return m_routeRenderer->HasTransitData();
}

bool FrontendRenderer::HasRouteData() const
{
  return m_routeRenderer->HasData();
}

void FrontendRenderer::RenderTransitSchemeLayer(ScreenBase const & modelView)
{
  TRACE_SECTION("[drape] RenderTransitSchemeLayer");
  CHECK(m_context != nullptr, ());
  if (m_transitSchemeEnabled && m_transitSchemeRenderer->IsSchemeVisible(GetCurrentZoom()))
  {
    DEBUG_LABEL(m_context, "Transit Scheme");
    m_context->Clear(dp::ClearBits::DepthBit, dp::kClearBitsStoreAll);
    RenderTransitBackground();
    m_transitSchemeRenderer->RenderTransit(m_context, make_ref(m_gpuProgramManager), modelView,
                                           make_ref(m_postprocessRenderer), m_frameValues,
                                           make_ref(m_debugRectRenderer));
  }
}

void FrontendRenderer::RenderTrafficLayer(ScreenBase const & modelView)
{
  TRACE_SECTION("[drape] RenderTrafficLayer");
  CHECK(m_context != nullptr, ());
  if (m_trafficRenderer->HasRenderData())
  {
    DEBUG_LABEL(m_context, "Traffic Layer");
    m_context->Clear(dp::ClearBits::DepthBit, dp::kClearBitsStoreAll);
    m_trafficRenderer->RenderTraffic(m_context, make_ref(m_gpuProgramManager), modelView,
                                     GetCurrentZoom(), 1.0f /* opacity */, m_frameValues);
  }
}

void FrontendRenderer::RenderTransitBackground()
{
  TRACE_SECTION("[drape] RenderTransitBackground");
  if (!m_finishTexturesInitialization)
    return;

  CHECK(m_context != nullptr, ());
  DEBUG_LABEL(m_context, "Transit Background");

  dp::TextureManager::ColorRegion region;
  m_texMng->GetColorRegion(df::GetColorConstant(kTransitBackgroundColor), region);
  CHECK(region.GetTexture() != nullptr, ("Texture manager is not initialized"));
  if (!m_transitBackground->IsInitialized())
    m_transitBackground->SetTextureRect(m_context, region.GetTexRect());

  m_transitBackground->RenderTexture(m_context, make_ref(m_gpuProgramManager),
                                     region.GetTexture(), 1.0f /* opacity */, false /* invertV */);
}

void FrontendRenderer::RenderRouteLayer(ScreenBase const & modelView)
{
  TRACE_SECTION("[drape] RenderRouteLayer");
  if (HasTransitRouteData())
    RenderTransitBackground();

  if (m_routeRenderer->HasData() || m_routeRenderer->HasPreviewData())
  {
    CHECK(m_context != nullptr, ());
    DEBUG_LABEL(m_context, "Route Layer");
    m_context->Clear(dp::ClearBits::DepthBit, dp::kClearBitsStoreAll);
    m_routeRenderer->RenderRoute(m_context, make_ref(m_gpuProgramManager), modelView,
                                 m_trafficRenderer->HasRenderData(), m_frameValues);
  }
}

void FrontendRenderer::RenderUserMarksLayer(ScreenBase const & modelView, DepthLayer layerId)
{
  TRACE_SECTION("[drape] RenderUserMarksLayer");
  auto & renderGroups = m_layers[static_cast<size_t>(layerId)].m_renderGroups;
  if (renderGroups.empty())
    return;

  CHECK(m_context != nullptr, ());
  DEBUG_LABEL(m_context, "User Marks: " + DebugPrint(layerId));
  m_context->Clear(dp::ClearBits::DepthBit, dp::kClearBitsStoreAll);

  for (drape_ptr<RenderGroup> & group : renderGroups)
    RenderSingleGroup(m_context, modelView, make_ref(group));
}

void FrontendRenderer::RenderNonDisplaceableUserMarksLayer(ScreenBase const & modelView,
                                                           DepthLayer layerId)
{
  TRACE_SECTION("[drape] RenderNonDisplaceableUserMarksLayer");
  auto & layer = m_layers[static_cast<size_t>(layerId)];
  layer.Sort(nullptr);
  for (drape_ptr<RenderGroup> & group : layer.m_renderGroups)
  {
    group->SetOverlayVisibility(true);
    group->Update(modelView);
  }
  RenderUserMarksLayer(modelView, layerId);
}

void FrontendRenderer::RenderEmptyFrame()
{
  TRACE_SECTION("[drape] RenderEmptyFrame");
  CHECK(m_context != nullptr, ());
  if (!m_context->Validate())
    return;

  if (!m_context->BeginRendering())
    return;

  m_context->SetFramebuffer(nullptr /* default */);
  auto const c = dp::Color(drule::rules().GetBgColor(1 /* scale */), 255);
  m_context->SetClearColor(c);
  m_context->Clear(dp::ClearBits::ColorBit, dp::ClearBits::ColorBit /* storeBits */);
  m_context->ApplyFramebuffer("Empty frame");
  m_viewport.Apply(m_context);
  m_context->EndRendering();
  m_context->Present();
}

void FrontendRenderer::RenderFrame()
{
  TRACE_SECTION("[drape] RenderFrame");
  DrapeMeasurerGuard drapeMeasurerGuard;

  CHECK(m_context != nullptr, ());
  if (!m_context->Validate())
  {
    m_frameData.m_forceFullRedrawNextFrame = true;
    m_frameData.m_inactiveFramesCounter = 0;
    return;
  }

  auto & scaleFpsHelper = gui::DrapeGui::Instance().GetScaleFpsHelper();
  m_frameData.m_timer.Reset();

  bool modelViewChanged, viewportChanged, needActiveFrame;
  ScreenBase const & modelView = ProcessEvents(modelViewChanged, viewportChanged, needActiveFrame);
  if (viewportChanged || m_needRestoreSize)
    OnResize(modelView);

  if (!m_context->BeginRendering())
    return;

  // Check for a frame is active.
  bool isActiveFrame = modelViewChanged || viewportChanged || needActiveFrame;

  if (isActiveFrame)
    PrepareScene(modelView);

  bool const needUpdateDynamicTextures = m_texMng->UpdateDynamicTextures(m_context);
  isActiveFrame |= needUpdateDynamicTextures;
  isActiveFrame |= m_userEventStream.IsWaitingForActionCompletion();
  isActiveFrame |= InterpolationHolder::Instance().IsActive();

  bool isActiveFrameForScene = isActiveFrame || m_frameData.m_forceFullRedrawNextFrame;
  if (AnimationSystem::Instance().HasAnimations())
  {
    isActiveFrameForScene |= AnimationSystem::Instance().HasMapAnimations();
    isActiveFrame = true;
  }

  m_routeRenderer->UpdatePreview(modelView);

#ifdef SHOW_FRAMES_STATS
  m_frameData.m_framesOverall += static_cast<uint64_t>(isActiveFrame);
  m_frameData.m_framesFast += static_cast<uint64_t>(!isActiveFrameForScene);
#endif

  RenderScene(modelView, isActiveFrameForScene);

  if (m_renderInjectionHandler)
    m_renderInjectionHandler(m_context, m_texMng, make_ref(m_gpuProgramManager), false);

  m_context->EndRendering();

  auto const hasForceUpdate = m_forceUpdateScene || m_forceUpdateUserMarks;
  isActiveFrame |= hasForceUpdate;
#if defined(SCENARIO_ENABLE)
  isActiveFrame = true;
#endif

  if (modelViewChanged || hasForceUpdate)
    UpdateScene(modelView);

  InterpolationHolder::Instance().Advance(m_frameData.m_frameTime);
  AnimationSystem::Instance().Advance(m_frameData.m_frameTime);

  // On the first inactive frame we invalidate overlay tree.
  if (!isActiveFrame)
  {
    if (m_frameData.m_inactiveFramesCounter == 0)
      m_overlayTree->InvalidateOnNextFrame();
    m_frameData.m_inactiveFramesCounter++;
  }
  else
  {
    m_frameData.m_inactiveFramesCounter = 0;
  }

  bool const canSuspend = m_frameData.m_inactiveFramesCounter > FrameData::kMaxInactiveFrames;
  m_frameData.m_forceFullRedrawNextFrame = m_overlayTree->IsNeedUpdate();
  if (canSuspend)
  {
#if defined(OMIM_OS_DESKTOP)
    EmitGraphicsReady();
#endif
    // Process a message or wait for a message.
    // IsRenderingEnabled() can return false in case of rendering disabling and we must prevent
    // possibility of infinity waiting in ProcessSingleMessage.
    ProcessSingleMessage(IsRenderingEnabled());
    m_frameData.m_forceFullRedrawNextFrame = true;
    m_frameData.m_timer.Reset();
    m_frameData.m_inactiveFramesCounter = 0;
    isActiveFrameForScene = false;
  }
  else
  {
    auto const syncInverval = (m_apiVersion == dp::ApiVersion::Metal ||
                               m_apiVersion == dp::ApiVersion::Vulkan)
                              ? kVSyncIntervalMetalVulkan
                              : kVSyncInterval;

    double availableTime;
    do
    {
      if (!ProcessSingleMessage(false /* waitForMessage */))
        break;
      m_frameData.m_forceFullRedrawNextFrame = true;
      m_frameData.m_inactiveFramesCounter = 0;
      availableTime = syncInverval - m_frameData.m_timer.ElapsedSeconds();
    }
    while (availableTime > 0.0);
  }

#ifndef DISABLE_SCREEN_PRESENTATION
  m_context->Present();
#endif

  // Limit fps in following mode.
  double constexpr kFrameTime = 1.0 / 30.0;
  auto const ft = m_frameData.m_timer.ElapsedSeconds();
  if (!canSuspend && ft < kFrameTime && m_myPositionController->IsRouteFollowingActive())
  {
    auto const ms = static_cast<uint32_t>((kFrameTime - ft) * 1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  }

  m_frameData.m_frameTime = m_frameData.m_timer.ElapsedSeconds();
  scaleFpsHelper.SetFrameTime(m_frameData.m_frameTime,
    m_frameData.m_inactiveFramesCounter + 1 < FrameData::kMaxInactiveFrames);

  m_debugRectRenderer->FinishRendering();

#ifndef DRAPE_MEASURER_BENCHMARK
  drapeMeasurerGuard.SetProperties(isActiveFrameForScene, modelView.GetOrg());
#endif
}

void FrontendRenderer::BuildOverlayTree(ScreenBase const & modelView)
{
  TRACE_SECTION("[drape] BuildOverlayTree");
  if (!IsValidCurrentZoom())
    return;

  BeginUpdateOverlayTree(modelView);
  for (auto const layerId : {DepthLayer::OverlayLayer,
                             DepthLayer::RoutingBottomMarkLayer,
                             DepthLayer::RoutingMarkLayer})
  {
    RenderLayer & overlay = m_layers[static_cast<size_t>(layerId)];
    overlay.Sort(make_ref(m_overlayTree));
    for (auto & group : overlay.m_renderGroups)
      UpdateOverlayTree(modelView, group);
  }
  if (m_transitSchemeRenderer->IsSchemeVisible(GetCurrentZoom()) && !HasTransitRouteData())
    m_transitSchemeRenderer->CollectOverlays(make_ref(m_overlayTree), modelView);
  EndUpdateOverlayTree();
}

void FrontendRenderer::PrepareBucket(dp::RenderState const & state, drape_ptr<dp::RenderBucket> & bucket)
{
  CHECK(m_context != nullptr, ());

  auto program = m_gpuProgramManager->GetProgram(m_userEventStream.GetCurrentScreen().isPerspective() ?
          state.GetProgram3d<gpu::Program>() : state.GetProgram<gpu::Program>());

  program->Bind();
  bucket->GetBuffer()->Build(m_context, program);
}

void FrontendRenderer::RenderSingleGroup(ref_ptr<dp::GraphicsContext> context,
                                         ScreenBase const & modelView,
                                         ref_ptr<BaseRenderGroup> group)
{
  group->UpdateAnimation();
  group->Render(context, make_ref(m_gpuProgramManager), modelView, m_frameValues,
                make_ref(m_debugRectRenderer));
}

void FrontendRenderer::RefreshProjection(ScreenBase const & screen)
{
  auto m = dp::MakeProjection(m_apiVersion, 0.0f, screen.GetWidth(), screen.GetHeight(), 0.0f);
  m_frameValues.m_projection = glsl::make_mat4(m.data());
}

void FrontendRenderer::RefreshZScale(ScreenBase const & screen)
{
  m_frameValues.m_zScale = static_cast<float>(screen.GetZScale());
}

void FrontendRenderer::RefreshPivotTransform(ScreenBase const & screen)
{
  if (screen.isPerspective())
  {
    math::Matrix<float, 4, 4> transform(screen.Pto3dMatrix());
    m_frameValues.m_pivotTransform = glsl::make_mat4(transform.m_data);
  }
  else if (m_isIsometry)
  {
    math::Matrix<float, 4, 4> transform(math::Identity<float, 4>());
    transform(2, 1) = -1.0f / static_cast<float>(tan(kIsometryAngle));
    transform(2, 2) = 1.0f / screen.GetHeight();
    if (m_apiVersion == dp::ApiVersion::Metal)
    {
      transform(3, 2) = transform(3, 2) + 0.5f;
      transform(2, 2) = transform(2, 2) * 0.5f;
    }
    m_frameValues.m_pivotTransform = glsl::make_mat4(transform.m_data);
  }
  else
  {
    math::Matrix<float, 4, 4> transform(math::Identity<float, 4>());
    m_frameValues.m_pivotTransform = glsl::make_mat4(transform.m_data);
  }
}

void FrontendRenderer::RefreshBgColor()
{
  auto const scale = std::min(df::GetDrawTileScale(m_userEventStream.GetCurrentScreen()),
                              scales::GetUpperStyleScale());
  auto const c = dp::Color(drule::rules().GetBgColor(scale), 255);

  CHECK(m_context != nullptr, ());
  m_context->SetClearColor(c);
}

void FrontendRenderer::DisablePerspective()
{
  AddUserEvent(make_unique_dp<SetAutoPerspectiveEvent>(false /* isAutoPerspective */));
}

void FrontendRenderer::CheckIsometryMinScale(ScreenBase const & screen)
{
  bool const isScaleAllowableIn3d = IsScaleAllowableIn3d(GetCurrentZoom());
  bool const isIsometry = m_enable3dBuildings && !m_choosePositionMode && isScaleAllowableIn3d;
  if (m_isIsometry != isIsometry)
  {
    m_isIsometry = isIsometry;
    RefreshPivotTransform(screen);
  }
}

void FrontendRenderer::ResolveZoomLevel(ScreenBase const & screen)
{
  int const prevZoomLevel = m_currentZoomLevel;
  m_currentZoomLevel = GetDrawTileScale(screen);
  gui::DrapeGui::Instance().GetScaleFpsHelper().SetScale(m_currentZoomLevel);

  if (prevZoomLevel != m_currentZoomLevel)
    UpdateCanBeDeletedStatus();

  CheckIsometryMinScale(screen);
  UpdateDisplacementEnabled();
}

void FrontendRenderer::UpdateDisplacementEnabled()
{
  if (m_choosePositionMode)
    m_overlayTree->SetDisplacementEnabled(GetCurrentZoom() < scales::GetAddNewPlaceScale());
  else
    m_overlayTree->SetDisplacementEnabled(true);
}

void FrontendRenderer::OnTap(m2::PointD const & pt, bool isLongTap)
{
  if (m_blockTapEvents)
    return;

  double const halfSize = VisualParams::Instance().GetTouchRectRadius();
  m2::PointD sizePoint(halfSize, halfSize);
  m2::RectD selectRect(pt - sizePoint, pt + sizePoint);

  ScreenBase const & screen = m_userEventStream.GetCurrentScreen();
  bool isMyPosition = false;

  m2::PointD const pxPoint2d = screen.P3dtoP(pt);
  m2::PointD mercator = screen.PtoG(pxPoint2d);

  // Long tap should show/hide the interface. There is no need to detect tapped features.
  if (isLongTap)
  {
    m_tapEventInfoHandler({mercator, isLongTap, isMyPosition, FeatureID(), kml::kInvalidMarkId});
    return;
  }

  if (m_myPositionController->IsModeHasPosition())
  {
    m2::PointD const pixelPos = screen.PtoP3d(screen.GtoP(m_myPositionController->Position()));
    isMyPosition = selectRect.IsPointInside(pixelPos);
    if (isMyPosition)
      mercator = m_myPositionController->Position();
  }

  ASSERT(m_tapEventInfoHandler != nullptr, ());
  auto [featureId, markId] = GetVisiblePOI(selectRect);
  m_tapEventInfoHandler({mercator, isLongTap, isMyPosition, featureId, markId});
}

void FrontendRenderer::OnForceTap(m2::PointD const & pt)
{
  // Emulate long tap on force tap.
  OnTap(pt, true /* isLongTap */);
}

void FrontendRenderer::OnDoubleTap(m2::PointD const & pt)
{
  m_userEventStream.AddEvent(make_unique_dp<ScaleEvent>(2.0 /* scale factor */, pt, true /* animated */));
}

void FrontendRenderer::OnTwoFingersTap()
{
  ScreenBase const & screen = m_userEventStream.GetCurrentScreen();
  m_userEventStream.AddEvent(make_unique_dp<ScaleEvent>(0.5 /* scale factor */, screen.PixelRect().Center(),
                                                        true /* animated */));
}

bool FrontendRenderer::OnSingleTouchFiltrate(m2::PointD const & pt, TouchEvent::ETouchType type)
{
  // This method can be called before gui renderer initialization.
  if (m_guiRenderer == nullptr)
    return false;

  float const rectHalfSize = df::VisualParams::Instance().GetTouchRectRadius();
  m2::RectD r(-rectHalfSize, -rectHalfSize, rectHalfSize, rectHalfSize);
  r.SetCenter(pt);

  switch(type)
  {
  case TouchEvent::ETouchType::TOUCH_DOWN:
    return m_guiRenderer->OnTouchDown(r);
  case TouchEvent::ETouchType::TOUCH_UP:
    m_guiRenderer->OnTouchUp(r);
    return false;
  case TouchEvent::ETouchType::TOUCH_CANCEL:
    m_guiRenderer->OnTouchCancel(r);
    return false;
  case TouchEvent::ETouchType::TOUCH_MOVE:
    return false;
  case TouchEvent::ETouchType::TOUCH_NONE:
    UNREACHABLE();
  }

  return false;
}

void FrontendRenderer::OnDragStarted()
{
  m_myPositionController->DragStarted();
}

void FrontendRenderer::OnDragEnded(m2::PointD const & distance)
{
  m_myPositionController->DragEnded(distance);
  PullToBoundArea(false /* randomPlace */, false /* applyZoom */);
  m_firstLaunchAnimationInterrupted = true;
}

void FrontendRenderer::OnScaleStarted()
{
  m_myPositionController->ScaleStarted();
}

void FrontendRenderer::OnRotated()
{
  m_myPositionController->Rotated();
}

void FrontendRenderer::OnScrolled(m2::PointD const & distance)
{
  m_myPositionController->Scrolled(distance);
}

void FrontendRenderer::CorrectScalePoint(m2::PointD & pt) const
{
  m_myPositionController->CorrectScalePoint(pt);
}

void FrontendRenderer::CorrectGlobalScalePoint(m2::PointD & pt) const
{
  m_myPositionController->CorrectGlobalScalePoint(pt);
}

void FrontendRenderer::CorrectScalePoint(m2::PointD & pt1, m2::PointD & pt2) const
{
  m_myPositionController->CorrectScalePoint(pt1, pt2);
}

void FrontendRenderer::OnScaleEnded()
{
  m_myPositionController->ScaleEnded();
  PullToBoundArea(false /* randomPlace */, false /* applyZoom */);
  m_firstLaunchAnimationInterrupted = true;
  m_selectionTrackInfo.reset();
}

void FrontendRenderer::OnAnimatedScaleEnded()
{
  m_myPositionController->ResetBlockAutoZoomTimer();
  PullToBoundArea(false /* randomPlace */, false /* applyZoom */);
  m_firstLaunchAnimationInterrupted = true;
  m_selectionTrackInfo.reset();
}

void FrontendRenderer::OnTouchMapAction(TouchEvent::ETouchType touchType, bool isMapTouch)
{
  // Here we block timer on start of touch actions. Timer will be unblocked on
  // the completion of touch actions. It helps to prevent the creation of redundant checks.
  auto const blockTimer = (touchType == TouchEvent::TOUCH_DOWN || touchType == TouchEvent::TOUCH_MOVE);
  m_myPositionController->ResetRoutingNotFollowTimer(blockTimer);
  if (isMapTouch)
    m_selectionTrackInfo.reset();
}

bool FrontendRenderer::OnNewVisibleViewport(m2::RectD const & oldViewport, m2::RectD const & newViewport,
                                            bool needOffset, m2::PointD & gOffset)
{
  if (!needOffset)
  {
    m_selectionTrackInfo.reset();
    return false;
  }

  gOffset = m2::PointD(0, 0);
  if (m_myPositionController->IsModeChangeViewport() || m_selectionShape == nullptr ||
      oldViewport == newViewport || !m_selectionTrackInfo.has_value())
  {
    return false;
  }

  ScreenBase const & screen = m_userEventStream.GetCurrentScreen();

  ScreenBase targetScreen;
  AnimationSystem::Instance().GetTargetScreen(screen, targetScreen);

  auto const pos = m_selectionShape->GetPixelPosition(screen, GetCurrentZoom());
  auto const targetPos = m_selectionShape->GetPixelPosition(targetScreen, GetCurrentZoom());
  if (!pos || !targetPos)
    return false;

  m2::RectD rect(*pos, *pos);
  m2::RectD targetRect(*targetPos, *targetPos);
  if (m_selectionShape->HasSelectionGeometry())
  {
    auto r = m_selectionShape->GetSelectionGeometryBoundingBox();
    r.Scale(kBoundingBoxScale);

    m2::RectD pixelRect;
    pixelRect.Add(screen.PtoP3d(screen.GtoP(r.LeftTop())));
    pixelRect.Add(screen.PtoP3d(screen.GtoP(r.RightBottom())));

    rect.Inflate(pixelRect.SizeX(), pixelRect.SizeY());
    targetRect.Inflate(pixelRect.SizeX(), pixelRect.SizeY());
  }
  else
  {
    if (m_overlayTree->IsNeedUpdate())
      BuildOverlayTree(screen);

    if (!(m_selectionShape->GetSelectedObject() == SelectionShape::OBJECT_POI &&
          m_overlayTree->GetSelectedFeatureRect(screen, rect) &&
          m_overlayTree->GetSelectedFeatureRect(targetScreen, targetRect)))
    {
      double const r = m_selectionShape->GetRadius();
      rect.Inflate(r, r);
      targetRect.Inflate(r, r);
    }

    double const kOffset = 50 * VisualParams::Instance().GetVisualScale();
    rect.Inflate(kOffset, kOffset);
    targetRect.Inflate(kOffset, kOffset);
  }

  if (newViewport.SizeX() < rect.SizeX() || newViewport.SizeY() < rect.SizeY())
    return false;

  m2::PointD pOffset(0.0, 0.0);
  if ((oldViewport.IsIntersect(targetRect) && !newViewport.IsRectInside(rect)) ||
      (newViewport.IsRectInside(rect) && m_selectionTrackInfo->m_snapSides != m2::PointI::Zero()))
  {
    // In case the rect of the selection is [partly] hidden, scroll the map to keep it visible.
    // In case the rect of the selection is visible after the map scrolling,
    // try to rollback part of that scrolling to return the map to its original position.
    if (rect.minX() < newViewport.minX() || m_selectionTrackInfo->m_snapSides.x < 0)
    {
      pOffset.x = std::max(m_selectionTrackInfo->m_startPos.x - pos->x, newViewport.minX() - rect.minX());
      m_selectionTrackInfo->m_snapSides.x = -1;
    }
    else if (rect.maxX() > newViewport.maxX() || m_selectionTrackInfo->m_snapSides.x > 0)
    {
      pOffset.x = std::min(m_selectionTrackInfo->m_startPos.x - pos->x, newViewport.maxX() - rect.maxX());
      m_selectionTrackInfo->m_snapSides.x = 1;
    }

    if (rect.minY() < newViewport.minY() || m_selectionTrackInfo->m_snapSides.y < 0)
    {
      pOffset.y = std::max(m_selectionTrackInfo->m_startPos.y - pos->y, newViewport.minY() - rect.minY());
      m_selectionTrackInfo->m_snapSides.y = -1;
    }
    else if (rect.maxY() > newViewport.maxY() || m_selectionTrackInfo->m_snapSides.y > 0)
    {
      pOffset.y = std::min(m_selectionTrackInfo->m_startPos.y - pos->y, newViewport.maxY() - rect.maxY());
      m_selectionTrackInfo->m_snapSides.y = 1;
    }
  }

  double const ptZ = m_selectionShape->GetPositionZ();
  gOffset = screen.PtoG(screen.P3dtoP(*pos + pOffset, ptZ)) - screen.PtoG(screen.P3dtoP(*pos, ptZ));
  return true;
}

TTilesCollection FrontendRenderer::ResolveTileKeys(ScreenBase const & screen)
{
  m2::RectD rect = screen.ClipRect();
  double const vs = VisualParams::Instance().GetVisualScale();
  double const extension = vs * dp::kScreenPixelRectExtension * screen.GetScale();
  rect.Inflate(extension, extension);
  int const dataZoomLevel = ClipTileZoomByMaxDataZoom(GetCurrentZoom());

  m_notFinishedTiles.clear();

  // Request new tiles.
  TTilesCollection tiles;
  /// @todo Introduce small_set class based on buffer_vector.
  buffer_vector<TileKey, 8> tilesToDelete;
  auto const result = CalcTilesCoverage(rect, dataZoomLevel,
                                  [this, &rect, &tiles, &tilesToDelete](int tileX, int tileY)
  {
    TileKey const key(tileX, tileY, GetCurrentZoom());
    if (rect.IsIntersect(key.GetGlobalRect()))
    {
      tiles.insert(key);
      m_notFinishedTiles.insert(key);
    }
    else
    {
      tilesToDelete.push_back(key);
    }
  });

  // Remove old tiles.
  auto removePredicate = [this, &result, &tilesToDelete](drape_ptr<RenderGroup> const & group)
  {
    TileKey const & key = group->GetTileKey();
    return key.m_zoomLevel == GetCurrentZoom() &&
           (key.m_x < result.m_minTileX || key.m_x >= result.m_maxTileX ||
           key.m_y < result.m_minTileY || key.m_y >= result.m_maxTileY ||
           base::IsExist(tilesToDelete, key));
  };
  for (RenderLayer & layer : m_layers)
    layer.m_isDirty |= RemoveGroups(removePredicate, layer.m_renderGroups, make_ref(m_overlayTree));

  RemoveRenderGroupsLater([this](drape_ptr<RenderGroup> const & group)
  {
    return group->GetTileKey().m_zoomLevel != GetCurrentZoom();
  });

  m_trafficRenderer->OnUpdateViewport(result, GetCurrentZoom(), tilesToDelete);

#if defined(DRAPE_MEASURER_BENCHMARK) && defined(GENERATING_STATISTIC)
  DrapeMeasurer::Instance().StartScenePreparing();
#endif

  return tiles;
}

void FrontendRenderer::OnContextDestroy()
{
  LOG(LINFO, ("On context destroy."));

  if (m_renderInjectionHandler)
    m_renderInjectionHandler(m_context, m_texMng, make_ref(m_gpuProgramManager), true);

  // Clear all graphics.
  for (RenderLayer & layer : m_layers)
  {
    layer.m_renderGroups.clear();
    layer.m_isDirty = false;
  }

  m_selectObjectMessage.reset();
  m_overlayTree->SetSelectedFeature(FeatureID());
  m_overlayTree->SetDebugRectRenderer(nullptr);
  m_overlayTree->Clear();

  m_guiRenderer.reset();
  m_selectionShape.reset();
  m_buildingsFramebuffer.reset();
  m_screenQuadRenderer.reset();

  m_myPositionController->ResetRenderShape();
  m_routeRenderer->ClearContextDependentResources();
  m_gpsTrackRenderer->ClearRenderData();
  m_trafficRenderer->ClearContextDependentResources();
  m_drapeApiRenderer->Clear();
  m_postprocessRenderer->ClearContextDependentResources();
  m_transitSchemeRenderer->ClearContextDependentResources(nullptr /* overlayTree */);

  m_transitBackground.reset();
  m_debugRectRenderer.reset();

  CHECK(m_context != nullptr, ());
  m_gpuProgramManager->Destroy(m_context);
  m_gpuProgramManager.reset();

  // Here we have to erase weak pointer to the context, since it
  // can be destroyed after this method.
  m_context->DoneCurrent();
  m_context = nullptr;

  m_needRestoreSize = true;
  m_firstTilesReady = false;
  m_firstLaunchAnimationInterrupted = false;

  m_finishTexturesInitialization = false;
}

void FrontendRenderer::OnContextCreate()
{
  TRACE_SECTION("[drape] OnContextCreate (FrontendRenderer)");
  LOG(LINFO, ("On context create."));

  m_context = make_ref(m_contextFactory->GetDrawContext());
  m_contextFactory->WaitForInitialization(m_context.get());

  m_context->MakeCurrent();

  m_context->Init(m_apiVersion);

  // Render empty frame here to avoid black initialization screen.
  RenderEmptyFrame();

  dp::SupportManager::Instance().Init(m_context);

  m_gpuProgramManager = make_unique_dp<gpu::ProgramManager>();
  m_gpuProgramManager->Init(m_context);

  dp::AlphaBlendingState::Apply(m_context);

  m_debugRectRenderer = make_unique_dp<DebugRectRenderer>(
    m_context, m_gpuProgramManager->GetProgram(gpu::Program::DebugRect),
    m_gpuProgramManager->GetParamsSetter());
  m_debugRectRenderer->SetEnabled(m_isDebugRectRenderingEnabled);

  m_overlayTree->SetDebugRectRenderer(make_ref(m_debugRectRenderer));
  m_overlayTree->SetVisualScale(VisualParams::Instance().GetVisualScale());

  // Resources recovering.
  m_screenQuadRenderer = make_unique_dp<ScreenQuadRenderer>(m_context);

  m_postprocessRenderer->Init(m_context, [this]()
  {
    CHECK(m_context != nullptr, ());
    if (!m_context->Validate())
      return false;
    m_context->SetFramebuffer(nullptr /* default */);
    return true;
  },
  [this](ScreenBase const & modelView)
  {
    PreRender3dLayer(modelView);
  });

#ifndef OMIM_OS_IPHONE_SIMULATOR
  for (auto const & effect : m_enabledOnStartEffects)
    m_postprocessRenderer->SetEffectEnabled(m_context, effect, true /* enabled */);
  m_enabledOnStartEffects.clear();

  if (dp::SupportManager::Instance().IsAntialiasingEnabledByDefault() || m_screenshotMode)
    m_postprocessRenderer->SetEffectEnabled(m_context, PostprocessRenderer::Antialiasing, true);
#endif

  m_buildingsFramebuffer = make_unique_dp<dp::Framebuffer>(dp::TextureFormat::RGBA8,
                                                           true /* depthEnabled */,
                                                           false /* stencilEnabled */);
  m_buildingsFramebuffer->SetFramebufferFallback([this]()
  {
    return m_postprocessRenderer->OnFramebufferFallback(m_context);
  });

  m_transitBackground = make_unique_dp<ScreenQuadRenderer>(m_context);
}

void FrontendRenderer::OnRenderingEnabled()
{
  m_overlayTree->InvalidateOnNextFrame();

#ifndef DRAPE_MEASURER_BENCHMARK
  DrapeMeasurer::Instance().Start();
#endif
}

void FrontendRenderer::OnRenderingDisabled()
{
#ifndef DRAPE_MEASURER_BENCHMARK
  DrapeMeasurer::Instance().Stop();
#endif
}

FrontendRenderer::Routine::Routine(FrontendRenderer & renderer) : m_renderer(renderer) {}

void FrontendRenderer::Routine::Do()
{
  LOG(LINFO, ("Start routine."));

  gui::DrapeGui::Instance().ConnectOnCompassTappedHandler(std::bind(&FrontendRenderer::OnCompassTapped, &m_renderer));
  m_renderer.m_myPositionController->SetListener(ref_ptr<MyPositionController::Listener>(&m_renderer));
  m_renderer.m_userEventStream.SetListener(ref_ptr<UserEventStream::Listener>(&m_renderer));

  m_renderer.CreateContext();

#if defined(DEBUG) || defined(DEBUG_DRAPE_XCODE) || defined(SCENARIO_ENABLE)
  gui::DrapeGui::Instance().GetScaleFpsHelper().SetVisible(true);
#endif

  m_renderer.ScheduleOverlayCollecting();

#ifdef SHOW_FRAMES_STATS
  m_renderer.m_notifier->Notify(ThreadsCommutator::RenderThread, std::chrono::seconds(5),
                                true /* repeating */, [this](uint64_t)
  {
    LOG(LINFO, ("framesOverall =", m_renderer.m_frameData.m_framesOverall,
                "framesFast =", m_renderer.m_frameData.m_framesFast));
#ifdef TRACK_GPU_MEM
    LOG(LINFO, (dp::GPUMemTracker::Inst().GetMemorySnapshot().ToString()));
#endif
  });
#endif

#ifndef DRAPE_MEASURER_BENCHMARK
  DrapeMeasurer::Instance().SetApiVersion(m_renderer.m_apiVersion);
  DrapeMeasurer::Instance().SetResolution(m2::PointU(m_renderer.m_viewport.GetWidth(),
                                                     m_renderer.m_viewport.GetHeight()));
  DrapeMeasurer::Instance().SetGpuName(m_renderer.m_context->GetRendererName());
  DrapeMeasurer::Instance().Start();
#endif
  while (!IsCancelled())
    m_renderer.IterateRenderLoop();
#ifndef DRAPE_MEASURER_BENCHMARK
  DrapeMeasurer::Instance().Stop(true /* forceProcessRealtimeStats */);
#endif

  m_renderer.CollectShowOverlaysEvents();
  m_renderer.ReleaseResources();
}

void FrontendRenderer::ReleaseResources()
{
  for (RenderLayer & layer : m_layers)
    layer.m_renderGroups.clear();

  m_guiRenderer.reset();
  m_myPositionController.reset();
  m_selectionShape.reset();
  m_routeRenderer.reset();
  m_buildingsFramebuffer.reset();
  m_screenQuadRenderer.reset();
  m_trafficRenderer.reset();
  m_transitSchemeRenderer.reset();
  m_postprocessRenderer.reset();
  m_transitBackground.reset();
  m_gpuProgramManager.reset();

  // Here m_context can be nullptr, so call the method
  // for the context from the factory.
  m_contextFactory->GetDrawContext()->DoneCurrent();
  m_context = nullptr;
}

void FrontendRenderer::AddUserEvent(drape_ptr<UserEvent> && event)
{
#ifdef SCENARIO_ENABLE
  if (m_scenarioManager->IsRunning() && event->GetType() == UserEvent::EventType::Touch)
    return;
#endif
  m_userEventStream.AddEvent(std::move(event));
  if (IsInInfinityWaiting())
    CancelMessageWaiting();
}

void FrontendRenderer::PositionChanged(m2::PointD const & position, bool hasPosition)
{
  m_userPositionChangedHandler(position, hasPosition);
}

void FrontendRenderer::ChangeModelView(m2::PointD const & center, int zoomLevel,
                                       TAnimationCreator const & parallelAnimCreator)
{
  AddUserEvent(make_unique_dp<SetCenterEvent>(center, zoomLevel, true /* isAnim */,
                                              false /* trackVisibleViewport */,
                                              parallelAnimCreator));
}

void FrontendRenderer::ChangeModelView(double azimuth,
                                       TAnimationCreator const & parallelAnimCreator)
{
  AddUserEvent(make_unique_dp<RotateEvent>(azimuth, true /* isAnim */, parallelAnimCreator));
}

void FrontendRenderer::ChangeModelView(m2::RectD const & rect,
                                       TAnimationCreator const & parallelAnimCreator)
{
  AddUserEvent(make_unique_dp<SetRectEvent>(rect, true /* rotate */, kDoNotChangeZoom, true /* isAnim */,
                                            false /* useVisibleViewport */, parallelAnimCreator));
}

void FrontendRenderer::ChangeModelView(m2::PointD const & userPos, double azimuth,
                                       m2::PointD const & pxZero, int preferredZoomLevel,
                                       Animation::TAction const & onFinishAction,
                                       TAnimationCreator const & parallelAnimCreator)
{
  AddUserEvent(make_unique_dp<FollowAndRotateEvent>(userPos, pxZero, azimuth, preferredZoomLevel,
                                                    true, onFinishAction, parallelAnimCreator));
}

void FrontendRenderer::ChangeModelView(double autoScale, m2::PointD const & userPos, double azimuth,
                                       m2::PointD const & pxZero, TAnimationCreator const & parallelAnimCreator)
{
  AddUserEvent(make_unique_dp<FollowAndRotateEvent>(userPos, pxZero, azimuth, autoScale, parallelAnimCreator));
}

void FrontendRenderer::OnEnterBackground()
{
  m_myPositionController->OnEnterBackground();
}

ScreenBase const & FrontendRenderer::ProcessEvents(bool & modelViewChanged, bool & viewportChanged, 
                                                   bool & needActiveFrame)
{
  TRACE_SECTION("[drape] ProcessEvents");
  ScreenBase const & modelView = m_userEventStream.ProcessEvents(modelViewChanged, viewportChanged, 
                                                                 needActiveFrame);
  gui::DrapeGui::Instance().SetInUserAction(m_userEventStream.IsInUserAction());

  // Location- or compass-update could have changed model view on the previous frame.
  // So we have to check it here.
  if (m_lastReadedModelView != modelView)
    modelViewChanged = true;

  return modelView;
}

void FrontendRenderer::PrepareScene(ScreenBase const & modelView)
{
  TRACE_SECTION("[drape] PrepareScene");
  RefreshZScale(modelView);
  RefreshPivotTransform(modelView);

  m_myPositionController->OnUpdateScreen(modelView);
  m_routeRenderer->PrepareRouteArrows(modelView, std::bind(&FrontendRenderer::OnPrepareRouteArrows, this, _1, _2));
}

void FrontendRenderer::UpdateScene(ScreenBase const & modelView)
{
  TRACE_SECTION("[drape] UpdateScene");
  ResolveZoomLevel(modelView);

  m_gpsTrackRenderer->Update();

  auto removePredicate = [this](drape_ptr<RenderGroup> const & group)
  {
    uint32_t const kMaxGenerationRange = 5;
    TileKey const & key = group->GetTileKey();

    return (GetDepthLayer(group->GetState()) == DepthLayer::OverlayLayer && key.m_zoomLevel > GetCurrentZoom()) ||
           (m_maxGeneration - key.m_generation > kMaxGenerationRange) ||
           (group->IsUserMark() &&
            (m_maxUserMarksGeneration - key.m_userMarksGeneration > kMaxGenerationRange));
  };
  for (RenderLayer & layer : m_layers)
    layer.m_isDirty |= RemoveGroups(removePredicate, layer.m_renderGroups, make_ref(m_overlayTree));

  if (m_forceUpdateScene || m_forceUpdateUserMarks || m_lastReadedModelView != modelView)
  {
    EmitModelViewChanged(modelView);
    m_lastReadedModelView = modelView;
    m_requestedTiles->Set(modelView, m_isIsometry || modelView.isPerspective(),
                          m_forceUpdateScene, m_forceUpdateUserMarks,
                          ResolveTileKeys(modelView));
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                              make_unique_dp<UpdateReadManagerMessage>(),
                              MessagePriority::UberHighSingleton);
    m_forceUpdateScene = false;
    m_forceUpdateUserMarks = false;
  }
}

void FrontendRenderer::EmitModelViewChanged(ScreenBase const & modelView) const
{
  m_modelViewChangedHandler(modelView);
}

#if defined(OMIM_OS_DESKTOP)
void FrontendRenderer::EmitGraphicsReady()
{
  if (m_graphicsStage == GraphicsStage::Rendered)
  {
    if (m_graphicsReadyFn)
      m_graphicsReadyFn();
    m_graphicsStage = GraphicsStage::Unknown;
    m_graphicsReadyFn = nullptr;
  }
}
#endif

void FrontendRenderer::OnPrepareRouteArrows(dp::DrapeID subrouteIndex, std::vector<ArrowBorders> && borders)
{
  m_commutator->PostMessage(ThreadsCommutator::RenderThread,
                            make_unique_dp<PrepareSubrouteArrowsMessage>(subrouteIndex, std::move(borders)),
                            MessagePriority::Normal);
}

void FrontendRenderer::OnCacheRouteArrows(dp::DrapeID subrouteIndex, std::vector<ArrowBorders> const & borders)
{
  m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                            make_unique_dp<CacheSubrouteArrowsMessage>(subrouteIndex, borders, m_lastRecacheRouteId),
                            MessagePriority::Normal);
}

void FrontendRenderer::CollectShowOverlaysEvents()
{
  ASSERT(m_overlaysShowStatsCallback != nullptr, ());
  m_overlaysShowStatsCallback(m_overlaysTracker->Collect());
}

void FrontendRenderer::CheckAndRunFirstLaunchAnimation()
{
  if (!m_firstTilesReady || m_firstLaunchAnimationInterrupted ||
      !m_myPositionController->IsModeHasPosition())
  {
    return;
  }

  int constexpr kDesiredZoomLevel = 13;
  m2::PointD const pos = m_myPositionController->GetDrawablePosition();
  AddUserEvent(make_unique_dp<SetCenterEvent>(pos, kDesiredZoomLevel, true /* isAnim */,
                                              false /* trackVisibleViewport */, nullptr /* parallelAnimCreator */));
}

void FrontendRenderer::ScheduleOverlayCollecting()
{
  auto const kCollectingEventsPeriod = std::chrono::seconds(10);
  m_notifier->Notify(ThreadsCommutator::RenderThread, kCollectingEventsPeriod,
                     true /* repeating */, [this](uint64_t)
  {
    if (m_overlaysTracker->IsValid())
      CollectShowOverlaysEvents();
  });
}

void FrontendRenderer::SearchInNonDisplaceableUserMarksLayer(ScreenBase const & modelView,
                                                             DepthLayer layerId,
                                                             m2::RectD const & selectionRect,
                                                             dp::TOverlayContainer & result)
{
  auto & layer = m_layers[static_cast<size_t>(layerId)];
  layer.Sort(nullptr);
  for (drape_ptr<RenderGroup> & group : layer.m_renderGroups)
  {
    group->SetOverlayVisibility(true);
    group->Update(modelView);
    group->ForEachOverlay(
      [&modelView, &result, selectionRect = m2::RectF(selectionRect)](ref_ptr<dp::OverlayHandle> const & h)
      {
        dp::OverlayHandle::Rects shapes;
        h->GetPixelShape(modelView, modelView.isPerspective(), shapes);
        for (m2::RectF const & shape : shapes)
        {
          if (selectionRect.IsIntersect(shape))
          {
            result.push_back(h);
            break;
          }
        }
      });
  }
}

void FrontendRenderer::RenderLayer::Sort(ref_ptr<dp::OverlayTree> overlayTree)
{
  if (!m_isDirty)
    return;

  RenderGroupComparator comparator;
  std::sort(m_renderGroups.begin(), m_renderGroups.end(), std::ref(comparator));
  m_isDirty = comparator.m_pendingOnDeleteFound;

  while (!m_renderGroups.empty() && m_renderGroups.back()->CanBeDeleted())
  {
    if (overlayTree)
      m_renderGroups.back()->RemoveOverlay(overlayTree);
    m_renderGroups.pop_back();
  }
}

// static
m2::AnyRectD TapInfo::GetDefaultTapRect(m2::PointD const & mercator, ScreenBase const & screen)
{
  m2::AnyRectD result;
  double const halfSize = VisualParams::Instance().GetTouchRectRadius();
  screen.GetTouchRect(screen.GtoP(mercator), halfSize, result);
  return result;
}

// static
m2::AnyRectD TapInfo::GetBookmarkTapRect(m2::PointD const & mercator, ScreenBase const & screen)
{
  static int constexpr kBmTouchPixelIncrease = 20;

  m2::AnyRectD result;
  double const bmAddition = kBmTouchPixelIncrease * VisualParams::Instance().GetVisualScale();
  double const halfSize = VisualParams::Instance().GetTouchRectRadius();
  double const pxWidth = halfSize;
  double const pxHeight = halfSize + bmAddition;
  screen.GetTouchRect(screen.GtoP(mercator) + m2::PointD(0, bmAddition),
                      pxWidth, pxHeight, result);
  return result;
}

// static
m2::AnyRectD TapInfo::GetRoutingPointTapRect(m2::PointD const & mercator, ScreenBase const & screen)
{
  static int constexpr kRoutingPointTouchPixelIncrease = 20;

  m2::AnyRectD result;
  double const bmAddition = kRoutingPointTouchPixelIncrease * VisualParams::Instance().GetVisualScale();
  double const halfSize = VisualParams::Instance().GetTouchRectRadius();
  screen.GetTouchRect(screen.GtoP(mercator), halfSize + bmAddition, result);
  return result;
}

// static
m2::AnyRectD TapInfo::GetPreciseTapRect(m2::PointD const & mercator, double eps)
{
  return m2::AnyRectD(mercator, ang::AngleD(0.0) /* angle */, m2::RectD(-eps, -eps, eps, eps));
}
}  // namespace df
