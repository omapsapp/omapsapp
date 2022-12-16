#include "map/framework.hpp"
#include "map/benchmark_tools.hpp"
#include "map/gps_tracker.hpp"
#include "map/user_mark.hpp"
#include "map/track_mark.hpp"

#include "ge0/geo_url_parser.hpp"
#include "ge0/parser.hpp"
#include "ge0/url_generator.hpp"

#include "routing/index_router.hpp"
#include "routing/route.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/speed_camera_prohibition.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "search/editor_delegate.hpp"
#include "search/engine.hpp"
#include "search/locality_finder.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/routing_helpers.hpp"
#include "storage/storage_helpers.hpp"

#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/gps_track_point.hpp"
#include "drape_frontend/visual_params.hpp"

#include "editor/editable_data_source.hpp"

#include "descriptions/loader.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/classificator.hpp"
#include "indexer/drawing_rules.hpp"
#include "indexer/editable_map_object.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_source.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/feature_visibility.hpp"
#include "indexer/map_style_reader.hpp"
#include "indexer/scales.hpp"
#include "indexer/transliteration_loader.hpp"

#include "platform/local_country_file_utils.hpp"
#include "platform/localization.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"
#include "platform/settings.hpp"

#include "coding/endianness.hpp"
#include "coding/point_coding.hpp"
#include "coding/string_utf8_multilang.hpp"
#include "coding/transliteration.hpp"
#include "coding/url.hpp"

#include "geometry/angles.hpp"
#include "geometry/any_rect2d.hpp"
#include "geometry/distance_on_sphere.hpp"
#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"
#include "geometry/rect2d.hpp"
#include "geometry/triangle2d.hpp"

#include "base/logging.hpp"
#include "base/math.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include "std/target_os.hpp"

#include "defines.hpp"

#include <algorithm>


using namespace location;
using namespace routing;
using namespace storage;
using namespace std::placeholders;
using namespace std;

using platform::CountryFile;
using platform::LocalCountryFile;

#ifdef FIXED_LOCATION
Framework::FixedPosition::FixedPosition()
{
  m_fixedLatLon = Settings::Get("FixPosition", m_latlon);
  m_fixedDir = Settings::Get("FixDirection", m_dirFromNorth);
}
#endif

namespace
{
char const kMapStyleKey[] = "MapStyleKeyV1";
char const kAllow3dKey[] = "Allow3d";
char const kAllow3dBuildingsKey[] = "Buildings3d";
char const kAllowAutoZoom[] = "AutoZoom";
char const kTrafficEnabledKey[] = "TrafficEnabled";
char const kTransitSchemeEnabledKey[] = "TransitSchemeEnabled";
char const kIsolinesEnabledKey[] = "IsolinesEnabled";
char const kTrafficSimplifiedColorsKey[] = "TrafficSimplifiedColors";
char const kLargeFontsSize[] = "LargeFontsSize";
char const kTranslitMode[] = "TransliterationMode";
char const kPreferredGraphicsAPI[] = "PreferredGraphicsAPI";
char const kShowDebugInfo[] = "DebugInfo";

auto constexpr kLargeFontsScaleFactor = 1.6;
size_t constexpr kMaxTrafficCacheSizeBytes = 64 /* Mb */ * 1024 * 1024;

// TODO!
// To adjust GpsTrackFilter was added secret command "?gpstrackaccuracy:xxx;"
// where xxx is a new value for horizontal accuracy.
// This is temporary solution while we don't have a good filter.
bool ParseSetGpsTrackMinAccuracyCommand(string const & query)
{
  const char kGpsAccuracy[] = "?gpstrackaccuracy:";
  if (!strings::StartsWith(query, kGpsAccuracy))
    return false;

  size_t const end = query.find(';', sizeof(kGpsAccuracy) - 1);
  if (end == string::npos)
    return false;

  string s(query.begin() + sizeof(kGpsAccuracy) - 1, query.begin() + end);
  double value;
  if (!strings::to_double(s, value))
    return false;

  GpsTrackFilter::StoreMinHorizontalAccuracy(value);
  return true;
}
}  // namespace

pair<MwmSet::MwmId, MwmSet::RegResult> Framework::RegisterMap(LocalCountryFile const & file)
{
  auto const res = m_featuresFetcher.RegisterMap(file);
  if (res.second == MwmSet::RegResult::Success)
  {
    auto const & id = res.first;
    ASSERT(id.IsAlive(), ());
    LOG(LINFO, ("Loaded", file.GetCountryName(), "map, of version", id.GetInfo()->GetVersion()));
  }

  return res;
}

void Framework::OnLocationError(TLocationError /*error*/)
{
  m_trafficManager.UpdateMyPosition(TrafficManager::MyPosition());
  for (const auto& [_, drapeEngineData] : m_drapeEngines)
  {
    if (drapeEngineData.m_drapeEngine)
        drapeEngineData.m_drapeEngine->LoseLocation();
  }
}

void Framework::OnLocationUpdate(GpsInfo const & info)
{
#ifdef FIXED_LOCATION
  GpsInfo rInfo(info);

  // get fixed coordinates
  m_fixedPos.GetLon(rInfo.m_longitude);
  m_fixedPos.GetLat(rInfo.m_latitude);

  // pretend like GPS position
  rInfo.m_horizontalAccuracy = 5.0;

  if (m_fixedPos.HasNorth())
  {
    // pass compass value (for devices without compass)
    CompassInfo compass;
    m_fixedPos.GetNorth(compass.m_bearing);
    OnCompassUpdate(compass);
  }

#else
  GpsInfo const & rInfo = info;
#endif

  m_routingManager.OnLocationUpdate(rInfo);
}

void Framework::OnCompassUpdate(CompassInfo const & info)
{
#ifdef FIXED_LOCATION
  CompassInfo rInfo(info);
  m_fixedPos.GetNorth(rInfo.m_bearing);
#else
  CompassInfo const & rInfo = info;
#endif

  for (const auto& [_, drapeEngineData] : m_drapeEngines)
  {
    if (drapeEngineData.m_drapeEngine)
        drapeEngineData.m_drapeEngine->SetCompassInfo(rInfo);
  }
}

void Framework::SwitchMyPositionNextMode()
{
  for (const auto& [engineId, drapeEngineData] : m_drapeEngines)
    if (drapeEngineData.m_drapeEngine)
      drapeEngineData.m_drapeEngine->SwitchMyPositionNextMode();
}

void Framework::SetMyPositionModeListener(TMyPositionModeChanged && fn)
{
  m_myPositionListener = std::move(fn);
}

void Framework::SetMyPositionPendingTimeoutListener(df::DrapeEngine::UserPositionPendingTimeoutHandler && fn)
{
  m_myPositionPendingTimeoutListener = std::move(fn);
}

EMyPositionMode Framework::GetMyPositionMode() const
{
  for (const auto& [engineId, drapeEngineData] : m_drapeEngines)
    if (drapeEngineData.m_drapeEngine)
      return drapeEngineData.m_drapeEngine->GetMyPositionMode();
  return PendingPosition;
}

TrafficManager & Framework::GetTrafficManager()
{
  return m_trafficManager;
}

TransitReadManager & Framework::GetTransitManager()
{
  return m_transitManager;
}

IsolinesManager & Framework::GetIsolinesManager()
{
  return m_isolinesManager;
}

IsolinesManager const & Framework::GetIsolinesManager() const
{
  return m_isolinesManager;
}

void Framework::OnUserPositionChanged(m2::PointD const & position, bool hasPosition)
{
  GetBookmarkManager().MyPositionMark().SetUserPosition(position, hasPosition);
  if (m_currentPlacePageInfo && m_currentPlacePageInfo->GetTrackId() != kml::kInvalidTrackId)
    GetBookmarkManager().UpdateElevationMyPosition(m_currentPlacePageInfo->GetTrackId());

  m_routingManager.SetUserCurrentPosition(position);
  m_trafficManager.UpdateMyPosition(TrafficManager::MyPosition(position));
}

void Framework::OnViewportChanged(ScreenBase const & screen)
{
  // Drape engine may spuriously call OnViewportChanged. Filter out the calls that
  // change the viewport from the drape engine's point of view but leave it almost
  // the same from the point of view of the framework and all its subsystems such as search api.
  // Additional filtering may be done by each subsystem.
  auto const isSameViewport = m2::IsEqual(screen.ClipRect(), m_currentModelView.ClipRect(),
                                          kMwmPointAccuracy, kMwmPointAccuracy);
  if (isSameViewport)
    return;

  m_currentModelView = screen;

  GetSearchAPI().OnViewportChanged(GetCurrentViewport());

  GetBookmarkManager().UpdateViewport(m_currentModelView);
  m_trafficManager.UpdateViewport(m_currentModelView);
  m_transitManager.UpdateViewport(m_currentModelView);
  m_isolinesManager.UpdateViewport(m_currentModelView);

  if (m_viewportChangedFn != nullptr)
    m_viewportChangedFn(screen);
}

Framework::Framework(FrameworkParams const & params)
  : m_enabledDiffs(params.m_enableDiffs)
  , m_isRenderingEnabled(true)
  , m_transitManager(m_featuresFetcher.GetDataSource(),
                     [this](FeatureCallback const & fn, vector<FeatureID> const & features) {
                       return m_featuresFetcher.ReadFeatures(fn, features);
                     },
                     bind(&Framework::GetMwmsByRect, this, _1, false /* rough */))
  , m_isolinesManager(m_featuresFetcher.GetDataSource(),
                      bind(&Framework::GetMwmsByRect, this, _1, false /* rough */))
  , m_routingManager(
        RoutingManager::Callbacks(
            [this]() -> DataSource & { return m_featuresFetcher.GetDataSource(); },
            [this]() -> storage::CountryInfoGetter const & { return GetCountryInfoGetter(); },
            [this](string const & id) -> string { return m_storage.GetParentIdFor(id); },
            [this]() -> StringsBundle const & { return m_stringsBundle; },
            [this]() -> power_management::PowerManager const & { return m_powerManager; }),
        static_cast<RoutingManager::Delegate &>(*this))
  , m_trafficManager(bind(&Framework::GetMwmsByRect, this, _1, false /* rough */),
                     kMaxTrafficCacheSizeBytes, m_routingManager.RoutingSession())
  , m_lastReportedCountry(kInvalidCountryId)
  , m_popularityLoader(m_featuresFetcher.GetDataSource(), POPULARITY_RANKS_FILE_TAG)
  , m_descriptionsLoader(std::make_unique<descriptions::Loader>(m_featuresFetcher.GetDataSource()))
{
  CHECK(IsLittleEndian(), ("Only little-endian architectures are supported."));

  // Editor should be initialized from the main thread to set its ThreadChecker.
  // However, search calls editor upon initialization thus setting the lazy editor's ThreadChecker
  // to a wrong thread. So editor should be initialiazed before serach.
  osm::Editor & editor = osm::Editor::Instance();

  // Restore map style before classificator loading
  MapStyle mapStyle = kDefaultMapStyle;
  string mapStyleStr;
  if (settings::Get(kMapStyleKey, mapStyleStr))
    mapStyle = MapStyleFromSettings(mapStyleStr);
  GetStyleReader().SetCurrentStyle(mapStyle);
  df::LoadTransitColors();

  m_connectToGpsTrack = GpsTracker::Instance().IsEnabled();

  // Init strings bundle.
  // @TODO. There are hardcoded strings below which are defined in strings.txt as well.
  // It's better to use strings from strings.txt instead of hardcoding them here.
  m_stringsBundle.SetDefaultString("core_entrance", "Entrance");
  m_stringsBundle.SetDefaultString("core_exit", "Exit");
  m_stringsBundle.SetDefaultString("core_placepage_unknown_place", "Unknown Place");
  m_stringsBundle.SetDefaultString("core_my_places", "My Places");
  m_stringsBundle.SetDefaultString("core_my_position", "My Position");
  m_stringsBundle.SetDefaultString("postal_code", "Postal Code");
  // Wi-Fi string is used in categories that's why does not have core_ prefix
  m_stringsBundle.SetDefaultString("wifi", "WiFi");

  m_featuresFetcher.InitClassificator();
  m_featuresFetcher.SetOnMapDeregisteredCallback(bind(&Framework::OnMapDeregistered, this, _1));
  LOG(LDEBUG, ("Classificator initialized"));

  m_displayedCategories = make_unique<search::DisplayedCategories>(GetDefaultCategories());

  // To avoid possible races - init country info getter in constructor.
  InitCountryInfoGetter();
  LOG(LDEBUG, ("Country info getter initialized"));

  InitSearchAPI(params.m_numSearchAPIThreads);
  LOG(LDEBUG, ("Search API initialized"));

  m_bmManager = make_unique<BookmarkManager>(BookmarkManager::Callbacks(
      [this]() -> StringsBundle const & { return m_stringsBundle; },
      [this]() -> SearchAPI & { return GetSearchAPI(); },
      [this](vector<BookmarkInfo> const & marks) { GetSearchAPI().OnBookmarksCreated(marks); },
      [this](vector<BookmarkInfo> const & marks) { GetSearchAPI().OnBookmarksUpdated(marks); },
      [this](vector<kml::MarkId> const & marks) { GetSearchAPI().OnBookmarksDeleted(marks); },
      [this](vector<BookmarkGroupInfo> const & marks) { GetSearchAPI().OnBookmarksAttached(marks); },
      [this](vector<BookmarkGroupInfo> const & marks) { GetSearchAPI().OnBookmarksDetached(marks); }));

  m_bmManager->InitRegionAddressGetter(m_featuresFetcher.GetDataSource(), *m_infoGetter);

  m_parsedMapApi.SetBookmarkManager(m_bmManager.get());
  m_routingManager.SetBookmarkManager(m_bmManager.get());
  m_searchMarks.SetBookmarkManager(m_bmManager.get());

  m_routingManager.SetTransitManager(&m_transitManager);

  // Init storage with needed callback.
  m_storage.Init(bind(&Framework::OnCountryFileDownloaded, this, _1, _2),
                 bind(&Framework::OnCountryFileDelete, this, _1, _2));

  m_storage.SetDownloadingPolicy(&m_storageDownloadingPolicy);
  m_storage.SetStartDownloadingCallback([this]() { UpdatePlacePageInfoForCurrentSelection(); });
  LOG(LDEBUG, ("Storage initialized"));

  RegisterAllMaps();
  LOG(LDEBUG, ("Maps initialized"));

  // Perform real initialization after World was loaded.
  GetSearchAPI().InitAfterWorldLoaded();

  m_routingManager.SetRouterImpl(RouterType::Vehicle);

  UpdateMinBuildingsTapZoom();

  LOG(LDEBUG, ("Routing engine initialized"));

  LOG(LINFO, ("System languages:", languages::GetPreferred()));

  editor.SetDelegate(make_unique<search::EditorDelegate>(m_featuresFetcher.GetDataSource()));
  editor.SetInvalidateFn([this](){ InvalidateRect(GetCurrentViewport()); });
  editor.LoadEdits();

  m_featuresFetcher.GetDataSource().AddObserver(editor);

  LOG(LDEBUG, ("Editor initialized"));

  m_trafficManager.SetCurrentDataVersion(m_storage.GetCurrentDataVersion());
  m_trafficManager.SetSimplifiedColorScheme(LoadTrafficSimplifiedColors());
  m_trafficManager.SetEnabled(LoadTrafficEnabled());

  m_isolinesManager.SetEnabled(LoadIsolinesEnabled());

  InitTransliteration();
  LOG(LDEBUG, ("Transliterators initialized"));

  GetPowerManager().Subscribe(this);
  GetPowerManager().Load();
}

