#include "app/organicmaps/Framework.hpp"

#include "app/organicmaps/core/jni_helper.hpp"
#include "app/organicmaps/UserMarkHelper.hpp"
#include "app/organicmaps/opengl/androidoglcontextfactory.hpp"
#include "app/organicmaps/platform/Platform.hpp"
#include "app/organicmaps/util/FeatureIdBuilder.hpp"
#include "app/organicmaps/util/NetworkPolicy.hpp"
#include "app/organicmaps/vulkan/android_vulkan_context_factory.hpp"

#include "map/bookmark_helpers.hpp"
#include "map/chart_generator.hpp"
#include "map/everywhere_search_params.hpp"
#include "map/user_mark.hpp"

#include "storage/storage_defines.hpp"
#include "storage/storage_helpers.hpp"

#include "drape_frontend/user_event_stream.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/pointers.hpp"
#include "drape/support_manager.hpp"
#include "drape/visual_scale.hpp"

#include "coding/files_container.hpp"

#include "geometry/angles.hpp"
#include "geometry/mercator.hpp"
#include "geometry/point_with_altitude.hpp"

#include "indexer/feature_altitude.hpp"

#include "routing/following_info.hpp"
#include "routing/speed_camera_manager.hpp"

#include "platform/country_file.hpp"
#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/location.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/network_policy.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"
#include "platform/settings.hpp"
#include "platform/localization.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/math.hpp"
#include "base/sunrise_sunset.hpp"

#include "3party/open-location-code/openlocationcode.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <android/api-level.h>

using namespace std;
using namespace std::placeholders;

unique_ptr<android::Framework> g_framework;

namespace platform
{
NetworkPolicy ToNativeNetworkPolicy(JNIEnv * env, jobject obj)
{
  return NetworkPolicy(network_policy::GetNetworkPolicyStatus(env, obj));
}
}  // namespace platform

using namespace storage;
using platform::CountryFile;
using platform::LocalCountryFile;
using platform::ToNativeNetworkPolicy;

static_assert(sizeof(int) >= 4, "Size of jint in less than 4 bytes.");

::Framework * frm() { return g_framework->NativeFramework(); }

namespace
{
jobject g_placePageActivationListener = nullptr;

android::AndroidVulkanContextFactory * CastFactory(drape_ptr<dp::GraphicsContextFactory> const & f)
{
  ASSERT(dynamic_cast<android::AndroidVulkanContextFactory *>(f.get()) != nullptr, ());
  return static_cast<android::AndroidVulkanContextFactory *>(f.get());
}
}  // namespace

