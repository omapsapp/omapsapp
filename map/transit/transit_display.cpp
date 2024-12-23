#include "transit_display.hpp"

#include "drape_frontend/route_renderer.hpp"
#include "drape_frontend/visual_params.hpp"

#include "routing/routing_session.hpp"

#include <memory>

using namespace std;

map<TransitType, string> const kTransitSymbols = {
    {TransitType::Subway, "transit_subway"},
    {TransitType::LightRail, "transit_light_rail"},
    {TransitType::Monorail, "transit_monorail"},
    {TransitType::Train, "transit_train"},
    {TransitType::Tram, "transit_tram"},
    {TransitType::Bus, "transit_bus"},
    {TransitType::Ferry, "transit_ferry"},
    {TransitType::CableTram, "transit_cable_tram"},
    {TransitType::AerialLift, "transit_aerial_lift"},
    {TransitType::Funicular, "transit_funicular"},
    {TransitType::Trolleybus, "transit_trolleybus"},
    {TransitType::AirService, "transit_air_service"},
    {TransitType::WaterService, "transit_water_service"}};

namespace
{
float const kStopMarkerScale = 2.2f;
float const kTransferMarkerScale = 4.0f;
const float kGateBgScale = 1.2f;

const int kSmallIconZoom = 1;
const int kMediumIconZoom = 10;

const int kMinStopTitleZoom = 13;

const int kTransferTitleOffset = 2;
const int kStopTitleOffset = 0;
const int kGateTitleOffset = 0;

std::string const kZeroIcon = "zero-icon";

TransitType GetTransitType(string const &type)
{
  if (type == "subway")
    return TransitType::Subway;
  if (type == "light_rail")
    return TransitType::LightRail;
  if (type == "monorail")
    return TransitType::Monorail;
  if (type == "train" || type == "rail")
    return TransitType::Train;

  if (type == "tram")
    return TransitType::Tram;
  if (type == "bus")
    return TransitType::Bus;
  if (type == "ferry")
    return TransitType::Ferry;
  if (type == "cabletram")
    return TransitType::CableTram;
  if (type == "aerial_lift")
    return TransitType::AerialLift;
  if (type == "funicular")
    return TransitType::Funicular;
  if (type == "trolleybus")
    return TransitType::Trolleybus;
  if (type == "air_service")
    return TransitType::AirService;
  if (type == "water_service")
    return TransitType::WaterService;

  LOG(LERROR, (type));
  UNREACHABLE();
}

uint32_t ColorToARGB(df::ColorConstant const & colorConstant)
{
  auto const color = df::GetColorConstant(colorConstant);
  return color.GetAlpha() << 24 | color.GetRed() << 16 | color.GetGreen() << 8 | color.GetBlue();
}

vector<m2::PointF> GetTransitMarkerSizes(float markerScale, float maxRouteWidth)
{
  auto const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());
  vector<m2::PointF> markerSizes;
  markerSizes.reserve(df::kRouteHalfWidthInPixelTransit.size());
  for (auto const halfWidth : df::kRouteHalfWidthInPixelTransit)
  {
    float const d = 2.0f * min(halfWidth * vs, maxRouteWidth * 0.5f) * markerScale;
    markerSizes.push_back(m2::PointF(d, d));
  }
  return markerSizes;
}
}  // namespace

TransitStepInfo::TransitStepInfo(TransitType type, double distance, int time, string const & number,
                                 uint32_t color, int intermediateIndex)
  : m_type(type)
  , m_distanceInMeters(distance)
  , m_timeInSec(time)
  , m_number(number)
  , m_colorARGB(color)
  , m_intermediateIndex(intermediateIndex)
{}

bool TransitStepInfo::IsEqualType(TransitStepInfo const & ts) const
{
  if (m_type != ts.m_type)
    return false;

  if (m_type != TransitType::Pedestrian && m_type != TransitType::IntermediatePoint)
    return m_number == ts.m_number && m_colorARGB == ts.m_colorARGB;
  return true;
}

void TransitRouteInfo::AddStep(TransitStepInfo const & step)
{
  if (!m_steps.empty() && m_steps.back().IsEqualType(step))
  {
    m_steps.back().m_distanceInMeters += step.m_distanceInMeters;
    m_steps.back().m_timeInSec += step.m_timeInSec;
  }
  else
  {
    m_steps.push_back(step);
  }

  if (step.m_type == TransitType::Pedestrian)
  {
    m_totalPedestrianDistInMeters += step.m_distanceInMeters;
    m_totalPedestrianTimeInSec += step.m_timeInSec;
  }
}

void TransitRouteInfo::UpdateDistanceStrings()
{
  if (m_steps.empty())
    return;
  for (auto & step : m_steps)
  {
    routing::FormatDistance(step.m_distanceInMeters, step.m_distanceStr,
                            step.m_distanceUnitsSuffix);
  }
  routing::FormatDistance(m_totalDistInMeters, m_totalDistanceStr, m_totalDistanceUnitsSuffix);
  routing::FormatDistance(m_totalPedestrianDistInMeters, m_totalPedestrianDistanceStr,
                          m_totalPedestrianUnitsSuffix);
}

