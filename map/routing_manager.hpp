#pragma once

#include "map/bookmark_manager.hpp"
#include "map/extrapolation/extrapolator.hpp"
#include "map/routing_mark.hpp"
#include "map/transit/transit_display.hpp"
#include "map/transit/transit_reader.hpp"

#include "routing/following_info.hpp"
#include "routing/route.hpp"
#include "routing/router.hpp"
#include "routing/routing_callbacks.hpp"
#include "routing/routing_session.hpp"
#include "routing/speed_camera_manager.hpp"

#include "storage/storage_defines.hpp"

#include "drape_frontend/drape_engine_safe_ptr.hpp"

#include "drape/pointers.hpp"

#include "geometry/point2d.hpp"
#include "geometry/point_with_altitude.hpp"

#include "base/thread_checker.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace storage
{
class CountryInfoGetter;
}

namespace routing
{
class NumMwmIds;
}

class DataSource;

namespace power_management
{
  class PowerManager;
}

struct RoutePointInfo
{
  std::string m_name;
  RouteMarkType m_markType = RouteMarkType::Start;
  size_t m_intermediateIndex = 0;
  bool m_isPassed = false;
  bool m_isMyPosition = false;
  m2::PointD m_position;
};

class RoutingManager final
{
public:
  class Delegate
  {
  public:
    virtual void OnRouteFollow(routing::RouterType type) = 0;
    virtual void RegisterCountryFilesOnRoute(std::shared_ptr<routing::NumMwmIds> ptr) const = 0;

    virtual ~Delegate() = default;
  };

  struct Callbacks
  {
    using DataSourceGetterFn = std::function<DataSource &()>;
    using CountryInfoGetterFn = std::function<storage::CountryInfoGetter const &()>;
    using CountryParentNameGetterFn = std::function<std::string(std::string const &)>;
    using GetStringsBundleFn = std::function<StringsBundle const &()>;
    using PowerManagerGetter = std::function<power_management::PowerManager const &()>;

    template <typename DataSourceGetter, typename CountryInfoGetter,
              typename CountryParentNameGetter, typename StringsBundleGetter,
              typename PowerManagerGetter>
    Callbacks(DataSourceGetter && dataSourceGetter, CountryInfoGetter && countryInfoGetter,
              CountryParentNameGetter && countryParentNameGetter,
              StringsBundleGetter && stringsBundleGetter, PowerManagerGetter && powerManagerGetter)
      : m_dataSourceGetter(std::forward<DataSourceGetter>(dataSourceGetter))
      , m_countryInfoGetter(std::forward<CountryInfoGetter>(countryInfoGetter))
      , m_countryParentNameGetterFn(std::forward<CountryParentNameGetter>(countryParentNameGetter))
      , m_stringsBundleGetter(std::forward<StringsBundleGetter>(stringsBundleGetter))
      , m_powerManagerGetter(std::forward<PowerManagerGetter>(powerManagerGetter))
    {
    }

    DataSourceGetterFn m_dataSourceGetter;
    CountryInfoGetterFn m_countryInfoGetter;
    CountryParentNameGetterFn m_countryParentNameGetterFn;
    GetStringsBundleFn m_stringsBundleGetter;
    PowerManagerGetter m_powerManagerGetter;
  };

  using RouteBuildingCallback =
      std::function<void(routing::RouterResultCode, storage::CountriesSet const &)>;
  using RouteSpeedCamShowCallback =
      std::function<void(m2::PointD const &, double)>;
  using RouteSpeedCamsClearCallback =
      std::function<void()>;

  using RouteStartBuildCallback = std::function<void(std::vector<RouteMarkData> const & points)>;

  enum class Recommendation
  {
    // It can be recommended if location is found almost immediately
    // after restoring route points from file. In this case we can
    // rebuild route using "my position".
    RebuildAfterPointsLoading = 0,
  };
  using RouteRecommendCallback = std::function<void(Recommendation)>;

  RoutingManager(Callbacks && callbacks, Delegate & delegate);

  void SetBookmarkManager(BookmarkManager * bmManager);
  void SetTransitManager(TransitReadManager * transitManager);