namespace android
{

enum MultiTouchAction
{
  MULTITOUCH_UP    =   0x00000001,
  MULTITOUCH_DOWN  =   0x00000002,
  MULTITOUCH_MOVE  =   0x00000003,
  MULTITOUCH_CANCEL =  0x00000004
};

Framework::Framework()
  : m_lastCompass(0.0)
  , m_isChoosePositionMode(false)
{
  m_work.GetTrafficManager().SetStateListener(bind(&Framework::TrafficStateChanged, this, _1));
  m_work.GetTransitManager().SetStateListener(bind(&Framework::TransitSchemeStateChanged, this, _1));
  m_work.GetIsolinesManager().SetStateListener(bind(&Framework::IsolinesSchemeStateChanged, this, _1));
  m_work.GetPowerManager().Subscribe(this);
}

void Framework::OnLocationError(int errorCode)
{
  m_work.OnLocationError(static_cast<location::TLocationError>(errorCode));
}

void Framework::OnLocationUpdated(location::GpsInfo const & info)
{
  m_work.OnLocationUpdate(info);
}

void Framework::OnCompassUpdated(location::CompassInfo const & info, bool forceRedraw)
{
  static double const COMPASS_THRESHOLD = base::DegToRad(1.0);

  /// @todo Do not emit compass bearing too often.
  /// Need to make more experiments in future.
  if (forceRedraw || fabs(ang::GetShortestDistance(m_lastCompass, info.m_bearing)) >= COMPASS_THRESHOLD)
  {
    m_lastCompass = info.m_bearing;
    m_work.OnCompassUpdate(info);
  }
}

void Framework::MyPositionModeChanged(location::EMyPositionMode mode, bool routingActive)
{
  if (m_myPositionModeSignal)
    m_myPositionModeSignal(mode, routingActive);
}

void Framework::TrafficStateChanged(TrafficManager::TrafficState state)
{
  if (m_onTrafficStateChangedFn)
    m_onTrafficStateChangedFn(state);
}

void Framework::TransitSchemeStateChanged(TransitReadManager::TransitSchemeState state)
{
  if (m_onTransitStateChangedFn)
    m_onTransitStateChangedFn(state);
}

void Framework::IsolinesSchemeStateChanged(IsolinesManager::IsolinesState state)
{
  if (m_onIsolinesStateChangedFn)
    m_onIsolinesStateChangedFn(state);
}

bool Framework::DestroySurfaceOnDetach(df::DrapeEngineId engineId)
{
  if (m_drapeEngines[engineId].m_vulkanContextFactory)
    return false;
  return true;
}

df::DrapeEngineId Framework::CreateDrapeEngineId()
{
  df::DrapeEngineId engineId = m_work.CreateDrapeEngineId();
  m_drapeEngines[engineId];
  return engineId;
}

bool Framework::CreateDrapeEngine(JNIEnv * env, df::DrapeEngineId engineId, jobject jSurface, int densityDpi,
                                  bool firstLaunch, bool launchByDeepLink, uint32_t appVersionCode)
{
  // Vulkan is supported only since Android 8.0, because some Android devices with Android 7.x
  // have fatal driver issue, which can lead to process termination and whole OS destabilization.
  int constexpr kMinSdkVersionForVulkan = 26;
  int const sdkVersion = android_get_device_api_level();
  LOG(LINFO, ("Android SDK version in the Drape Engine:", sdkVersion));
  auto const vulkanForbidden = sdkVersion < kMinSdkVersionForVulkan ||
                               dp::SupportManager::Instance().IsVulkanForbidden();
  if (vulkanForbidden)
    LOG(LWARNING, ("Vulkan API is forbidden on this device."));

  DrapeEngineData &drapeEngineData = m_drapeEngines[engineId];

  if (m_work.LoadPreferredGraphicsAPI() == dp::ApiVersion::Vulkan && !vulkanForbidden)
  {
    drapeEngineData.m_vulkanContextFactory =
        make_unique_dp<AndroidVulkanContextFactory>(appVersionCode, sdkVersion);
    if (!CastFactory(drapeEngineData.m_vulkanContextFactory)->IsVulkanSupported())
    {
      LOG(LWARNING, ("Vulkan API is not supported."));
      drapeEngineData.m_vulkanContextFactory.reset();
    }

    if (drapeEngineData.m_vulkanContextFactory)
    {
      auto f = CastFactory(drapeEngineData.m_vulkanContextFactory);
      f->SetSurface(env, jSurface);
      if (!f->IsValid())
      {
        LOG(LWARNING, ("Invalid Vulkan API context."));
        drapeEngineData.m_vulkanContextFactory.reset();
      }
    }
  }

  AndroidOGLContextFactory * oglFactory = nullptr;
  if (!drapeEngineData.m_vulkanContextFactory)
  {
    drapeEngineData.m_oglContextFactory = make_unique_dp<dp::ThreadSafeFactory>(
      new AndroidOGLContextFactory(env, jSurface));
    oglFactory = drapeEngineData.m_oglContextFactory->CastFactory<AndroidOGLContextFactory>();
    if (!oglFactory->IsValid())
    {
      LOG(LWARNING, ("Invalid GL context."));
      return false;
    }
  }

  ::Framework::DrapeCreationParams p;
  if (drapeEngineData.m_vulkanContextFactory)
  {
    auto f = CastFactory(drapeEngineData.m_vulkanContextFactory);
    p.m_apiVersion = dp::ApiVersion::Vulkan;
    p.m_surfaceWidth = f->GetWidth();
    p.m_surfaceHeight = f->GetHeight();
  }
  else
  {
    CHECK(oglFactory != nullptr, ());
    p.m_apiVersion = oglFactory->IsSupportedOpenGLES3() ? dp::ApiVersion::OpenGLES3 :
                                                          dp::ApiVersion::OpenGLES2;
    p.m_surfaceWidth = oglFactory->GetWidth();
    p.m_surfaceHeight = oglFactory->GetHeight();
  }
  p.m_visualScale = static_cast<float>(dp::VisualScale(densityDpi));
  p.m_isChoosePositionMode = m_isChoosePositionMode;
  p.m_hints.m_isFirstLaunch = firstLaunch;
  p.m_hints.m_isLaunchByDeepLink = launchByDeepLink;
  ASSERT(!drapeEngineData.m_guiPositions.empty(), ("GUI elements must be set-up before engine is created"));
  p.m_widgetsInitInfo = drapeEngineData.m_guiPositions;

  m_work.SetMyPositionModeListener(bind(&Framework::MyPositionModeChanged, this, _1, _2));

  if (drapeEngineData.m_vulkanContextFactory)
    m_work.CreateDrapeEngine(engineId, make_ref(drapeEngineData.m_vulkanContextFactory), move(p));
  else
    m_work.CreateDrapeEngine(engineId, make_ref(drapeEngineData.m_oglContextFactory), move(p));

  m_work.EnterForeground();

  return true;
}

bool Framework::IsDrapeEngineCreated(df::DrapeEngineId engineId) const
{
  return m_work.IsDrapeEngineCreated(engineId);
}

void Framework::Resize(JNIEnv * env, df::DrapeEngineId engineId, jobject jSurface, int w, int h)
{
  auto& drapeEngineData = m_drapeEngines[engineId];
  if (drapeEngineData.m_vulkanContextFactory)
  {
    auto vulkanContextFactory = CastFactory(drapeEngineData.m_vulkanContextFactory);
    if (vulkanContextFactory->GetWidth() != w || vulkanContextFactory->GetHeight() != h)
    {
      drapeEngineData.m_vulkanContextFactory->SetPresentAvailable(false);
      m_work.SetRenderingDisabled(false /* destroySurface */);

      vulkanContextFactory->ChangeSurface(env, jSurface, w, h);

      vulkanContextFactory->SetPresentAvailable(true);
      m_work.SetRenderingEnabled();
    }
  }
  else
  {
    drapeEngineData.m_oglContextFactory->CastFactory<AndroidOGLContextFactory>()->UpdateSurfaceSize(w, h);
  }
  m_work.OnSize(engineId, w, h);
}

void Framework::DetachSurface(df::DrapeEngineId engineId, bool destroySurface)
{
  LOG(LINFO, ("Detach surface started. destroySurface =", destroySurface));
  auto& drapeEngineData = m_drapeEngines[engineId];
  if (drapeEngineData.m_vulkanContextFactory)
  {
    drapeEngineData.m_vulkanContextFactory->SetPresentAvailable(false);
  }
  else
  {
    ASSERT(drapeEngineData.m_oglContextFactory != nullptr, ());
    drapeEngineData.m_oglContextFactory->SetPresentAvailable(false);
  }

  if (destroySurface)
  {
    LOG(LINFO, ("Destroy surface."));
    drapeEngineData.m_isSurfaceDestroyed = true;
    m_work.OnDestroySurface();
  }

  if (drapeEngineData.m_vulkanContextFactory)
  {
    // With Vulkan we don't need to recreate all graphics resources,
    // we have to destroy only resources bound with surface (swapchains,
    // image views, framebuffers and command buffers). All these resources will be
    // destroyed in ResetSurface().
    m_work.SetRenderingDisabled(false /* destroySurface */);

    // Allow pipeline dump only on enter background.
    CastFactory(drapeEngineData.m_vulkanContextFactory)->ResetSurface(destroySurface /* allowPipelineDump */);
  }
  else
  {
    m_work.SetRenderingDisabled(destroySurface);
    auto factory = drapeEngineData.m_oglContextFactory->CastFactory<AndroidOGLContextFactory>();
    factory->ResetSurface();
  }
  LOG(LINFO, ("Detach surface finished."));
}

bool Framework::AttachSurface(JNIEnv * env, df::DrapeEngineId engineId, jobject jSurface)
{
  LOG(LINFO, ("Attach surface started."));
  auto& drapeEngineData = m_drapeEngines[engineId];
  int w = 0, h = 0;
  if (drapeEngineData.m_vulkanContextFactory)
  {
    auto factory = CastFactory(drapeEngineData.m_vulkanContextFactory);
    factory->SetSurface(env, jSurface);
    if (!factory->IsValid())
    {
      LOG(LWARNING, ("Invalid Vulkan API context."));
      return false;
    }
    w = factory->GetWidth();
    h = factory->GetHeight();
  }
  else
  {
    ASSERT(drapeEngineData.m_oglContextFactory != nullptr, ());
    auto factory = drapeEngineData.m_oglContextFactory->CastFactory<AndroidOGLContextFactory>();
    factory->SetSurface(env, jSurface);
    if (!factory->IsValid())
    {
      LOG(LWARNING, ("Invalid GL context."));
      return false;
    }
    w = factory->GetWidth();
    h = factory->GetHeight();
  }

  ASSERT(!drapeEngineData.m_guiPositions.empty(), ("GUI elements must be set-up before engine is created"));

  if (drapeEngineData.m_vulkanContextFactory)
  {
    drapeEngineData.m_vulkanContextFactory->SetPresentAvailable(true);
    m_work.SetRenderingEnabled();
  }
  else
  {
    drapeEngineData.m_oglContextFactory->SetPresentAvailable(true);
    m_work.SetRenderingEnabled(make_ref(drapeEngineData.m_oglContextFactory));
  }

  if (drapeEngineData.m_isSurfaceDestroyed)
  {
    LOG(LINFO, ("Recover surface, viewport size:", w, h));
    bool const recreateContextDependentResources = (drapeEngineData.m_vulkanContextFactory == nullptr);
    m_work.OnRecoverSurface(engineId, w, h, recreateContextDependentResources);
    drapeEngineData.m_isSurfaceDestroyed = false;
  }

  LOG(LINFO, ("Attach surface finished."));

  return true;
}

void Framework::PauseSurfaceRendering(df::DrapeEngineId engineId)
{
  auto& drapeEngineData = m_drapeEngines[engineId];
  if (drapeEngineData.m_vulkanContextFactory)
    drapeEngineData.m_vulkanContextFactory->SetPresentAvailable(false);
  if (drapeEngineData.m_oglContextFactory)
    drapeEngineData.m_oglContextFactory->SetPresentAvailable(false);

  LOG(LINFO, ("Pause surface rendering."));
}

void Framework::ResumeSurfaceRendering(df::DrapeEngineId engineId)
{
  auto& drapeEngineData = m_drapeEngines[engineId];
  if (drapeEngineData.m_vulkanContextFactory)
  {
    if (CastFactory(drapeEngineData.m_vulkanContextFactory)->IsValid())
      drapeEngineData.m_vulkanContextFactory->SetPresentAvailable(true);
  }
  if (drapeEngineData.m_oglContextFactory)
  {
    AndroidOGLContextFactory * factory = drapeEngineData.m_oglContextFactory->CastFactory<AndroidOGLContextFactory>();
    if (factory->IsValid())
      factory->SetPresentAvailable(true);
  }
  LOG(LINFO, ("Resume surface rendering."));
}

void Framework::SetMapStyle(MapStyle mapStyle)
{
  m_work.SetMapStyle(mapStyle);
}

void Framework::MarkMapStyle(MapStyle mapStyle)
{
  // In case of Vulkan rendering we don't recreate geometry and textures data, so
  // we need use SetMapStyle instead of MarkMapStyle in all cases.
  for (const auto& [engineId, engineData] : m_drapeEngines)
  {
    if (engineData.m_vulkanContextFactory)
      m_work.SetMapStyle(mapStyle);
    else
      m_work.MarkMapStyle(mapStyle);
  }
}

MapStyle Framework::GetMapStyle() const
{
  return m_work.GetMapStyle();
}

void Framework::Save3dMode(bool allow3d, bool allow3dBuildings)
{
  m_work.Save3dMode(allow3d, allow3dBuildings);
}

void Framework::Set3dMode(bool allow3d, bool allow3dBuildings)
{
  m_work.Allow3dMode(allow3d, allow3dBuildings);
}

void Framework::Get3dMode(bool & allow3d, bool & allow3dBuildings)
{
  m_work.Load3dMode(allow3d, allow3dBuildings);
}

void Framework::SetChoosePositionMode(bool isChoosePositionMode, bool isBusiness,
                                      bool hasPosition, m2::PointD const & position)
{
  m_isChoosePositionMode = isChoosePositionMode;
  m_work.BlockTapEvents(isChoosePositionMode);
  m_work.EnableChoosePositionMode(isChoosePositionMode, isBusiness, hasPosition, position);
}

bool Framework::GetChoosePositionMode()
{
  return m_isChoosePositionMode;
}

Storage & Framework::GetStorage()
{
  return m_work.GetStorage();
}

DataSource const & Framework::GetDataSource() { return m_work.GetDataSource(); }

void Framework::ShowNode(CountryId const & idx, bool zoomToDownloadButton)
{
  if (zoomToDownloadButton)
  {
    m2::RectD const rect = CalcLimitRect(idx, m_work.GetStorage(), m_work.GetCountryInfoGetter());
    m_work.SetViewportCenter(rect.Center(), 10);
  }
  else
  {
    m_work.ShowNode(idx);
  }
}

void Framework::Scale(df::DrapeEngineId engineId, double factor, m2::PointD const & pxPoint, bool isAnim)
{
  m_work.Scale(engineId, factor, pxPoint, isAnim);
}

void Framework::Move(df::DrapeEngineId engineId, double factorX, double factorY, bool isAnim)
{
  m_work.Move(engineId, factorX, factorY, isAnim);
}

void Framework::Touch(df::DrapeEngineId engineId, int action, Finger const & f1, Finger const & f2, uint8_t maskedPointer)
{
  MultiTouchAction eventType = static_cast<MultiTouchAction>(action);
  df::TouchEvent event;

  switch(eventType)
  {
  case MULTITOUCH_DOWN:
    event.SetTouchType(df::TouchEvent::TOUCH_DOWN);
    break;
  case MULTITOUCH_MOVE:
    event.SetTouchType(df::TouchEvent::TOUCH_MOVE);
    break;
  case MULTITOUCH_UP:
    event.SetTouchType(df::TouchEvent::TOUCH_UP);
    break;
  case MULTITOUCH_CANCEL:
    event.SetTouchType(df::TouchEvent::TOUCH_CANCEL);
    break;
  default:
    return;
  }

  df::Touch touch;
  touch.m_location = m2::PointD(f1.m_x, f1.m_y);
  touch.m_id = f1.m_id;
  event.SetFirstTouch(touch);

  touch.m_location = m2::PointD(f2.m_x, f2.m_y);
  touch.m_id = f2.m_id;
  event.SetSecondTouch(touch);

  event.SetFirstMaskedPointer(maskedPointer);
  m_work.TouchEvent(engineId, event);
}

m2::PointD Framework::GetViewportCenter() const
{
  return m_work.GetViewportCenter();
}

void Framework::AddString(string const & name, string const & value)
{
  m_work.AddString(name, value);
}

void Framework::Scale(df::DrapeEngineId engineId, ::Framework::EScaleMode mode)
{
  m_work.Scale(engineId, mode, true);
}

void Framework::Scale(df::DrapeEngineId engineId, m2::PointD const & centerPt, int targetZoom, bool animate)
{
  ref_ptr<df::DrapeEngine> engine = m_work.GetDrapeEngine(engineId);
  if (engine)
    engine->SetModelViewCenter(centerPt, targetZoom, animate, false);
}

::Framework * Framework::NativeFramework()
{
  return &m_work;
}

bool Framework::Search(search::EverywhereSearchParams const & params)
{
  m_searchQuery = params.m_query;
  return m_work.GetSearchAPI().SearchEverywhere(params);
}

void Framework::AddLocalMaps()
{
  m_work.RegisterAllMaps();
}

void Framework::RemoveLocalMaps()
{
  m_work.DeregisterAllMaps();
}

void Framework::ReloadWorldMaps()
{
  /// @todo Can invent more optimal routine to remove/add World files only.
  RemoveLocalMaps();
  AddLocalMaps();
}

void Framework::ReplaceBookmark(kml::MarkId markId, kml::BookmarkData & bm)
{
  m_work.GetBookmarkManager().GetEditSession().UpdateBookmark(markId, bm);
}

void Framework::MoveBookmark(kml::MarkId markId, kml::MarkGroupId curCat, kml::MarkGroupId newCat)
{
  m_work.GetBookmarkManager().GetEditSession().MoveBookmark(markId, curCat, newCat);
}

bool Framework::ShowMapForURL(string const & url)
{
  return m_work.ShowMapForURL(url);
}

void Framework::DeactivatePopup()
{
  m_work.DeactivateMapSelection(false);
}

/*
string Framework::GetOutdatedCountriesString()
{
  vector<Country const *> countries;
  class Storage const & storage = GetStorage();
  storage.GetOutdatedCountries(countries);

  string res;
  NodeAttrs attrs;

  for (size_t i = 0; i < countries.size(); ++i)
  {
    storage.GetNodeAttrs(countries[i]->Name(), attrs);

    if (i > 0)
      res += ", ";

    res += attrs.m_nodeLocalName;
  }

  return res;
}
*/

void Framework::SetTrafficStateListener(TrafficManager::TrafficStateChangedFn const & fn)
{
  m_onTrafficStateChangedFn = fn;
}

void Framework::SetTransitSchemeListener(TransitReadManager::TransitStateChangedFn const & function)
{
  m_onTransitStateChangedFn = function;
}

void Framework::SetIsolinesListener(IsolinesManager::IsolinesStateChangedFn const & function)
{
  m_onIsolinesStateChangedFn = function;
}

bool Framework::IsTrafficEnabled()
{
  return m_work.GetTrafficManager().IsEnabled();
}

void Framework::EnableTraffic()
{
  m_work.GetTrafficManager().SetEnabled(true);
  NativeFramework()->SaveTrafficEnabled(true);
}

void Framework::DisableTraffic()
{
  m_work.GetTrafficManager().SetEnabled(false);
  NativeFramework()->SaveTrafficEnabled(false);
}

void Framework::SetMyPositionModeListener(location::TMyPositionModeChanged const & fn)
{
  m_myPositionModeSignal = fn;
}

location::EMyPositionMode Framework::GetMyPositionMode() const
{
  // No need in assertion here, return location::PendingPosition if no engine created.
  //ASSERT(IsDrapeEngineCreated(), ());

  return m_work.GetMyPositionMode();
}

void Framework::SwitchMyPositionNextMode()
{
  ASSERT(IsDrapeEngineCreated(0), ());
  m_work.SwitchMyPositionNextMode();
}

void Framework::SetupWidget(df::DrapeEngineId engineId, gui::EWidget widget, float x, float y, dp::Anchor anchor)
{
  m_drapeEngines[engineId].m_guiPositions[widget] = gui::Position(m2::PointF(x, y), anchor);
}

void Framework::ApplyWidgets(df::DrapeEngineId engineId)
{
  gui::TWidgetsLayoutInfo layout;
  for (auto const & widget : m_drapeEngines[engineId].m_guiPositions)
    layout[widget.first] = widget.second.m_pixelPivot;

  m_work.SetWidgetLayout(engineId, move(layout));
}

void Framework::CleanWidgets(df::DrapeEngineId engineId)
{
  m_drapeEngines[engineId].m_guiPositions.clear();
}

void Framework::SetupMeasurementSystem()
{
  m_work.SetupMeasurementSystem();
}

place_page::Info & Framework::GetPlacePageInfo()
{
  return m_work.GetCurrentPlacePageInfo();
}

bool Framework::IsAutoRetryDownloadFailed()
{
  return m_work.GetDownloadingPolicy().IsAutoRetryDownloadFailed();
}

bool Framework::IsDownloadOn3gEnabled()
{
  return m_work.GetDownloadingPolicy().IsCellularDownloadEnabled();
}

void Framework::EnableDownloadOn3g()
{
  m_work.GetDownloadingPolicy().EnableCellularDownload(true);
}

/*
int Framework::ToDoAfterUpdate() const
{
  return (int) m_work.ToDoAfterUpdate();
}
*/

void Framework::OnPowerFacilityChanged(power_management::Facility const facility, bool enabled)
{
  // Dummy
  // TODO: provide information for UI Properties.
}

void Framework::OnPowerSchemeChanged(power_management::Scheme const actualScheme)
{
  // Dummy
  // TODO: provide information for UI Properties.
}

FeatureID Framework::BuildFeatureId(JNIEnv * env, jobject featureId)
{
  static FeatureIdBuilder const builder(env);

  return builder.Build(env, featureId);
}
}  // namespace android