void AddTransitGateSegment(m2::PointD const & destPoint, df::ColorConstant const & color, df::Subroute & subroute)
{
  ASSERT_GREATER(subroute.m_polyline.GetSize(), 0, ());
  df::SubrouteStyle style(color, df::RoutePattern(4.0, 2.0));
  style.m_startIndex = subroute.m_polyline.GetSize() - 1;
  subroute.m_polyline.Add(destPoint);
  style.m_endIndex = subroute.m_polyline.GetSize() - 1;
  subroute.AddStyle(style);
}

void AddTransitPedestrianSegment(m2::PointD const & destPoint, df::Subroute & subroute)
{
  ASSERT_GREATER(subroute.m_polyline.GetSize(), 0, ());
  df::SubrouteStyle style(df::kRoutePedestrian, df::RoutePattern(4.0, 2.0));
  style.m_startIndex = subroute.m_polyline.GetSize() - 1;
  subroute.m_polyline.Add(destPoint);
  style.m_endIndex = subroute.m_polyline.GetSize() - 1;
  subroute.AddStyle(style);
}

void AddTransitShapes(vector<routing::transit::ShapeId> const & shapeIds,
                      TransitShapesInfo const & shapes, df::ColorConstant const & color,
                      bool isInverted, df::Subroute & subroute)
{
  ASSERT_GREATER(subroute.m_polyline.GetSize(), 0, ());
  df::SubrouteStyle style(color);
  style.m_startIndex = subroute.m_polyline.GetSize() - 1;

  for (auto it = shapeIds.crbegin(); it != shapeIds.crend(); ++it)
  {
    auto const & shapePolyline = shapes.at(*it).GetPolyline();
    if (isInverted)
      subroute.m_polyline.Append(shapePolyline.crbegin(), shapePolyline.crend());
    else
      subroute.m_polyline.Append(shapePolyline.cbegin(), shapePolyline.cend());
  }

  style.m_endIndex = subroute.m_polyline.GetSize() - 1;
  subroute.AddStyle(style);
}

void AddTransitShapes(::transit::ShapeLink shapeLink, TransitShapesInfoPT const & shapesInfo,
                      df::ColorConstant const & color, df::Subroute & subroute)
{
  ASSERT_GREATER(subroute.m_polyline.GetSize(), 0, ());
  df::SubrouteStyle style(color);
  style.m_startIndex = subroute.m_polyline.GetSize() - 1;

  bool const isInverted = shapeLink.m_startIndex > shapeLink.m_endIndex;

  auto const it = shapesInfo.find(shapeLink.m_shapeId);
  CHECK(it != shapesInfo.end(), (shapeLink.m_shapeId));

  size_t const startIdx = isInverted ? shapeLink.m_endIndex : shapeLink.m_startIndex;
  size_t const endIdx = isInverted ? shapeLink.m_startIndex : shapeLink.m_endIndex;

  auto const & edgePolyline = vector<m2::PointD>(it->second.GetPolyline().begin() + startIdx,
                                                 it->second.GetPolyline().begin() + endIdx + 1);

  if (isInverted)
    subroute.m_polyline.Append(edgePolyline.crbegin(), edgePolyline.crend());
  else
    subroute.m_polyline.Append(edgePolyline.cbegin(), edgePolyline.cend());

  style.m_endIndex = subroute.m_polyline.GetSize() - 1;
  subroute.AddStyle(style);
}

TransitRouteDisplay::TransitRouteDisplay(TransitReadManager & transitReadManager,
                                         GetMwmIdFn const & getMwmIdFn,
                                         GetStringsBundleFn const & getStringsBundleFn,
                                         BookmarkManager * bmManager,
                                         map<string, m2::PointF> const & transitSymbolSizes)
  : m_transitReadManager(transitReadManager)
  , m_getMwmIdFn(getMwmIdFn)
  , m_getStringsBundleFn(getStringsBundleFn)
  , m_bmManager(bmManager)
  , m_symbolSizes(transitSymbolSizes)
{
  float maxSymbolSize = -1.0f;
  for (auto const & symbolSize : m_symbolSizes)
  {
    if (maxSymbolSize < symbolSize.second.x)
      maxSymbolSize = symbolSize.second.x;
    if (maxSymbolSize < symbolSize.second.y)
      maxSymbolSize = symbolSize.second.y;
  }
  m_maxSubrouteWidth = maxSymbolSize * kGateBgScale / kStopMarkerScale;
}

TransitRouteInfo const & TransitRouteDisplay::GetRouteInfo()
{
  m_routeInfo.UpdateDistanceStrings();
  return m_routeInfo;
}

