#pragma once

#include "map/api_mark_point.hpp"
#include "map/bookmark.hpp"
#include "map/bookmark_manager.hpp"
#include "map/caching_address_getter.hpp"
#include "map/features_fetcher.hpp"
#include "map/isolines_manager.hpp"
#include "map/mwm_url.hpp"
#include "map/place_page_info.hpp"
#include "map/position_provider.hpp"
#include "map/power_management/power_management_schemas.hpp"
#include "map/power_management/power_manager.hpp"
#include "map/routing_manager.hpp"
#include "map/routing_mark.hpp"
#include "map/search_api.hpp"
#include "map/search_mark.hpp"
#include "map/track.hpp"
#include "map/traffic_manager.hpp"
#include "map/transit/transit_reader.hpp"

#include "drape_frontend/gui/skin.hpp"
#include "drape_frontend/drape_api.hpp"
#include "drape_frontend/drape_engine.hpp"
#include "drape_frontend/user_event_stream.hpp"

#include "drape/drape_global.hpp"
#include "drape/graphics_context_factory.hpp"

#include "kml/type_utils.hpp"

#include "editor/new_feature_categories.hpp"
#include "editor/osm_editor.hpp"

#include "indexer/caching_rank_table_loader.hpp"
#include "indexer/data_source.hpp"
#include "indexer/data_source_helpers.hpp"
#include "indexer/map_object.hpp"
#include "indexer/map_style.hpp"

#include "search/displayed_categories.hpp"
#include "search/result.hpp"
#include "search/reverse_geocoder.hpp"

#include "storage/downloading_policy.hpp"
#include "storage/storage.hpp"

#include "platform/location.hpp"
#include "platform/platform.hpp"

#include "routing/router.hpp"

#include "geometry/rect2d.hpp"
#include "geometry/screenbase.hpp"

#include "base/macros.hpp"
#include "base/strings_bundle.hpp"

#include "std/target_os.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace osm
{
class EditableMapObject;
}

namespace search
{
struct EverywhereSearchParams;
struct ViewportSearchParams;
}

namespace storage
{
class CountryInfoGetter;
struct DownloaderSearchParams;
}

namespace routing { namespace turns{ class Settings; } }

namespace platform
{
class NetworkPolicy;
}

namespace descriptions
{
class Loader;
}

/// Uncomment line to make fixed position settings and
/// build version for screenshots.
//#define FIXED_LOCATION

struct FrameworkParams
{
  bool m_enableDiffs = true;
  size_t m_numSearchAPIThreads = 1;

  FrameworkParams() = default;
  FrameworkParams(bool enableDiffs)
    : m_enableDiffs(enableDiffs)
  {}
};