//============ GLUE CODE for app.organicmaps.Framework class =============//
/*            ____
 *          _ |||| _
 *          \\    //
 *           \\  //
 *            \\//
 *             \/
 */

extern "C"
{
void CallRoutingListener(shared_ptr<jobject> listener, int errorCode,
                         storage::CountriesSet const & absentMaps)
{
  JNIEnv * env = jni::GetEnv();
  jmethodID const method = jni::GetMethodID(env, *listener, "onRoutingEvent", "(I[Ljava/lang/String;)V");
  ASSERT(method, ());

  env->CallVoidMethod(*listener, method, errorCode, jni::TScopedLocalObjectArrayRef(env, jni::ToJavaStringArray(env, absentMaps)).get());
}

void CallRouteProgressListener(shared_ptr<jobject> listener, float progress)
{
  JNIEnv * env = jni::GetEnv();
  jmethodID const methodId = jni::GetMethodID(env, *listener, "onRouteBuildingProgress", "(F)V");
  env->CallVoidMethod(*listener, methodId, progress);
}

void CallRouteRecommendationListener(shared_ptr<jobject> listener,
                                     RoutingManager::Recommendation recommendation)
{
  JNIEnv * env = jni::GetEnv();
  jmethodID const methodId = jni::GetMethodID(env, *listener, "onRecommend", "(I)V");
  env->CallVoidMethod(*listener, methodId, static_cast<int>(recommendation));
}

void CallSetRoutingLoadPointsListener(shared_ptr<jobject> listener, bool success)
{
  JNIEnv * env = jni::GetEnv();
  jmethodID const methodId = jni::GetMethodID(env, *listener, "onRoutePointsLoaded", "(Z)V");
  env->CallVoidMethod(*listener, methodId, static_cast<jboolean>(success));
}

RoutingManager::LoadRouteHandler g_loadRouteHandler;

/// @name JNI EXPORTS
JNIEXPORT jstring JNICALL
Java_app_organicmaps_Framework_nativeGetAddress(JNIEnv * env, jclass clazz, jdouble lat, jdouble lon)
{
  auto const info = frm()->GetAddressAtPoint(mercator::FromLatLon(lat, lon));
  return jni::ToJavaString(env, info.FormatAddress());
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeClearApiPoints(JNIEnv * env, jclass clazz)
{
  frm()->GetBookmarkManager().GetEditSession().ClearGroup(UserMark::Type::API);
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_Framework_nativeParseAndSetApiUrl(JNIEnv * env, jclass clazz, jstring url)
{
  static jmethodID const resultConstructor = jni::GetConstructorID(env, g_parsingResultClazz, "(IZ)V");
  auto const result = frm()->ParseAndSetApiURL(jni::ToNativeString(env, url));
  return env->NewObject(g_parsingResultClazz, resultConstructor, static_cast<jint>(result.m_type),
                        static_cast<jboolean>(result.m_isSuccess));
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_Framework_nativeGetParsedRoutingData(JNIEnv * env, jclass clazz)
{
  using namespace url_scheme;
  static jclass const pointClazz = jni::GetGlobalClassRef(env, "app/organicmaps/api/RoutePoint");
  // Java signature : RoutePoint(double lat, double lon, String name)
  static jmethodID const pointConstructor = jni::GetConstructorID(env, pointClazz, "(DDLjava/lang/String;)V");

  static jclass const routeDataClazz = jni::GetGlobalClassRef(env, "app/organicmaps/api/ParsedRoutingData");
  // Java signature : ParsedRoutingData(RoutePoint[] points, int routerType) {
  static jmethodID const routeDataConstructor = jni::GetConstructorID(env, routeDataClazz, "([Lapp/organicmaps/api/RoutePoint;I)V");

  auto const & routingData = frm()->GetParsedRoutingData();
  jobjectArray points = jni::ToJavaArray(env, pointClazz, routingData.m_points,
                                         [](JNIEnv * env, RoutePoint const & point)
                                         {
                                           jni::TScopedLocalRef const name(env, jni::ToJavaString(env, point.m_name));
                                           return env->NewObject(pointClazz, pointConstructor,
                                                                 mercator::YToLat(point.m_org.y),
                                                                 mercator::XToLon(point.m_org.x), name.get());
                                         });

  return env->NewObject(routeDataClazz, routeDataConstructor, points, routingData.m_type);
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_Framework_nativeGetParsedSearchRequest(JNIEnv * env, jclass clazz)
{
  using namespace url_scheme;
  static jclass const cl = jni::GetGlobalClassRef(env, "app/organicmaps/api/ParsedSearchRequest");
  // Java signature : ParsedSearchRequest(String query, String locale, double lat, double lon, boolean isSearchOnMap)
  static jmethodID const ctor = jni::GetConstructorID(env, cl, "(Ljava/lang/String;Ljava/lang/String;DDZ)V");
  auto const & r = frm()->GetParsedSearchRequest();
  ms::LatLon const center = frm()->GetParsedCenterLatLon();
  return env->NewObject(cl, ctor, jni::ToJavaString(env, r.m_query), jni::ToJavaString(env, r.m_locale), center.m_lat, center.m_lon, r.m_isSearchOnMap);
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_Framework_nativeGetParsedAppName(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, frm()->GetParsedAppName());
}

JNIEXPORT jdoubleArray JNICALL
Java_app_organicmaps_Framework_nativeGetParsedCenterLatLon(JNIEnv * env, jclass)
{
  ms::LatLon const center = frm()->GetParsedCenterLatLon();

  double latlon[] = {center.m_lat, center.m_lon};
  jdoubleArray jLatLon = env->NewDoubleArray(2);
  env->SetDoubleArrayRegion(jLatLon, 0, 2, latlon);

  return jLatLon;
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativePlacePageActivationListener(JNIEnv *env, jclass clazz,
                                                                     jobject jListener)
{
  LOG(LINFO, ("Set global map object listener"));
  g_placePageActivationListener = env->NewGlobalRef(jListener);
  // void onPlacePageActivated(MapObject object);
  jmethodID const activatedId = jni::GetMethodID(env, g_placePageActivationListener,
                                                 "onPlacePageActivated",
                                                 "(Lapp/organicmaps/widget/placepage/PlacePageData;)V");
  // void onPlacePageDeactivated(boolean switchFullScreenMode);
  jmethodID const deactivateId = jni::GetMethodID(env, g_placePageActivationListener,
                                                  "onPlacePageDeactivated", "(Z)V");
  auto const fillPlacePage = [activatedId]()
  {
    JNIEnv * env = jni::GetEnv();
    auto const & info = frm()->GetCurrentPlacePageInfo();
    jni::TScopedLocalRef placePageDataRef(env, nullptr);
    if (info.IsTrack())
    {
      auto const elevationInfo = frm()->GetBookmarkManager().MakeElevationInfo(info.GetTrackId());
      placePageDataRef.reset(usermark_helper::CreateElevationInfo(env, elevationInfo));
    }
    else
    {
      placePageDataRef.reset(usermark_helper::CreateMapObject(env, info));
    }
    env->CallVoidMethod(g_placePageActivationListener, activatedId, placePageDataRef.get());
  };
  auto const closePlacePage = [deactivateId](bool switchFullScreenMode)
  {
    JNIEnv * env = jni::GetEnv();
    env->CallVoidMethod(g_placePageActivationListener, deactivateId, switchFullScreenMode);
  };
  frm()->SetPlacePageListeners(fillPlacePage, closePlacePage, fillPlacePage);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeRemovePlacePageActivationListener(JNIEnv *env, jclass)
{
  if (g_placePageActivationListener == nullptr)
    return;

  frm()->SetPlacePageListeners({} /* onOpen */, {} /* onClose */, {} /* onUpdate */);
  LOG(LINFO, ("Remove global map object listener"));
  env->DeleteGlobalRef(g_placePageActivationListener);
  g_placePageActivationListener = nullptr;
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_Framework_nativeGetGe0Url(JNIEnv * env, jclass, jdouble lat, jdouble lon, jdouble zoomLevel, jstring name)
{
  ::Framework * fr = frm();
  double const scale = (zoomLevel > 0 ? zoomLevel : fr->GetDrawScale());
  string const url = fr->CodeGe0url(lat, lon, scale, jni::ToNativeString(env, name));
  return jni::ToJavaString(env, url);
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_Framework_nativeGetDistanceAndAzimuth(
    JNIEnv * env, jclass, jdouble merX, jdouble merY, jdouble cLat, jdouble cLon, jdouble north)
{
  string distance;
  double azimut = -1.0;
  frm()->GetDistanceAndAzimut(m2::PointD(merX, merY), cLat, cLon, north, distance, azimut);

  static jclass const daClazz = jni::GetGlobalClassRef(env, "app/organicmaps/bookmarks/data/DistanceAndAzimut");
  // Java signature : DistanceAndAzimut(String distance, double azimuth)
  static jmethodID const methodID = jni::GetConstructorID(env, daClazz, "(Ljava/lang/String;D)V");

  return env->NewObject(daClazz, methodID,
                        jni::ToJavaString(env, distance.c_str()),
                        static_cast<jdouble>(azimut));
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_Framework_nativeGetDistanceAndAzimuthFromLatLon(
    JNIEnv * env, jclass clazz, jdouble lat, jdouble lon, jdouble cLat, jdouble cLon, jdouble north)
{
  double const merY = mercator::LatToY(lat);
  double const merX = mercator::LonToX(lon);
  return Java_app_organicmaps_Framework_nativeGetDistanceAndAzimuth(env, clazz, merX, merY, cLat, cLon, north);
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_Framework_nativeFormatLatLon(JNIEnv * env, jclass, jdouble lat, jdouble lon, int coordsFormat)
{
  switch (static_cast<android::CoordinatesFormat>(coordsFormat))
  {
    default:
    case android::CoordinatesFormat::LatLonDMS: // DMS, comma separated
      return jni::ToJavaString(env, measurement_utils::FormatLatLonAsDMS(lat, lon, true /*withComma*/, 2));
    case android::CoordinatesFormat::LatLonDecimal: // Decimal, comma separated
      return jni::ToJavaString(env, measurement_utils::FormatLatLon(lat, lon, true /* withComma */, 6));
    case android::CoordinatesFormat::OLCFull: // Open location code, long format
      return jni::ToJavaString(env, openlocationcode::Encode({lat, lon}));
    case android::CoordinatesFormat::OSMLink: // Link to osm.org
      return jni::ToJavaString(env, measurement_utils::FormatOsmLink(lat, lon, 14));
  }
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_Framework_nativeFormatAltitude(JNIEnv * env, jclass, jdouble alt)
{
  auto const localizedUnits = platform::GetLocalizedAltitudeUnits();
  return jni::ToJavaString(env, measurement_utils::FormatAltitudeWithLocalization(alt, localizedUnits.m_low));
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_Framework_nativeFormatSpeed(JNIEnv * env, jclass, jdouble speed)
{
  auto const units = measurement_utils::GetMeasurementUnits();
  return jni::ToJavaString(env, measurement_utils::FormatSpeedNumeric(speed, units) + " " +
                                platform::GetLocalizedSpeedUnits(units));
}

/*
JNIEXPORT jobject JNICALL
Java_app_organicmaps_Framework_nativeGetOutdatedCountriesString(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, g_framework->GetOutdatedCountriesString());
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_Framework_nativeGetOutdatedCountries(JNIEnv * env, jclass)
{
  vector<Country const *> countries;
  Storage const & storage = g_framework->GetStorage();
  storage.GetOutdatedCountries(countries);

  vector<string> ids;
  for (auto country : countries)
    ids.push_back(country->Name());

  return jni::ToJavaStringArray(env, ids);
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_Framework_nativeToDoAfterUpdate(JNIEnv * env, jclass)
{
  return g_framework->ToDoAfterUpdate();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeIsDataVersionChanged(JNIEnv * env, jclass)
{
  return frm()->IsDataVersionUpdated() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeUpdateSavedDataVersion(JNIEnv * env, jclass)
{
  frm()->UpdateSavedDataVersion();
}
*/

JNIEXPORT jlong JNICALL
Java_app_organicmaps_Framework_nativeGetDataVersion(JNIEnv * env, jclass)
{
  return frm()->GetCurrentDataVersion();
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_Framework_nativeGetDrawScale(JNIEnv * env, jclass)
{
  return static_cast<jint>(frm()->GetDrawScale());
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativePokeSearchInViewport(JNIEnv * env, jclass)
{
  frm()->GetSearchAPI().PokeSearchInViewport();
}

JNIEXPORT jdoubleArray JNICALL
Java_app_organicmaps_Framework_nativeGetScreenRectCenter(JNIEnv * env, jclass)
{
  m2::PointD const center = frm()->GetViewportCenter();

  double latlon[] = {mercator::YToLat(center.y), mercator::XToLon(center.x)};
  jdoubleArray jLatLon = env->NewDoubleArray(2);
  env->SetDoubleArrayRegion(jLatLon, 0, 2, latlon);

  return jLatLon;
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeShowTrackRect(JNIEnv * env, jclass, jlong track)
{
  frm()->ShowTrack(static_cast<kml::TrackId>(track));
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_Framework_nativeGetBookmarkDir(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, GetPlatform().SettingsDir().c_str());
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_Framework_nativeGetWritableDir(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, GetPlatform().WritableDir().c_str());
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_Framework_nativeGetSettingsDir(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, GetPlatform().SettingsDir().c_str());
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_Framework_nativeGetDataFileExt(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, DATA_FILE_EXTENSION);
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_Framework_nativeGetMovableFilesExts(JNIEnv * env, jclass)
{
  vector<string> exts = { DATA_FILE_EXTENSION, FONT_FILE_EXTENSION };
  platform::CountryIndexes::GetIndexesExts(exts);
  return jni::ToJavaStringArray(env, exts);
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_Framework_nativeGetBookmarksFilesExts(JNIEnv * env, jclass)
{
  const vector<string> exts = { kKmzExtension, kKmlExtension, kKmbExtension };
  return jni::ToJavaStringArray(env, exts);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeChangeWritableDir(JNIEnv * env, jclass, jstring jNewPath)
{
  string newPath = jni::ToNativeString(env, jNewPath);
  g_framework->RemoveLocalMaps();
  android::Platform::Instance().SetWritableDir(newPath);
  g_framework->AddLocalMaps();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeIsRoutingActive(JNIEnv * env, jclass)
{
  return frm()->GetRoutingManager().IsRoutingActive();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeIsRouteBuilding(JNIEnv * env, jclass)
{
  return frm()->GetRoutingManager().IsRouteBuilding();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeIsRouteBuilt(JNIEnv * env, jclass)
{
  return frm()->GetRoutingManager().IsRouteBuilt();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeCloseRouting(JNIEnv * env, jclass)
{
  frm()->GetRoutingManager().CloseRouting(true /* remove route points */);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeBuildRoute(JNIEnv * env, jclass)
{
  frm()->GetRoutingManager().BuildRoute();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeRemoveRoute(JNIEnv * env, jclass)
{
  frm()->GetRoutingManager().RemoveRoute(false /* deactivateFollowing */);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeFollowRoute(JNIEnv * env, jclass)
{
  frm()->GetRoutingManager().FollowRoute();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeDisableFollowing(JNIEnv * env, jclass)
{
  frm()->GetRoutingManager().DisableFollowMode();
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_Framework_nativeGenerateNotifications(JNIEnv * env, jclass)
{
  ::Framework * fr = frm();
  if (!fr->GetRoutingManager().IsRoutingActive())
    return nullptr;

  vector<string> notifications;
  fr->GetRoutingManager().GenerateNotifications(notifications);
  if (notifications.empty())
    return nullptr;

  return jni::ToJavaStringArray(env, notifications);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetSpeedCamManagerMode(JNIEnv * env, jclass, jint mode)
{
  frm()->GetRoutingManager().GetSpeedCamManager().SetMode(
    static_cast<routing::SpeedCameraManagerMode>(mode));
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_Framework_nativeGetSpeedCamManagerMode(JNIEnv * env, jclass)
{
  return static_cast<jint>(frm()->GetRoutingManager().GetSpeedCamManager().GetMode());
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_Framework_nativeGetRouteFollowingInfo(JNIEnv * env, jclass)
{
  ::Framework * fr = frm();
  if (!fr->GetRoutingManager().IsRoutingActive())
    return nullptr;

  routing::FollowingInfo info;
  fr->GetRoutingManager().GetRouteFollowingInfo(info);
  if (!info.IsValid())
    return nullptr;

  static jclass const klass = jni::GetGlobalClassRef(env, "app/organicmaps/routing/RoutingInfo");
  // Java signature : RoutingInfo(String distToTarget, String units, String distTurn, String
  //                              turnSuffix, String currentStreet, String nextStreet,
  //                              double completionPercent, int vehicleTurnOrdinal, int
  //                              vehicleNextTurnOrdinal, int pedestrianTurnOrdinal, int exitNum,
  //                              int totalTime, SingleLaneInfo[] lanes)
  static jmethodID const ctorRouteInfoID =
      jni::GetConstructorID(env, klass,
                            "(Ljava/lang/String;Ljava/lang/String;"
                            "Ljava/lang/String;Ljava/lang/String;"
                            "Ljava/lang/String;Ljava/lang/String;DIIIII"
                            "[Lapp/organicmaps/routing/SingleLaneInfo;ZZ)V");

  vector<routing::FollowingInfo::SingleLaneInfoClient> const & lanes = info.m_lanes;
  jobjectArray jLanes = nullptr;
  if (!lanes.empty())
  {
    static jclass const laneClass = jni::GetGlobalClassRef(env, "app/organicmaps/routing/SingleLaneInfo");
    auto const lanesSize = static_cast<jsize>(lanes.size());
    jLanes = env->NewObjectArray(lanesSize, laneClass, nullptr);
    ASSERT(jLanes, (jni::DescribeException()));
    static jmethodID const ctorSingleLaneInfoID = jni::GetConstructorID(env, laneClass, "([BZ)V");

    for (jsize j = 0; j < lanesSize; ++j)
    {
      auto const laneSize = static_cast<jsize>(lanes[j].m_lane.size());
      jni::TScopedLocalByteArrayRef singleLane(env, env->NewByteArray(laneSize));
      ASSERT(singleLane.get(), (jni::DescribeException()));
      env->SetByteArrayRegion(singleLane.get(), 0, laneSize, lanes[j].m_lane.data());

      jni::TScopedLocalRef singleLaneInfo(
          env, env->NewObject(laneClass, ctorSingleLaneInfoID, singleLane.get(),
                              lanes[j].m_isRecommended));
      ASSERT(singleLaneInfo.get(), (jni::DescribeException()));
      env->SetObjectArrayElement(jLanes, j, singleLaneInfo.get());
    }
  }

  auto const & rm = frm()->GetRoutingManager();
  auto const isSpeedCamLimitExceeded = rm.IsRoutingActive() ? rm.IsSpeedCamLimitExceeded() : false;
  auto const shouldPlaySignal = frm()->GetRoutingManager().GetSpeedCamManager().ShouldPlayBeepSignal();
  jobject const result = env->NewObject(
      klass, ctorRouteInfoID, jni::ToJavaString(env, info.m_distToTarget),
      jni::ToJavaString(env, info.m_targetUnitsSuffix), jni::ToJavaString(env, info.m_distToTurn),
      jni::ToJavaString(env, info.m_turnUnitsSuffix), jni::ToJavaString(env, info.m_sourceName),
      jni::ToJavaString(env, info.m_displayedStreetName), info.m_completionPercent, info.m_turn,
      info.m_nextTurn, info.m_pedestrianTurn, info.m_exitNum, info.m_time, jLanes,
      static_cast<jboolean>(isSpeedCamLimitExceeded), static_cast<jboolean>(shouldPlaySignal));
  ASSERT(result, (jni::DescribeException()));
  return result;
}

JNIEXPORT jintArray JNICALL
Java_app_organicmaps_Framework_nativeGenerateRouteAltitudeChartBits(JNIEnv * env, jclass, jint width, jint height, jobject routeAltitudeLimits)
{
  RoutingManager::DistanceAltitude altitudes;
  if (!frm()->GetRoutingManager().GetRouteAltitudesAndDistancesM(altitudes))
  {
    LOG(LWARNING, ("Can't get distance to route points and altitude."));
    return nullptr;
  }

  altitudes.Simplify();

  vector<uint8_t> imageRGBAData;
  if (!altitudes.GenerateRouteAltitudeChart(width, height, imageRGBAData))
  {
    LOG(LWARNING, ("Can't generate route altitude image."));
    return nullptr;
  }

  uint32_t totalAscent, totalDescent;
  altitudes.CalculateAscentDescent(totalAscent, totalDescent);

  // Android platform code has specific result string formatting, so make conversion here.
  using namespace measurement_utils;
  auto units = Units::Metric;
  if (settings::Get(settings::kMeasurementUnits, units) && units == Units::Imperial)
  {
    totalAscent = measurement_utils::MetersToFeet(totalAscent);
    totalDescent = measurement_utils::MetersToFeet(totalDescent);
  }

  // Passing route limits.
  // Do not use jni::GetGlobalClassRef, because this class is used only to init static fieldId vars.
  static jclass const routeAltitudeLimitsClass = env->GetObjectClass(routeAltitudeLimits);
  ASSERT(routeAltitudeLimitsClass, ());

  static jfieldID const totalAscentField = env->GetFieldID(routeAltitudeLimitsClass, "totalAscent", "I");
  ASSERT(totalAscentField, ());
  env->SetIntField(routeAltitudeLimits, totalAscentField, static_cast<jint>(totalAscent));

  static jfieldID const totalDescentField = env->GetFieldID(routeAltitudeLimitsClass, "totalDescent", "I");
  ASSERT(totalDescentField, ());
  env->SetIntField(routeAltitudeLimits, totalDescentField, static_cast<jint>(totalDescent));

  static jfieldID const isMetricUnitsField = env->GetFieldID(routeAltitudeLimitsClass, "isMetricUnits", "Z");
  ASSERT(isMetricUnitsField, ());
  env->SetBooleanField(routeAltitudeLimits, isMetricUnitsField, units == Units::Metric);

  size_t const imageRGBADataSize = imageRGBAData.size();
  ASSERT_NOT_EQUAL(imageRGBADataSize, 0, ("GenerateRouteAltitudeChart returns true but the vector with altitude image bits is empty."));

  size_t const pxlCount = width * height;
  if (maps::kAltitudeChartBPP * pxlCount != imageRGBADataSize)
  {
    LOG(LWARNING, ("Wrong size of vector with altitude image bits. Expected size:", pxlCount, ". Real size:", imageRGBADataSize));
    return nullptr;
  }

  jintArray imageRGBADataArray = env->NewIntArray(static_cast<jsize>(pxlCount));
  ASSERT(imageRGBADataArray, ());
  jint * arrayElements = env->GetIntArrayElements(imageRGBADataArray, 0);
  ASSERT(arrayElements, ());

  for (size_t i = 0; i < pxlCount; ++i)
  {
    size_t const shiftInBytes = i * maps::kAltitudeChartBPP;
    // Type of |imageRGBAData| elements is uint8_t. But uint8_t is promoted to unsinged int in code below before shifting.
    // So there's no data lost in code below.
    arrayElements[i] = (imageRGBAData[shiftInBytes + 3] << 24) /* alpha */
        | (imageRGBAData[shiftInBytes] << 16) /* red */
        | (imageRGBAData[shiftInBytes + 1] << 8) /* green */
        | (imageRGBAData[shiftInBytes + 2]); /* blue */
  }
  env->ReleaseIntArrayElements(imageRGBADataArray, arrayElements, 0);

  return imageRGBADataArray;
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeShowCountry(JNIEnv * env, jclass, jstring countryId, jboolean zoomToDownloadButton)
{
  g_framework->ShowNode(jni::ToNativeString(env, countryId), (bool) zoomToDownloadButton);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetRoutingListener(JNIEnv * env, jclass, jobject listener)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  auto rf = jni::make_global_ref(listener);
  frm()->GetRoutingManager().SetRouteBuildingListener(
      [rf](routing::RouterResultCode e, storage::CountriesSet const & countries) {
        CallRoutingListener(rf, static_cast<int>(e), countries);
      });
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetRouteProgressListener(JNIEnv * env, jclass, jobject listener)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  frm()->GetRoutingManager().SetRouteProgressListener(
      bind(&CallRouteProgressListener, jni::make_global_ref(listener), _1));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetRoutingRecommendationListener(JNIEnv * env, jclass,
                                                                          jobject listener)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  frm()->GetRoutingManager().SetRouteRecommendationListener(
      bind(&CallRouteRecommendationListener, jni::make_global_ref(listener), _1));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetRoutingLoadPointsListener(
        JNIEnv *, jclass, jobject listener)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  if (listener != nullptr)
    g_loadRouteHandler = bind(&CallSetRoutingLoadPointsListener, jni::make_global_ref(listener), _1);
  else
    g_loadRouteHandler = nullptr;
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeDeactivatePopup(JNIEnv * env, jclass)
{
  return g_framework->DeactivatePopup();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetMapStyle(JNIEnv * env, jclass, jint mapStyle)
{
  MapStyle const val = static_cast<MapStyle>(mapStyle);
  if (val != g_framework->GetMapStyle())
    g_framework->SetMapStyle(val);
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_Framework_nativeGetMapStyle(JNIEnv * env, jclass)
{
  return g_framework->GetMapStyle();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeMarkMapStyle(JNIEnv * env, jclass, jint mapStyle)
{
  MapStyle const val = static_cast<MapStyle>(mapStyle);
  if (val != g_framework->GetMapStyle())
    g_framework->MarkMapStyle(val);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetRouter(JNIEnv * env, jclass, jint routerType)
{
  using Type = routing::RouterType;
  Type type = Type::Vehicle;
  switch (routerType)
  {
    case 0: break;
    case 1: type = Type::Pedestrian; break;
    case 2: type = Type::Bicycle; break;
    case 3: type = Type::Transit; break;
    default: assert(false); break;
  }
  g_framework->GetRoutingManager().SetRouter(type);
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_Framework_nativeGetRouter(JNIEnv * env, jclass)
{
  return static_cast<jint>(g_framework->GetRoutingManager().GetRouter());
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_Framework_nativeGetLastUsedRouter(JNIEnv * env, jclass)
{
  return static_cast<jint>(g_framework->GetRoutingManager().GetLastUsedRouter());
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_Framework_nativeGetBestRouter(JNIEnv * env, jclass,
                                                       jdouble srcLat, jdouble srcLon,
                                                       jdouble dstLat, jdouble dstLon)
{
  return static_cast<jint>(frm()->GetRoutingManager().GetBestRouter(
      mercator::FromLatLon(srcLat, srcLon), mercator::FromLatLon(dstLat, dstLon)));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeAddRoutePoint(JNIEnv * env, jclass, jstring title,
                                                       jstring subtitle, jint markType,
                                                       jint intermediateIndex,
                                                       jboolean isMyPosition,
                                                       jdouble lat, jdouble lon)
{
  RouteMarkData data;
  data.m_title = jni::ToNativeString(env, title);
  data.m_subTitle = jni::ToNativeString(env, subtitle);
  data.m_pointType = static_cast<RouteMarkType>(markType);
  data.m_intermediateIndex = static_cast<size_t>(intermediateIndex);
  data.m_isMyPosition = static_cast<bool>(isMyPosition);
  data.m_position = m2::PointD(mercator::FromLatLon(lat, lon));

  frm()->GetRoutingManager().AddRoutePoint(std::move(data));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeRemoveRoutePoint(JNIEnv * env, jclass,
                                                          jint markType, jint intermediateIndex)
{
  frm()->GetRoutingManager().RemoveRoutePoint(static_cast<RouteMarkType>(markType),
                                              static_cast<size_t>(intermediateIndex));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeRemoveIntermediateRoutePoints(JNIEnv * env, jclass)
{
  frm()->GetRoutingManager().RemoveIntermediateRoutePoints();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeCouldAddIntermediatePoint(JNIEnv * env, jclass)
{
  return frm()->GetRoutingManager().CouldAddIntermediatePoint();
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_Framework_nativeGetRoutePoints(JNIEnv * env, jclass)
{
  auto const points = frm()->GetRoutingManager().GetRoutePoints();

  static jclass const pointClazz = jni::GetGlobalClassRef(env,
                                   "app/organicmaps/routing/RouteMarkData");
  // Java signature : RouteMarkData(String title, String subtitle,
  //                                @RoutePointInfo.RouteMarkType int pointType,
  //                                int intermediateIndex, boolean isVisible, boolean isMyPosition,
  //                                boolean isPassed, double lat, double lon)
  static jmethodID const pointConstructor = jni::GetConstructorID(env, pointClazz,
                                            "(Ljava/lang/String;Ljava/lang/String;IIZZZDD)V");
  return jni::ToJavaArray(env, pointClazz, points, [&](JNIEnv * jEnv, RouteMarkData const & data)
  {
    jni::TScopedLocalRef const title(env, jni::ToJavaString(env, data.m_title));
    jni::TScopedLocalRef const subtitle(env, jni::ToJavaString(env, data.m_subTitle));
    return env->NewObject(pointClazz, pointConstructor,
                          title.get(), subtitle.get(),
                          static_cast<jint>(data.m_pointType),
                          static_cast<jint>(data.m_intermediateIndex),
                          static_cast<jboolean>(data.m_isVisible),
                          static_cast<jboolean>(data.m_isMyPosition),
                          static_cast<jboolean>(data.m_isPassed),
                          mercator::YToLat(data.m_position.y),
                          mercator::XToLon(data.m_position.x));
  });
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_Framework_nativeGetTransitRouteInfo(JNIEnv * env, jclass)
{
  auto const routeInfo = frm()->GetRoutingManager().GetTransitRouteInfo();

  static jclass const transitStepClass = jni::GetGlobalClassRef(env,
                                         "app/organicmaps/routing/TransitStepInfo");
  // Java signature : TransitStepInfo(@TransitType int type, @Nullable String distance, @Nullable String distanceUnits,
  //                                  int timeInSec, @Nullable String number, int color, int intermediateIndex)
  static jmethodID const transitStepConstructor = jni::GetConstructorID(env, transitStepClass,
                                                  "(ILjava/lang/String;Ljava/lang/String;ILjava/lang/String;II)V");

  jni::TScopedLocalRef const steps(env, jni::ToJavaArray(env, transitStepClass,
                                                         routeInfo.m_steps,
                                                         [&](JNIEnv * jEnv, TransitStepInfo const & stepInfo)
  {
      jni::TScopedLocalRef const distance(env, jni::ToJavaString(env, stepInfo.m_distanceStr));
      jni::TScopedLocalRef const distanceUnits(env, jni::ToJavaString(env, stepInfo.m_distanceUnitsSuffix));
      jni::TScopedLocalRef const number(env, jni::ToJavaString(env, stepInfo.m_number));
      return env->NewObject(transitStepClass, transitStepConstructor,
                            static_cast<jint>(stepInfo.m_type),
                            distance.get(),
                            distanceUnits.get(),
                            static_cast<jint>(stepInfo.m_timeInSec),
                            number.get(),
                            static_cast<jint>(stepInfo.m_colorARGB),
                            static_cast<jint>(stepInfo.m_intermediateIndex));
  }));

  static jclass const transitRouteInfoClass = jni::GetGlobalClassRef(env,
                                                                     "app/organicmaps/routing/TransitRouteInfo");
  // Java signature : TransitRouteInfo(@NonNull String totalDistance, @NonNull String totalDistanceUnits, int totalTimeInSec,
  //                                   @NonNull String totalPedestrianDistance, @NonNull String totalPedestrianDistanceUnits,
  //                                   int totalPedestrianTimeInSec, @NonNull TransitStepInfo[] steps)
  static jmethodID const transitRouteInfoConstructor = jni::GetConstructorID(env, transitRouteInfoClass,
                                                                             "(Ljava/lang/String;Ljava/lang/String;I"
                                                                             "Ljava/lang/String;Ljava/lang/String;I"
                                                                             "[Lapp/organicmaps/routing/TransitStepInfo;)V");
  jni::TScopedLocalRef const distance(env, jni::ToJavaString(env, routeInfo.m_totalDistanceStr));
  jni::TScopedLocalRef const distanceUnits(env, jni::ToJavaString(env, routeInfo.m_totalDistanceUnitsSuffix));
  jni::TScopedLocalRef const distancePedestrian(env, jni::ToJavaString(env, routeInfo.m_totalPedestrianDistanceStr));
  jni::TScopedLocalRef const distancePedestrianUnits(env, jni::ToJavaString(env, routeInfo.m_totalPedestrianUnitsSuffix));
  return env->NewObject(transitRouteInfoClass, transitRouteInfoConstructor,
                        distance.get(), distanceUnits.get(), static_cast<jint>(routeInfo.m_totalTimeInSec),
                        distancePedestrian.get(), distancePedestrianUnits.get(), static_cast<jint>(routeInfo.m_totalPedestrianTimeInSec),
                        steps.get());
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeReloadWorldMaps(JNIEnv * env, jclass)
{
  g_framework->ReloadWorldMaps();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeIsDayTime(JNIEnv * env, jclass, jlong utcTimeSeconds, jdouble lat, jdouble lon)
{
  DayTimeType const dt = GetDayTime(static_cast<time_t>(utcTimeSeconds), lat, lon);
  return (dt == DayTimeType::Day || dt == DayTimeType::PolarDay);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSet3dMode(JNIEnv * env, jclass, jboolean allow, jboolean allowBuildings)
{
  bool const allow3d = static_cast<bool>(allow);
  bool const allow3dBuildings = static_cast<bool>(allowBuildings);

  g_framework->Save3dMode(allow3d, allow3dBuildings);
  g_framework->Set3dMode(allow3d, allow3dBuildings);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeGet3dMode(JNIEnv * env, jclass, jobject result)
{
  bool enabled;
  bool buildings;
  g_framework->Get3dMode(enabled, buildings);

  static jclass const resultClass = env->GetObjectClass(result);

  static jfieldID const enabledField = env->GetFieldID(resultClass, "enabled", "Z");
  env->SetBooleanField(result, enabledField, enabled);

  static jfieldID const buildingsField = env->GetFieldID(resultClass, "buildings", "Z");
  env->SetBooleanField(result, buildingsField, buildings);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetAutoZoomEnabled(JNIEnv * env, jclass, jboolean enabled)
{
  bool const autoZoomEnabled = static_cast<bool>(enabled);
  frm()->SaveAutoZoom(autoZoomEnabled);
  frm()->AllowAutoZoom(autoZoomEnabled);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetTransitSchemeEnabled(JNIEnv * env, jclass, jboolean enabled)
{
  frm()->GetTransitManager().EnableTransitSchemeMode(static_cast<bool>(enabled));
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeIsTransitSchemeEnabled(JNIEnv * env, jclass)
{
  return static_cast<jboolean>(frm()->LoadTransitSchemeEnabled());
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetIsolinesLayerEnabled(JNIEnv * env, jclass, jboolean enabled)
{
  auto const isolinesEnabled = static_cast<bool>(enabled);
  frm()->GetIsolinesManager().SetEnabled(isolinesEnabled);
  frm()->SaveIsolinesEnabled(isolinesEnabled);
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeIsIsolinesLayerEnabled(JNIEnv * env, jclass)
{
  return static_cast<jboolean>(frm()->LoadIsolinesEnabled());
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSaveSettingSchemeEnabled(JNIEnv * env, jclass, jboolean enabled)
{
  frm()->SaveTransitSchemeEnabled(static_cast<bool>(enabled));
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeGetAutoZoomEnabled(JNIEnv *, jclass)
{
  return frm()->LoadAutoZoom();
}

// static void nativeZoomToPoint(double lat, double lon, int zoom, boolean animate);
JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeZoomToPoint(JNIEnv * env, jclass, jdouble lat, jdouble lon, jint zoom, jboolean animate)
{
  // TODO: fix
  g_framework->Scale(0, m2::PointD(mercator::FromLatLon(lat, lon)), zoom, animate);
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_Framework_nativeDeleteBookmarkFromMapObject(JNIEnv * env, jclass)
{
  if (!frm()->HasPlacePageInfo())
    return nullptr;

  place_page::Info const & info = g_framework->GetPlacePageInfo();
  auto const bookmarkId = info.GetBookmarkId();
  frm()->GetBookmarkManager().GetEditSession().DeleteBookmark(bookmarkId);

  auto buildInfo = info.GetBuildInfo();
  buildInfo.m_match = place_page::BuildInfo::Match::FeatureOnly;
  buildInfo.m_userMarkId = kml::kInvalidMarkId;
  buildInfo.m_source = place_page::BuildInfo::Source::Other;
  frm()->UpdatePlacePageInfoForCurrentSelection(buildInfo);

  return usermark_helper::CreateMapObject(env, g_framework->GetPlacePageInfo());
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeTurnOnChoosePositionMode(JNIEnv *, jclass, jboolean isBusiness, jboolean applyPosition)
{
  auto const pos = applyPosition && frm()->HasPlacePageInfo()
      ? g_framework->GetPlacePageInfo().GetMercator()
      : m2::PointD();
  g_framework->SetChoosePositionMode(true, isBusiness, applyPosition, pos);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeTurnOffChoosePositionMode(JNIEnv *, jclass)
{
  g_framework->SetChoosePositionMode(false, false, false, m2::PointD());
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeIsInChoosePositionMode(JNIEnv *, jclass)
{
  return g_framework->GetChoosePositionMode();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeIsDownloadedMapAtScreenCenter(JNIEnv *, jclass)
{
  ::Framework * fr = frm();
  return storage::IsPointCoveredByDownloadedMaps(fr->GetViewportCenter(), fr->GetStorage(), fr->GetCountryInfoGetter());
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_Framework_nativeGetActiveObjectFormattedCuisine(JNIEnv * env, jclass)
{
  ::Framework * frm = g_framework->NativeFramework();
  if (!frm->HasPlacePageInfo())
    return {};

  return jni::ToJavaString(env, g_framework->GetPlacePageInfo().FormatCuisines());
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetVisibleRect(JNIEnv * env, jclass, jint left, jint top, jint right, jint bottom)
{
//  frm()->SetVisibleViewport(m2::RectD(left, top, right, bottom));
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeIsRouteFinished(JNIEnv * env, jclass)
{
  return frm()->GetRoutingManager().IsRouteFinished();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeRunFirstLaunchAnimation(JNIEnv * env, jclass)
{
  frm()->RunFirstLaunchAnimation();
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_Framework_nativeOpenRoutePointsTransaction(JNIEnv * env, jclass)
{
  return frm()->GetRoutingManager().OpenRoutePointsTransaction();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeApplyRoutePointsTransaction(JNIEnv * env, jclass,
                                                                     jint transactionId)
{
  frm()->GetRoutingManager().ApplyRoutePointsTransaction(transactionId);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeCancelRoutePointsTransaction(JNIEnv * env, jclass,
                                                                      jint transactionId)
{
  frm()->GetRoutingManager().CancelRoutePointsTransaction(transactionId);
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_Framework_nativeInvalidRoutePointsTransactionId(JNIEnv * env, jclass)
{
  return frm()->GetRoutingManager().InvalidRoutePointsTransactionId();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeHasSavedRoutePoints(JNIEnv *, jclass)
{
  return frm()->GetRoutingManager().HasSavedRoutePoints();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeLoadRoutePoints(JNIEnv *, jclass)
{
  frm()->GetRoutingManager().LoadRoutePoints(g_loadRouteHandler);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSaveRoutePoints(JNIEnv *, jclass)
{
  frm()->GetRoutingManager().SaveRoutePoints();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeDeleteSavedRoutePoints(JNIEnv *, jclass)
{
  frm()->GetRoutingManager().DeleteSavedRoutePoints();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeShowFeature(JNIEnv * env, jclass, jobject featureId)
{
  auto const f = g_framework->BuildFeatureId(env, featureId);

  if (f.IsValid())
    frm()->ShowFeature(f);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeMakeCrash(JNIEnv *env, jclass type)
{
  CHECK(false, ("Diagnostic native crash!"));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetPowerManagerFacility(JNIEnv *, jclass,
                                                                 jint facilityType, jboolean state)
{
  frm()->GetPowerManager().SetFacility(static_cast<power_management::Facility>(facilityType),
                                       static_cast<bool>(state));
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_Framework_nativeGetPowerManagerScheme(JNIEnv *, jclass)
{
  return static_cast<jint>(frm()->GetPowerManager().GetScheme());
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetPowerManagerScheme(JNIEnv *, jclass, jint schemeType)
{
  frm()->GetPowerManager().SetScheme(static_cast<power_management::Scheme>(schemeType));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetViewportCenter(JNIEnv *, jclass, jdouble lat, jdouble lon, jint zoom)
{
  // isAnim = true because of previous nativeTurnOnChoosePositionMode animations.
  frm()->SetViewportCenter(mercator::FromLatLon(lat, lon), static_cast<int>(zoom), true /* isAnim */);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeStopLocationFollow(JNIEnv *, jclass)
{
  frm()->StopLocationFollow();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Framework_nativeSetSearchViewport(JNIEnv *, jclass, jdouble lat,
                                                           jdouble lon, jint zoom)
{
  auto const center = mercator::FromLatLon(static_cast<double>(lat),
                                           static_cast<double>(lon));
  auto const rect = df::GetRectForDrawScale(static_cast<int>(zoom), center);
  frm()->GetSearchAPI().OnViewportChanged(rect);
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Framework_nativeHasPlacePageInfo(JNIEnv *, jclass)
{
  return static_cast<jboolean>(frm()->HasPlacePageInfo());
}
}  // extern "C"