void TransitRouteDisplay::AddEdgeSubwayForSubroute(routing::RouteSegment const & segment,
                                                   df::Subroute & subroute, SubrouteParams & sp,
                                                   SubrouteSegmentParams & ssp)
{
  CHECK_EQUAL(ssp.m_displayInfo.m_transitVersion, ::transit::TransitVersion::OnlySubway, ());

  auto const & edge = ssp.m_transitInfo.GetEdgeSubway();

  auto const currentLineId = edge.m_lineId;

  auto const & line = ssp.m_displayInfo.m_linesSubway.at(currentLineId);
  auto const currentColor = df::GetTransitColorName(line.GetColor());
  sp.m_transitType = GetTransitType(line.GetType());

  m_routeInfo.AddStep(TransitStepInfo(sp.m_transitType, ssp.m_distance, ssp.m_time,
                                      line.GetNumber(), ColorToARGB(currentColor)));

  auto const & stop1 = ssp.m_displayInfo.m_stopsSubway.at(edge.m_stop1Id);
  auto const & stop2 = ssp.m_displayInfo.m_stopsSubway.at(edge.m_stop2Id);
  bool const isTransfer1 = stop1.GetTransferId() != routing::transit::kInvalidTransferId;
  bool const isTransfer2 = stop2.GetTransferId() != routing::transit::kInvalidTransferId;

  sp.m_marker.m_distance = sp.m_prevDistance;
  sp.m_marker.m_scale = kStopMarkerScale;
  sp.m_marker.m_innerColor = currentColor;
  if (isTransfer1)
  {
    auto const & transferData = ssp.m_displayInfo.m_transfersSubway.at(stop1.GetTransferId());
    sp.m_marker.m_position = transferData.GetPoint();
  }
  else
  {
    sp.m_marker.m_position = stop1.GetPoint();
  }
  sp.m_transitMarkInfo.m_point = sp.m_marker.m_position;

  if (sp.m_pendingEntrance)
  {
    sp.m_transitMarkInfo.m_type = TransitMarkInfo::Type::KeyStop;
    sp.m_transitMarkInfo.m_symbolName = kTransitSymbols.at(sp.m_transitType);
    sp.m_transitMarkInfo.m_color = currentColor;
    AddTransitGateSegment(sp.m_marker.m_position, currentColor, subroute);
    sp.m_pendingEntrance = false;
  }

  auto const id1 = isTransfer1 ? stop1.GetTransferId() : stop1.GetId();
  auto const id2 = isTransfer2 ? stop2.GetTransferId() : stop2.GetId();

  if (id1 != id2)
  {
    bool const isInverted = id1 > id2;
    AddTransitShapes(edge.m_shapeIds, ssp.m_displayInfo.m_shapesSubway, currentColor, isInverted,
                     subroute);
  }

  CHECK_GREATER(subroute.m_polyline.GetSize(), 1, ());
  auto const & p1 = *(subroute.m_polyline.End() - 2);
  auto const & p2 = *(subroute.m_polyline.End() - 1);
  m2::PointD currentDir = (p2 - p1).Normalize();

  if (sp.m_lastLineId != currentLineId)
  {
    if (sp.m_lastLineId != routing::transit::kInvalidLineId)
    {
      sp.m_marker.m_scale = kTransferMarkerScale;
      sp.m_transitMarkInfo.m_type = TransitMarkInfo::Type::Transfer;
    }
    sp.m_marker.m_colors.push_back(currentColor);

    if (stop1.GetFeatureId() != kInvalidFeatureId)
    {
      auto const fid = FeatureID(ssp.m_mwmId, stop1.GetFeatureId());
      sp.m_transitMarkInfo.m_featureId = fid;
      sp.m_transitMarkInfo.m_titles.emplace_back(ssp.m_displayInfo.m_features.at(fid).m_title,
                                                 df::GetTransitTextColorName(line.GetColor()));
    }
  }
  sp.m_lastColor = currentColor;
  sp.m_lastLineId = currentLineId;

  if (sp.m_marker.m_colors.size() > 1)
  {
    sp.m_marker.m_innerColor = df::kTransitStopInnerMarkerColor;
    sp.m_marker.m_up = (currentDir - sp.m_lastDir).Normalize();
    if (m2::CrossProduct(sp.m_marker.m_up, -sp.m_lastDir) < 0)
      sp.m_marker.m_up = -sp.m_marker.m_up;
  }

  subroute.m_markers.push_back(sp.m_marker);
  sp.m_marker = df::SubrouteMarker();

  m_transitMarks.push_back(sp.m_transitMarkInfo);
  sp.m_transitMarkInfo = TransitMarkInfo();

  sp.m_lastDir = currentDir;

  sp.m_marker.m_distance = segment.GetDistFromBeginningMeters();
  sp.m_marker.m_scale = kStopMarkerScale;
  sp.m_marker.m_innerColor = currentColor;
  sp.m_marker.m_colors.push_back(currentColor);

  if (isTransfer2)
  {
    auto const & transferData = ssp.m_displayInfo.m_transfersSubway.at(stop2.GetTransferId());
    sp.m_marker.m_position = transferData.GetPoint();
  }
  else
  {
    sp.m_marker.m_position = stop2.GetPoint();
  }

  sp.m_transitMarkInfo.m_point = sp.m_marker.m_position;
  if (stop2.GetFeatureId() != kInvalidFeatureId)
  {
    auto const fid = FeatureID(ssp.m_mwmId, stop2.GetFeatureId());
    sp.m_transitMarkInfo.m_featureId = fid;
    sp.m_transitMarkInfo.m_titles.push_back(
        TransitTitle(ssp.m_displayInfo.m_features.at(fid).m_title,
                     df::GetTransitTextColorName(line.GetColor())));
  }
}