Framework::~Framework()
{
  GetPowerManager().UnsubscribeAll();

  m_threadRunner.reset();

  osm::Editor & editor = osm::Editor::Instance();

  editor.SetDelegate({});
  editor.SetInvalidateFn({});

  GetBookmarkManager().Teardown();
  m_trafficManager.Teardown();
  DestroyDrapeEngine();
  m_featuresFetcher.SetOnMapDeregisteredCallback(nullptr);
}

void Framework::ShowNode(storage::CountryId const & countryId)
{
  StopLocationFollow();

  ShowRect(CalcLimitRect(countryId, GetStorage(), GetCountryInfoGetter()));
}

void Framework::OnCountryFileDownloaded(storage::CountryId const &,
                                        storage::LocalFilePtr const localFile)
{
  // Soft reset to signal that mwm file may be out of date in routing caches.
  m_routingManager.ResetRoutingSession();

  m2::RectD rect = mercator::Bounds::FullRect();

  if (localFile && localFile->OnDisk(MapFileType::Map))
  {
    auto const res = RegisterMap(*localFile);
    MwmSet::MwmId const & id = res.first;
    if (id.IsAlive())
      rect = id.GetInfo()->m_bordersRect;
  }

  m_trafficManager.Invalidate();
  m_transitManager.Invalidate();
  m_isolinesManager.Invalidate();

  InvalidateRect(rect);
  GetSearchAPI().ClearCaches();
}

bool Framework::OnCountryFileDelete(storage::CountryId const & countryId,
                                    storage::LocalFilePtr const localFile)
{
  // Soft reset to signal that mwm file may be out of date in routing caches.
  m_routingManager.ResetRoutingSession();

  if (countryId == m_lastReportedCountry)
    m_lastReportedCountry = kInvalidCountryId;

  GetSearchAPI().CancelAllSearches();

  m2::RectD rect = mercator::Bounds::FullRect();

  bool deferredDelete = false;
  if (localFile)
  {
    rect = m_infoGetter->GetLimitRectForLeaf(countryId);
    m_featuresFetcher.DeregisterMap(platform::CountryFile(countryId));
    deferredDelete = true;
  }
  InvalidateRect(rect);

  GetSearchAPI().ClearCaches();
  return deferredDelete;
}

void Framework::OnMapDeregistered(platform::LocalCountryFile const & localFile)
{
  auto action = [this, localFile]
  {
    m_transitManager.OnMwmDeregistered(localFile);
    m_isolinesManager.OnMwmDeregistered(localFile);
    m_trafficManager.OnMwmDeregistered(localFile);
    m_popularityLoader.OnMwmDeregistered(localFile);

    m_storage.DeleteCustomCountryVersion(localFile);
  };

  // Call action on thread in which the framework was created
  // For more information look at comment for Observer class in mwm_set.hpp
  if (m_storage.GetThreadChecker().CalledOnOriginalThread())
    action();
  else
    GetPlatform().RunTask(Platform::Thread::Gui, action);
}

bool Framework::HasUnsavedEdits(storage::CountryId const & countryId)
{
  bool hasUnsavedChanges = false;
  auto const forEachInSubtree = [&hasUnsavedChanges, this](storage::CountryId const & fileName,
                                                           bool groupNode) {
    if (groupNode)
      return;
    hasUnsavedChanges |= osm::Editor::Instance().HaveMapEditsToUpload(
          m_featuresFetcher.GetDataSource().GetMwmIdByCountryFile(platform::CountryFile(fileName)));
  };
  GetStorage().ForEachInSubtree(countryId, forEachInSubtree);
  return hasUnsavedChanges;
}

void Framework::RegisterAllMaps()
{
  ASSERT(!m_storage.IsDownloadInProgress(),
         ("Registering maps while map downloading leads to removing downloading maps from "
          "ActiveMapsListener::m_items."));

  m_storage.RegisterAllLocalMaps(m_enabledDiffs);

  vector<shared_ptr<LocalCountryFile>> maps;
  m_storage.GetLocalMaps(maps);
  for (auto const & localFile : maps)
    UNUSED_VALUE(RegisterMap(*localFile));
}

void Framework::DeregisterAllMaps()
{
  m_featuresFetcher.Clear();
  m_storage.Clear();
}

void Framework::LoadBookmarks()
{
  GetBookmarkManager().LoadBookmarks();
}

kml::MarkGroupId Framework::AddCategory(string const & categoryName)
{
  return GetBookmarkManager().CreateBookmarkCategory(categoryName);
}

void Framework::FillPointInfoForBookmark(Bookmark const & bmk, place_page::Info & info) const
{
  // Convert indices to sorted classifier types.
  Classificator const & cl = classif();
  buffer_vector<uint8_t, 8> types;
  for (uint32_t i : bmk.GetData().m_featureTypes)
    types.push_back(cl.GetTypeForIndex(i));
  std::sort(types.begin(), types.end());

  FillPointInfo(info, bmk.GetPivot(), {} /* customTitle */, [&types](FeatureType & ft)
  {
    if (types.empty() || ft.GetTypesCount() != types.size())
      return false;

    // Strict equal types.
    feature::TypesHolder fTypes(ft);
    std::sort(fTypes.begin(), fTypes.end());
    return std::equal(types.begin(), types.end(), fTypes.begin(), fTypes.end());
  });
}

void Framework::FillBookmarkInfo(Bookmark const & bmk, place_page::Info & info) const
{
  info.SetBookmarkCategoryName(GetBookmarkManager().GetCategoryName(bmk.GetGroupId()));
  info.SetBookmarkData(bmk.GetData());
  info.SetBookmarkId(bmk.GetId());
  info.SetBookmarkCategoryId(bmk.GetGroupId());
  auto const description = GetPreferredBookmarkStr(info.GetBookmarkData().m_description);
  auto const openingMode = m_routingManager.IsRoutingActive() || description.empty()
                     ? place_page::OpeningMode::Preview
                     : place_page::OpeningMode::PreviewPlus;
  info.SetOpeningMode(openingMode);
  if (bmk.CanFillPlacePageMetadata())
  {
    info.SetMercator(bmk.GetPivot());
    info.SetTitlesForBookmark();
    info.SetCanEditOrAdd(false);
    info.SetFromBookmarkProperties(bmk.GetData().m_properties);
  }
  else
  {
    FillPointInfoForBookmark(bmk, info);
  }
}

void Framework::FillTrackInfo(Track const & track, m2::PointD const & trackPoint,
                              place_page::Info & info) const
{
  info.SetTrackId(track.GetId());
  info.SetBookmarkCategoryId(track.GetGroupId());
  info.SetMercator(trackPoint);
}

search::ReverseGeocoder::Address Framework::GetAddressAtPoint(m2::PointD const & pt,
                                                              double distanceThresholdMeters) const
{
  return m_addressGetter.GetAddressAtPoint(m_featuresFetcher.GetDataSource(), pt, distanceThresholdMeters);
}

void Framework::FillFeatureInfo(FeatureID const & fid, place_page::Info & info) const
{
  if (!fid.IsValid())
  {
    LOG(LERROR, ("FeatureID is invalid:", fid));
    return;
  }

  FeaturesLoaderGuard const guard(m_featuresFetcher.GetDataSource(), fid.m_mwmId);
  auto ft = guard.GetFeatureByIndex(fid.m_index);
  if (!ft)
  {
    LOG(LERROR, ("Feature can't be loaded:", fid));
    return;
  }

  FillInfoFromFeatureType(*ft, info);
}

void Framework::FillPointInfo(place_page::Info & info, m2::PointD const & mercator,
                              string const & customTitle /* = {} */,
                              FeatureMatcher && matcher /* = nullptr */) const
{
  auto const fid = GetFeatureAtPoint(mercator, std::move(matcher));
  if (fid.IsValid())
  {
    m_featuresFetcher.GetDataSource().ReadFeature(
        [&](FeatureType & ft) { FillInfoFromFeatureType(ft, info); }, fid);
    // This line overwrites mercator center from area feature which can be far away.
    info.SetMercator(mercator);
  }
  else
  {
    FillNotMatchedPlaceInfo(info, mercator, customTitle);
  }
}

void Framework::FillNotMatchedPlaceInfo(place_page::Info & info, m2::PointD const & mercator,
                                        std::string const & customTitle /* = {} */) const
{
  if (customTitle.empty())
    info.SetCustomNameWithCoordinates(mercator, m_stringsBundle.GetString("core_placepage_unknown_place"));
  else
    info.SetCustomName(customTitle);
  info.SetCanEditOrAdd(CanEditMap());
  info.SetMercator(mercator);
}

void Framework::FillPostcodeInfo(string const & postcode, m2::PointD const & mercator,
                                 place_page::Info & info) const
{
  info.SetCustomNames(postcode, m_stringsBundle.GetString("postal_code"));
  info.SetMercator(mercator);
}

void Framework::FillInfoFromFeatureType(FeatureType & ft, place_page::Info & info) const
{
  auto const featureStatus = osm::Editor::Instance().GetFeatureStatus(ft.GetID());
  ASSERT_NOT_EQUAL(featureStatus, FeatureStatus::Deleted,
                   ("Deleted features cannot be selected from UI."));
  info.SetFeatureStatus(featureStatus);
  info.SetLocalizedWifiString(m_stringsBundle.GetString("wifi"));

  if (ftypes::IsAddressObjectChecker::Instance()(ft))
    info.SetAddress(GetAddressAtPoint(feature::GetCenter(ft)).FormatAddress());

  info.SetFromFeatureType(ft);

  FillDescription(ft, info);

  auto const mwmInfo = ft.GetID().m_mwmId.GetInfo();
  bool const isMapVersionEditable = mwmInfo && mwmInfo->m_version.IsEditableMap();
  bool const canEditOrAdd = featureStatus != FeatureStatus::Obsolete && CanEditMap() &&
                            isMapVersionEditable;
  info.SetCanEditOrAdd(canEditOrAdd);
  info.SetPopularity(m_popularityLoader.Get(ft.GetID()));

  // Fill countryId for place page info
  auto const & types = info.GetTypes();
  bool const isState = ftypes::IsStateChecker::Instance()(types);
  if (isState || ftypes::IsCountryChecker::Instance()(types))
  {
    size_t const level = isState ? 1 : 0;
    CountriesVec countries;
    CountryId countryId = m_infoGetter->GetRegionCountryId(info.GetMercator());
    GetStorage().GetTopmostNodesFor(countryId, countries, level);
    if (countries.size() == 1)
      countryId = countries.front();

    info.SetCountryId(countryId);
    info.SetTopmostCountryIds(std::move(countries));
  }
}

void Framework::FillApiMarkInfo(ApiMarkPoint const & api, place_page::Info & info) const
{
  FillPointInfo(info, api.GetPivot());
  string const & name = api.GetName();
  if (!name.empty())
    info.SetCustomName(name);
  info.SetApiId(api.GetApiID());
  info.SetApiUrl(GenerateApiBackUrl(api));
}

void Framework::FillSearchResultInfo(SearchMarkPoint const & smp, place_page::Info & info) const
{
  if (smp.GetFeatureID().IsValid())
    FillFeatureInfo(smp.GetFeatureID(), info);
  else
    FillPointInfo(info, smp.GetPivot(), smp.GetMatchedName());
}

void Framework::FillMyPositionInfo(place_page::Info & info, place_page::BuildInfo const & buildInfo) const
{
  auto const position = GetCurrentPosition();
  CHECK(position, ());
  info.SetMercator(*position);
  info.SetCustomName(m_stringsBundle.GetString("core_my_position"));

  UserMark const * mark = FindUserMarkInTapPosition(buildInfo);
  if (mark != nullptr && mark->GetMarkType() == UserMark::Type::ROUTING)
  {
    auto routingMark = static_cast<RouteMarkPoint const *>(mark);
    info.SetIsRoutePoint();
    info.SetRouteMarkType(routingMark->GetRoutePointType());
    info.SetIntermediateIndex(routingMark->GetIntermediateIndex());
  }
}

void Framework::FillRouteMarkInfo(RouteMarkPoint const & rmp, place_page::Info & info) const
{
  FillPointInfo(info, rmp.GetPivot());
  info.SetIsRoutePoint();
  info.SetRouteMarkType(rmp.GetRoutePointType());
  info.SetIntermediateIndex(rmp.GetIntermediateIndex());
}

void Framework::FillSpeedCameraMarkInfo(SpeedCameraMark const & speedCameraMark, place_page::Info & info) const
{
  info.SetCanEditOrAdd(false);
  info.SetMercator(speedCameraMark.GetPivot());

  // Title is a speed limit, if any.
  auto title = speedCameraMark.GetTitle();
  if (!title.empty())
    title = title + " " + platform::GetLocalizedSpeedUnits(measurement_utils::GetMeasurementUnits());

  info.SetCustomNames(title, platform::GetLocalizedTypeName("highway-speed_camera"));
}

void Framework::FillTransitMarkInfo(TransitMark const & transitMark, place_page::Info & info) const
{
  FillFeatureInfo(transitMark.GetFeatureID(), info);
  /// @todo Add useful info in PP for TransitMark (public transport).
}

void Framework::FillRoadTypeMarkInfo(RoadWarningMark const & roadTypeMark, place_page::Info & info) const
{
  if (roadTypeMark.GetFeatureID().IsValid())
  {
    FeaturesLoaderGuard const guard(m_featuresFetcher.GetDataSource(), roadTypeMark.GetFeatureID().m_mwmId);
    auto ft = guard.GetFeatureByIndex(roadTypeMark.GetFeatureID().m_index);
    if (ft)
    {
      FillInfoFromFeatureType(*ft, info);

      info.SetRoadType(*ft, roadTypeMark.GetRoadWarningType(),
                       RoadWarningMark::GetLocalizedRoadWarningType(roadTypeMark.GetRoadWarningType()),
                       roadTypeMark.GetDistance());
      info.SetMercator(roadTypeMark.GetPivot());
      return;
    }
    else
    {
      LOG(LERROR, ("Feature can't be loaded:", roadTypeMark.GetFeatureID()));
    }
  }

  info.SetRoadType(roadTypeMark.GetRoadWarningType(),
                   RoadWarningMark::GetLocalizedRoadWarningType(roadTypeMark.GetRoadWarningType()),
                   roadTypeMark.GetDistance());
  info.SetMercator(roadTypeMark.GetPivot());
}

