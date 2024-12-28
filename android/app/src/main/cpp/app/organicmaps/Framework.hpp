#pragma once

#include <jni.h>

#include "map/framework.hpp"
#include "map/place_page_info.hpp"
#include "map/power_management/power_manager.hpp"

#include "search/result.hpp"

#include "drape_frontend/gui/skin.hpp"

#include "drape/pointers.hpp"
#include "drape/graphics_context_factory.hpp"


#include "indexer/feature_decl.hpp"
#include "indexer/map_style.hpp"

#include "platform/country_defines.hpp"
#include "platform/location.hpp"

#include "geometry/avg_vector.hpp"

#include "base/timer.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

class DataSource;
struct FeatureID;

namespace search
{
struct EverywhereSearchParams;
}

namespace android
{
  enum CoordinatesFormat // See Java enum app.organicmaps.widget.placepage.CoordinatesFormat for all possible values.
  {
    LatLonDMS = 0,     // Latitude, Longitude in degrees minutes seconds format, comma separated
    LatLonDecimal = 1, // Latitude, Longitude in decimal format, comma separated
    OLCFull = 2,       // Open location code, full format
    OSMLink = 3,       // Link to the OSM. E.g. https://osm.org/go/xcXjyqQlq-?m=
    UTM = 4,           // Universal Transverse Mercator
    MGRS = 5           // Military Grid Reference System
  };

  // Keep in sync `public @interface ChoosePositionMode`in Framework.java.
  enum class ChoosePositionMode
  {
    None = 0,
    Editor = 1,
    Api = 2,
  };

  class Framework : private power_management::PowerManager::Subscriber
  {
  private:
    drape_ptr<dp::ThreadSafeFactory> m_oglContextFactory;
    drape_ptr<dp::GraphicsContextFactory> m_vulkanContextFactory;
    ::Framework m_work;

    math::LowPassVector<float, 3> m_sensors[2];
    double m_lastCompass = 0;

    std::string m_searchQuery;

    std::map<gui::EWidget, gui::Position> m_guiPositions;

    void TrafficStateChanged(TrafficManager::TrafficState state);
    void TransitSchemeStateChanged(TransitReadManager::TransitSchemeState state);
    void IsolinesSchemeStateChanged(IsolinesManager::IsolinesState state);

    void MyPositionModeChanged(location::EMyPositionMode mode, bool routingActive);

    location::TMyPositionModeChanged m_myPositionModeSignal;

    TrafficManager::TrafficStateChangedFn m_onTrafficStateChangedFn;
    TransitReadManager::TransitStateChangedFn m_onTransitStateChangedFn;
    IsolinesManager::IsolinesStateChangedFn m_onIsolinesStateChangedFn;

    ChoosePositionMode m_isChoosePositionMode = ChoosePositionMode::None;
    bool m_isSurfaceDestroyed = false;

  public:
    Framework(std::function<void()> && afterMapsLoaded);

    storage::Storage & GetStorage();
    DataSource const & GetDataSource();

    void ShowNode(storage::CountryId const & countryId, bool zoomToDownloadButton);

    void OnLocationError(int/* == location::TLocationStatus*/ newStatus);
    void OnLocationUpdated(location::GpsInfo const & info);
    void OnCompassUpdated(location::CompassInfo const & info, bool forceRedraw);

    bool CreateDrapeEngine(JNIEnv * env, jobject jSurface, int densityDpi, bool firstLaunch,
                           bool launchByDeepLink, uint32_t appVersionCode, bool isCustomROM);
    bool IsDrapeEngineCreated() const;
    void UpdateDpi(int dpi);
    bool DestroySurfaceOnDetach();
    void DetachSurface(bool destroySurface);
    bool AttachSurface(JNIEnv * env, jobject jSurface);
    void PauseSurfaceRendering();
    void ResumeSurfaceRendering();

    void SetMapStyle(MapStyle mapStyle);
    void MarkMapStyle(MapStyle mapStyle);
    MapStyle GetMapStyle() const;

    void SetupMeasurementSystem();