void TransitRouteDisplay::AddEdgePTForSubroute(routing::RouteSegment const & segment,
                                               df::Subroute & subroute, SubrouteParams & sp,
                                               SubrouteSegmentParams & ssp)
{
  CHECK_EQUAL(ssp.m_displayInfo.m_transitVersion, ::transit::TransitVersion::AllPublicTransport,
              (segment.GetSegment()));

  auto const & edge = ssp.m_transitInfo.GetEdgePT();

  auto const currentLineId = edge.m_lineId;
  auto const & line = ssp.m_displayInfo.m_linesPT.at(currentLineId);

  auto const it = ssp.m_displayInfo.m_routesPT.find(line.GetRouteId());
  CHECK(it != ssp.m_displayInfo.m_routesPT.end(), (line.GetRouteId()));
  auto const & route = it->second;

  auto const currentColor = df::GetTransitColorName(route.GetColor());
  sp.m_transitType = GetTransitType(route.GetType());

  m_routeInfo.AddStep(TransitStepInfo(sp.m_transitType, ssp.m_distance, ssp.m_time,
                                      route.GetTitle(), ColorToARGB(currentColor)));

  auto const & stop1 = ssp.m_displayInfo.m_stopsPT.at(edge.m_stop1Id);
  auto const & stop2 = ssp.m_displayInfo.m_stopsPT.at(edge.m_stop2Id);

  bool const isTransfer1 = !stop1.GetTransferIds().empty();
  bool const isTransfer2 = !stop2.GetTransferIds().empty();

  sp.m_marker.m_distance = sp.m_prevDistance;
  sp.m_marker.m_scale = kStopMarkerScale;
  sp.m_marker.m_innerColor = currentColor;

  if (isTransfer1)
  {
    auto itTransfer = ssp.m_displayInfo.m_transfersPT.find(stop1.GetTransferIds().front());
    ASSERT(itTransfer != ssp.m_displayInfo.m_transfersPT.end(), ());
    sp.m_marker.m_position = itTransfer->second.GetPoint();
  }
  else
  {
    sp.m_marker.m_position = stop1.GetPoint();
  }

  sp.m_transitMarkInfo.m_point = sp.m_marker.m_position;

  if (sp.m_pendingEntrance)
  {
    sp.m_transitMarkInfo.m_type = TransitMarkInfo::Type::KeyStop;
    sp.m_transitMarkInfo.m_symbolName = kTransitSymbols.at(sp.m_transitType);
    sp.m_transitMarkInfo.m_color = currentColor;
    AddTransitGateSegment(sp.m_marker.m_position, currentColor, subroute);
    sp.m_pendingEntrance = false;
  }

  auto const id1 = isTransfer1 ? stop1.GetTransferIds().front() : stop1.GetId();
  auto const id2 = isTransfer2 ? stop2.GetTransferIds().front() : stop2.GetId();

  if (id1 != id2)
  {
    AddTransitShapes(edge.m_shapeLink, ssp.m_displayInfo.m_shapesPT, currentColor, subroute);
  }

  CHECK_GREATER(subroute.m_polyline.GetSize(), 1, ());
  auto const & p1 = *(subroute.m_polyline.End() - 2);
  auto const & p2 = *(subroute.m_polyline.End() - 1);
  m2::PointD currentDir = (p2 - p1).Normalize();

  if (sp.m_lastLineId != currentLineId)
  {
    if (sp.m_lastLineId != routing::transit::kInvalidLineId)
    {
      sp.m_marker.m_scale = kTransferMarkerScale;
      sp.m_transitMarkInfo.m_type = TransitMarkInfo::Type::Transfer;
    }
    sp.m_marker.m_colors.push_back(currentColor);

    if (stop1.GetFeatureId() != kInvalidFeatureId)
    {
      auto const fid = FeatureID(ssp.m_mwmId, stop1.GetFeatureId());
      sp.m_transitMarkInfo.m_featureId = fid;
      sp.m_transitMarkInfo.m_titles.emplace_back(ssp.m_displayInfo.m_features.at(fid).m_title,
                                                 df::GetTransitTextColorName(route.GetColor()));
    }
  }
  sp.m_lastColor = currentColor;
  sp.m_lastLineId = currentLineId;

  if (sp.m_marker.m_colors.size() > 1)
  {
    sp.m_marker.m_innerColor = df::kTransitStopInnerMarkerColor;
    sp.m_marker.m_up = (currentDir - sp.m_lastDir).Normalize();
    if (m2::CrossProduct(sp.m_marker.m_up, -sp.m_lastDir) < 0)
      sp.m_marker.m_up = -sp.m_marker.m_up;
  }

  subroute.m_markers.push_back(sp.m_marker);
  sp.m_marker = df::SubrouteMarker();

  m_transitMarks.push_back(sp.m_transitMarkInfo);
  sp.m_transitMarkInfo = TransitMarkInfo();

  sp.m_lastDir = currentDir;

  sp.m_marker.m_distance = segment.GetDistFromBeginningMeters();
  sp.m_marker.m_scale = kStopMarkerScale;
  sp.m_marker.m_innerColor = currentColor;
  sp.m_marker.m_colors.push_back(currentColor);

  if (isTransfer2)
  {
    auto itTransfer = ssp.m_displayInfo.m_transfersPT.find(stop2.GetTransferIds().front());
    ASSERT(itTransfer != ssp.m_displayInfo.m_transfersPT.end(), ());
    sp.m_marker.m_position = itTransfer->second.GetPoint();
  }
  else
  {
    sp.m_marker.m_position = stop2.GetPoint();
  }

  sp.m_transitMarkInfo.m_point = sp.m_marker.m_position;
  if (stop2.GetFeatureId() != kInvalidFeatureId)
  {
    auto const fid = FeatureID(ssp.m_mwmId, stop2.GetFeatureId());
    sp.m_transitMarkInfo.m_featureId = fid;
    sp.m_transitMarkInfo.m_titles.emplace_back(ssp.m_displayInfo.m_features.at(fid).m_title,
                                               df::GetTransitTextColorName(route.GetColor()));
  }
}