void Framework::ShowBookmark(kml::MarkId id)
{
  auto const * mark = m_bmManager->GetBookmark(id);
  ShowBookmark(mark);
}

void Framework::ShowBookmark(Bookmark const * mark)
{
  if (mark == nullptr)
    return;

  StopLocationFollow();

  place_page::BuildInfo info;
  info.m_mercator = mark->GetPivot();
  info.m_userMarkId = mark->GetId();
  m_currentPlacePageInfo = BuildPlacePageInfo(info);

  auto scale = static_cast<int>(mark->GetScale());
  if (scale == 0)
    scale = scales::GetUpperComfortScale();

  auto es = GetBookmarkManager().GetEditSession();
  es.SetIsVisible(mark->GetGroupId(), true /* visible */);

//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//  {
//      m_drapeEngines.begin()->second.m_drapeEngine->SetModelViewCenter(mark->GetPivot(), scale, true /* isAnim */,
//                                                          true /* trackVisibleViewport */);
//  }

  ActivateMapSelection();
}

void Framework::ShowTrack(kml::TrackId trackId)
{
  auto & bm = GetBookmarkManager();
  auto const track = bm.GetTrack(trackId);
  if (track == nullptr)
    return;

  auto rect = track->GetLimitRect();
  ExpandRectForPreview(rect);

  StopLocationFollow();
  ShowRect(rect);

  auto es = GetBookmarkManager().GetEditSession();
  es.SetIsVisible(track->GetGroupId(), true /* visible */);

  if (track->IsInteractive())
    bm.SetDefaultTrackSelection(trackId, true /* showInfoSign */);
}

void Framework::ShowBookmarkCategory(kml::MarkGroupId categoryId, bool animation)
{
  auto & bm = GetBookmarkManager();
  auto rect = bm.GetCategoryRect(categoryId, true /* addIconsSize */);
  if (!rect.IsValid())
    return;

  ExpandRectForPreview(rect);

  StopLocationFollow();
  ShowRect(rect, -1 /* maxScale */, animation);

  auto es = bm.GetEditSession();
  es.SetIsVisible(categoryId, true /* visible */);

  auto const trackIds = bm.GetTrackIds(categoryId);
  for (auto trackId : trackIds)
  {
    if (!bm.GetTrack(trackId)->IsInteractive())
      continue;
    bm.SetDefaultTrackSelection(trackId, true /* showInfoSign */);
    break;
  }
}

void Framework::ShowFeature(FeatureID const & featureId)
{
  StopLocationFollow();

  place_page::BuildInfo info;
  info.m_featureId = featureId;
  info.m_match = place_page::BuildInfo::Match::FeatureOnly;
  m_currentPlacePageInfo = BuildPlacePageInfo(info);

//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//  {
//    auto const pt = m_currentPlacePageInfo->GetMercator();
//    auto const scale = scales::GetUpperComfortScale();
//    m_drapeEngines.begin()->second.m_drapeEngine->SetModelViewCenter(pt, scale, true /* isAnim */, true /* trackVisibleViewport */);
//  }
//  ActivateMapSelection();
}

void Framework::AddBookmarksFile(string const & filePath, bool isTemporaryFile)
{
  GetBookmarkManager().LoadBookmark(filePath, isTemporaryFile);
}

void Framework::PrepareToShutdown()
{
  DestroyDrapeEngine();
}

void Framework::SaveViewport()
{
  m2::AnyRectD rect;
  if (m_currentModelView.isPerspective())
  {
    ScreenBase modelView = m_currentModelView;
    modelView.ResetPerspective();
    rect = modelView.GlobalRect();
  }
  else
  {
    rect = m_currentModelView.GlobalRect();
  }
  settings::Set("ScreenClipRect", rect);
}

void Framework::LoadViewport()
{
  m2::AnyRectD rect;
  if (settings::Get("ScreenClipRect", rect) && df::GetWorldRect().IsRectInside(rect.GetGlobalRect()))
  {
//    if (m_drapeEngines.begin()->second.m_drapeEngine)
//      m_drapeEngines.begin()->second.m_drapeEngine->SetModelViewAnyRect(rect, false /* isAnim */, false /* useVisibleViewport */);
  }
  else
  {
    ShowAll();
  }
}

void Framework::ShowAll()
{
//  if (!m_drapeEngines.begin()->second.m_drapeEngine)
//    return;
//  m_drapeEngines.begin()->second.m_drapeEngine->SetModelViewAnyRect(m2::AnyRectD(m_featuresFetcher.GetWorldRect()), false /* isAnim */,
//                                                       false /* useVisibleViewport */);
}

m2::PointD Framework::GetVisiblePixelCenter() const
{
  return m_visibleViewport.Center();
}

m2::PointD const & Framework::GetViewportCenter() const
{
  return m_currentModelView.GetOrg();
}

void Framework::SetViewportCenter(m2::PointD const & pt, int zoomLevel /* = -1 */,
                                  bool isAnim /* = true */)
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//    m_drapeEngines.begin()->second.m_drapeEngine->SetModelViewCenter(pt, zoomLevel, isAnim, false /* trackVisibleViewport */);
}

m2::RectD Framework::GetCurrentViewport() const
{
  return m_currentModelView.ClipRect();
}

void Framework::SetVisibleViewport(df::DrapeEngineId engineId, m2::RectD const & rect)
{
  if (!m_drapeEngines[engineId].m_drapeEngine)
    return;

  double constexpr kEps = 0.5;
  if (m2::IsEqual(m_visibleViewport, rect, kEps, kEps))
    return;

  double constexpr kMinSize = 100.0;
  if (rect.SizeX() < kMinSize || rect.SizeY() < kMinSize)
    return;

  m_visibleViewport = rect;
  m_drapeEngines[engineId].m_drapeEngine->SetVisibleViewport(rect);
}

void Framework::ShowRect(m2::RectD const & rect, int maxScale, bool animation, bool useVisibleViewport)
{
//  if (!m_drapeEngines.begin()->second.m_drapeEngine)
//    return;
//
//  m_drapeEngines.begin()->second.m_drapeEngine->SetModelViewRect(rect, true /* applyRotation */, maxScale /* zoom */, animation,
//                                                    useVisibleViewport);
}

void Framework::ShowRect(m2::AnyRectD const & rect, bool animation, bool useVisibleViewport)
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//    m_drapeEngines.begin()->second.m_drapeEngine->SetModelViewAnyRect(rect, animation, useVisibleViewport);
}

void Framework::GetTouchRect(m2::PointD const & center, uint32_t pxRadius, m2::AnyRectD & rect)
{
  m_currentModelView.GetTouchRect(center, static_cast<double>(pxRadius), rect);
}

void Framework::SetViewportListener(TViewportChangedFn const & fn)
{
  m_viewportChangedFn = fn;
}

#if defined(OMIM_OS_MAC) || defined(OMIM_OS_LINUX)
void Framework::NotifyGraphicsReady(TGraphicsReadyFn const & fn, bool needInvalidate)
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//    m_drapeEngines.begin()->second.m_drapeEngine->NotifyGraphicsReady(fn, needInvalidate);
}
#endif

void Framework::StopLocationFollow()
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//    m_drapeEngines.begin()->second.m_drapeEngine->StopLocationFollow();
}

void Framework::OnSize(df::DrapeEngineId engineId, int w, int h)
{
  if (m_drapeEngines[engineId].m_drapeEngine != nullptr)
    m_drapeEngines[engineId].m_drapeEngine->Resize(std::max(w, 2), std::max(h, 2));

  /// @todo Expected that DrapeEngine::Resize does all the work, but nope ..
  /// - Strange, but seems like iOS works fine without it.
  /// - Test Android screen orientation and position mark in map and navigation modes.
  SetVisibleViewport(engineId,m2::RectD(0, 0, w, h));
}

namespace
{

double ScaleModeToFactor(Framework::EScaleMode mode)
{
  double factors[] = { 2.0, 1.5, 0.5, 0.67 };
  return factors[mode];
}

} // namespace

void Framework::Scale(df::DrapeEngineId engineId, EScaleMode mode, bool isAnim)
{
  Scale(engineId, ScaleModeToFactor(mode), isAnim);
}

void Framework::Scale(df::DrapeEngineId engineId, Framework::EScaleMode mode, m2::PointD const & pxPoint, bool isAnim)
{
  Scale(engineId, ScaleModeToFactor(mode), pxPoint, isAnim);
}

void Framework::Scale(df::DrapeEngineId engineId, double factor, bool isAnim)
{
  Scale(engineId, factor, GetVisiblePixelCenter(), isAnim);
}

void Framework::Scale(df::DrapeEngineId engineId, double factor, m2::PointD const & pxPoint, bool isAnim)
{
  if (m_drapeEngines[engineId].m_drapeEngine != nullptr)
      m_drapeEngines[engineId].m_drapeEngine->Scale(factor, pxPoint, isAnim);
}

void Framework::Move(df::DrapeEngineId engineId, double factorX, double factorY, bool isAnim)
{
  if (m_drapeEngines[engineId].m_drapeEngine != nullptr)
    m_drapeEngines[engineId].m_drapeEngine->Move(factorX, factorY, isAnim);
}

void Framework::Rotate(double azimuth, bool isAnim)
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr)
//    m_drapeEngines.begin()->second.m_drapeEngine->Rotate(azimuth, isAnim);
}

void Framework::TouchEvent(df::DrapeEngineId engineId, df::TouchEvent const & touch)
{
  if (m_drapeEngines[engineId].m_drapeEngine != nullptr)
    m_drapeEngines[engineId].m_drapeEngine->AddTouchEvent(touch);
}

int Framework::GetDrawScale() const
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr)
//    return df::GetDrawTileScale(m_currentModelView);

  return 0;
}

void Framework::RunFirstLaunchAnimation()
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr)
//    m_drapeEngines.begin()->second.m_drapeEngine->RunFirstLaunchAnimation();
}

bool Framework::IsCountryLoadedByName(string_view name) const
{
  return m_featuresFetcher.IsLoaded(name);
}

void Framework::InvalidateRect(m2::RectD const & rect)
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr)
//    m_drapeEngines.begin()->second.m_drapeEngine->InvalidateRect(rect);
}

void Framework::ClearAllCaches()
{
  m_featuresFetcher.ClearCaches();
  m_infoGetter->ClearCaches();
  GetSearchAPI().ClearCaches();
}

void Framework::OnUpdateCurrentCountry(m2::PointD const & pt, int zoomLevel)
{
  storage::CountryId newCountryId;
  if (zoomLevel > scales::GetUpperWorldScale())
    newCountryId = m_infoGetter->GetRegionCountryId(pt);

  if (newCountryId == m_lastReportedCountry)
    return;

  m_lastReportedCountry = newCountryId;

  GetPlatform().RunTask(Platform::Thread::Gui, [this, newCountryId]()
  {
    if (m_currentCountryChanged != nullptr)
      m_currentCountryChanged(newCountryId);
  });
}

void Framework::SetCurrentCountryChangedListener(TCurrentCountryChanged listener)
{
  m_currentCountryChanged = std::move(listener);
  m_lastReportedCountry = kInvalidCountryId;
}

void Framework::MemoryWarning()
{
  LOG(LINFO, ("MemoryWarning"));
  ClearAllCaches();
  SharedBufferManager::instance().clearReserved();
}

void Framework::EnterBackground()
{
  for (const auto& [engineId, drapeEngineData] : m_drapeEngines)
    if (drapeEngineData.m_drapeEngine)
      drapeEngineData.m_drapeEngine->OnEnterBackground();

  SaveViewport();

  m_trafficManager.OnEnterBackground();

  // Do not clear caches for Android. This function is called when main activity is paused,
  // but at the same time search activity (for example) is enabled.
  // TODO(AlexZ): Use onStart/onStop on Android to correctly detect app background and remove #ifndef.
#ifndef OMIM_OS_ANDROID
  ClearAllCaches();
#endif
}

void Framework::EnterForeground()
{
  for (const auto& [engineId, drapeEngineData] : m_drapeEngines)
    if (drapeEngineData.m_drapeEngine)
      drapeEngineData.m_drapeEngine->OnEnterForeground();

  m_trafficManager.OnEnterForeground();
}

void Framework::InitCountryInfoGetter()
{
  ASSERT(!m_infoGetter.get(), ("InitCountryInfoGetter() must be called only once."));

  auto const & platform = GetPlatform();
  m_infoGetter = CountryInfoReader::CreateCountryInfoGetter(platform);

  // Storage::GetAffiliations() pointer never changed.
  m_infoGetter->SetAffiliations(m_storage.GetAffiliations());
}

void Framework::InitSearchAPI(size_t numThreads)
{
  ASSERT(!m_searchAPI.get(), ("InitSearchAPI() must be called only once."));
  ASSERT(m_infoGetter.get(), ());
  try
  {
    m_searchAPI =
        make_unique<SearchAPI>(m_featuresFetcher.GetDataSource(), m_storage, *m_infoGetter,
                               numThreads, static_cast<SearchAPI::Delegate &>(*this));
  }
  catch (RootException const & e)
  {
    LOG(LCRITICAL, ("Can't load needed resources for SearchAPI:", e.Msg()));
  }
}

void Framework::InitTransliteration()
{
  InitTransliterationInstanceWithDefaultDirs();

  if (!LoadTransliteration())
    Transliteration::Instance().SetMode(Transliteration::Mode::Disabled);
}

string Framework::GetCountryName(m2::PointD const & pt) const
{
  storage::CountryInfo info;
  m_infoGetter->GetRegionInfo(pt, info);
  return info.m_name;
}

/*
Framework::DoAfterUpdate Framework::ToDoAfterUpdate() const
{
  auto const connectionStatus = Platform::ConnectionStatus();
  if (connectionStatus == Platform::EConnectionType::CONNECTION_NONE)
    return DoAfterUpdate::Nothing;

  auto const & s = GetStorage();
  auto const & rootId = s.GetRootId();
  if (!IsEnoughSpaceForUpdate(rootId, s))
    return DoAfterUpdate::Nothing;

  NodeAttrs attrs;
  s.GetNodeAttrs(rootId, attrs);
  MwmSize const countrySizeInBytes = attrs.m_localMwmSize;

  if (countrySizeInBytes == 0 || attrs.m_status != NodeStatus::OnDiskOutOfDate)
    return DoAfterUpdate::Nothing;

  if (s.IsPossibleToAutoupdate() && connectionStatus == Platform::EConnectionType::CONNECTION_WIFI)
    return DoAfterUpdate::AutoupdateMaps;

  return DoAfterUpdate::AskForUpdateMaps;
}
*/

