#include "map/mwm_url.hpp"

#include "map/api_mark_point.hpp"
#include "map/bookmark_manager.hpp"
#include "map/framework.hpp"

#include "ge0/parser.hpp"
#include "ge0/geo_url_parser.hpp"

#include "geometry/mercator.hpp"
#include "geometry/latlon.hpp"
#include "indexer/scales.hpp"

#include "drape_frontend/visual_params.hpp"

#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"

#include <array>
#include <tuple>

namespace url_scheme
{
namespace
{
std::string_view constexpr kCenterLatLon = "cll";
std::string_view constexpr kAppName = "appname";

namespace map
{
std::string_view constexpr kLatLon = "ll";
std::string_view constexpr kZoomLevel = "z";
std::string_view constexpr kName = "n";
std::string_view constexpr kId = "id";
std::string_view constexpr kStyle = "s";
std::string_view constexpr kBackUrl = "backurl";
std::string_view constexpr kVersion = "v";
std::string_view constexpr kBalloonAction = "balloonaction";
}  // namespace map

namespace route
{
std::string_view constexpr kSourceLatLon = "sll";
std::string_view constexpr kDestLatLon = "dll";
std::string_view constexpr kSourceName = "saddr";
std::string_view constexpr kDestName = "daddr";
std::string_view constexpr kRouteType = "type";
std::string_view constexpr kRouteTypeVehicle = "vehicle";
std::string_view constexpr kRouteTypePedestrian = "pedestrian";
std::string_view constexpr kRouteTypeBicycle = "bicycle";
std::string_view constexpr kRouteTypeTransit = "transit";
}  // namespace route

namespace search
{
std::string_view constexpr kQuery = "query";
std::string_view constexpr kLocale = "locale";
std::string_view constexpr kSearchOnMap = "map";
}  // namespace search

namespace highlight_feature
{
std::string_view constexpr kHighlight = "highlight";
}  // namespace feature highlight

// See also kGe0Prefixes in ge0/parser.hpp
constexpr std::array<std::string_view, 3> kLegacyMwmPrefixes = {{
  "mapsme://", "mwm://", "mapswithme://"
}};

bool ParseLatLon(std::string const & key, std::string const & value, double & lat, double & lon)
{
  size_t const firstComma = value.find(',');
  if (firstComma == std::string::npos)
  {
    LOG(LWARNING, ("Map API: no comma between lat and lon for key:", key, " value:", value));
    return false;
  }

  if (!strings::to_double(value.substr(0, firstComma), lat) ||
      !strings::to_double(value.substr(firstComma + 1), lon))
  {
    LOG(LWARNING, ("Map API: can't parse lat,lon for key:", key, " value:", value));
    return false;
  }

  if (!mercator::ValidLat(lat) || !mercator::ValidLon(lon))
  {
    LOG(LWARNING, ("Map API: incorrect value for lat and/or lon", key, value, lat, lon));
    return false;
  }
  return true;
}

// A helper for SetUrlAndParse() below.
// kGe0Prefixes supports API and ge0 links, kLegacyMwmPrefixes - only API.
std::tuple<size_t, bool> FindUrlPrefix(std::string const & url)
{
  for (auto const prefix : ge0::Ge0Parser::kGe0Prefixes)
  {
    if (url.starts_with(prefix))
      return {prefix.size(), true};
  }
  for (auto const prefix : kLegacyMwmPrefixes)
  {
    if (url.starts_with(prefix))
      return {prefix.size(), false};
  }
  return {std::string::npos, false};
}
}  // namespace

ParsedMapApi::UrlType ParsedMapApi::SetUrlAndParse(std::string const & raw)
{
  Reset();
  SCOPE_GUARD(guard, [this]
  {
    if (m_requestType == UrlType::Incorrect)
      Reset();
  });

  if (auto const [prefix, checkForGe0Link] = FindUrlPrefix(raw); prefix != std::string::npos)
  {
    url::Url const url {"om://" + raw.substr(prefix)};
    if (!url.IsValid())
      return m_requestType = UrlType::Incorrect;

    std::string const & type = url.GetHost();
    // The URL is prefixed by one of the kGe0Prefixes or kLegacyMwmPrefixes prefixes
    // => check for well-known API methods first.
    if (type == "map")
    {
      bool correctOrder = true;
      url.ForEachParam([&correctOrder, this](auto const & key, auto const & value)
      {
        ParseMapParam(key, value, correctOrder);
      });

      if (m_mapPoints.empty() || !correctOrder)
        return m_requestType = UrlType::Incorrect;

      return m_requestType = UrlType::Map;
    }
    else if (type == "route")
    {
      m_routePoints.clear();
      using namespace route;
      std::vector pattern{kSourceLatLon, kSourceName, kDestLatLon, kDestName, kRouteType};
      url.ForEachParam([&pattern, this](auto const & key, auto const & value)
      {
        ParseRouteParam(key, value, pattern);
      });

      if (pattern.size() != 0)
        return m_requestType = UrlType::Incorrect;

      if (m_routePoints.size() != 2)
        return m_requestType = UrlType::Incorrect;

      return m_requestType = UrlType::Route;
    }
    else if (type == "search")
    {
      url.ForEachParam([this](auto const & key, auto const & value)
      {
        ParseSearchParam(key, value);
      });
      if (m_searchRequest.m_query.empty())
        return m_requestType = UrlType::Incorrect;

      return m_requestType = UrlType::Search;
    }
    else if (type == "crosshair")
    {
      url.ForEachParam([this](auto const & key, auto const & value)
      {
         ParseCommonParam(key, value);
      });

      return m_requestType = UrlType::Crosshair;
    }
    else if (type == "menu")
    {
      url.ForEachParam([this](auto const & key, auto const & value)
      {
        ParseInAppFeatureHighlightParam(key, value);
      });
      return m_requestType = UrlType::Menu;
    }
    else if (type == "settings")
    {
      url.ForEachParam([this](auto const & key, auto const & value)
      {
        ParseInAppFeatureHighlightParam(key, value);
      });
      return m_requestType = UrlType::Settings;
    }
    else if (type == "oauth2")
    {
      if (url.GetPath() != "osm/callback")
        return m_requestType = UrlType::Incorrect;

      url.ForEachParam([this](auto const & key, auto const & value)
      {
        if (key == "code")
          m_oauth2code = value;
      });

      if (m_oauth2code.empty())
        return m_requestType = UrlType::Incorrect;
      else
        return m_requestType = UrlType::OAuth2;
    }
    else if (checkForGe0Link)
    {
      // The URL is prefixed by one of the kGe0Prefixes AND doesn't match any supported API call:
      // => try to decode ge0 short link.
      ge0::Ge0Parser::Result info;
      if (!ge0::Ge0Parser{}.ParseAfterPrefix(raw, prefix, info))
        return UrlType::Incorrect;

      m_zoomLevel = info.m_zoomLevel;
      m_mapPoints.push_back({info.m_lat, info.m_lon, info.m_name, "" /* m_id */, "" /* m_style */});

      return m_requestType = UrlType::Map;
    }
    else
    {
      // The URL is prefixed by one of the kLegacyPrefixes AND doesn't match any supported API call => error.
      LOG(LWARNING, ("Unsupported legacy API URL:", raw));
      return m_requestType = UrlType::Incorrect;
    }
  }
  else
  {
    // The URL is not prefixed by any special prefixes => try to parse for lat,lon and other geo parameters.
    geo::GeoURLInfo info;
    if (!geo::UnifiedParser().Parse(raw, info))
      return m_requestType = UrlType::Incorrect;

    if (!info.m_query.empty())
    {
      // The URL has "q=" parameter => convert to a Search API request.
      m_searchRequest = SearchRequest();
      m_searchRequest.m_query = info.m_query;
      m_centerLatLon = {info.m_lat, info.m_lon};
      m_zoomLevel = info.m_zoom;
      return m_requestType = UrlType::Search;
    }
    else
    {
     // The URL doesn't have "q=" parameter => convert to a Map API request.
      ASSERT(info.IsLatLonValid(), ());
      m_centerLatLon = {info.m_lat, info.m_lon};
      m_zoomLevel = info.m_zoom;
      m_mapPoints.push_back({info.m_lat, info.m_lon, info.m_label, "" /* m_id */, "" /* m_style */});
      return m_requestType = UrlType::Map;
    }
  }
  UNREACHABLE();
}

void ParsedMapApi::ParseMapParam(std::string const & key, std::string const & value, bool & correctOrder)
{
  using namespace map;

  if (key == kLatLon)
  {
    double lat = 0.0;
    double lon = 0.0;
    if (!ParseLatLon(key, value, lat, lon))
    {
      LOG(LWARNING, ("Incorrect 'll':", value));
      return;
    }

    m_mapPoints.push_back({lat, lon, "" /* m_name */, "" /* m_id */, "" /* m_style */});
  }
  else if (key == kZoomLevel)
  {
    double zoomLevel;
    if (strings::to_double(value, zoomLevel))
      m_zoomLevel = zoomLevel;
  }
  else if (key == kName)
  {
    if (!m_mapPoints.empty())
    {
      m_mapPoints.back().m_name = value;
    }
    else
    {
      LOG(LWARNING, ("Map API: Point name with no point. 'll' should come first!"));
      correctOrder = false;
      return;
    }
  }
  else if (key == kId)
  {
    if (!m_mapPoints.empty())
    {
      m_mapPoints.back().m_id = value;
    }
    else
    {
      LOG(LWARNING, ("Map API: Point url with no point. 'll' should come first!"));
      correctOrder = false;
      return;
    }
  }
  else if (key == kStyle)
  {
    if (!m_mapPoints.empty())
    {
      m_mapPoints.back().m_style = value;
    }
    else
    {
      LOG(LWARNING, ("Map API: Point style with no point. 'll' should come first!"));
      return;
    }
  }
  else if (key == kBackUrl)
  {
    // Fix missing :// in back url, it's important for iOS
    if (value.find("://") == std::string::npos)
      m_globalBackUrl = value + "://";
    else
      m_globalBackUrl = value;
  }
  else if (key == kVersion)
  {
    if (!strings::to_int(value, m_version))
      m_version = 0;
  }
  else if (key == kBalloonAction)
  {
    m_goBackOnBalloonClick = true;
  }
  else
  {
    ParseCommonParam(key, value);
  }
}

void ParsedMapApi::ParseRouteParam(std::string const & key, std::string const & value,
                                   std::vector<std::string_view> & pattern)
{
  using namespace route;

  if (pattern.empty() || key != pattern.front())
    return;

  if (key == kSourceLatLon || key == kDestLatLon)
  {
    double lat = 0.0;
    double lon = 0.0;
    if (!ParseLatLon(key, value, lat, lon))
    {
      LOG(LWARNING, ("Incorrect 'sll':", value));
      return;
    }

    RoutePoint p;
    p.m_org = mercator::FromLatLon(lat, lon);
    m_routePoints.push_back(p);
  }
  else if (key == kSourceName || key == kDestName)
  {
    m_routePoints.back().m_name = value;
  }
  else if (key == kRouteType)
  {
    std::string const lowerValue = strings::MakeLowerCase(value);
    if (lowerValue == kRouteTypePedestrian || lowerValue == kRouteTypeVehicle ||
        lowerValue == kRouteTypeBicycle || lowerValue == kRouteTypeTransit)
    {
      m_routingType = lowerValue;
    }
    else
    {
      LOG(LWARNING, ("Incorrect routing type:", value));
      return;
    }
  }

  pattern.erase(pattern.begin());
}

void ParsedMapApi::ParseSearchParam(std::string const & key, std::string const & value)
{
  using namespace search;

  if (key == kQuery)
    m_searchRequest.m_query = value;
  else if (key == kLocale)
    m_searchRequest.m_locale = value;
  else if (key == kSearchOnMap)
    m_searchRequest.m_isSearchOnMap = true;
  else
    ParseCommonParam(key, value);
}

void ParsedMapApi::ParseCommonParam(std::string const & key, std::string const & value)
{
  if (key == kAppName)
  {
    m_appName = value;
  }
  else if (key == kCenterLatLon)
  {
    if (double lat, lon; ParseLatLon(key, value, lat, lon))
      m_centerLatLon = {lat, lon};
    else
      LOG(LWARNING, ("Incorrect 'cll':", value));
  }
}

InAppFeatureHighlightRequest::InAppFeatureType ParseInAppFeatureType(std::string const & value)
{
  if (value == "track-recorder")
    return InAppFeatureHighlightRequest::InAppFeatureType::TrackRecorder;
  if (value == "icloud")
    return InAppFeatureHighlightRequest::InAppFeatureType::iCloud;
  LOG(LERROR, ("Incorrect InAppFeatureType:", value));
  return InAppFeatureHighlightRequest::InAppFeatureType::None;
}

void ParsedMapApi::ParseInAppFeatureHighlightParam(std::string const & key, std::string const & value)
{
  if (key == highlight_feature::kHighlight)
    m_inAppFeatureHighlightRequest.m_feature = ParseInAppFeatureType(value);
}


void ParsedMapApi::Reset()
{
  m_requestType = UrlType::Incorrect;
  m_mapPoints.clear();
  m_routePoints.clear();
  m_searchRequest = {};
  m_oauth2code = {};
  m_globalBackUrl ={};
  m_appName = {};
  m_centerLatLon = ms::LatLon::Invalid();
  m_routingType = {};
  m_version = 0;
  m_zoomLevel = 0.0;
  m_goBackOnBalloonClick = false;
  m_inAppFeatureHighlightRequest = {};
}

void ParsedMapApi::ExecuteMapApiRequest(Framework & fm) const
{
  ASSERT_EQUAL(m_requestType, UrlType::Map, ("Must be a Map API request"));
  VERIFY(m_mapPoints.size() > 0, ("Map API request must have at least one point"));

  // Clear every current API-mark.
  auto editSession = fm.GetBookmarkManager().GetEditSession();
  editSession.ClearGroup(UserMark::Type::API);
  editSession.SetIsVisible(UserMark::Type::API, true);

  // Add marks from the request.
  m2::RectD viewport;
  for (auto const & [lat, lon, name, id, style] : m_mapPoints)
  {
    m2::PointD const glPoint(mercator::FromLatLon(lat, lon));
    auto * mark = editSession.CreateUserMark<ApiMarkPoint>(glPoint);
    mark->SetName(name);
    mark->SetApiID(id);
    mark->SetStyle(style::GetSupportedStyle(style));
    viewport.Add(glPoint);
  }

  // Calculate the optimal viewport.
  VERIFY(viewport.IsValid(), ());
  m2::PointD const center = viewport.Center();

  // Calculate the best zoom.
  int zoomLevel;
  if (m_mapPoints.size() == 1)
  {
    if (m_zoomLevel >= 1.0)  // 0 means uninitialized/not passed to the API.
      zoomLevel = std::min(scales::GetUpperComfortScale(), static_cast<int>(m_zoomLevel));
    else
      zoomLevel = scales::GetUpperComfortScale();
  }
  else
  {
    zoomLevel = df::GetDrawTileScale(viewport);
  }

  // Always hide current map selection.
  fm.DeactivateMapSelection();

  // Set viewport and stop follow mode.
  fm.StopLocationFollow();

  // ShowRect function interferes with ActivateMapSelection and we have strange behaviour as a result.
  // Use more obvious SetModelViewCenter here.
  /// @todo Funny, but animation is still present, but now centering works fine.
  /// Looks like there is one more set viewport call somewhere.
  fm.SetViewportCenter(center, zoomLevel, false /* isAnim */, true /* trackVisibleViewport */);

  // Don't show the place page in case of multiple points.
  if (m_mapPoints.size() > 1)
    return;

  place_page::BuildInfo info;
  info.m_needAnimationOnSelection = false;
  info.m_mercator = mercator::FromLatLon(m_mapPoints[0].m_lat, m_mapPoints[0].m_lon);
  // Other details will be filled in by BuildPlacePageInfo().
  fm.BuildAndSetPlacePageInfo(info);
}

std::string DebugPrint(ParsedMapApi::UrlType type)
{
  switch(type)
  {
  case ParsedMapApi::UrlType::Incorrect: return "Incorrect";
  case ParsedMapApi::UrlType::Map: return "Map";
  case ParsedMapApi::UrlType::Route: return "Route";
  case ParsedMapApi::UrlType::Search: return "Search";
  case ParsedMapApi::UrlType::Crosshair: return "Crosshair";
  case ParsedMapApi::UrlType::OAuth2: return "OAuth2";
  case ParsedMapApi::UrlType::Menu: return "Menu";
  case ParsedMapApi::UrlType::Settings: return "Settings";
  }
  UNREACHABLE();
}
}  // namespace url_scheme