  routing::RoutingSession const & RoutingSession() const { return m_routingSession; }
  routing::RoutingSession & RoutingSession() { return m_routingSession; }
  //std::vector<routing::Route::SubrouteAttrs> GetSubroutes() { return m_routingSession.GetSubroutes(); }
  void SetRouter(routing::RouterType type);
  routing::RouterType GetRouter() const { return m_currentRouterType; }
  bool IsRoutingActive() const { return m_routingSession.IsActive(); }
  bool IsRouteBuilt() const { return m_routingSession.IsBuilt(); }
  bool IsRouteBuilding() const { return m_routingSession.IsBuilding(); }
  bool IsRouteRebuildingOnly() const { return m_routingSession.IsRebuildingOnly(); }
  bool IsRouteFinished() const { return m_routingSession.IsFinished(); }
  bool IsOnRoute() const { return m_routingSession.IsOnRoute(); }
  bool IsRoutingFollowing() const { return m_routingSession.IsFollowing(); }
  bool IsRouteValid() const { return m_routingSession.IsRouteValid(); }
  void BuildRoute(uint32_t timeoutSec = routing::RouterDelegate::kNoTimeout);
  void SetUserCurrentPosition(m2::PointD const & position);
  void ResetRoutingSession() { m_routingSession.Reset(); }
  // FollowRoute has a bug where the router follows the route even if the method hads't been called.
  // This method was added because we do not want to break the behaviour that is familiar to our
  // users.
  bool DisableFollowMode();

  void SetRouteBuildingListener(RouteBuildingCallback const & buildingCallback)
  {
    m_routingBuildingCallback = buildingCallback;
  }

  void SetRouteSpeedCamShowListener(RouteSpeedCamShowCallback const & speedCamShowCallback)
  {
    m_routeSpeedCamShowCallback = speedCamShowCallback;
  }

  void SetRouteSpeedCamsClearListener(RouteSpeedCamsClearCallback const & speedCamsClearCallback)
  {
    m_routeSpeedCamsClearCallback = speedCamsClearCallback;
  }

  /// See warning above.
  void SetRouteProgressListener(routing::ProgressCallback const & progressCallback)
  {
    m_routingSession.SetProgressCallback(progressCallback);
  }
  void SetRouteRecommendationListener(RouteRecommendCallback const & recommendCallback)
  {
    m_routeRecommendCallback = recommendCallback;
  }
  void FollowRoute();
  void CloseRouting(bool removeRoutePoints);
  void GetRouteFollowingInfo(routing::FollowingInfo & info) const
  {
    m_routingSession.GetRouteFollowingInfo(info);
  }

  TransitRouteInfo GetTransitRouteInfo() const;

  m2::PointD GetRouteEndPoint() const { return m_routingSession.GetEndPoint(); }
  /// Returns the most situable router engine type.
  routing::RouterType GetBestRouter(m2::PointD const & startPoint,
                                    m2::PointD const & finalPoint) const;
  routing::RouterType GetLastUsedRouter() const;
  void SetLastUsedRouter(routing::RouterType type);
  // Sound notifications for turn instructions.
  void EnableTurnNotifications(bool enable) { m_routingSession.EnableTurnNotifications(enable); }
  bool AreTurnNotificationsEnabled() const
  {
    return m_routingSession.AreTurnNotificationsEnabled();
  }
  /// \brief Sets a locale for TTS.
  /// \param locale is a string with locale code. For example "en", "ru", "zh-Hant" and so on.
  /// \note See sound/tts/languages.txt for the full list of available locales.
  void SetTurnNotificationsLocale(std::string const & locale)
  {
    m_routingSession.SetTurnNotificationsLocale(locale);
  }
  /// @return current TTS locale. For example "en", "ru", "zh-Hant" and so on.
  /// In case of error returns an empty string.
  /// \note The method returns correct locale after SetTurnNotificationsLocale has been called.
  /// If not, it returns an empty string.
  std::string GetTurnNotificationsLocale() const
  {
    return m_routingSession.GetTurnNotificationsLocale();
  }
  // @return polyline of the route.
  routing::FollowedPolyline const & GetRoutePolylineForTests() const
  {
    return m_routingSession.GetRouteForTests()->GetFollowedPolyline();
  }
  // @return generated turns on the route.
  std::vector<routing::turns::TurnItem> GetTurnsOnRouteForTests() const
  {
    std::vector<routing::turns::TurnItem> turns;
    m_routingSession.GetRouteForTests()->GetTurnsForTesting(turns);
    return turns;
  }