    RoutingManager & GetRoutingManager() { return m_work.GetRoutingManager(); }
    void SetRouter(routing::RouterType type) { m_work.GetRoutingManager().SetRouter(type); }
    routing::RouterType GetRouter() const { return m_work.GetRoutingManager().GetRouter(); }
    routing::RouterType GetLastUsedRouter() const
    {
      return m_work.GetRoutingManager().GetLastUsedRouter();
    }

    void Resize(JNIEnv * env, jobject jSurface, int w, int h);

    struct Finger
    {
      Finger(int64_t id, float x, float y)
        : m_id(id)
        , m_x(x)
        , m_y(y)
      {
      }

      int64_t m_id;
      float m_x, m_y;
    };

    void Scale(double factor, m2::PointD const & pxPoint, bool isAnim);

    void Scroll(double distanceX, double distanceY);

    void Touch(int action, Finger const & f1, Finger const & f2, uint8_t maskedPointer);

    bool Search(search::EverywhereSearchParams const & params);
    std::string GetLastSearchQuery() { return m_searchQuery; }
    void ClearLastSearchQuery() { m_searchQuery.clear(); }

    void AddLocalMaps();
    void RemoveLocalMaps();
    void ReloadWorldMaps();

    m2::PointD GetViewportCenter() const;

    void AddString(std::string const & name, std::string const & value);

    void Scale(::Framework::EScaleMode mode);
    void Scale(m2::PointD const & centerPt, int targetZoom, bool animate);

    void ChangeTrackColor(kml::TrackId trackId, dp::Color color);
    void ReplaceBookmark(kml::MarkId markId, kml::BookmarkData & bm);
    void ReplaceTrack(kml::TrackId trackId, kml::TrackData & trackData);
    void MoveBookmark(kml::MarkId markId, kml::MarkGroupId curCat, kml::MarkGroupId newCat);
    void MoveTrack(kml::TrackId trackId, kml::MarkGroupId curCat, kml::MarkGroupId newCat);

    ::Framework * NativeFramework();

    bool IsDownloadingActive();

    void ExecuteMapApiRequest();

    void DeactivatePopup();
    void DeactivateMapSelectionCircle();

//    std::string GetOutdatedCountriesString();

    void SetMyPositionModeListener(location::TMyPositionModeChanged const & fn);
    location::EMyPositionMode GetMyPositionMode() const;
    void SwitchMyPositionNextMode();

    void SetTrafficStateListener(TrafficManager::TrafficStateChangedFn const & fn);
    void SetTransitSchemeListener(TransitReadManager::TransitStateChangedFn const & fn);
    void SetIsolinesListener(IsolinesManager::IsolinesStateChangedFn const & fn);

    bool IsTrafficEnabled();
    void EnableTraffic();
    void DisableTraffic();

    void Save3dMode(bool allow3d, bool allow3dBuildings);
    void Set3dMode(bool allow3d, bool allow3dBuildings);
    void Get3dMode(bool & allow3d, bool & allow3dBuildings);

    void SetMapLanguageCode(std::string const & languageCode);
    std::string GetMapLanguageCode();

    void SetChoosePositionMode(ChoosePositionMode mode, bool isBusiness, m2::PointD const * optionalPosition);
    ChoosePositionMode GetChoosePositionMode();

    void UpdateMyPositionRoutingOffset(int offsetY);
    void SetupWidget(gui::EWidget widget, float x, float y, dp::Anchor anchor);
    void ApplyWidgets();
    void CleanWidgets();

    place_page::Info & GetPlacePageInfo();

    bool IsAutoRetryDownloadFailed();
    bool IsDownloadOn3gEnabled();
    void EnableDownloadOn3g();

//    int ToDoAfterUpdate() const;

    // PowerManager::Subscriber overrides:
    void OnPowerFacilityChanged(power_management::Facility const facility, bool enabled) override;
    void OnPowerSchemeChanged(power_management::Scheme const actualScheme) override;

    FeatureID BuildFeatureId(JNIEnv * env, jobject featureId);
  };
}

extern std::unique_ptr<android::Framework> g_framework;
::Framework * frm();