void TransitRouteDisplay::AddGateSubwayForSubroute(routing::RouteSegment const & segment,
                                                   df::Subroute & subroute, SubrouteParams & sp,
                                                   SubrouteSegmentParams & ssp)
{
  auto const & gate = ssp.m_transitInfo.GetGateSubway();
  if (sp.m_lastLineId != routing::transit::kInvalidLineId)
  {
    m_routeInfo.AddStep(TransitStepInfo(TransitType::Pedestrian, ssp.m_distance, ssp.m_time));

    AddTransitGateSegment(segment.GetJunction().GetPoint(), sp.m_lastColor, subroute);

    subroute.m_markers.push_back(sp.m_marker);
    sp.m_marker = df::SubrouteMarker();

    sp.m_transitMarkInfo.m_type = TransitMarkInfo::Type::KeyStop;
    sp.m_transitMarkInfo.m_symbolName = kTransitSymbols.at(sp.m_transitType);
    sp.m_transitMarkInfo.m_color = sp.m_lastColor;
    m_transitMarks.push_back(sp.m_transitMarkInfo);
    sp.m_transitMarkInfo = TransitMarkInfo();
  }
  else
  {
    sp.m_pendingEntrance = true;
  }

  auto gateMarkInfo = TransitMarkInfo();
  gateMarkInfo.m_point =
      sp.m_pendingEntrance ? subroute.m_polyline.Back() : segment.GetJunction().GetPoint();
  gateMarkInfo.m_type = TransitMarkInfo::Type::Gate;
  gateMarkInfo.m_symbolName = kZeroIcon;
  if (gate.m_featureId != kInvalidFeatureId)
  {
    auto const fid = FeatureID(ssp.m_mwmId, gate.m_featureId);
    auto const & featureInfo = ssp.m_displayInfo.m_features.at(fid);
    auto symbolName = featureInfo.m_gateSymbolName;
    if (symbolName.ends_with("-s") || symbolName.ends_with("-m") ||
        symbolName.ends_with("-l"))
    {
      symbolName = symbolName.substr(0, symbolName.rfind('-'));
    }

    gateMarkInfo.m_featureId = fid;
    if (!symbolName.empty())
      gateMarkInfo.m_symbolName = symbolName;
    auto const title = m_getStringsBundleFn().GetString(sp.m_pendingEntrance ? om::localization::kCoreEntrance
                                                                             : om::localization::kCoreExit);
    gateMarkInfo.m_titles.emplace_back(title, df::GetTransitTextColorName("default"));
  }

  m_transitMarks.push_back(gateMarkInfo);
}

void TransitRouteDisplay::AddGatePTForSubroute(routing::RouteSegment const & segment,
                                               df::Subroute & subroute, SubrouteParams & sp,
                                               SubrouteSegmentParams & ssp)
{
  auto const & gate = ssp.m_transitInfo.GetGatePT();
  if (sp.m_lastLineId != ::transit::kInvalidTransitId)
  {
    m_routeInfo.AddStep(TransitStepInfo(TransitType::Pedestrian, ssp.m_distance, ssp.m_time));

    AddTransitGateSegment(segment.GetJunction().GetPoint(), sp.m_lastColor, subroute);

    subroute.m_markers.push_back(sp.m_marker);
    sp.m_marker = df::SubrouteMarker();

    sp.m_transitMarkInfo.m_type = TransitMarkInfo::Type::KeyStop;
    sp.m_transitMarkInfo.m_symbolName = kTransitSymbols.at(sp.m_transitType);
    sp.m_transitMarkInfo.m_color = sp.m_lastColor;
    m_transitMarks.push_back(sp.m_transitMarkInfo);
    sp.m_transitMarkInfo = TransitMarkInfo();
  }
  else
  {
    sp.m_pendingEntrance = true;
  }

  auto gateMarkInfo = TransitMarkInfo();
  gateMarkInfo.m_point =
      sp.m_pendingEntrance ? subroute.m_polyline.Back() : segment.GetJunction().GetPoint();
  gateMarkInfo.m_type = TransitMarkInfo::Type::Gate;
  gateMarkInfo.m_symbolName = kZeroIcon;
  if (gate.m_featureId != kInvalidFeatureId)
  {
    auto const fid = FeatureID(ssp.m_mwmId, gate.m_featureId);
    auto const & featureInfo = ssp.m_displayInfo.m_features.at(fid);
    auto symbolName = featureInfo.m_gateSymbolName;
    if (symbolName.ends_with("-s") || symbolName.ends_with("-m") ||
        symbolName.ends_with("-l"))
    {
      symbolName = symbolName.substr(0, symbolName.rfind('-'));
    }

    gateMarkInfo.m_featureId = fid;
    if (!symbolName.empty())
      gateMarkInfo.m_symbolName = symbolName;
    auto const title = m_getStringsBundleFn().GetString(sp.m_pendingEntrance ? om::localization::kCoreEntrance
                                                                             : om::localization::kCoreExit);
    gateMarkInfo.m_titles.emplace_back(title, df::GetTransitTextColorName("default"));
  }

  m_transitMarks.push_back(gateMarkInfo);
}