  Callbacks & GetCallbacksForTests() { return m_callbacks; }
  /// \brief Adds to @param notifications strings - notifications, which are ready to be
  /// pronounced to end user right now.
  /// Adds notifications about turns and speed camera on the road.
  /// \param announceStreets is true when TTS street names should be pronounced
  ///
  /// \note Current notifications will be deleted after call and second call
  /// will not return previous data, only newer.
  void GenerateNotifications(std::vector<std::string> & notifications, bool announceStreets);

  void AddRoutePoint(RouteMarkData && markData);
  void ContinueRouteToPoint(RouteMarkData && markData);
  std::vector<RouteMarkData> GetRoutePoints() const;
  size_t GetRoutePointsCount() const;
  void RemoveRoutePoint(RouteMarkType type, size_t intermediateIndex = 0);
  void RemoveRoutePoints();
  void RemoveIntermediateRoutePoints();
  void MoveRoutePoint(size_t currentIndex, size_t targetIndex);
  void MoveRoutePoint(RouteMarkType currentType, size_t currentIntermediateIndex,
                      RouteMarkType targetType, size_t targetIntermediateIndex);
  void HideRoutePoint(RouteMarkType type, size_t intermediateIndex = 0);
  bool CouldAddIntermediatePoint() const;
  bool IsMyPosition(RouteMarkType type, size_t intermediateIndex = 0);

  void SetRouterImpl(routing::RouterType type);
  void RemoveRoute(bool deactivateFollowing);

  void CheckLocationForRouting(location::GpsInfo const & info);
  void CallRouteBuilded(routing::RouterResultCode code,
                        storage::CountriesSet const & absentCountries);
  void OnBuildRouteReady(routing::Route const & route, routing::RouterResultCode code);
  void OnRebuildRouteReady(routing::Route const & route, routing::RouterResultCode code);
  void OnNeedMoreMaps(uint64_t routeId, storage::CountriesSet const & absentCountries);
  void OnRemoveRoute(routing::RouterResultCode code);
  void OnRoutePointPassed(RouteMarkType type, size_t intermediateIndex);
  void OnLocationUpdate(location::GpsInfo const & info);

  routing::SpeedCameraManager & GetSpeedCamManager() { return m_routingSession.GetSpeedCamManager(); }
  bool IsSpeedCamLimitExceeded() const;

  void SetTurnNotificationsUnits(measurement_utils::Units const units)
  {
    m_routingSession.SetTurnNotificationsUnits(units);
  }
  void SetDrapeEngine(ref_ptr<df::DrapeEngine> engine, bool is3dAllowed);
  /// \returns true if altitude information along |m_route| is available and
  /// false otherwise.
  bool HasRouteAltitude() const;

  struct DistanceAltitude
  {
    std::vector<double> m_distances;
    geometry::Altitudes m_altitudes;

    size_t GetSize() const
    {
      ASSERT_EQUAL(m_distances.size(), m_altitudes.size(), ());
      return m_distances.size();
    }

    // Default altitudeDeviation ~ sqrt(2).
    void Simplify(double altitudeDeviation = 1.415);

    /// \brief Generates 4 bytes per point image (RGBA) and put the data to |imageRGBAData|.
    /// \param width is width of chart shall be generated in pixels.
    /// \param height is height of chart shall be generated in pixels.
    /// \param imageRGBAData is bits of result image in RGBA.
    /// \returns If there is valid route info and the chart was generated returns true
    /// and false otherwise. If the method returns true it is guaranteed that the size of
    /// |imageRGBAData| is not zero.
    bool GenerateRouteAltitudeChart(uint32_t width, uint32_t height, std::vector<uint8_t> & imageRGBAData) const;

    /// \param totalAscent is total ascent of the route in meters.
    /// \param totalDescent is total descent of the route in meters.
    void CalculateAscentDescent(uint32_t & totalAscentM, uint32_t & totalDescentM) const;

    friend std::string DebugPrint(DistanceAltitude const & da);
  };

  /// \brief Fills altitude of current route points and distance in meters form the beginning
  /// of the route point based on the route in RoutingSession.
  /// \return False if current route is invalid or doesn't have altitudes.
  bool GetRouteAltitudesAndDistancesM(DistanceAltitude & da) const;