class Framework : public PositionProvider,
                  public SearchAPI::Delegate,
                  public RoutingManager::Delegate,
                  private power_management::PowerManager::Subscriber
{
  DISALLOW_COPY(Framework);

#ifdef FIXED_LOCATION
  class FixedPosition
  {
    std::pair<double, double> m_latlon;
    double m_dirFromNorth;
    bool m_fixedLatLon, m_fixedDir;

  public:
    FixedPosition();

    void GetLat(double & l) const { if (m_fixedLatLon) l = m_latlon.first; }
    void GetLon(double & l) const { if (m_fixedLatLon) l = m_latlon.second; }

    void GetNorth(double & n) const { if (m_fixedDir) n = m_dirFromNorth; }
    bool HasNorth() const { return m_fixedDir; }

  } m_fixedPos;
#endif

private:
  // Must be first member in Framework and must be destroyed first in Framework destructor.
  std::unique_ptr<Platform::ThreadRunner> m_threadRunner = std::make_unique<Platform::ThreadRunner>();

protected:
  using TDrapeFunction = std::function<void (df::DrapeEngine *)>;

  StringsBundle m_stringsBundle;

  FeaturesFetcher m_featuresFetcher;

  // The order matters here: DisplayedCategories may be used only
  // after classificator is loaded by |m_featuresFetcher|.
  std::unique_ptr<search::DisplayedCategories> m_displayedCategories;

  // The order matters here: storage::CountryInfoGetter and
  // m_FeaturesFetcher must be initialized before
  // search::Engine and, therefore, destroyed after search::Engine.
  std::unique_ptr<storage::CountryInfoGetter> m_infoGetter;

  std::unique_ptr<SearchAPI> m_searchAPI;

  ScreenBase m_currentModelView;
  m2::RectD m_visibleViewport;

  using TViewportChangedFn = df::DrapeEngine::ModelViewChangedHandler;
  TViewportChangedFn m_viewportChangedFn;

  struct DrapeEngineData
  {
    drape_ptr<df::DrapeEngine> m_drapeEngine;
    df::OnGraphicsContextInitialized m_onGraphicsContextInitialized;
    df::DrapeApi m_drapeApi;
  };
  std::unordered_map<df::DrapeEngineId, DrapeEngineData> m_drapeEngines;

  StorageDownloadingPolicy m_storageDownloadingPolicy;
  storage::Storage m_storage;
  bool m_enabledDiffs;

  location::TMyPositionModeChanged m_myPositionListener;
  df::DrapeEngine::UserPositionPendingTimeoutHandler m_myPositionPendingTimeoutListener;

  std::unique_ptr<BookmarkManager> m_bmManager;

  SearchMarks m_searchMarks;

  bool m_isRenderingEnabled;

  // Note. |m_powerManager| should be declared before |m_routingManager|
  power_management::PowerManager m_powerManager;

  TransitReadManager m_transitManager;
  IsolinesManager m_isolinesManager;

  // Note. |m_routingManager| should be declared before |m_trafficManager|
  RoutingManager m_routingManager;

  TrafficManager m_trafficManager;

  /// This function will be called by m_storage when latest local files
  /// is downloaded.
  void OnCountryFileDownloaded(storage::CountryId const & countryId,
                               storage::LocalFilePtr const localFile);

  /// This function will be called by m_storage before latest local files
  /// is deleted.
  bool OnCountryFileDelete(storage::CountryId const & countryId,
                           storage::LocalFilePtr const localFile);

  /// This function is called by m_featuresFetcher when the map file is deregistered.
  void OnMapDeregistered(platform::LocalCountryFile const & localFile);

  void ClearAllCaches();

  void OnViewportChanged(ScreenBase const & screen);

  void InitTransliteration();

public:
  explicit Framework(FrameworkParams const & params = {});
  virtual ~Framework() override;

  df::DrapeApi & GetDrapeApi() { return m_drapeEngines.begin()->second.m_drapeApi; }

  /// \returns true if there're unsaved changes in map with |countryId| and false otherwise.
  /// \note It works for group and leaf node.
  bool HasUnsavedEdits(storage::CountryId const & countryId);

  /// Registers all local map files in internal indexes.
  void RegisterAllMaps();

  /// Deregisters all registered map files.
  void DeregisterAllMaps();

  /// Registers a local map file in internal indexes.
  std::pair<MwmSet::MwmId, MwmSet::RegResult> RegisterMap(
      platform::LocalCountryFile const & localFile);

  /// Shows group or leaf mwm on the map.
  void ShowNode(storage::CountryId const & countryId);

  /// Checks, whether the country is loaded.
  bool IsCountryLoadedByName(std::string_view name) const;

  void InvalidateRect(m2::RectD const & rect);

  std::string GetCountryName(m2::PointD const & pt) const;

  enum class DoAfterUpdate
  {
    Nothing,
    AutoupdateMaps,
    AskForUpdateMaps,
    Migrate
  };
//  DoAfterUpdate ToDoAfterUpdate() const;

  storage::Storage & GetStorage() { return m_storage; }
  storage::Storage const & GetStorage() const { return m_storage; }
  search::DisplayedCategories const & GetDisplayedCategories();
  storage::CountryInfoGetter const & GetCountryInfoGetter() { return *m_infoGetter; }
  StorageDownloadingPolicy & GetDownloadingPolicy() { return m_storageDownloadingPolicy; }

  DataSource const & GetDataSource() const { return m_featuresFetcher.GetDataSource(); }

  SearchAPI & GetSearchAPI();
  SearchAPI const & GetSearchAPI() const;

  /// @name Bookmarks, Tracks and other UserMarks
  /// Scans and loads all kml files with bookmarks in WritableDir.
  void LoadBookmarks();

  /// @return Created bookmark category id.
  kml::MarkGroupId AddCategory(std::string const & categoryName);

  kml::MarkGroupId LastEditedBMCategory() { return GetBookmarkManager().LastEditedBMCategory(); }
  kml::PredefinedColor LastEditedBMColor() const { return GetBookmarkManager().LastEditedBMColor(); }

  void ShowBookmark(kml::MarkId id);
  void ShowBookmark(Bookmark const * bookmark);
  void ShowTrack(kml::TrackId trackId);
  void ShowFeature(FeatureID const & featureId);
  void ShowBookmarkCategory(kml::MarkGroupId categoryId, bool animation = true);

  void AddBookmarksFile(std::string const & filePath, bool isTemporaryFile);

  BookmarkManager & GetBookmarkManager();
  BookmarkManager const & GetBookmarkManager() const;

  /// @name Visualize utilities, used in desktop only. Implemented in framework_visualize.cpp
  /// @{
  void VisualizeRoadsInRect(m2::RectD const & rect);
  void VisualizeCityBoundariesInRect(m2::RectD const & rect);
  void VisualizeCityRoadsInRect(m2::RectD const & rect);
  void VisualizeCrossMwmTransitionsInRect(m2::RectD const & rect);
  /// @}

public:
  // SearchAPI::Delegate overrides:
  void RunUITask(std::function<void()> fn) override;
  using SearchResultsIterT = SearchAPI::Delegate::ResultsIterT;
  void ShowViewportSearchResults(SearchResultsIterT begin, SearchResultsIterT end, bool clear) override;
  void ClearViewportSearchResults() override;
  // PositionProvider, SearchApi::Delegate and TipsApi::Delegate override.
  std::optional<m2::PointD> GetCurrentPosition() const override;
  bool ParseSearchQueryCommand(search::SearchParams const & params) override;
  m2::PointD GetMinDistanceBetweenResults() const override;

private:
  void ActivateMapSelection();
  void InvalidateUserMarks();

  void DeactivateHotelSearchMark();

public:
  void DeactivateMapSelection(bool notifyUI);
  /// Used to "refresh" UI in some cases (e.g. feature editing).
  void UpdatePlacePageInfoForCurrentSelection(
      std::optional<place_page::BuildInfo> const & overrideInfo = {});

  struct PlacePageEvent
  {
    /// Called to notify UI that object on a map was selected (UI should show Place Page, for example).
    using OnOpen = std::function<void()>;
    /// Called to notify UI that object on a map was deselected (UI should hide Place Page).
    /// If switchFullScreenMode is true, ui can [optionally] enter or exit full screen mode.
    using OnClose = std::function<void(bool /*switchFullScreenMode*/)>;
    using OnUpdate = std::function<void()>;
  };

  void SetPlacePageListeners(PlacePageEvent::OnOpen onOpen,
                             PlacePageEvent::OnClose onClose,
                             PlacePageEvent::OnUpdate onUpdate);
  bool HasPlacePageInfo() const { return m_currentPlacePageInfo.has_value(); }
  place_page::Info const & GetCurrentPlacePageInfo() const;
  place_page::Info & GetCurrentPlacePageInfo();

  void InvalidateRendering();
  void EnableDebugRectRendering(bool enabled);

  void EnableChoosePositionMode(bool enable, bool enableBounds, bool applyPosition, m2::PointD const & position);
  void BlockTapEvents(bool block);

  using TCurrentCountryChanged = std::function<void(storage::CountryId const &)>;
  storage::CountryId const & GetLastReportedCountry() { return m_lastReportedCountry; }
  /// Guarantees that listener is called in the main thread context.
  void SetCurrentCountryChangedListener(TCurrentCountryChanged listener);

  std::vector<std::string> GetRegionsCountryIdByRect(m2::RectD const & rect, bool rough) const;
  std::vector<MwmSet::MwmId> GetMwmsByRect(m2::RectD const & rect, bool rough) const;

  void ReadFeatures(std::function<void(FeatureType &)> const & reader,
                    std::vector<FeatureID> const & features);

private:
  std::optional<place_page::Info> m_currentPlacePageInfo;

  void OnTapEvent(place_page::BuildInfo const & buildInfo);
  std::optional<place_page::Info> BuildPlacePageInfo(place_page::BuildInfo const & buildInfo);
  void BuildTrackPlacePage(Track::TrackSelectionInfo const & trackSelectionInfo, place_page::Info & info);
  Track::TrackSelectionInfo FindTrackInTapPosition(place_page::BuildInfo const & buildInfo) const;
  UserMark const * FindUserMarkInTapPosition(place_page::BuildInfo const & buildInfo) const;
  FeatureID FindBuildingAtPoint(m2::PointD const & mercator) const;

  void UpdateMinBuildingsTapZoom();

  int m_minBuildingsTapZoom;

  PlacePageEvent::OnOpen m_onPlacePageOpen;
  PlacePageEvent::OnClose m_onPlacePageClose;
  PlacePageEvent::OnUpdate m_onPlacePageUpdate;

private:
  std::vector<m2::TriangleD> GetSelectedFeatureTriangles() const;

public:
  /// @name GPS location updates routine.
  void OnLocationError(location::TLocationError error);
  void OnLocationUpdate(location::GpsInfo const & info);
  void OnCompassUpdate(location::CompassInfo const & info);
  void SwitchMyPositionNextMode();
  /// Should be set before Drape initialization. Guarantees that fn is called in main thread context.
  void SetMyPositionModeListener(location::TMyPositionModeChanged && fn);
  void SetMyPositionPendingTimeoutListener(df::DrapeEngine::UserPositionPendingTimeoutHandler && fn);

  location::EMyPositionMode GetMyPositionMode() const;

private:
  void OnUserPositionChanged(m2::PointD const & position, bool hasPosition);

public:
  struct DrapeCreationParams
  {
    dp::ApiVersion m_apiVersion = dp::ApiVersion::OpenGLES2;
    float m_visualScale = 1.0f;
    int m_surfaceWidth = 0;
    int m_surfaceHeight = 0;
    gui::TWidgetsInitInfo m_widgetsInitInfo;

    bool m_isChoosePositionMode = false;
    df::Hints m_hints;
  };

  df::DrapeEngineId CreateDrapeEngineId();
  void CreateDrapeEngine(df::DrapeEngineId engineId, ref_ptr<dp::GraphicsContextFactory> contextFactory, DrapeCreationParams && params);
  ref_ptr<df::DrapeEngine> GetDrapeEngine(df::DrapeEngineId engineId);
  bool IsDrapeEngineCreated(df::DrapeEngineId engineId) const { return m_drapeEngines.count(engineId) != 0 && m_drapeEngines.at(engineId).m_drapeEngine != nullptr; }
  void DestroyDrapeEngine();
  /// Called when graphics engine should be temporarily paused and then resumed.
  void SetRenderingEnabled(ref_ptr<dp::GraphicsContextFactory> contextFactory = nullptr);
  void SetRenderingDisabled(bool destroySurface);

  void SetGraphicsContextInitializationHandler(df::DrapeEngineId engineId, df::OnGraphicsContextInitialized && handler);

  void OnRecoverSurface(df::DrapeEngineId engineId, int width, int height, bool recreateContextDependentResources);
  void OnDestroySurface();

  void UpdateVisualScale(double vs);

  /// Sets the distance between the bottom edge of the arrow and the bottom edge of the visible viewport
  /// in follow routing mode or resets it to the default value.
  void UpdateMyPositionRoutingOffset(bool useDefault, int offsetY);

private:
  /// Depends on initialized Drape engine.
  void SaveViewport();
  /// Depends on initialized Drape engine.
  void LoadViewport();

public:
  void ConnectToGpsTracker();
  void DisconnectFromGpsTracker();

  void SetMapStyle(MapStyle mapStyle);
  void MarkMapStyle(MapStyle mapStyle);
  MapStyle GetMapStyle() const;

  void SetupMeasurementSystem();

  void SetWidgetLayout(df::DrapeEngineId engineId, gui::TWidgetsLayoutInfo && layout);

  void PrepareToShutdown();

private:
  void InitCountryInfoGetter();
  void InitSearchAPI(size_t numThreads);

  bool m_connectToGpsTrack; // need to connect to tracker when Drape is being constructed

  void OnUpdateCurrentCountry(m2::PointD const & pt, int zoomLevel);

  storage::CountryId m_lastReportedCountry;
  TCurrentCountryChanged m_currentCountryChanged;

  void OnUpdateGpsTrackPointsCallback(std::vector<std::pair<size_t, location::GpsTrackInfo>> && toAdd,
                                      std::pair<size_t, size_t> const & toRemove);

  CachingRankTableLoader m_popularityLoader;

  std::unique_ptr<descriptions::Loader> m_descriptionsLoader;

public:
  using SearchRequest = search::QuerySaver::SearchRequest;

  // Moves viewport to the search result and taps on it.
  void SelectSearchResult(search::Result const & res, bool animation);

  // Cancels all searches, stops location follow and then selects
  // search result.
  void ShowSearchResult(search::Result const & res, bool animation = true);

  size_t ShowSearchResults(search::Results const & results);

  void FillSearchResultsMarks(bool clear, search::Results const & results);
  void FillSearchResultsMarks(SearchResultsIterT beg, SearchResultsIterT end, bool clear);

  /// Calculate distance and direction to POI for the given position.
  /// @param[in]  point             POI's position;
  /// @param[in]  lat, lon, north   Current position and heading from north;
  /// @param[out] distance          Formatted distance string;
  /// @param[out] azimut            Azimut to point from (lat, lon);
  /// @return true  If the POI is near the current position (distance < 25 km);
  bool GetDistanceAndAzimut(m2::PointD const & point,
                            double lat, double lon, double north,
                            std::string & distance, double & azimut);

  /// @name Manipulating with model view
  m2::PointD PtoG(m2::PointD const & p) const { return m_currentModelView.PtoG(p); }
  m2::PointD P3dtoG(m2::PointD const & p) const { return m_currentModelView.PtoG(m_currentModelView.P3dtoP(p)); }
  m2::PointD GtoP(m2::PointD const & p) const { return m_currentModelView.GtoP(p); }
  m2::PointD GtoP3d(m2::PointD const & p) const { return m_currentModelView.PtoP3d(m_currentModelView.GtoP(p)); }

  /// Show all model by it's world rect.
  void ShowAll();

  m2::PointD GetVisiblePixelCenter() const;

  m2::PointD const & GetViewportCenter() const;
  void SetViewportCenter(m2::PointD const & pt, int zoomLevel = -1, bool isAnim = true);

  m2::RectD GetCurrentViewport() const;
  void SetVisibleViewport(df::DrapeEngineId engineId, m2::RectD const & rect);

  /// - Check minimal visible scale according to downloaded countries.
  void ShowRect(m2::RectD const & rect, int maxScale = -1, bool animation = true,
                bool useVisibleViewport = false);
  void ShowRect(m2::AnyRectD const & rect, bool animation = true, bool useVisibleViewport = false);

  void GetTouchRect(m2::PointD const & center, uint32_t pxRadius, m2::AnyRectD & rect);

  void SetViewportListener(TViewportChangedFn const & fn);

#if defined(OMIM_OS_MAC) || defined(OMIM_OS_LINUX)
  using TGraphicsReadyFn = df::DrapeEngine::GraphicsReadyHandler;
  void NotifyGraphicsReady(TGraphicsReadyFn const & fn, bool needInvalidate);
#endif

  void StopLocationFollow();

  /// Resize event from window.
  void OnSize(df::DrapeEngineId engineId, int w, int h);

  enum EScaleMode
  {
    SCALE_MAG,
    SCALE_MAG_LIGHT,
    SCALE_MIN,
    SCALE_MIN_LIGHT
  };

  void Scale(df::DrapeEngineId engineId, EScaleMode mode, bool isAnim);
  void Scale(df::DrapeEngineId engineId, EScaleMode mode, m2::PointD const & pxPoint, bool isAnim);
  void Scale(df::DrapeEngineId engineId, double factor, bool isAnim);
  void Scale(df::DrapeEngineId engineId, double factor, m2::PointD const & pxPoint, bool isAnim);

  /// Moves the viewport a distance of factorX * viewportWidth and factorY * viewportHeight.
  /// E.g. factorX == 1.0 moves the map one screen size to the right, factorX == -0.5 moves the map
  /// half screen size to the left, factorY == -2.0 moves the map two sizes down,
  /// factorY = 1.5 moves the map one and a half size up.
  void Move(df::DrapeEngineId engineId, double factorX, double factorY, bool isAnim);

  void Rotate(double azimuth, bool isAnim);

  void TouchEvent(df::DrapeEngineId engineId, df::TouchEvent const & touch);

  int GetDrawScale() const;

  void RunFirstLaunchAnimation();

  /// Set correct viewport, parse API, show balloon.
  bool ShowMapForURL(std::string const & url);
  url_scheme::ParsedMapApi::ParsingResult ParseAndSetApiURL(std::string const & url);

  struct ParsedRoutingData
  {
    ParsedRoutingData(std::vector<url_scheme::RoutePoint> const & points, routing::RouterType type)
      : m_points(points), m_type(type)
    {
    }
    std::vector<url_scheme::RoutePoint> m_points;
    routing::RouterType m_type;
  };

  ParsedRoutingData GetParsedRoutingData() const;
  url_scheme::SearchRequest GetParsedSearchRequest() const;
  std::string const & GetParsedAppName() const;
  ms::LatLon GetParsedCenterLatLon() const;

  using FeatureMatcher = std::function<bool(FeatureType & ft)>;

private:
  /// @returns true if command was handled by editor.
  bool ParseEditorDebugCommand(search::SearchParams const & params);

  /// @returns true if command was handled by drape.
  bool ParseDrapeDebugCommand(std::string const & query);

  /// This function can be used for enabling some experimental features for routing.
  bool ParseRoutingDebugCommand(search::SearchParams const & params);

  void FillFeatureInfo(FeatureID const & fid, place_page::Info & info) const;
  /// @param customTitle, if not empty, overrides any other calculated name.
  void FillPointInfo(place_page::Info & info, m2::PointD const & mercator,
                     std::string const & customTitle = {},
                     FeatureMatcher && matcher = nullptr) const;
  void FillNotMatchedPlaceInfo(place_page::Info & info, m2::PointD const & mercator,
                               std::string const & customTitle = {}) const;
  void FillPostcodeInfo(std::string const & postcode, m2::PointD const & mercator,
                        place_page::Info & info) const;

  void FillInfoFromFeatureType(FeatureType & ft, place_page::Info & info) const;
  void FillApiMarkInfo(ApiMarkPoint const & api, place_page::Info & info) const;
  void FillSearchResultInfo(SearchMarkPoint const & smp, place_page::Info & info) const;
  void FillMyPositionInfo(place_page::Info & info, place_page::BuildInfo const & buildInfo) const;
  void FillRouteMarkInfo(RouteMarkPoint const & rmp, place_page::Info & info) const;
  void FillSpeedCameraMarkInfo(SpeedCameraMark const & speedCameraMark, place_page::Info & info) const;
  void FillTransitMarkInfo(TransitMark const & transitMark, place_page::Info & info) const;
  void FillRoadTypeMarkInfo(RoadWarningMark const & roadTypeMark, place_page::Info & info) const;
  void FillPointInfoForBookmark(Bookmark const & bmk, place_page::Info & info) const;
  void FillBookmarkInfo(Bookmark const & bmk, place_page::Info & info) const;
  void FillTrackInfo(Track const & track, m2::PointD const & trackPoint,
                     place_page::Info & info) const;
  void SetPlacePageLocation(place_page::Info & info);
  void FillDescription(FeatureType & ft, place_page::Info & info) const;

public:
  search::ReverseGeocoder::Address GetAddressAtPoint(m2::PointD const & pt,
                                                     double distanceThresholdMeters = 0.5) const;

  /// Get "best for the user" feature at given point even if it's invisible on the screen.
  /// Ignores coastlines and prefers buildings over other area features.
  /// @returns invalid FeatureID if no feature was found at the given mercator point.
  FeatureID GetFeatureAtPoint(m2::PointD const & mercator,
                              FeatureMatcher && matcher = nullptr) const;
  template <typename TFn>
  void ForEachFeatureAtPoint(TFn && fn, m2::PointD const & mercator) const
  {
    indexer::ForEachFeatureAtPoint(m_featuresFetcher.GetDataSource(), fn, mercator, 0.0);
  }

  osm::MapObject GetMapObjectByID(FeatureID const & fid) const;

  void MemoryWarning();
  void EnterBackground();
  void EnterForeground();

  /// Set the localized strings bundle
  void AddString(std::string const & name, std::string const & value)
  {
    m_stringsBundle.SetString(name, value);
  }

  StringsBundle const & GetStringsBundle();

  /// [in] lat, lon - last known location
  /// [out] lat, lon - predicted location
  static void PredictLocation(double & lat, double & lon, double accuracy,
                              double bearing, double speed, double elapsedSeconds);

public:
  static std::string CodeGe0url(Bookmark const * bmk, bool addName);
  static std::string CodeGe0url(double lat, double lon, double zoomLevel, std::string const & name);

  /// @name Api
  std::string GenerateApiBackUrl(ApiMarkPoint const & point) const;
  url_scheme::ParsedMapApi const & GetApiDataHolder() const { return m_parsedMapApi; }

private:
  url_scheme::ParsedMapApi m_parsedMapApi;

public:
  /// @name Data versions
  bool IsDataVersionUpdated();
  void UpdateSavedDataVersion();
  int64_t GetCurrentDataVersion() const;

public:
  void AllowTransliteration(bool allowTranslit);
  bool LoadTransliteration();
  void SaveTransliteration(bool allowTranslit);

  void Allow3dMode(bool allow3d, bool allow3dBuildings);
  void Save3dMode(bool allow3d, bool allow3dBuildings);
  void Load3dMode(bool & allow3d, bool & allow3dBuildings);

  void SetLargeFontsSize(bool isLargeSize);
  void SaveLargeFontsSize(bool isLargeSize);
  bool LoadLargeFontsSize();

  bool LoadAutoZoom();
  void AllowAutoZoom(bool allowAutoZoom);
  void SaveAutoZoom(bool allowAutoZoom);

  TrafficManager & GetTrafficManager();

  TransitReadManager & GetTransitManager();

  IsolinesManager & GetIsolinesManager();
  IsolinesManager const & GetIsolinesManager() const;

  bool LoadTrafficEnabled();
  void SaveTrafficEnabled(bool trafficEnabled);

  bool LoadTrafficSimplifiedColors();
  void SaveTrafficSimplifiedColors(bool simplified);

  bool LoadTransitSchemeEnabled();
  void SaveTransitSchemeEnabled(bool enabled);

  bool LoadIsolinesEnabled();
  void SaveIsolinesEnabled(bool enabled);

  dp::ApiVersion LoadPreferredGraphicsAPI();
  void SavePreferredGraphicsAPI(dp::ApiVersion apiVersion);

public:
  /// Routing Manager
  RoutingManager & GetRoutingManager() { return m_routingManager; }
  RoutingManager const & GetRoutingManager() const { return m_routingManager; }

protected:
  /// RoutingManager::Delegate
  void OnRouteFollow(routing::RouterType type) override;
  void RegisterCountryFilesOnRoute(std::shared_ptr<routing::NumMwmIds> ptr) const override;

public:
  /// @name Editor interface.
  /// Initializes feature for Create Object UI.
  /// @returns false in case when coordinate is in the ocean or mwm is not downloaded.
  bool CanEditMap() const;

  bool CreateMapObject(m2::PointD const & mercator, uint32_t const featureType, osm::EditableMapObject & emo) const;
  /// @returns false if feature is invalid or can't be edited.
  bool GetEditableMapObject(FeatureID const & fid, osm::EditableMapObject & emo) const;
  osm::Editor::SaveResult SaveEditedMapObject(osm::EditableMapObject emo);
  void DeleteFeature(FeatureID const & fid);
  osm::NewFeatureCategories GetEditorCategories() const;
  bool RollBackChanges(FeatureID const & fid);
  void CreateNote(osm::MapObject const & mapObject, osm::Editor::NoteProblemType const type,
                  std::string const & note);

private:
  CachingAddressGetter m_addressGetter;

public:
  power_management::PowerManager & GetPowerManager() { return m_powerManager; }

  // PowerManager::Subscriber override.
  void OnPowerFacilityChanged(power_management::Facility const facility, bool enabled) override;
  void OnPowerSchemeChanged(power_management::Scheme const actualScheme) override;
};
