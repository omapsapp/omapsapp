#pragma once

#include "geometry/point2d.hpp"
#include "geometry/latlon.hpp"

#include <string>
#include <vector>

class Framework;

namespace url_scheme
{
struct MapPoint
{
  double m_lat;
  double m_lon;
  std::string m_name;
  std::string m_id;
  std::string m_style;
};

struct RoutePoint
{
  RoutePoint() = default;
  RoutePoint(m2::PointD const & org, std::string const & name) : m_org(org), m_name(name) {}
  m2::PointD m_org = m2::PointD::Zero();
  std::string m_name;
};

struct SearchRequest
{
  std::string m_query;
  std::string m_locale;
  bool m_isSearchOnMap = false;
};

struct InAppFeatureHighlightRequest
{
  enum class InAppFeatureType
  {
    None = 0,
    TrackRecorder = 1,
    iCloud = 2,
  };

  InAppFeatureType m_feature = InAppFeatureType::None;
};

/// Handles [mapswithme|mwm|mapsme]://map|route|search?params - everything related to displaying info on a map
class ParsedMapApi
{
public:
  enum class UrlType
  {
    Incorrect = 0,
    Map = 1,
    Route = 2,
    Search = 3,
    Crosshair = 4,
    OAuth2 = 5,
    Menu = 6,
    Settings = 7
  };

  ParsedMapApi() = default;
  explicit ParsedMapApi(std::string const & url) { SetUrlAndParse(url); }

  UrlType SetUrlAndParse(std::string const & url);
  UrlType GetRequestType() const { return m_requestType; };
  std::string const & GetGlobalBackUrl() const { return m_globalBackUrl; }
  std::string const & GetAppName() const { return m_appName; }
  ms::LatLon GetCenterLatLon() const { return m_centerLatLon; }
  int GetApiVersion() const { return m_version; }
  void Reset();
  bool GoBackOnBalloonClick() const { return m_goBackOnBalloonClick; }

  void ExecuteMapApiRequest(Framework & fm) const;

  // Unit test only.
  std::vector<MapPoint> const & GetMapPoints() const
  {
    ASSERT_EQUAL(m_requestType, UrlType::Map, ("Expected Map API"));
    return m_mapPoints;
  }

  // Unit test only.
  double GetZoomLevel() const
  {
    ASSERT_EQUAL(m_requestType, UrlType::Map, ("Expected Map API"));
    return m_zoomLevel;
  }

  std::vector<RoutePoint> const & GetRoutePoints() const
  {
    ASSERT_EQUAL(m_requestType, UrlType::Route, ("Expected Route API"));
    return m_routePoints;
  }

  std::string const & GetRoutingType() const
  {
    ASSERT_EQUAL(m_requestType, UrlType::Route, ("Expected Route API"));
    return m_routingType;
  }

  SearchRequest const & GetSearchRequest() const
  {
    ASSERT_EQUAL(m_requestType, UrlType::Search, ("Expected Search API"));
    return m_searchRequest;
  }

  std::string const & GetOAuth2Code() const
  {
    ASSERT_EQUAL(m_requestType, UrlType::OAuth2, ("Expected OAuth2 API"));
    return m_oauth2code;
  }

  InAppFeatureHighlightRequest const & GetInAppFeatureHighlightRequest() const
  {
    ASSERT_EQUAL(m_requestType, UrlType::Menu, ("Expected Menu API"));
    ASSERT_EQUAL(m_requestType, UrlType::Settings, ("Expected Settings API"));
    return m_inAppFeatureHighlightRequest;
  }

private:
  void ParseMapParam(std::string const & key, std::string const & value,
                     bool & correctOrder);
  void ParseRouteParam(std::string const & key, std::string const & value,
                       std::vector<std::string_view> & pattern);
  void ParseSearchParam(std::string const & key, std::string const & value);
  void ParseInAppFeatureHighlightParam(std::string const & key, std::string const & value);
  void ParseCommonParam(std::string const & key, std::string const & value);

  UrlType m_requestType;
  std::vector<MapPoint> m_mapPoints;
  std::vector<RoutePoint> m_routePoints;
  SearchRequest m_searchRequest;
  InAppFeatureHighlightRequest m_inAppFeatureHighlightRequest;
  std::string m_globalBackUrl;
  std::string m_appName;
  std::string m_oauth2code;
  ms::LatLon m_centerLatLon = ms::LatLon::Invalid();
  std::string m_routingType;
  int m_version = 0;
  /// Zoom level in OSM format (e.g. from 1.0 to 20.0)
  /// Taken into an account when calculating viewport rect, but only if points count is == 1
  double m_zoomLevel = 0.0;
  bool m_goBackOnBalloonClick = false;
};

std::string DebugPrint(ParsedMapApi::UrlType type);
}  // namespace url_scheme