bool TransitRouteDisplay::ProcessSubroute(vector<routing::RouteSegment> const & segments,
                                          df::Subroute & subroute)
{
  if (m_subrouteIndex > 0)
  {
    TransitStepInfo step;
    step.m_type = TransitType::IntermediatePoint;
    step.m_intermediateIndex = m_subrouteIndex - 1;
    m_routeInfo.AddStep(step);
  }

  ++m_subrouteIndex;

  TransitDisplayInfos transitDisplayInfos;
  CollectTransitDisplayInfo(segments, transitDisplayInfos);

  // Read transit display info.
  if (!m_transitReadManager.GetTransitDisplayInfo(transitDisplayInfos))
    return false;

  subroute.m_maxPixelWidth = m_maxSubrouteWidth;
  subroute.m_styleType = df::SubrouteStyleType::Multiple;
  subroute.m_style.clear();
  subroute.m_style.reserve(segments.size());

  SubrouteParams sp;
  sp.m_prevDistance = m_routeInfo.m_totalDistInMeters;
  sp.m_prevTime = m_routeInfo.m_totalTimeInSec;

  for (auto const & s : segments)
  {
    auto const time = static_cast<int>(ceil(s.GetTimeFromBeginningSec() - sp.m_prevTime));
    auto const distance = s.GetDistFromBeginningMeters() - sp.m_prevDistance;
    sp.m_prevDistance = s.GetDistFromBeginningMeters();
    sp.m_prevTime = s.GetTimeFromBeginningSec();

    if (!s.HasTransitInfo())
    {
      m_routeInfo.AddStep(TransitStepInfo(TransitType::Pedestrian, distance, time));

      AddTransitPedestrianSegment(s.GetJunction().GetPoint(), subroute);
      sp.m_lastColor = "";
      sp.m_lastLineId = routing::transit::kInvalidLineId;
      sp.m_transitType = TransitType::Pedestrian;
      continue;
    }

    SubrouteSegmentParams ssp(s.GetTransitInfo());

    ssp.m_time = time;
    ssp.m_distance = distance;
    ssp.m_mwmId = m_getMwmIdFn(s.GetSegment().GetMwmId());
    ssp.m_displayInfo = *transitDisplayInfos.at(ssp.m_mwmId).get();

    if (ssp.m_transitInfo.GetVersion() == ::transit::TransitVersion::OnlySubway)
    {
      if (ssp.m_transitInfo.GetType() == routing::TransitInfo::Type::Edge)
        AddEdgeSubwayForSubroute(s, subroute, sp, ssp);
      else if (ssp.m_transitInfo.GetType() == routing::TransitInfo::Type::Gate)
        AddGateSubwayForSubroute(s, subroute, sp, ssp);
    }
    else if (ssp.m_transitInfo.GetVersion() == ::transit::TransitVersion::AllPublicTransport)
    {
      if (ssp.m_transitInfo.GetType() == routing::TransitInfo::Type::Edge)
        AddEdgePTForSubroute(s, subroute, sp, ssp);
      else if (ssp.m_transitInfo.GetType() == routing::TransitInfo::Type::Gate)
        AddGatePTForSubroute(s, subroute, sp, ssp);
    }
    else
    {
      CHECK(false, (ssp.m_transitInfo.GetVersion()));
    }
  }

  m_routeInfo.m_totalDistInMeters = sp.m_prevDistance;
  m_routeInfo.m_totalTimeInSec = static_cast<int>(ceil(sp.m_prevTime));

  bool const isValidSubroute = subroute.m_polyline.GetSize() > 1;
  if (!isValidSubroute)
    LOG(LWARNING, ("Invalid subroute. Points number =", subroute.m_polyline.GetSize()));

  return isValidSubroute;
}

template <class T>
void FillMwmTransitSubway(unique_ptr<TransitDisplayInfo> & mwmTransit,
                          routing::TransitInfo const & transitInfo, T mwmId)
{
  switch (transitInfo.GetType())
  {
  case routing::TransitInfo::Type::Edge:
  {
    auto const & edge = transitInfo.GetEdgeSubway();

    mwmTransit->m_stopsSubway[edge.m_stop1Id] = {};
    mwmTransit->m_stopsSubway[edge.m_stop2Id] = {};
    mwmTransit->m_linesSubway[edge.m_lineId] = {};
    for (auto const & shapeId : edge.m_shapeIds)
      mwmTransit->m_shapesSubway[shapeId] = {};
    break;
  }
  case routing::TransitInfo::Type::Transfer:
  {
    auto const & transfer = transitInfo.GetTransferSubway();
    mwmTransit->m_stopsSubway[transfer.m_stop1Id] = {};
    mwmTransit->m_stopsSubway[transfer.m_stop2Id] = {};
    break;
  }
  case routing::TransitInfo::Type::Gate:
  {
    auto const & gate = transitInfo.GetGateSubway();
    if (gate.m_featureId != kInvalidFeatureId)
    {
      auto const featureId = FeatureID(mwmId, gate.m_featureId);
      TransitFeatureInfo featureInfo;
      featureInfo.m_isGate = true;
      mwmTransit->m_features[featureId] = featureInfo;
    }
    break;
  }
  }
}