SearchAPI & Framework::GetSearchAPI()
{
  ASSERT(m_searchAPI != nullptr, ("Search API is not initialized."));
  return *m_searchAPI;
}

SearchAPI const & Framework::GetSearchAPI() const
{
  ASSERT(m_searchAPI != nullptr, ("Search API is not initialized."));
  return *m_searchAPI;
}

search::DisplayedCategories const & Framework::GetDisplayedCategories()
{
  ASSERT(m_displayedCategories, ());
  return *m_displayedCategories;
}

void Framework::SelectSearchResult(search::Result const & result, bool animation)
{
  using namespace search;
  place_page::BuildInfo info;
  info.m_source = place_page::BuildInfo::Source::Search;
  info.m_needAnimationOnSelection = false;
  int scale = -1;
  switch (result.GetResultType())
  {
  case Result::Type::Feature:
    info.m_mercator = result.GetFeatureCenter();
    info.m_featureId = result.GetFeatureID();
    info.m_isGeometrySelectionAllowed = true;
    break;

  case Result::Type::LatLon:
    info.m_mercator = result.GetFeatureCenter();
    info.m_match = place_page::BuildInfo::Match::Nothing;
    scale = scales::GetUpperComfortScale();
    break;

  case Result::Type::Postcode:
    info.m_mercator = result.GetFeatureCenter();
    info.m_match = place_page::BuildInfo::Match::Nothing;
    info.m_postcode = result.GetString();
    scale = scales::GetUpperComfortScale();
    break;

  case Result::Type::SuggestFromFeature:
  case Result::Type::PureSuggest:
    m_currentPlacePageInfo = {};
    ASSERT(false, ("Suggests should not be here."));
    return;
  }

  m_currentPlacePageInfo = BuildPlacePageInfo(info);
  if (m_currentPlacePageInfo)
  {
//    if (m_drapeEngines.begin()->second.m_drapeEngine) {
//      if (scale < 0)
//        scale = GetFeatureViewportScale(m_currentPlacePageInfo->GetTypes());
//      m2::PointD const center = m_currentPlacePageInfo->GetMercator();
//      m_drapeEngines.begin()->second.m_drapeEngine->SetModelViewCenter(center, scale, animation, true /* trackVisibleViewport */);
//    }

    ActivateMapSelection();
  }
}

void Framework::ShowSearchResult(search::Result const & res, bool animation)
{
  GetSearchAPI().CancelAllSearches();
  StopLocationFollow();
  SelectSearchResult(res, animation);
}

size_t Framework::ShowSearchResults(search::Results const & results)
{
  using namespace search;

  size_t count = results.GetCount();
  if (count == 0)
    return 0;

  if (count == 1)
  {
    Result const & r = results[0];
    if (!r.IsSuggest())
      ShowSearchResult(r);
    else
      return 0;
  }

  FillSearchResultsMarks(true /* clear */, results);

  // Setup viewport according to results.
  m2::AnyRectD viewport = m_currentModelView.GlobalRect();
  m2::PointD const center = viewport.Center();

  double minDistance = numeric_limits<double>::max();
  int minInd = -1;
  for (size_t i = 0; i < count; ++i)
  {
    Result const & r = results[i];
    if (r.HasPoint())
    {
      double const dist = center.SquaredLength(r.GetFeatureCenter());
      if (dist < minDistance)
      {
        minDistance = dist;
        minInd = static_cast<int>(i);
      }
    }
  }

  if (minInd != -1)
  {
    m2::PointD const pt = results[minInd].GetFeatureCenter();

    if (m_currentModelView.isPerspective())
    {
      StopLocationFollow();
      SetViewportCenter(pt);
      return count;
    }

    if (!viewport.IsPointInside(pt))
    {
      viewport.SetSizesToIncludePoint(pt);
      StopLocationFollow();
    }
  }

  // Graphics engine can be recreated (on Android), so we always set up viewport here.
  ShowRect(viewport);

  return count;
}

void Framework::FillSearchResultsMarks(bool clear, search::Results const & results)
{
  FillSearchResultsMarks(results.begin(), results.end(), clear);
}

void Framework::FillSearchResultsMarks(SearchResultsIterT beg, SearchResultsIterT end, bool clear)
{
  auto editSession = GetBookmarkManager().GetEditSession();
  if (clear)
    editSession.ClearGroup(UserMark::Type::SEARCH);
  editSession.SetIsVisible(UserMark::Type::SEARCH, true);

  for (auto it = beg; it != end; ++it)
  {
    auto const & r = *it;
    if (!r.HasPoint())
      continue;

    auto * mark = editSession.CreateUserMark<SearchMarkPoint>(r.GetFeatureCenter());
    mark->SetMatchedName(r.GetString());

    if (r.GetResultType() == search::Result::Type::Feature)
    {
      auto const fID = r.GetFeatureID();
      mark->SetFoundFeature(fID);

      if (r.m_details.m_isHotel)
        mark->SetHotelType();
      else
        mark->SetFromType(r.GetFeatureType());
      mark->SetVisited(m_searchMarks.IsVisited(fID));
      mark->SetSelected(m_searchMarks.IsSelected(fID));
    }
  }
}

bool Framework::GetDistanceAndAzimut(m2::PointD const & point,
                                     double lat, double lon, double north,
                                     string & distance, double & azimut)
{
#ifdef FIXED_LOCATION
  m_fixedPos.GetLat(lat);
  m_fixedPos.GetLon(lon);
  m_fixedPos.GetNorth(north);
#endif

  double const d = ms::DistanceOnEarth(lat, lon, mercator::YToLat(point.y), mercator::XToLon(point.x));

  // Distance may be less than 1.0
  distance = measurement_utils::FormatDistance(d);

  // We calculate azimuth even when distance is very short (d ~ 0),
  // because return value has 2 states (near me or far from me).

  azimut = ang::Azimuth(mercator::FromLatLon(lat, lon), point, north);

  double const pi2 = 2.0*math::pi;
  if (azimut < 0.0)
    azimut += pi2;
  else if (azimut > pi2)
    azimut -= pi2;

  // This constant and return value is using for arrow/flag choice.
  return (d < 25000.0);
}

df::DrapeEngineId Framework::CreateDrapeEngineId()
{
  df::DrapeEngineId engineId = static_cast<df::DrapeEngineId>(random());
  m_drapeEngines[engineId];
  return engineId;
}

void Framework::CreateDrapeEngine(df::DrapeEngineId engineId, ref_ptr<dp::GraphicsContextFactory> contextFactory, DrapeCreationParams && params)
{
  auto idReadFn = [this](df::MapDataProvider::TReadCallback<FeatureID const> const & fn,
                         m2::RectD const & r,
                         int scale) -> void { m_featuresFetcher.ForEachFeatureID(r, fn, scale); };

  auto featureReadFn = [this](df::MapDataProvider::TReadCallback<FeatureType> const & fn,
                              vector<FeatureID> const & ids) -> void
  {
    m_featuresFetcher.ReadFeatures(fn, ids);
  };

  auto myPositionModeChangedFn = [this](location::EMyPositionMode mode, bool routingActive)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, mode, routingActive]()
    {
      // Deactivate selection (and hide place page) if we return to routing in F&R mode.
      if (routingActive && mode == location::FollowAndRotate)
        DeactivateMapSelection(true /* notifyUI */);

      if (m_myPositionListener != nullptr)
        m_myPositionListener(mode, routingActive);
    });
  };

  auto overlaysShowStatsFn = [](list<df::OverlayShowEvent> &&)
  {
  };

  auto onGraphicsContextInitialized = [this, engineId]()
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, engineId]()
    {
      if (m_drapeEngines[engineId].m_onGraphicsContextInitialized)
          m_drapeEngines[engineId].m_onGraphicsContextInitialized();
    });
  };

  auto isCountryLoadedByNameFn = bind(&Framework::IsCountryLoadedByName, this, _1);
  auto updateCurrentCountryFn = bind(&Framework::OnUpdateCurrentCountry, this, _1, _2);

  bool allow3d;
  bool allow3dBuildings;
  Load3dMode(allow3d, allow3dBuildings);

  auto const isAutozoomEnabled = LoadAutoZoom();

  auto const trafficEnabled = m_trafficManager.IsEnabled();
  auto const isolinesEnabled = m_isolinesManager.IsEnabled();

  auto const simplifiedTrafficColors = m_trafficManager.HasSimplifiedColorScheme();
  auto const fontsScaleFactor = LoadLargeFontsSize() ? kLargeFontsScaleFactor : 1.0;

  df::DrapeEngine::Params p(
      params.m_apiVersion, contextFactory,
      dp::Viewport(0, 0, params.m_surfaceWidth, params.m_surfaceHeight),
      df::MapDataProvider(std::move(idReadFn), std::move(featureReadFn),
                          std::move(isCountryLoadedByNameFn), std::move(updateCurrentCountryFn)),
      params.m_hints, params.m_visualScale, fontsScaleFactor, std::move(params.m_widgetsInitInfo),
      std::move(myPositionModeChangedFn), allow3dBuildings,
      trafficEnabled, isolinesEnabled,
      params.m_isChoosePositionMode, params.m_isChoosePositionMode, GetSelectedFeatureTriangles(),
      m_routingManager.IsRoutingActive() && m_routingManager.IsRoutingFollowing(),
      isAutozoomEnabled, simplifiedTrafficColors, std::move(overlaysShowStatsFn),
      std::move(onGraphicsContextInitialized));

  DrapeEngineData &drapeEngineData = m_drapeEngines[engineId];
  drapeEngineData.m_drapeEngine = make_unique_dp<df::DrapeEngine>(std::move(p));
  drapeEngineData.m_drapeEngine->SetModelViewListener([this](ScreenBase const & screen)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, screen](){ OnViewportChanged(screen); });
  });
  drapeEngineData.m_drapeEngine->SetTapEventInfoListener([this](df::TapInfo const & tapInfo)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, tapInfo]()
    {
      OnTapEvent(place_page::BuildInfo(tapInfo));
    });
  });
  drapeEngineData.m_drapeEngine->SetUserPositionListener([this](m2::PointD const & position, bool hasPosition)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, position, hasPosition]()
    {
      OnUserPositionChanged(position, hasPosition);
    });
  });
  drapeEngineData.m_drapeEngine->SetUserPositionPendingTimeoutListener([this]()
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this]()
    {
      if (m_myPositionPendingTimeoutListener)
        m_myPositionPendingTimeoutListener();
    });
  });

  OnSize(engineId, params.m_surfaceWidth, params.m_surfaceHeight);

  Allow3dMode(allow3d, allow3dBuildings);

  LoadViewport();

  if (m_connectToGpsTrack)
    GpsTracker::Instance().Connect(bind(&Framework::OnUpdateGpsTrackPointsCallback, this, _1, _2));

//  GetBookmarkManager().SetDrapeEngine(make_ref(drapeEngineData.m_drapeEngine));
  drapeEngineData.m_drapeApi.SetDrapeEngine(make_ref(drapeEngineData.m_drapeEngine));
//  m_routingManager.SetDrapeEngine(make_ref(drapeEngineData.m_drapeEngine), allow3d);
//  m_trafficManager.SetDrapeEngine(make_ref(drapeEngineData.m_drapeEngine));
//  m_transitManager.SetDrapeEngine(make_ref(drapeEngineData.m_drapeEngine));
//  m_isolinesManager.SetDrapeEngine(make_ref(drapeEngineData.m_drapeEngine));
//  m_searchMarks.SetDrapeEngine(make_ref(drapeEngineData.m_drapeEngine));

//  InvalidateUserMarks();

  auto const transitSchemeEnabled = LoadTransitSchemeEnabled();
  m_transitManager.EnableTransitSchemeMode(transitSchemeEnabled);

  // Show debug info if it's enabled in the config.
  bool showDebugInfo;
  if (!settings::Get(kShowDebugInfo, showDebugInfo))
    showDebugInfo = false;
  if (showDebugInfo)
    drapeEngineData.m_drapeEngine->ShowDebugInfo(showDebugInfo);

  benchmark::RunGraphicsBenchmark(this);
}

void Framework::OnRecoverSurface(df::DrapeEngineId engineId, int width, int height, bool recreateContextDependentResources)
{
  DrapeEngineData &drapeEngineData = m_drapeEngines[engineId];
  if (drapeEngineData.m_drapeEngine)
  {
    drapeEngineData.m_drapeEngine->RecoverSurface(width, height, recreateContextDependentResources);

    InvalidateUserMarks();

    drapeEngineData.m_drapeApi.Invalidate();
  }

  m_trafficManager.OnRecoverSurface();
  m_transitManager.Invalidate();
  m_isolinesManager.Invalidate();
}

void Framework::OnDestroySurface()
{
  m_trafficManager.OnDestroySurface();
}

void Framework::UpdateVisualScale(double vs)
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr)
//    m_drapeEngines.begin()->second.m_drapeEngine->UpdateVisualScale(vs, m_isRenderingEnabled);
}

void Framework::UpdateMyPositionRoutingOffset(bool useDefault, int offsetY)
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr)
//    m_drapeEngines.begin()->second.m_drapeEngine->UpdateMyPositionRoutingOffset(useDefault, offsetY);
}

ref_ptr<df::DrapeEngine> Framework::GetDrapeEngine(df::DrapeEngineId engineId)
{
  return make_ref(m_drapeEngines[engineId].m_drapeEngine);
}

void Framework::DestroyDrapeEngine()
{
  for (auto& [engineId, drapeEngineData] : m_drapeEngines)
  {
    if (drapeEngineData.m_drapeEngine != nullptr)
    {
      drapeEngineData.m_drapeApi.SetDrapeEngine(nullptr);
      m_routingManager.SetDrapeEngine(nullptr, false);
      m_trafficManager.SetDrapeEngine(nullptr);
      m_transitManager.SetDrapeEngine(nullptr);
      m_isolinesManager.SetDrapeEngine(nullptr);
      m_searchMarks.SetDrapeEngine(nullptr);
      GetBookmarkManager().SetDrapeEngine(nullptr);
      drapeEngineData.m_drapeEngine.reset();
    }
    m_trafficManager.Teardown();
    GpsTracker::Instance().Disconnect();
  }
}