  uint32_t OpenRoutePointsTransaction();
  void ApplyRoutePointsTransaction(uint32_t transactionId);
  void CancelRoutePointsTransaction(uint32_t transactionId);
  static uint32_t InvalidRoutePointsTransactionId();

  /// \returns true if there are route points saved in file and false otherwise.
  bool HasSavedRoutePoints() const;
  /// \brief It loads road points from file and delete file after loading.
  /// The result of the loading will be sent via SafeCallback.
  using LoadRouteHandler = platform::SafeCallback<void(bool success)>;
  void LoadRoutePoints(LoadRouteHandler const & handler);
  /// \brief It saves route points to file.
  void SaveRoutePoints();
  /// \brief It deletes file with saved route points if it exists.
  void DeleteSavedRoutePoints();

  void UpdatePreviewMode();
  void CancelPreviewMode();

  routing::RouterType GetCurrentRouterType() const { return m_currentRouterType; }

private:
  /// \returns true if the route has warnings.
  bool InsertRoute(routing::Route const & route);

  struct RoadInfo
  {
    RoadInfo() = default;

    explicit RoadInfo(m2::PointD const & pt, FeatureID const & featureId)
      : m_startPoint(pt)
      , m_featureId(featureId)
    {}

    m2::PointD m_startPoint;
    FeatureID m_featureId;
    double m_distance = 0.0;
  };
  using RoadWarningsCollection = std::map<routing::RoutingOptions::Road, std::vector<RoadInfo>>;

  using GetMwmIdFn = std::function<MwmSet::MwmId (routing::NumMwmId numMwmId)>;
  void CollectRoadWarnings(std::vector<routing::RouteSegment> const & segments, m2::PointD const & startPt,
                           double baseDistance, GetMwmIdFn const & getMwmIdFn, RoadWarningsCollection & roadWarnings);
  void CreateRoadWarningMarks(RoadWarningsCollection && roadWarnings);

  /// \returns false if the location could not be matched to the route and should be matched to the
  /// road graph. Otherwise returns true.
  void MatchLocationToRoute(location::GpsInfo & info,
                            location::RouteMatchingInfo & routeMatchingInfo);
  location::RouteMatchingInfo GetRouteMatchingInfo(location::GpsInfo & info);
  uint32_t GenerateRoutePointsTransactionId() const;

  void SetPointsFollowingMode(bool enabled);

  void ReorderIntermediatePoints();

  m2::RectD ShowPreviewSegments(std::vector<RouteMarkData> const & routePoints);
  void HidePreviewSegments();

  void SetSubroutesVisibility(bool visible);

  void CancelRecommendation(Recommendation recommendation);

  std::vector<RouteMarkData> GetRoutePointsToSave() const;

  void OnExtrapolatedLocationUpdate(location::GpsInfo const & info);

  RouteBuildingCallback m_routingBuildingCallback;
  RouteSpeedCamShowCallback m_routeSpeedCamShowCallback;
  RouteSpeedCamsClearCallback m_routeSpeedCamsClearCallback;
  RouteRecommendCallback m_routeRecommendCallback;
  Callbacks m_callbacks;
  df::DrapeEngineSafePtr m_drapeEngine;
  routing::RouterType m_currentRouterType = routing::RouterType::Count;
  bool m_loadAltitudes = false;
  routing::RoutingSession m_routingSession;
  Delegate & m_delegate;

  BookmarkManager * m_bmManager = nullptr;
  extrapolation::Extrapolator m_extrapolator;

  std::vector<dp::DrapeID> m_drapeSubroutes;
  mutable std::mutex m_drapeSubroutesMutex;

  std::unique_ptr<location::GpsInfo> m_gpsInfoCache;

  TransitRouteInfo m_transitRouteInfo;

  struct RoutePointsTransaction
  {
    std::vector<RouteMarkData> m_routeMarks;
  };
  std::map<uint32_t, RoutePointsTransaction> m_routePointsTransactions;
  std::chrono::steady_clock::time_point m_loadRoutePointsTimestamp;
  std::map<std::string, m2::PointF> m_transitSymbolSizes;

  TransitReadManager * m_transitReadManager = nullptr;

  DECLARE_THREAD_CHECKER(m_threadChecker);
};