template <class T>
void FillMwmTransitPT(unique_ptr<TransitDisplayInfo> & mwmTransit,
                      routing::TransitInfo const & transitInfo, T mwmId)
{
  switch (transitInfo.GetType())
  {
  case routing::TransitInfo::Type::Edge:
  {
    auto const & edge = transitInfo.GetEdgePT();

    mwmTransit->m_stopsPT[edge.m_stop1Id] = {};
    mwmTransit->m_stopsPT[edge.m_stop2Id] = {};
    mwmTransit->m_linesPT[edge.m_lineId] = {};
    mwmTransit->m_shapesPT[edge.m_shapeLink.m_shapeId] = {};
    break;
  }
  case routing::TransitInfo::Type::Transfer:
  {
    auto const & transfer = transitInfo.GetTransferPT();
    mwmTransit->m_stopsPT[transfer.m_stop1Id] = {};
    mwmTransit->m_stopsPT[transfer.m_stop2Id] = {};
    break;
  }
  case routing::TransitInfo::Type::Gate:
  {
    auto const & gate = transitInfo.GetGatePT();
    if (gate.m_featureId != kInvalidFeatureId)
    {
      auto const featureId = FeatureID(mwmId, gate.m_featureId);
      TransitFeatureInfo featureInfo;
      featureInfo.m_isGate = true;
      mwmTransit->m_features[featureId] = featureInfo;
    }
    break;
  }
  }
}

void TransitRouteDisplay::CollectTransitDisplayInfo(vector<routing::RouteSegment> const & segments,
                                                    TransitDisplayInfos & transitDisplayInfos)
{
  for (auto const & s : segments)
  {
    if (!s.HasTransitInfo())
      continue;

    auto const mwmId = m_getMwmIdFn(s.GetSegment().GetMwmId());

    auto & mwmTransit = transitDisplayInfos[mwmId];
    if (mwmTransit == nullptr)
      mwmTransit = make_unique<TransitDisplayInfo>();

    routing::TransitInfo const & transitInfo = s.GetTransitInfo();

    mwmTransit->m_transitVersion = transitInfo.GetVersion();

    if (transitInfo.GetVersion() == ::transit::TransitVersion::OnlySubway)
      FillMwmTransitSubway(mwmTransit, transitInfo, mwmId);
    else if (transitInfo.GetVersion() == ::transit::TransitVersion::AllPublicTransport)
      FillMwmTransitPT(mwmTransit, transitInfo, mwmId);
  }
}

TransitMark * TransitRouteDisplay::CreateMark(m2::PointD const & pt, FeatureID const & fid)
{
  uint32_t const nextIndex = static_cast<uint32_t>(m_bmManager->GetUserMarkIds(UserMark::Type::TRANSIT).size());

  auto editSession = m_bmManager->GetEditSession();
  auto * transitMark = editSession.CreateUserMark<TransitMark>(pt);
  transitMark->SetFeatureId(fid);
  transitMark->SetIndex(nextIndex);
  return transitMark;
}