void Framework::SetRenderingEnabled(ref_ptr<dp::GraphicsContextFactory> contextFactory)
{
  m_isRenderingEnabled = true;
//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//    m_drapeEngines.begin()->second.m_drapeEngine->SetRenderingEnabled(contextFactory);
}

void Framework::SetRenderingDisabled(bool destroySurface)
{
  m_isRenderingEnabled = false;
//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//    m_drapeEngines.begin()->second.m_drapeEngine->SetRenderingDisabled(destroySurface);
}

void Framework::SetGraphicsContextInitializationHandler(df::DrapeEngineId engineId, df::OnGraphicsContextInitialized && handler)
{
  m_drapeEngines[engineId].m_onGraphicsContextInitialized = std::move(handler);
}

void Framework::EnableDebugRectRendering(bool enabled)
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//    m_drapeEngines.begin()->second.m_drapeEngine->EnableDebugRectRendering(enabled);
}

void Framework::ConnectToGpsTracker()
{
  m_connectToGpsTrack = true;
//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//  {
//    m_drapeEngines.begin()->second.m_drapeEngine->ClearGpsTrackPoints();
//    GpsTracker::Instance().Connect(bind(&Framework::OnUpdateGpsTrackPointsCallback, this, _1, _2));
//  }
}

void Framework::DisconnectFromGpsTracker()
{
  m_connectToGpsTrack = false;
  GpsTracker::Instance().Disconnect();
//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//    m_drapeEngines.begin()->second.m_drapeEngine->ClearGpsTrackPoints();
}

void Framework::OnUpdateGpsTrackPointsCallback(vector<pair<size_t, location::GpsTrackInfo>> && toAdd,
                                               pair<size_t, size_t> const & toRemove)
{
//  ASSERT(m_drapeEngines.begin()->second.m_drapeEngine.get() != nullptr, ());

  vector<df::GpsTrackPoint> pointsAdd;
  pointsAdd.reserve(toAdd.size());
  for (auto const & ip : toAdd)
  {
    df::GpsTrackPoint pt;
    ASSERT_LESS(ip.first, static_cast<size_t>(numeric_limits<uint32_t>::max()), ());
    pt.m_id = static_cast<uint32_t>(ip.first);
    pt.m_speedMPS = ip.second.m_speed;
    pt.m_timestamp = ip.second.m_timestamp;
    pt.m_point = mercator::FromLatLon(ip.second.m_latitude, ip.second.m_longitude);
    pointsAdd.emplace_back(pt);
  }

  vector<uint32_t> indicesRemove;
  if (toRemove.first != GpsTrack::kInvalidId)
  {
    ASSERT_LESS_OR_EQUAL(toRemove.first, toRemove.second, ());
    indicesRemove.reserve(toRemove.second - toRemove.first + 1);
    for (size_t i = toRemove.first; i <= toRemove.second; ++i)
      indicesRemove.emplace_back(i);
  }

//  m_drapeEngines.begin()->second.m_drapeEngine->UpdateGpsTrackPoints(std::move(pointsAdd), std::move(indicesRemove));
}

void Framework::MarkMapStyle(MapStyle mapStyle)
{
  ASSERT_NOT_EQUAL(mapStyle, MapStyle::MapStyleMerged, ());

  // Store current map style before classificator reloading
  string mapStyleStr = MapStyleToString(mapStyle);
  if (mapStyleStr.empty())
  {
    mapStyle = kDefaultMapStyle;
    mapStyleStr = MapStyleToString(mapStyle);
  }
  settings::Set(kMapStyleKey, mapStyleStr);
  GetStyleReader().SetCurrentStyle(mapStyle);
}

void Framework::SetMapStyle(MapStyle mapStyle)
{
//  MarkMapStyle(mapStyle);
//  if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr)
//    m_drapeEngines.begin()->second.m_drapeEngine->UpdateMapStyle();
//  InvalidateUserMarks();
//  UpdateMinBuildingsTapZoom();
}

MapStyle Framework::GetMapStyle() const
{
  return GetStyleReader().GetCurrentStyle();
}

void Framework::SetupMeasurementSystem()
{
  GetPlatform().SetupMeasurementSystem();

  m_routingManager.SetTurnNotificationsUnits(measurement_utils::GetMeasurementUnits());
}

void Framework::SetWidgetLayout(df::DrapeEngineId engineId, gui::TWidgetsLayoutInfo && layout)
{
  ASSERT(m_drapeEngines[engineId].m_drapeEngine != nullptr, ());
  m_drapeEngines[engineId].m_drapeEngine->SetWidgetLayout(std::move(layout));
}

bool Framework::ShowMapForURL(string const & url)
{
  m2::PointD point;
  double scale = 0;
  string name;
  ApiMarkPoint const * apiMark = nullptr;

  enum ResultT { FAILED, NEED_CLICK, NO_NEED_CLICK };
  ResultT result = FAILED;

  // It's an API request, parsed in parseAndSetApiURL and nativeParseAndSetApiUrl.
  if (m_parsedMapApi.IsValid())
  {
    if (!m_parsedMapApi.GetViewportParams(point, scale))
    {
      point = {0, 0};
      scale = 0;
    }

    apiMark = m_parsedMapApi.GetSinglePoint();
    result = apiMark ? NEED_CLICK : NO_NEED_CLICK;
  }
  else if (strings::StartsWith(url, "om") || strings::StartsWith(url, "ge0"))
  {
    // Note that om scheme is used to encode both API and ge0 links.
    ge0::Ge0Parser parser;
    ge0::Ge0Parser::Result parseResult;

    if (parser.Parse(url, parseResult))
    {
      point = mercator::FromLatLon(parseResult.m_lat, parseResult.m_lon);
      scale = parseResult.m_zoomLevel;
      name = std::move(parseResult.m_name);
      result = NEED_CLICK;
    }
  }
  else  // Actually, we can parse any geo url scheme with correct coordinates.
  {
    geo::GeoURLInfo const info = geo::UnifiedParser().Parse(url);
    if (info.IsValid())
    {
      point = mercator::FromLatLon(info.m_lat, info.m_lon);
      scale = info.m_zoom;
      result = NEED_CLICK;
    }
  }

  if (result != FAILED)
  {
    // Always hide current map selection.
    DeactivateMapSelection(true /* notifyUI */);

    // Set viewport and stop follow mode.
    StopLocationFollow();

    // ShowRect function interferes with ActivateMapSelection and we have strange behaviour as a result.
    // Use more obvious SetModelViewCenter here.
//    if (m_drapeEngines.begin()->second.m_drapeEngine)
//      m_drapeEngines.begin()->second.m_drapeEngine->SetModelViewCenter(point, scale, true, true);

    if (result != NO_NEED_CLICK)
    {
      place_page::BuildInfo info;
      info.m_needAnimationOnSelection = false;
      if (apiMark != nullptr)
      {
        info.m_mercator = apiMark->GetPivot();
        info.m_userMarkId = apiMark->GetId();
      }
      else
      {
        info.m_mercator = point;
      }

      m_currentPlacePageInfo = BuildPlacePageInfo(info);
      ActivateMapSelection();
    }

    return true;
  }

  return false;
}

url_scheme::ParsedMapApi::ParsingResult Framework::ParseAndSetApiURL(string const & url)
{
  using namespace url_scheme;

  // Clear every current API-mark.
  {
    auto editSession = GetBookmarkManager().GetEditSession();
    editSession.ClearGroup(UserMark::Type::API);
    editSession.SetIsVisible(UserMark::Type::API, true);
  }

  return m_parsedMapApi.SetUrlAndParse(url);
}

Framework::ParsedRoutingData Framework::GetParsedRoutingData() const
{
  return Framework::ParsedRoutingData(m_parsedMapApi.GetRoutePoints(),
                                      routing::FromString(m_parsedMapApi.GetRoutingType()));
}

url_scheme::SearchRequest Framework::GetParsedSearchRequest() const
{
  return m_parsedMapApi.GetSearchRequest();
}

std::string const & Framework::GetParsedAppName() const
{
  return m_parsedMapApi.GetAppName();
}

ms::LatLon Framework::GetParsedCenterLatLon() const
{
  return m_parsedMapApi.GetCenterLatLon();
}

FeatureID Framework::GetFeatureAtPoint(m2::PointD const & mercator,
                                       FeatureMatcher && matcher /* = nullptr */) const
{
  FeatureID fullMatch, poi, line, area;
  auto haveBuilding = false;
  auto closestDistanceToCenter = numeric_limits<double>::max();
  auto currentDistance = numeric_limits<double>::max();

  indexer::ForEachFeatureAtPoint(m_featuresFetcher.GetDataSource(), [&](FeatureType & ft)
  {
    if (fullMatch.IsValid())
      return;

    if (matcher && matcher(ft))
    {
      fullMatch = ft.GetID();
      return;
    }

    switch (ft.GetGeomType())
    {
    case feature::GeomType::Point:
      poi = ft.GetID();
      break;
    case feature::GeomType::Line:
      // Skip/ignore isolines.
      if (ftypes::IsIsolineChecker::Instance()(ft))
        return;
      line = ft.GetID();
      break;
    case feature::GeomType::Area:
    {
      // Buildings have higher priority over other types.
      if (haveBuilding)
        return;

      // Skip/ignore coastlines.
      feature::TypesHolder types(ft);
      if (ftypes::IsCoastlineChecker::Instance()(types))
        return;

      haveBuilding = ftypes::IsBuildingChecker::Instance()(types);
      currentDistance = mercator::DistanceOnEarth(mercator, feature::GetCenter(ft));
      // Choose the first matching building or, if no buildings are matched,
      // the first among the closest matching non-buildings.
      if (!haveBuilding && currentDistance >= closestDistanceToCenter)
        return;

      area = ft.GetID();
      closestDistanceToCenter = currentDistance;
      break;
    }
    case feature::GeomType::Undefined:
      ASSERT(false, ("case feature::Undefined"));
      break;
    }
  }, mercator);

  return fullMatch.IsValid() ? fullMatch : (poi.IsValid() ? poi : (line.IsValid() ? line : area));
}

osm::MapObject Framework::GetMapObjectByID(FeatureID const & fid) const
{
  osm::MapObject res;
  ASSERT(fid.IsValid(), ());
  FeaturesLoaderGuard guard(m_featuresFetcher.GetDataSource(), fid.m_mwmId);
  auto ft = guard.GetFeatureByIndex(fid.m_index);
  if (ft)
    res.SetFromFeatureType(*ft);
  return res;
}

BookmarkManager & Framework::GetBookmarkManager()
{
  ASSERT(m_bmManager != nullptr, ("Bookmark manager is not initialized."));
  return *m_bmManager.get();
}

BookmarkManager const & Framework::GetBookmarkManager() const
{
  ASSERT(m_bmManager != nullptr, ("Bookmark manager is not initialized."));
  return *m_bmManager.get();
}

void Framework::SetPlacePageListeners(PlacePageEvent::OnOpen onOpen,
                                      PlacePageEvent::OnClose onClose,
                                      PlacePageEvent::OnUpdate onUpdate)
{
  m_onPlacePageOpen = std::move(onOpen);
  m_onPlacePageClose = std::move(onClose);
  m_onPlacePageUpdate = std::move(onUpdate);

#ifdef OMIM_OS_ANDROID
  // A click on the Search result from the search activity in Android calls
  // ShowSearchResult/SelectSearchResult, but SetPlacePageListeners is set later,
  // when MWMActivity::onStart is called. So PP is displayed here if its info was set previously.
  // TODO: A better approach is to use an intent with params to pass the search result into MWMActivity,
  // like it is done when selected bookmark is displayed on the map.
  if (m_onPlacePageOpen && HasPlacePageInfo())
    m_onPlacePageOpen();
#endif  // OMIM_OS_ANDROID
}

place_page::Info const & Framework::GetCurrentPlacePageInfo() const
{
  CHECK(HasPlacePageInfo(), ());
  return *m_currentPlacePageInfo;
}

place_page::Info & Framework::GetCurrentPlacePageInfo()
{
  CHECK(HasPlacePageInfo(), ());
  return *m_currentPlacePageInfo;
}

void Framework::ActivateMapSelection()
{
  if (!m_currentPlacePageInfo)
    return;

  auto & bm = GetBookmarkManager();

  if (m_currentPlacePageInfo->GetSelectedObject() == df::SelectionShape::OBJECT_TRACK)
    bm.OnTrackSelected(m_currentPlacePageInfo->GetTrackId());
  else
    bm.OnTrackDeselected();

  auto const & featureId = m_currentPlacePageInfo->GetID();

  m_searchMarks.SetSelected(featureId);

  auto const selObj = m_currentPlacePageInfo->GetSelectedObject();
  CHECK_NOT_EQUAL(selObj, df::SelectionShape::OBJECT_EMPTY, ("Empty selections are impossible."));
//  if (m_drapeEngines.begin()->second.m_drapeEngine)
//  {
//    auto const & bi = m_currentPlacePageInfo->GetBuildInfo();
//    m_drapeEngines.begin()->second.m_drapeEngine->SelectObject(selObj, m_currentPlacePageInfo->GetMercator(), featureId,
//                                bi.m_needAnimationOnSelection, bi.m_isGeometrySelectionAllowed, true);
//  }

  /// @todo Current android logic is strange (see SetPlacePageListeners comments), so skip assert.
  //ASSERT(m_onPlacePageOpen, ());
  if (m_onPlacePageOpen)
    m_onPlacePageOpen();
}

void Framework::DeactivateMapSelection(bool notifyUI)
{
  bool const somethingWasAlreadySelected = m_currentPlacePageInfo.has_value();

  if (notifyUI && m_onPlacePageClose)
    m_onPlacePageClose(!somethingWasAlreadySelected);

  if (somethingWasAlreadySelected)
  {
    DeactivateHotelSearchMark();
    GetBookmarkManager().OnTrackDeselected();

    m_currentPlacePageInfo = {};

//    if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr)
//      m_drapeEngines.begin()->second.m_drapeEngine->DeselectObject();
  }
}

void Framework::InvalidateUserMarks()
{
  // Actual invalidation call happens in EditSession dtor.
  GetBookmarkManager().GetEditSession();
}

void Framework::DeactivateHotelSearchMark()
{
  if (!m_currentPlacePageInfo)
    return;

  m_searchMarks.SetSelected({});
  if (m_currentPlacePageInfo->GetHotelType().has_value())
  {
    auto const & featureId = m_currentPlacePageInfo->GetID();
    if (m_searchMarks.IsThereSearchMarkForFeature(featureId))
    {
      m_searchMarks.SetVisited(featureId);
      m_searchMarks.OnDeactivate(featureId);
    }

    if (!GetSearchAPI().IsViewportSearchActive())
    {
      GetBookmarkManager().GetEditSession().ClearGroup(UserMark::Type::SEARCH);
    }
  }
}

void Framework::OnTapEvent(place_page::BuildInfo const & buildInfo)
{
  auto placePageInfo = BuildPlacePageInfo(buildInfo);

  if (placePageInfo)
  {
    auto const prevTrackId = m_currentPlacePageInfo ? m_currentPlacePageInfo->GetTrackId()
                                                    : kml::kInvalidTrackId;
    DeactivateHotelSearchMark();

    m_currentPlacePageInfo = placePageInfo;

    if (m_currentPlacePageInfo->GetTrackId() != kml::kInvalidTrackId)
    {
      if (m_currentPlacePageInfo->GetTrackId() == prevTrackId)
      {
//        if (m_drapeEngines.begin()->second.m_drapeEngine)
//        {
//          m_drapeEngines.begin()->second.m_drapeEngine->SelectObject(df::SelectionShape::ESelectedObject::OBJECT_TRACK,
//                                      m_currentPlacePageInfo->GetMercator(), FeatureID(),
//                                      false /* isAnim */, false /* isGeometrySelectionAllowed */,
//                                      true /* isSelectionShapeVisible */);
//        }
        return;
      }
      GetBookmarkManager().UpdateElevationMyPosition(m_currentPlacePageInfo->GetTrackId());
    }

    ActivateMapSelection();
  }
  else
  {
    // UI is always notified even if empty map is tapped,
    // because empty tap event switches on/off full screen map view mode.
    DeactivateMapSelection(true /* notifyUI */);
  }
}

void Framework::InvalidateRendering()
{
  if (m_drapeEngines.begin()->second.m_drapeEngine)
    m_drapeEngines.begin()->second.m_drapeEngine->Invalidate();
}

void Framework::UpdateMinBuildingsTapZoom()
{
  constexpr int kMinTapZoom = 16;
  m_minBuildingsTapZoom = max(kMinTapZoom,
                              feature::GetDrawableScaleRange(classif().GetTypeByPath({"building"})).first);
}

FeatureID Framework::FindBuildingAtPoint(m2::PointD const & mercator) const
{
  FeatureID featureId;
  if (GetDrawScale() >= m_minBuildingsTapZoom)
  {
    constexpr int kScale = scales::GetUpperScale();
    constexpr double kSelectRectWidthInMeters = 1.1;
    m2::RectD const rect =
        mercator::RectByCenterXYAndSizeInMeters(mercator, kSelectRectWidthInMeters);
    m_featuresFetcher.ForEachFeature(rect, [&](FeatureType & ft)
    {
      if (!featureId.IsValid() &&
        ft.GetGeomType() == feature::GeomType::Area &&
          ftypes::IsBuildingChecker::Instance()(ft) &&
          ft.GetLimitRect(kScale).IsPointInside(mercator) &&
          feature::GetMinDistanceMeters(ft, mercator) == 0.0)
      {
        featureId = ft.GetID();
      }
    }, kScale);
  }
  return featureId;
}

void Framework::BuildTrackPlacePage(Track::TrackSelectionInfo const & trackSelectionInfo, place_page::Info & info)
{
  info.SetSelectedObject(df::SelectionShape::OBJECT_TRACK);
  auto const & track = *GetBookmarkManager().GetTrack(trackSelectionInfo.m_trackId);
  FillTrackInfo(track, trackSelectionInfo.m_trackPoint, info);
  GetBookmarkManager().SetTrackSelectionInfo(trackSelectionInfo, true /* notifyListeners */);
}

std::optional<place_page::Info> Framework::BuildPlacePageInfo(
    place_page::BuildInfo const & buildInfo)
{
  place_page::Info outInfo;
  outInfo.SetBuildInfo(buildInfo);

  if (buildInfo.IsUserMarkMatchingEnabled())
  {
    UserMark const * mark = FindUserMarkInTapPosition(buildInfo);
    if (mark != nullptr)
    {
      outInfo.SetSelectedObject(df::SelectionShape::OBJECT_USER_MARK);
      switch (mark->GetMarkType())
      {
      case UserMark::Type::API:
        FillApiMarkInfo(*static_cast<ApiMarkPoint const *>(mark), outInfo);
        break;
      case UserMark::Type::BOOKMARK:
        FillBookmarkInfo(*static_cast<Bookmark const *>(mark), outInfo);
        break;
      case UserMark::Type::SEARCH:
        FillSearchResultInfo(*static_cast<SearchMarkPoint const *>(mark), outInfo);
        break;
      case UserMark::Type::ROUTING:
        FillRouteMarkInfo(*static_cast<RouteMarkPoint const *>(mark), outInfo);
        break;
      case UserMark::Type::ROAD_WARNING:
        FillRoadTypeMarkInfo(*static_cast<RoadWarningMark const *>(mark), outInfo);
        break;
      case UserMark::Type::TRACK_INFO:
      {
        auto const & infoMark = *static_cast<TrackInfoMark const *>(mark);
        BuildTrackPlacePage(GetBookmarkManager().GetTrackSelectionInfo(infoMark.GetTrackId()),
                            outInfo);
        return outInfo;
      }
      case UserMark::Type::TRACK_SELECTION:
      {
        auto const & selMark = *static_cast<TrackSelectionMark const *>(mark);
        BuildTrackPlacePage(GetBookmarkManager().GetTrackSelectionInfo(selMark.GetTrackId()),
                            outInfo);
        return outInfo;
      }
      case UserMark::Type::TRANSIT:
      {
        FillTransitMarkInfo(*static_cast<TransitMark const *>(mark), outInfo);
        break;
      }
      case UserMark::Type::SPEED_CAM:
      {
        FillSpeedCameraMarkInfo(*static_cast<SpeedCameraMark const *>(mark), outInfo);
        break;
      }
      default:
        ASSERT(false, ("FindNearestUserMark returned invalid mark."));
      }

      SetPlacePageLocation(outInfo);
      return outInfo;
    }
  }

  if (buildInfo.m_isMyPosition)
  {
    outInfo.SetSelectedObject(df::SelectionShape::OBJECT_MY_POSITION);
    FillMyPositionInfo(outInfo, buildInfo);
    SetPlacePageLocation(outInfo);
    return outInfo;
  }

  if (!buildInfo.m_postcode.empty())
  {
    outInfo.SetSelectedObject(df::SelectionShape::OBJECT_POI);
    FillPostcodeInfo(buildInfo.m_postcode, buildInfo.m_mercator, outInfo);
    GetBookmarkManager().SelectionMark().SetPtOrg(outInfo.GetMercator());
    SetPlacePageLocation(outInfo);

    return outInfo;
  }

  FeatureID selectedFeature = buildInfo.m_featureId;
  auto const isFeatureMatchingEnabled = buildInfo.IsFeatureMatchingEnabled();

  // Using VisualParams inside FindTrackInTapPosition/GetDefaultTapRect requires drapeEngine.
//  if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr && buildInfo.IsTrackMatchingEnabled() && !buildInfo.m_isLongTap &&
//      !(isFeatureMatchingEnabled && selectedFeature.IsValid()))
//  {
//    auto const trackSelectionInfo = FindTrackInTapPosition(buildInfo);
//    if (trackSelectionInfo.m_trackId != kml::kInvalidTrackId)
//    {
//      BuildTrackPlacePage(trackSelectionInfo, outInfo);
//      return outInfo;
//    }
//  }

  if (isFeatureMatchingEnabled && !selectedFeature.IsValid())
    selectedFeature = FindBuildingAtPoint(buildInfo.m_mercator);

  bool showMapSelection = false;
  if (selectedFeature.IsValid())
  {
    FillFeatureInfo(selectedFeature, outInfo);
    if (buildInfo.m_isLongTap)
      outInfo.SetMercator(buildInfo.m_mercator);
    showMapSelection = true;
  }
  else if (buildInfo.m_isLongTap || buildInfo.m_source != place_page::BuildInfo::Source::User)
  {
    if (isFeatureMatchingEnabled)
      FillPointInfo(outInfo, buildInfo.m_mercator, {});
    else
      FillNotMatchedPlaceInfo(outInfo, buildInfo.m_mercator, {});
    showMapSelection = true;
  }

  if (showMapSelection)
  {
    outInfo.SetSelectedObject(df::SelectionShape::OBJECT_POI);
    GetBookmarkManager().SelectionMark().SetPtOrg(outInfo.GetMercator());
    SetPlacePageLocation(outInfo);

    return outInfo;
  }

  return {};
}

void Framework::UpdatePlacePageInfoForCurrentSelection(
    std::optional<place_page::BuildInfo> const & overrideInfo)
{
  if (!m_currentPlacePageInfo)
    return;

  m_currentPlacePageInfo = BuildPlacePageInfo(overrideInfo.has_value() ? *overrideInfo :
    m_currentPlacePageInfo->GetBuildInfo());
  if (m_currentPlacePageInfo && m_onPlacePageUpdate)
    m_onPlacePageUpdate();
}

Track::TrackSelectionInfo Framework::FindTrackInTapPosition(place_page::BuildInfo const & buildInfo) const
{
  auto const & bm = GetBookmarkManager();
  if (buildInfo.m_trackId != kml::kInvalidTrackId)
  {
    if (bm.GetTrack(buildInfo.m_trackId) == nullptr)
      return {};
    auto const selection = bm.GetTrackSelectionInfo(buildInfo.m_trackId);
    CHECK_NOT_EQUAL(selection.m_trackId, kml::kInvalidTrackId, ());
    return selection;
  }
  auto const touchRect = df::TapInfo::GetDefaultTapRect(buildInfo.m_mercator,
                                                           m_currentModelView).GetGlobalRect();
  return bm.FindNearestTrack(touchRect);
}

UserMark const * Framework::FindUserMarkInTapPosition(place_page::BuildInfo const & buildInfo) const
{
  auto const & bm = GetBookmarkManager();
  if (buildInfo.m_userMarkId != kml::kInvalidMarkId)
  {
    auto mark = bm.IsBookmark(buildInfo.m_userMarkId) ? bm.GetBookmark(buildInfo.m_userMarkId)
                                                      : bm.GetUserMark(buildInfo.m_userMarkId);
    if (mark != nullptr)
      return mark;
  }

  UserMark const * mark = bm.FindNearestUserMark(
    [this, &buildInfo](UserMark::Type type)
    {
      double constexpr kEps = 1e-7;
      if (buildInfo.m_source != place_page::BuildInfo::Source::User)
        return df::TapInfo::GetPreciseTapRect(buildInfo.m_mercator, kEps);

      if (type == UserMark::Type::BOOKMARK || type == UserMark::Type::TRACK_INFO)
        return df::TapInfo::GetBookmarkTapRect(buildInfo.m_mercator, m_currentModelView);

      if (type == UserMark::Type::ROUTING || type == UserMark::Type::ROAD_WARNING)
        return df::TapInfo::GetRoutingPointTapRect(buildInfo.m_mercator, m_currentModelView);

      return df::TapInfo::GetDefaultTapRect(buildInfo.m_mercator, m_currentModelView);
    },
    [](UserMark::Type type)
    {
      return type == UserMark::Type::TRACK_INFO || type == UserMark::Type::TRACK_SELECTION;
    });
  return mark;
}

void Framework::PredictLocation(double & lat, double & lon, double accuracy,
                                double bearing, double speed, double elapsedSeconds)
{
  double offsetInM = speed * elapsedSeconds;
  double angle = base::DegToRad(90.0 - bearing);

  m2::PointD mercatorPt = mercator::MetersToXY(lon, lat, accuracy).Center();
  mercatorPt = mercator::GetSmPoint(mercatorPt, offsetInM * cos(angle), offsetInM * sin(angle));
  lon = mercator::XToLon(mercatorPt.x);
  lat = mercator::YToLat(mercatorPt.y);
}

StringsBundle const & Framework::GetStringsBundle()
{
  return m_stringsBundle;
}

// static
string Framework::CodeGe0url(Bookmark const * bmk, bool addName)
{
  double lat = mercator::YToLat(bmk->GetPivot().y);
  double lon = mercator::XToLon(bmk->GetPivot().x);
  return ge0::GenerateShortShowMapUrl(lat, lon, bmk->GetScale(), addName ? bmk->GetPreferredName() : "");
}

// static
string Framework::CodeGe0url(double lat, double lon, double zoomLevel, string const & name)
{
  return ge0::GenerateShortShowMapUrl(lat, lon, zoomLevel, name);
}

string Framework::GenerateApiBackUrl(ApiMarkPoint const & point) const
{
  string res = m_parsedMapApi.GetGlobalBackUrl();
  if (!res.empty())
  {
    ms::LatLon const ll = point.GetLatLon();
    res += "pin?ll=" + strings::to_string(ll.m_lat) + "," + strings::to_string(ll.m_lon);
    if (!point.GetName().empty())
      res += "&n=" + url::UrlEncode(point.GetName());
    if (!point.GetApiID().empty())
      res += "&id=" + url::UrlEncode(point.GetApiID());
  }
  return res;
}

bool Framework::IsDataVersionUpdated()
{
  int64_t storedVersion;
  if (settings::Get("DataVersion", storedVersion))
  {
    return storedVersion < m_storage.GetCurrentDataVersion();
  }
  // no key in the settings, assume new version
  return true;
}

void Framework::UpdateSavedDataVersion()
{
  settings::Set("DataVersion", m_storage.GetCurrentDataVersion());
}

int64_t Framework::GetCurrentDataVersion() const { return m_storage.GetCurrentDataVersion(); }

dp::ApiVersion Framework::LoadPreferredGraphicsAPI()
{
  std::string apiStr;
  if (settings::Get(kPreferredGraphicsAPI, apiStr))
    return dp::ApiVersionFromString(apiStr);
  return dp::ApiVersionFromString({});
}

void Framework::SavePreferredGraphicsAPI(dp::ApiVersion apiVersion)
{
  settings::Set(kPreferredGraphicsAPI, DebugPrint(apiVersion));
}

void Framework::AllowTransliteration(bool allowTranslit)
{
  Transliteration::Instance().SetMode(allowTranslit ? Transliteration::Mode::Enabled
                                                    : Transliteration::Mode::Disabled);
  InvalidateRect(GetCurrentViewport());
}

bool Framework::LoadTransliteration()
{
  Transliteration::Mode mode;
  if (settings::Get(kTranslitMode, mode))
    return mode == Transliteration::Mode::Enabled;
  return true;
}

void Framework::SaveTransliteration(bool allowTranslit)
{
  settings::Set(kTranslitMode, allowTranslit ? Transliteration::Mode::Enabled
                                             : Transliteration::Mode::Disabled);
}