void TransitRouteDisplay::CreateTransitMarks()
{
  if (m_transitMarks.empty())
    return;

  vector<m2::PointF> const transferMarkerSizes =
      GetTransitMarkerSizes(kTransferMarkerScale, m_maxSubrouteWidth);
  vector<m2::PointF> const stopMarkerSizes =
      GetTransitMarkerSizes(kStopMarkerScale, m_maxSubrouteWidth);

  vector<m2::PointF> transferArrowOffsets;
  transferArrowOffsets.reserve(transferMarkerSizes.size());
  for (auto const & size : transferMarkerSizes)
    transferArrowOffsets.emplace_back(0.0f, size.y * 0.5f);

  auto const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());

  auto editSession = m_bmManager->GetEditSession();
  for (auto const & mark : m_transitMarks)
  {
    auto transitMark = CreateMark(mark.m_point, mark.m_featureId);

    dp::TitleDecl titleDecl;
    if (mark.m_type == TransitMarkInfo::Type::Gate)
    {
      if (!mark.m_titles.empty())
      {
        TransitMark::GetDefaultTransitTitle(titleDecl);
        titleDecl.m_primaryText = mark.m_titles.front().m_text;
        titleDecl.m_primaryOffset.y = kGateTitleOffset;
        titleDecl.m_anchor = dp::Anchor::Top;
        titleDecl.m_primaryOptional = true;
        transitMark->AddTitle(titleDecl);
      }

      auto const GetSymbolName = [](std::string const & name, char const * suffix)
      {
        return name != kZeroIcon ? name + suffix : name;
      };
      df::UserPointMark::SymbolNameZoomInfo symbolNames;
      symbolNames[kSmallIconZoom] = GetSymbolName(mark.m_symbolName, "-s");
      symbolNames[kMediumIconZoom] = GetSymbolName(mark.m_symbolName, "-m");
      transitMark->SetSymbolNames(symbolNames);

      transitMark->SetPriority(UserMark::Priority::TransitGate);
    }
    else if (mark.m_type == TransitMarkInfo::Type::Transfer)
    {
      if (mark.m_titles.size() > 1)
      {
        auto titleTransitMark = CreateMark(mark.m_point, mark.m_featureId);

        TransitMark::GetDefaultTransitTitle(titleDecl);
        titleDecl.m_primaryText = mark.m_titles.front().m_text;
        titleDecl.m_primaryTextFont.m_color = df::GetColorConstant(mark.m_titles.front().m_color);
        titleDecl.m_primaryOffset.x = -kTransferTitleOffset * vs;
        titleDecl.m_anchor = dp::Anchor::Right;
        titleDecl.m_primaryOptional = false;
        titleTransitMark->AddTitle(titleDecl);

        titleDecl.m_primaryText = mark.m_titles.back().m_text;
        titleDecl.m_primaryTextFont.m_color = df::GetColorConstant(mark.m_titles.back().m_color);
        titleDecl.m_primaryOffset.x = kTransferTitleOffset * vs;
        titleDecl.m_anchor = dp::Anchor::Left;
        titleDecl.m_primaryOptional = false;
        titleTransitMark->AddTitle(titleDecl);

        titleTransitMark->SetAnchor(dp::Top);
        titleTransitMark->SetSymbolNames({{1 /* minZoom */, "transfer_arrow"}});
        titleTransitMark->SetSymbolOffsets(transferArrowOffsets);
        titleTransitMark->SetPriority(UserMark::Priority::TransitTransfer);
      }
      df::UserPointMark::ColoredSymbolZoomInfo coloredSymbol;
      for (size_t sizeIndex = 0; sizeIndex < transferMarkerSizes.size(); ++sizeIndex)
      {
        auto const zoomLevel = sizeIndex + 1;
        auto const & sz = transferMarkerSizes[sizeIndex];
        df::ColoredSymbolViewParams params;
        params.m_radiusInPixels = max(sz.x, sz.y) * 0.5f;
        params.m_color = dp::Color::Transparent();
        if (coloredSymbol.m_zoomInfo.empty() ||
            coloredSymbol.m_zoomInfo.rbegin()->second.m_radiusInPixels != params.m_radiusInPixels)
        {
          coloredSymbol.m_zoomInfo.insert(make_pair(zoomLevel, params));
        }
      }
      transitMark->SetColoredSymbols(coloredSymbol);
      transitMark->SetPriority(UserMark::Priority::TransitTransfer);
    }
    else
    {
      if (!mark.m_titles.empty())
      {
        TransitMark::GetDefaultTransitTitle(titleDecl);
        titleDecl.m_primaryText = mark.m_titles.front().m_text;
        titleDecl.m_primaryTextFont.m_color = df::GetColorConstant(mark.m_titles.front().m_color);
        titleDecl.m_primaryOffset.y = kStopTitleOffset;
        titleDecl.m_anchor = dp::Anchor::Top;
        titleDecl.m_primaryOptional = true;
        transitMark->AddTitle(titleDecl);
      }
      if (mark.m_type == TransitMarkInfo::Type::KeyStop)
      {
        df::UserPointMark::SymbolNameZoomInfo symbolNames;
        symbolNames[kSmallIconZoom] = mark.m_symbolName + "-s";
        symbolNames[kMediumIconZoom] = mark.m_symbolName + "-m";
        transitMark->SetSymbolNames(symbolNames);

        df::UserPointMark::ColoredSymbolZoomInfo coloredSymbol;
        df::ColoredSymbolViewParams params;
        params.m_color = df::GetColorConstant(mark.m_color);

        auto sz = m_symbolSizes.at(symbolNames[kSmallIconZoom]);
        params.m_radiusInPixels = max(sz.x, sz.y) * kGateBgScale * 0.5f;
        coloredSymbol.m_zoomInfo[kSmallIconZoom] = params;

        sz = m_symbolSizes.at(symbolNames[kMediumIconZoom]);
        params.m_radiusInPixels = max(sz.x, sz.y) * kGateBgScale * 0.5f;
        coloredSymbol.m_zoomInfo[kMediumIconZoom] = params;

        transitMark->SetColoredSymbols(coloredSymbol);
        transitMark->SetPriority(UserMark::Priority::TransitKeyStop);
      }
      else
      {
        df::UserPointMark::ColoredSymbolZoomInfo coloredSymbol;
        for (size_t sizeIndex = 0; sizeIndex < stopMarkerSizes.size(); ++sizeIndex)
        {
          auto const zoomLevel = sizeIndex + 1;
          auto const & sz = stopMarkerSizes[sizeIndex];
          df::ColoredSymbolViewParams params;
          params.m_radiusInPixels = max(sz.x, sz.y) * 0.5f;
          params.m_color = dp::Color::Transparent();
          if (coloredSymbol.m_zoomInfo.empty() ||
              coloredSymbol.m_zoomInfo.rbegin()->second.m_radiusInPixels != params.m_radiusInPixels)
          {
            coloredSymbol.m_zoomInfo.insert(make_pair(zoomLevel, params));
          }
        }
        transitMark->SetSymbolSizes(stopMarkerSizes);
        transitMark->SetColoredSymbols(coloredSymbol);
        transitMark->SetPriority(UserMark::Priority::TransitStop);
        transitMark->SetMinTitleZoom(kMinStopTitleZoom);
      }
    }
  }
}