void Framework::Allow3dMode(bool allow3d, bool allow3dBuildings)
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine == nullptr)
//    return;
//
//  if (!m_powerManager.IsFacilityEnabled(power_management::Facility::PerspectiveView))
//    allow3d = false;
//
//  if (!m_powerManager.IsFacilityEnabled(power_management::Facility::Buildings3d))
//    allow3dBuildings = false;
//
//  m_drapeEngines.begin()->second.m_drapeEngine->Allow3dMode(allow3d, allow3dBuildings);
}

void Framework::Save3dMode(bool allow3d, bool allow3dBuildings)
{
  settings::Set(kAllow3dKey, allow3d);
  settings::Set(kAllow3dBuildingsKey, allow3dBuildings);
}

void Framework::Load3dMode(bool & allow3d, bool & allow3dBuildings)
{
  if (!settings::Get(kAllow3dKey, allow3d))
    allow3d = true;

  if (!settings::Get(kAllow3dBuildingsKey, allow3dBuildings))
    allow3dBuildings = true;
}

void Framework::SaveLargeFontsSize(bool isLargeSize)
{
  settings::Set(kLargeFontsSize, isLargeSize);
}

bool Framework::LoadLargeFontsSize()
{
  bool isLargeSize;
  if (!settings::Get(kLargeFontsSize, isLargeSize))
    isLargeSize = false;
  return isLargeSize;
}

void Framework::SetLargeFontsSize(bool isLargeSize)
{
  double const scaleFactor = isLargeSize ? kLargeFontsScaleFactor : 1.0;

//  ASSERT(m_drapeEngines.begin()->second.m_drapeEngine.get() != nullptr, ());
//  m_drapeEngines.begin()->second.m_drapeEngine->SetFontScaleFactor(scaleFactor);

//  InvalidateRect(GetCurrentViewport());
}

bool Framework::LoadTrafficEnabled()
{
  bool enabled;
  if (!settings::Get(kTrafficEnabledKey, enabled))
    enabled = false;
  return enabled;
}

void Framework::SaveTrafficEnabled(bool trafficEnabled)
{
  settings::Set(kTrafficEnabledKey, trafficEnabled);
}

bool Framework::LoadTrafficSimplifiedColors()
{
  bool simplified;
  if (!settings::Get(kTrafficSimplifiedColorsKey, simplified))
    simplified = true;
  return simplified;
}

void Framework::SaveTrafficSimplifiedColors(bool simplified)
{
  settings::Set(kTrafficSimplifiedColorsKey, simplified);
}

bool Framework::LoadAutoZoom()
{
  bool allowAutoZoom;
  if (!settings::Get(kAllowAutoZoom, allowAutoZoom))
    allowAutoZoom = true;
  return allowAutoZoom;
}

void Framework::AllowAutoZoom(bool allowAutoZoom)
{
  routing::RouterType const type = m_routingManager.GetRouter();
  bool const isPedestrianRoute = type == RouterType::Pedestrian;

//  if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr)
//    m_drapeEngines.begin()->second.m_drapeEngine->AllowAutoZoom(allowAutoZoom && !isPedestrianRoute);
}

void Framework::SaveAutoZoom(bool allowAutoZoom)
{
  settings::Set(kAllowAutoZoom, allowAutoZoom);
}

bool Framework::LoadTransitSchemeEnabled()
{
  bool enabled;
  if (!settings::Get(kTransitSchemeEnabledKey, enabled))
    enabled = false;
  return enabled;
}

void Framework::SaveTransitSchemeEnabled(bool enabled)
{
  settings::Set(kTransitSchemeEnabledKey, enabled);
}

bool Framework::LoadIsolinesEnabled()
{
  bool enabled;
  if (!settings::Get(kIsolinesEnabledKey, enabled))
    enabled = false;
  return enabled;
}

void Framework::SaveIsolinesEnabled(bool enabled)
{
  settings::Set(kIsolinesEnabledKey, enabled);
}

void Framework::EnableChoosePositionMode(bool enable, bool enableBounds, bool applyPosition,
                                         m2::PointD const & position)
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr)
//  {
//    m_drapeEngines.begin()->second.m_drapeEngine->EnableChoosePositionMode(enable,
//      enableBounds ? GetSelectedFeatureTriangles() : vector<m2::TriangleD>(), applyPosition, position);
//  }
}

vector<m2::TriangleD> Framework::GetSelectedFeatureTriangles() const
{
  vector<m2::TriangleD> triangles;
  if (!m_currentPlacePageInfo || !m_currentPlacePageInfo->GetID().IsValid())
    return triangles;

  FeaturesLoaderGuard const guard(m_featuresFetcher.GetDataSource(), m_currentPlacePageInfo->GetID().m_mwmId);
  auto ft = guard.GetFeatureByIndex(m_currentPlacePageInfo->GetID().m_index);
  if (!ft)
    return triangles;

  if (ftypes::IsBuildingChecker::Instance()(feature::TypesHolder(*ft)))
  {
    triangles.reserve(10);
    ft->ForEachTriangle([&](m2::PointD const & p1, m2::PointD const & p2, m2::PointD const & p3)
    {
      triangles.emplace_back(p1, p2, p3);
    }, scales::GetUpperScale());
  }

  return triangles;
}

void Framework::BlockTapEvents(bool block)
{
//  if (m_drapeEngines.begin()->second.m_drapeEngine != nullptr)
//    m_drapeEngines.begin()->second.m_drapeEngine->BlockTapEvents(block);
}

bool Framework::ParseDrapeDebugCommand(string const & query)
{
  MapStyle desiredStyle = MapStyleCount;
  if (query == "?dark" || query == "mapstyle:dark")
    desiredStyle = MapStyleDark;
  else if (query == "?light" || query == "mapstyle:light")
    desiredStyle = MapStyleClear;
  else if (query == "?vdark" || query == "mapstyle:vehicle_dark")
    desiredStyle = MapStyleVehicleDark;
  else if (query == "?vlight" || query == "mapstyle:vehicle_light")
    desiredStyle = MapStyleVehicleClear;

  if (desiredStyle != MapStyleCount)
  {
#if defined(OMIM_OS_ANDROID)
    MarkMapStyle(desiredStyle);
#else
    SetMapStyle(desiredStyle);
#endif
    return true;
  }

  if (query == "?aa" || query == "effect:antialiasing")
  {
//    m_drapeEngines.begin()->second.m_drapeEngine->SetPosteffectEnabled(df::PostprocessRenderer::Antialiasing,
//                                        true /* enabled */);
    return true;
  }
  if (query == "?no-aa" || query == "effect:no-antialiasing")
  {
//    m_drapeEngines.begin()->second.m_drapeEngine->SetPosteffectEnabled(df::PostprocessRenderer::Antialiasing,
//                                        false /* enabled */);
    return true;
  }
  if (query == "?scheme")
  {
    m_transitManager.EnableTransitSchemeMode(true /* enable */);
    return true;
  }
  if (query == "?no-scheme")
  {
    m_transitManager.EnableTransitSchemeMode(false /* enable */);
    return true;
  }
  if (query == "?isolines")
  {
    m_isolinesManager.SetEnabled(true /* enable */);
    return true;
  }
  if (query == "?no-isolines")
  {
    m_isolinesManager.SetEnabled(false /* enable */);
    return true;
  }
  if (query == "?debug-info")
  {
//    m_drapeEngines.begin()->second.m_drapeEngine->ShowDebugInfo(true /* shown */);
    return true;
  }
  if (query == "?debug-info-always")
  {
//    m_drapeEngines.begin()->second.m_drapeEngine->ShowDebugInfo(true /* shown */);
//    settings::Set(kShowDebugInfo, true);
    return true;
  }
  if (query == "?no-debug-info")
  {
//    m_drapeEngines.begin()->second.m_drapeEngine->ShowDebugInfo(false /* shown */);
//    settings::Set(kShowDebugInfo, false);
    return true;
  }
  if (query == "?debug-rect")
  {
//    m_drapeEngines.begin()->second.m_drapeEngine->EnableDebugRectRendering(true /* shown */);
    return true;
  }
  if (query == "?no-debug-rect")
  {
//    m_drapeEngines.begin()->second.m_drapeEngine->EnableDebugRectRendering(false /* shown */);
    return true;
  }
#if defined(OMIM_METAL_AVAILABLE)
  if (query == "?metal")
  {
    SavePreferredGraphicsAPI(dp::ApiVersion::Metal);
    return true;
  }
#endif
#if defined(OMIM_OS_ANDROID)
  if (query == "?vulkan")
  {
    SavePreferredGraphicsAPI(dp::ApiVersion::Vulkan);
    return true;
  }
#endif
  if (query == "?gl")
  {
    SavePreferredGraphicsAPI(dp::ApiVersion::OpenGLES3);
    return true;
  }
  return false;
}

bool Framework::ParseEditorDebugCommand(search::SearchParams const & params)
{
  if (params.m_query == "?edits")
  {
    osm::Editor::Stats stats = osm::Editor::Instance().GetStats();
    search::Results results;
    results.AddResultNoChecks(search::Result("Uploaded: " + strings::to_string(stats.m_uploadedCount), "?edits"));
    for (auto & edit : stats.m_edits)
    {
      FeatureID const & fid = edit.first;

      FeaturesLoaderGuard guard(m_featuresFetcher.GetDataSource(), fid.m_mwmId);
      auto ft = guard.GetFeatureByIndex(fid.m_index);
      if (!ft)
      {
        LOG(LERROR, ("Feature can't be loaded:", fid));
        return true;
      }

      search::Result res(feature::GetCenter(*ft), string(ft->GetReadableName()));
      res.SetAddress(std::move(edit.second));
      res.FromFeature(fid, feature::TypesHolder(*ft).GetBestType(), {});

      results.AddResultNoChecks(std::move(res));
    }

    params.m_onResults(results);

    results.SetEndMarker(false /* isCancelled */);
    params.m_onResults(results);
    return true;
  }
  else if (params.m_query == "?eclear")
  {
    osm::Editor::Instance().ClearAllLocalEdits();
    return true;
  }

  return false;
}

bool Framework::ParseRoutingDebugCommand(search::SearchParams const & params)
{
  if (params.m_query == "?debug-cam")
  {
    settings::Set(kDebugSpeedCamSetting, true);
    return true;
  }
  else if (params.m_query == "?no-debug-cam")
  {
    settings::Set(kDebugSpeedCamSetting, false);
    return true;
  }
  return false;
}

// Editable map object helper functions.
namespace
{
bool LocalizeStreet(DataSource const & dataSource, FeatureID const & fid, osm::LocalizedStreet & result)
{
  FeaturesLoaderGuard g(dataSource, fid.m_mwmId);
  auto ft = g.GetFeatureByIndex(fid.m_index);
  if (!ft)
    return false;

  result.m_defaultName = ft->GetName(StringUtf8Multilang::kDefaultCode);

  result.m_localizedName = ft->GetReadableName();

  if (result.m_localizedName == result.m_defaultName)
    result.m_localizedName.clear();
  return true;
}

vector<osm::LocalizedStreet> TakeSomeStreetsAndLocalize(
    vector<search::ReverseGeocoder::Street> const & streets, DataSource const & dataSource)

{
  vector<osm::LocalizedStreet> results;
  // Exact feature street always goes first in Editor UI street list.

  // Reasonable number of different nearby street names to display in UI.
  constexpr size_t kMaxNumberOfNearbyStreetsToDisplay = 8;
  for (auto const & street : streets)
  {
    auto const isDuplicate = find_if(begin(results), end(results),
                                     [&street](osm::LocalizedStreet const & s)
                                     {
                                       return s.m_defaultName == street.m_name ||
                                              s.m_localizedName == street.m_name;
                                     }) != results.end();
    if (isDuplicate)
      continue;

    osm::LocalizedStreet ls;
    if (!LocalizeStreet(dataSource, street.m_id, ls))
      continue;

    results.emplace_back(std::move(ls));
    if (results.size() >= kMaxNumberOfNearbyStreetsToDisplay)
      break;
  }
  return results;
}

void SetStreet(search::ReverseGeocoder const & coder, DataSource const & dataSource,
               FeatureType & ft, osm::EditableMapObject & emo)
{
  // Get exact feature's street address (if any) from mwm,
  // together with all nearby streets.
  vector<search::ReverseGeocoder::Street> streets;
  coder.GetNearbyStreets(ft, streets);

  string street = coder.GetFeatureStreetName(ft);

  auto localizedStreets = TakeSomeStreetsAndLocalize(streets, dataSource);

  if (!street.empty())
  {
    auto it = find_if(begin(streets), end(streets),
                      [&street](search::ReverseGeocoder::Street const & s)
                      {
                        return s.m_name == street;
                      });

    if (it != end(streets))
    {
      osm::LocalizedStreet ls;
      if (!LocalizeStreet(dataSource, it->m_id, ls))
        ls.m_defaultName = street;

      emo.SetStreet(ls);

      // A street that a feature belongs to should always be in the first place in the list.
      auto it =
          find_if(begin(localizedStreets), end(localizedStreets), [&ls](osm::LocalizedStreet const & rs)
                  {
                    return ls.m_defaultName == rs.m_defaultName;
                  });
      if (it != end(localizedStreets))
        iter_swap(it, begin(localizedStreets));
      else
        localizedStreets.insert(begin(localizedStreets), ls);
    }
    else
    {
      emo.SetStreet({street, ""});
    }
  }
  else
  {
    emo.SetStreet({});
  }

  emo.SetNearbyStreets(std::move(localizedStreets));
}

void SetHostingBuildingAddress(FeatureID const & hostingBuildingFid, DataSource const & dataSource,
                               search::ReverseGeocoder const & coder, osm::EditableMapObject & emo)
{
  if (!hostingBuildingFid.IsValid())
    return;

  FeaturesLoaderGuard g(dataSource, hostingBuildingFid.m_mwmId);
  auto hostingBuildingFeature = g.GetFeatureByIndex(hostingBuildingFid.m_index);
  if (!hostingBuildingFeature)
    return;

  search::ReverseGeocoder::Address address;
  if (coder.GetExactAddress(*hostingBuildingFeature, address))
  {
    if (emo.GetHouseNumber().empty())
      emo.SetHouseNumber(address.GetHouseNumber());
    if (emo.GetStreet().m_defaultName.empty())
      // TODO(mgsergio): Localize if localization is required by UI.
      emo.SetStreet({address.GetStreetName(), ""});
  }
}
}  // namespace

bool Framework::CanEditMap() const
{
  return !GetStorage().IsDownloadInProgress();
}

bool Framework::CreateMapObject(m2::PointD const & mercator, uint32_t const featureType,
                                osm::EditableMapObject & emo) const
{
  emo = {};
  auto const & dataSource = m_featuresFetcher.GetDataSource();
  MwmSet::MwmId const mwmId = dataSource.GetMwmIdByCountryFile(
        platform::CountryFile(m_infoGetter->GetRegionCountryId(mercator)));
  if (!mwmId.IsAlive())
    return false;

  search::ReverseGeocoder const coder(m_featuresFetcher.GetDataSource());
  vector<search::ReverseGeocoder::Street> streets;

  coder.GetNearbyStreets(mwmId, mercator, streets);
  emo.SetNearbyStreets(TakeSomeStreetsAndLocalize(streets, m_featuresFetcher.GetDataSource()));

  // TODO(mgsergio): Check emo is a poi. For now it is the only option.
  SetHostingBuildingAddress(FindBuildingAtPoint(mercator), dataSource, coder, emo);

  return osm::Editor::Instance().CreatePoint(featureType, mercator, mwmId, emo);
}

bool Framework::GetEditableMapObject(FeatureID const & fid, osm::EditableMapObject & emo) const
{
  if (!fid.IsValid())
    return false;

  FeaturesLoaderGuard guard(m_featuresFetcher.GetDataSource(), fid.m_mwmId);
  auto ft = guard.GetFeatureByIndex(fid.m_index);
  if (!ft)
    return false;

  emo = {};
  emo.SetFromFeatureType(*ft);
  auto const & editor = osm::Editor::Instance();
  emo.SetEditableProperties(editor.GetEditableProperties(*ft));

  auto const & dataSource = m_featuresFetcher.GetDataSource();
  search::ReverseGeocoder const coder(dataSource);
  SetStreet(coder, dataSource, *ft, emo);

  if (!ftypes::IsBuildingChecker::Instance()(*ft) &&
      (emo.GetHouseNumber().empty() || emo.GetStreet().m_defaultName.empty()))
  {
    SetHostingBuildingAddress(FindBuildingAtPoint(feature::GetCenter(*ft)), dataSource, coder, emo);
  }

  return true;
}

osm::Editor::SaveResult Framework::SaveEditedMapObject(osm::EditableMapObject emo)
{
  auto & editor = osm::Editor::Instance();

  ms::LatLon issueLatLon;

  auto shouldNotify = false;
  // Notify if a poi address and it's hosting building address differ.
  do
  {
    auto const isCreatedFeature = editor.IsCreatedFeature(emo.GetID());

    FeaturesLoaderGuard g(m_featuresFetcher.GetDataSource(), emo.GetID().m_mwmId);
    std::unique_ptr<FeatureType> originalFeature;
    if (!isCreatedFeature)
    {
      originalFeature = g.GetOriginalFeatureByIndex(emo.GetID().m_index);
      if (!originalFeature)
        return osm::Editor::SaveResult::NoUnderlyingMapError;
    }
    else
    {
      originalFeature = FeatureType::CreateFromMapObject(emo);
    }

    // Handle only pois.
    if (ftypes::IsBuildingChecker::Instance()(*originalFeature))
      break;

    auto const hostingBuildingFid = FindBuildingAtPoint(feature::GetCenter(*originalFeature));
    // The is no building to take address from. Fallback to simple saving.
    if (!hostingBuildingFid.IsValid())
      break;

    auto hostingBuildingFeature = g.GetFeatureByIndex(hostingBuildingFid.m_index);
    if (!hostingBuildingFeature)
      break;

    issueLatLon = mercator::ToLatLon(feature::GetCenter(*hostingBuildingFeature));

    search::ReverseGeocoder::Address hostingBuildingAddress;
    search::ReverseGeocoder const coder(m_featuresFetcher.GetDataSource());
    // The is no address to take from a hosting building. Fallback to simple saving.
    if (!coder.GetExactAddress(*hostingBuildingFeature, hostingBuildingAddress))
      break;

    string originalFeatureStreet;
    if (!isCreatedFeature)
    {
      originalFeatureStreet = coder.GetOriginalFeatureStreetName(originalFeature->GetID());
    }
    else
    {
      originalFeatureStreet = emo.GetStreet().m_defaultName;
    }

    auto isStreetOverridden = false;
    if (!hostingBuildingAddress.GetStreetName().empty() &&
        emo.GetStreet().m_defaultName != hostingBuildingAddress.GetStreetName())
    {
      isStreetOverridden = true;
      shouldNotify = true;
    }
    auto isHouseNumberOverridden = false;
    if (!hostingBuildingAddress.GetHouseNumber().empty() &&
        emo.GetHouseNumber() != hostingBuildingAddress.GetHouseNumber())
    {
      isHouseNumberOverridden = true;
      shouldNotify = true;
    }

    // TODO(mgsergio): Consider this:
    // User changed a feature that had no original address and the address he entered
    // is different from the one on the hosting building. He saved it and the changes
    // were uploaded to OSM. Then he set the address from the hosting building back
    // to the feature. As a result this address won't be merged in OSM because emo.SetStreet({})
    // and emo.SetHouseNumber("") will be called in the following code. So OSM ends up
    // with incorrect data.

    // There is (almost) always a street and/or house number set in emo. We must keep them from
    // saving to editor and pushing to OSM if they ware not overidden. To be saved to editor
    // emo is first converted to FeatureType and FeatureType is then saved to a file and editor.
    // To keep street and house number from penetrating to FeatureType we set them to be empty.

    // Do not save street if it was taken from hosting building.
    if ((originalFeatureStreet.empty() || isCreatedFeature) && !isStreetOverridden)
        emo.SetStreet({});
    // Do not save house number if it was taken from hosting building.
    if ((originalFeature->GetHouseNumber().empty() || isCreatedFeature) && !isHouseNumberOverridden)
      emo.SetHouseNumber("");

    if (!isStreetOverridden && !isHouseNumberOverridden)
    {
      // Address was taken from the hosting building of a feature. Nothing to note.
      shouldNotify = false;
      break;
    }

    if (shouldNotify)
    {
      auto editedFeature = editor.GetEditedFeature(emo.GetID());
      string editedFeatureStreet;
      // Such a notification have been already sent. I.e at least one of
      // street of house number should differ in emo and editor.
      shouldNotify =
          !isCreatedFeature &&
          ((editedFeature && !editedFeature->GetHouseNumber().empty() &&
            editedFeature->GetHouseNumber() != emo.GetHouseNumber()) ||
           (editor.GetEditedFeatureStreet(emo.GetID(), editedFeatureStreet) &&
            !editedFeatureStreet.empty() && editedFeatureStreet != emo.GetStreet().m_defaultName));
    }
  } while (0);

  if (shouldNotify)
  {
    // TODO @mgsergio fill with correct NoteProblemType
    editor.CreateNote(emo.GetLatLon(), emo.GetID(), emo.GetTypes(), emo.GetDefaultName(),
                      osm::Editor::NoteProblemType::General,
                      "The address on this POI is different from the building address."
                      " It is either a user's mistake, or an issue in the data. Please"
                      " check this and fix if needed. (This note was created automatically"
                      " without a user's input. Feel free to close it if it's wrong).");
  }

  emo.RemoveNeedlessNames();

  auto const result = osm::Editor::Instance().SaveEditedFeature(emo);

  // Automatically select newly created objects.
  if (!m_currentPlacePageInfo)
  {
    place_page::BuildInfo info;
    info.m_mercator = emo.GetMercator();
    info.m_featureId = emo.GetID();
    m_currentPlacePageInfo = BuildPlacePageInfo(info);
    ActivateMapSelection();
  }

  return result;
}

void Framework::DeleteFeature(FeatureID const & fid)
{
  osm::Editor::Instance().DeleteFeature(fid);
  UpdatePlacePageInfoForCurrentSelection();
}

osm::NewFeatureCategories Framework::GetEditorCategories() const
{
  return osm::Editor::Instance().GetNewFeatureCategories();
}

bool Framework::RollBackChanges(FeatureID const & fid)
{
  auto & editor = osm::Editor::Instance();
  auto const status = editor.GetFeatureStatus(fid);
  auto const rolledBack = editor.RollBackChanges(fid);
  if (rolledBack)
  {
    if (status == FeatureStatus::Created)
      DeactivateMapSelection(true /* notifyUI */);
    else
      UpdatePlacePageInfoForCurrentSelection();
  }
  return rolledBack;
}

void Framework::CreateNote(osm::MapObject const & mapObject,
                           osm::Editor::NoteProblemType const type, string const & note)
{
  osm::Editor::Instance().CreateNote(mapObject.GetLatLon(), mapObject.GetID(), mapObject.GetTypes(),
                                     mapObject.GetDefaultName(), type, note);
  if (type == osm::Editor::NoteProblemType::PlaceDoesNotExist)
    DeactivateMapSelection(true /* notifyUI */);
}

void Framework::RunUITask(function<void()> fn)
{
  GetPlatform().RunTask(Platform::Thread::Gui, std::move(fn));
}

void Framework::ShowViewportSearchResults(SearchResultsIterT begin, SearchResultsIterT end, bool clear)
{
  FillSearchResultsMarks(begin, end, clear);
}

void Framework::ClearViewportSearchResults()
{
  m_searchMarks.ClearTrackedProperties();
  GetBookmarkManager().GetEditSession().ClearGroup(UserMark::Type::SEARCH);
}

std::optional<m2::PointD> Framework::GetCurrentPosition() const
{
  auto const & myPosMark = GetBookmarkManager().MyPositionMark();
  if (!myPosMark.HasPosition())
    return {};
  return myPosMark.GetPivot();
}

bool Framework::ParseSearchQueryCommand(search::SearchParams const & params)
{
  if (ParseDrapeDebugCommand(params.m_query))
    return true;
  if (ParseSetGpsTrackMinAccuracyCommand(params.m_query))
    return true;
  if (ParseEditorDebugCommand(params))
    return true;
  if (ParseRoutingDebugCommand(params))
    return true;
  return false;
}

m2::PointD Framework::GetMinDistanceBetweenResults() const
{
  return m_searchMarks.GetMaxDimension(m_currentModelView);
}

std::vector<std::string> Framework::GetRegionsCountryIdByRect(m2::RectD const & rect, bool rough) const
{
  return m_infoGetter->GetRegionsCountryIdByRect(rect, rough);
}

vector<MwmSet::MwmId> Framework::GetMwmsByRect(m2::RectD const & rect, bool rough) const
{
  vector<MwmSet::MwmId> result;
  if (!m_infoGetter)
    return result;

  auto const & dataSource = m_featuresFetcher.GetDataSource();
  auto countryIds = GetRegionsCountryIdByRect(rect, rough);
  for (auto & id : countryIds)
    result.push_back(dataSource.GetMwmIdByCountryFile(platform::CountryFile(std::move(id))));

  return result;
}

void Framework::ReadFeatures(function<void(FeatureType &)> const & reader,
                             vector<FeatureID> const & features)
{
  m_featuresFetcher.ReadFeatures(reader, features);
}

// RoutingManager::Delegate
void Framework::OnRouteFollow(routing::RouterType type)
{
  bool const isPedestrianRoute = type == RouterType::Pedestrian;
  bool const enableAutoZoom = isPedestrianRoute ? false : LoadAutoZoom();
  int const scale =
      isPedestrianRoute ? scales::GetPedestrianNavigationScale() : scales::GetNavigationScale();
  int scale3d =
      isPedestrianRoute ? scales::GetPedestrianNavigation3dScale() : scales::GetNavigation3dScale();
  if (enableAutoZoom)
    ++scale3d;

  bool const isBicycleRoute = type == RouterType::Bicycle;
  if ((isPedestrianRoute || isBicycleRoute) && LoadTrafficEnabled())
  {
    m_trafficManager.SetEnabled(false /* enabled */);
    SaveTrafficEnabled(false /* enabled */);
  }
  // TODO. We need to sync two enums VehicleType and RouterType to be able to pass
  // GetRoutingSettings(type).m_matchRoute to the FollowRoute() instead of |isPedestrianRoute|.
  // |isArrowGlued| parameter fully corresponds to |m_matchRoute| in RoutingSettings.
  m_drapeEngines.begin()->second.m_drapeEngine->FollowRoute(scale, scale3d, enableAutoZoom, !isPedestrianRoute /* isArrowGlued */);
}

// RoutingManager::Delegate
void Framework::RegisterCountryFilesOnRoute(shared_ptr<routing::NumMwmIds> ptr) const
{
  m_storage.ForEachCountry([&ptr](storage::Country const & country)
  {
    ptr->RegisterFile(country.GetFile());
  });
}

void Framework::SetPlacePageLocation(place_page::Info & info)
{
  ASSERT(m_infoGetter, ());

  if (info.GetCountryId().empty())
    info.SetCountryId(m_infoGetter->GetRegionCountryId(info.GetMercator()));

  CountriesVec countries;
  if (info.GetTopmostCountryIds().empty())
  {
    GetStorage().GetTopmostNodesFor(info.GetCountryId(), countries);
    info.SetTopmostCountryIds(std::move(countries));
  }
}

void Framework::FillDescription(FeatureType & ft, place_page::Info & info) const
{
  if (!ft.GetID().m_mwmId.IsAlive())
    return;
  auto const & regionData = ft.GetID().m_mwmId.GetInfo()->GetRegionData();
  auto const deviceLang = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());
  auto const langPriority = feature::GetDescriptionLangPriority(regionData, deviceLang);

  std::string description = m_descriptionsLoader->GetDescription(ft.GetID(), langPriority);
  if (!description.empty())
  {
    info.SetDescription(std::move(description));
    info.SetOpeningMode(m_routingManager.IsRoutingActive()
                        ? place_page::OpeningMode::Preview
                        : place_page::OpeningMode::PreviewPlus);
  }
}

void Framework::OnPowerFacilityChanged(power_management::Facility const facility, bool enabled)
{
  if (facility == power_management::Facility::PerspectiveView ||
      facility == power_management::Facility::Buildings3d)
  {
    bool allow3d = true, allow3dBuildings = true;
    Load3dMode(allow3d, allow3dBuildings);

    if (facility == power_management::Facility::PerspectiveView)
      allow3d = allow3d && enabled;
    else
      allow3dBuildings = allow3dBuildings && enabled;

    Allow3dMode(allow3d, allow3dBuildings);
  }
  else if (facility == power_management::Facility::TrafficJams)
  {
    auto trafficState = enabled && LoadTrafficEnabled();
    if (trafficState == GetTrafficManager().IsEnabled())
      return;

    GetTrafficManager().SetEnabled(trafficState);
  }
}

void Framework::OnPowerSchemeChanged(power_management::Scheme const actualScheme)
{
  if (actualScheme == power_management::Scheme::EconomyMaximum && GetTrafficManager().IsEnabled())
    GetTrafficManager().SetEnabled(false);
}
