#include "drape_frontend/apply_feature_functors.hpp"

#include "drape_frontend/area_shape.hpp"
#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/colored_symbol_shape.hpp"
#include "drape_frontend/line_shape.hpp"
#include "drape_frontend/path_symbol_shape.hpp"
#include "drape_frontend/path_text_shape.hpp"
#include "drape_frontend/poi_symbol_shape.hpp"
#include "drape_frontend/text_layout.hpp"
#include "drape_frontend/text_shape.hpp"
#include "drape_frontend/visual_params.hpp"

#include "editor/osm_editor.hpp"

#include "indexer/drules_include.hpp"
#include "indexer/feature_source.hpp"
#include "indexer/map_style_reader.hpp"
#include "indexer/road_shields_parser.hpp"

#include "geometry/clipping.hpp"
#include "geometry/mercator.hpp"
#include "geometry/smoothing.hpp"

#include "drape/color.hpp"
#include "drape/stipple_pen_resource.hpp"
#include "drape/texture_manager.hpp"
#include "drape/utils/projection.hpp"

#include "base/logging.hpp"
#include "base/small_map.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <mutex>

namespace df
{
dp::Color ToDrapeColor(uint32_t src)
{
  return dp::Color(src, static_cast<uint8_t>(255 - (src >> 24)));
}

namespace
{
double const kMinVisibleFontSize = 8.0;

df::ColorConstant const kPoiDeletedMaskColor = "PoiDeletedMask";
df::ColorConstant const kRoadShieldBlackTextColor = "RoadShieldBlackText";
df::ColorConstant const kRoadShieldWhiteTextColor = "RoadShieldWhiteText";
df::ColorConstant const kRoadShieldUKYellowTextColor = "RoadShieldUKYellowText";
df::ColorConstant const kRoadShieldGreenBackgroundColor = "RoadShieldGreenBackground";
df::ColorConstant const kRoadShieldBlueBackgroundColor = "RoadShieldBlueBackground";
df::ColorConstant const kRoadShieldRedBackgroundColor = "RoadShieldRedBackground";
df::ColorConstant const kRoadShieldOrangeBackgroundColor = "RoadShieldOrangeBackground";

uint32_t const kPathTextBaseTextIndex = 128;
uint32_t const kShieldBaseTextIndex = 0;

#ifdef LINES_GENERATION_CALC_FILTERED_POINTS
class LinesStat
{
public:
  ~LinesStat()
  {
    std::map<int, TValue> zoomValues;
    for (std::pair<TKey, TValue> const & f : m_features)
    {
      TValue & v = zoomValues[f.first.second];
      v.m_neededPoints += f.second.m_neededPoints;
      v.m_readPoints += f.second.m_readPoints;
    }

    LOG(LINFO, ("===== Lines filtering stats ====="));
    for (std::pair<int, TValue> const & v : zoomValues)
    {
      int const filtered = v.second.m_readPoints - v.second.m_neededPoints;
      LOG(LINFO, ("Zoom =", v.first, "Filtered", 100 * filtered / (double)v.second.m_readPoints, "% (",
                  filtered, "out of", v.second.m_readPoints, "points)"));
    }
  }

  static LinesStat & Get()
  {
    static LinesStat s_stat;
    return s_stat;
  }

  void InsertLine(FeatureID const & id, int scale, int vertexCount, int renderVertexCount)
  {
    TKey key(id, scale);
    std::lock_guard g(m_mutex);
    if (m_features.find(key) != m_features.end())
      return;

    TValue & v = m_features[key];
    v.m_readPoints = vertexCount;
    v.m_neededPoints = renderVertexCount;
  }

private:
  LinesStat() = default;

  using TKey = std::pair<FeatureID, int>;
  struct TValue
  {
    int m_readPoints = 0;
    int m_neededPoints = 0;
  };

  std::map<TKey, TValue> m_features;
  std::mutex m_mutex;
};
#endif

void ExtractLineParams(LineRuleProto const & lineRule, LineViewParams & params)
{
  double const scale = df::VisualParams::Instance().GetVisualScale();
  params.m_color = ToDrapeColor(lineRule.color());
  params.m_width = static_cast<float>(std::max(lineRule.width() * scale, 1.0));

  if (lineRule.has_dashdot())
  {
    DashDotProto const & dd = lineRule.dashdot();

    int const count = dd.dd_size();
    params.m_pattern.reserve(count);
    for (int i = 0; i < count; ++i)
      params.m_pattern.push_back(dp::PatternFloat2Pixel(dd.dd(i) * scale));
  }

  switch (lineRule.cap())
  {
  case ::ROUNDCAP : params.m_cap = dp::RoundCap;
    break;
  case ::BUTTCAP  : params.m_cap = dp::ButtCap;
    break;
  case ::SQUARECAP: params.m_cap = dp::SquareCap;
    break;
  default:
    CHECK(false, ());
  }

  switch (lineRule.join())
  {
  case ::NOJOIN    : params.m_join = dp::MiterJoin;
    break;
  case ::ROUNDJOIN : params.m_join = dp::RoundJoin;
    break;
  case ::BEVELJOIN : params.m_join = dp::BevelJoin;
    break;
  default:
    CHECK(false, ());
  }
}

void CaptionDefProtoToFontDecl(CaptionDefProto const * capRule, dp::FontDecl & params)
{
  double const vs = df::VisualParams::Instance().GetVisualScale();
  params.m_color = ToDrapeColor(capRule->color());
  params.m_size = static_cast<float>(std::max(kMinVisibleFontSize, capRule->height() * vs));

  if (capRule->stroke_color() != 0)
    params.m_outlineColor = ToDrapeColor(capRule->stroke_color());
}

void ShieldRuleProtoToFontDecl(ShieldRuleProto const * shieldRule, dp::FontDecl & params)
{
  double const vs = df::VisualParams::Instance().GetVisualScale();
  params.m_color = ToDrapeColor(shieldRule->text_color());
  params.m_size = static_cast<float>(std::max(kMinVisibleFontSize, shieldRule->height() * vs));
  if (shieldRule->text_stroke_color() != 0)
    params.m_outlineColor = ToDrapeColor(shieldRule->text_stroke_color());
}

dp::Anchor GetAnchor(int offsetX, int offsetY)
{
  if (offsetY != 0)
  {
    if (offsetY > 0)
      return dp::Top;
    else
      return dp::Bottom;
  }
  if (offsetX != 0)
  {
    if (offsetX > 0)
      return dp::Left;
    else
      return dp::Right;
  }
  return dp::Center;
}

m2::PointF GetOffset(int offsetX, int offsetY)
{
  double const vs = VisualParams::Instance().GetVisualScale();
  return { static_cast<float>(offsetX * vs), static_cast<float>(offsetY * vs) };
}

bool IsSymbolRoadShield(ftypes::RoadShield const & shield)
{
  return shield.m_type == ftypes::RoadShieldType::US_Interstate ||
         shield.m_type == ftypes::RoadShieldType::US_Highway;
}

std::string GetRoadShieldSymbolName(ftypes::RoadShield const & shield, double fontScale)
{
  ASSERT(IsSymbolRoadShield(shield), ());
  std::string result = "";
  if (shield.m_type == ftypes::RoadShieldType::US_Interstate)
    result = shield.m_name.size() <= 2 ? "shield-us-i-thin" : "shield-us-i-wide";
  else if (shield.m_type == ftypes::RoadShieldType::US_Highway)
    result = shield.m_name.size() <= 2 ? "shield-us-hw-thin" : "shield-us-hw-wide";
  else
  {
    ASSERT(false, ("This shield type doesn't support symbols:", shield.m_type));
  }

  if (fontScale > 1.0)
    result += "-scaled";

  return result;
}

bool IsColoredRoadShield(ftypes::RoadShield const & shield)
{
  return shield.m_type == ftypes::RoadShieldType::Default ||
         shield.m_type == ftypes::RoadShieldType::UK_Highway ||
         shield.m_type == ftypes::RoadShieldType::Generic_White ||
         shield.m_type == ftypes::RoadShieldType::Generic_Blue ||
         shield.m_type == ftypes::RoadShieldType::Generic_Green ||
         shield.m_type == ftypes::RoadShieldType::Generic_Red ||
         shield.m_type == ftypes::RoadShieldType::Generic_Orange;
}

void UpdateRoadShieldTextFont(dp::FontDecl & font, ftypes::RoadShield const & shield)
{
  font.m_outlineColor = dp::Color::Transparent();

  using ftypes::RoadShieldType;

  static base::SmallMapBase<RoadShieldType, df::ColorConstant> kColors = {
      {RoadShieldType::Generic_Green, kRoadShieldWhiteTextColor},
      {RoadShieldType::Generic_Blue, kRoadShieldWhiteTextColor},
      {RoadShieldType::UK_Highway, kRoadShieldUKYellowTextColor},
      {RoadShieldType::US_Interstate, kRoadShieldWhiteTextColor},
      {RoadShieldType::US_Highway, kRoadShieldBlackTextColor},
      {RoadShieldType::Generic_Red, kRoadShieldWhiteTextColor},
      {RoadShieldType::Generic_Orange, kRoadShieldBlackTextColor}
  };

  if (auto const * cl = kColors.Find(shield.m_type); cl)
    font.m_color = df::GetColorConstant(*cl);
}

dp::Color GetRoadShieldColor(dp::Color const & baseColor, ftypes::RoadShield const & shield)
{
  using ftypes::RoadShieldType;

  static base::SmallMapBase<ftypes::RoadShieldType, df::ColorConstant> kColors = {
      {RoadShieldType::Generic_Green, kRoadShieldGreenBackgroundColor},
      {RoadShieldType::Generic_Blue, kRoadShieldBlueBackgroundColor},
      {RoadShieldType::UK_Highway, kRoadShieldGreenBackgroundColor},
      {RoadShieldType::Generic_Red, kRoadShieldRedBackgroundColor},
      {RoadShieldType::Generic_Orange, kRoadShieldOrangeBackgroundColor}
  };

  if (auto const * cl = kColors.Find(shield.m_type); cl)
    return df::GetColorConstant(*cl);

  return baseColor;
}

float GetRoadShieldOutlineWidth(float baseWidth, ftypes::RoadShield const & shield)
{
  if (shield.m_type == ftypes::RoadShieldType::UK_Highway ||
      shield.m_type == ftypes::RoadShieldType::Generic_Blue ||
      shield.m_type == ftypes::RoadShieldType::Generic_Green ||
      shield.m_type == ftypes::RoadShieldType::Generic_Red)
    return 0.0f;

  return baseWidth;
}

dp::Anchor GetShieldAnchor(uint8_t shieldIndex, uint8_t shieldCount)
{
  switch (shieldCount)
  {
    case 2:
      if (shieldIndex == 0) return dp::Bottom;
      return dp::Top;
    case 3:
      if (shieldIndex == 0) return dp::RightBottom;
      else if (shieldIndex == 1) return dp::LeftBottom;
      return dp::Top;
    case 4:
      if (shieldIndex == 0) return dp::RightBottom;
      else if (shieldIndex == 1) return dp::LeftBottom;
      else if (shieldIndex == 2) return dp::RightTop;
      return dp::LeftTop;
  }
  // A single shield.
  return dp::Center;
}

m2::PointF GetShieldOffset(dp::Anchor anchor, double paddingWidth, double paddingHeight)
{
  m2::PointF offset(0.0f, 0.0f);
  if (anchor & dp::Left)
    offset.x = static_cast<float>(paddingWidth);
  else if (anchor & dp::Right)
    offset.x = -static_cast<float>(paddingWidth);

  if (anchor & dp::Top)
    offset.y = static_cast<float>(paddingHeight);
  else if (anchor & dp::Bottom)
    offset.y = -static_cast<float>(paddingHeight);
  return offset;
}

void CalculateRoadShieldPositions(std::vector<double> const & offsets,
                                  m2::SharedSpline const & spline,
                                  std::vector<m2::PointD> & shieldPositions)
{
  ASSERT(!offsets.empty(), ());
  if (offsets.size() == 1)
  {
    shieldPositions.push_back(spline->GetPoint(0.5 * spline->GetLength()).m_pos);
  }
  else
  {
    for (size_t i = 0; i + 1 < offsets.size(); i++)
    {
      double const p = 0.5 * (offsets[i] + offsets[i + 1]);
      shieldPositions.push_back(spline->GetPoint(p).m_pos);
    }
  }
}
}  // namespace

BaseApplyFeature::BaseApplyFeature(TileKey const & tileKey, TInsertShapeFn const & insertShape,
                                   FeatureType & f, CaptionDescription const & captions)
  : m_insertShape(insertShape)
  , m_f(f)
  , m_captions(captions)
  , m_tileKey(tileKey)
  , m_tileRect(tileKey.GetGlobalRect())
{
  ASSERT(m_insertShape != nullptr, ());
}

void BaseApplyFeature::FillCommonParams(CommonOverlayViewParams & p) const
{
  p.m_rank = m_f.GetRank();
  p.m_tileCenter = m_tileRect.Center();
  p.m_featureId = m_f.GetID();
}

void ApplyPointFeature::ExtractCaptionParams(CaptionDefProto const * primaryProto,
                                             CaptionDefProto const * secondaryProto,
                                             TextViewParams & params) const
{
  FillCommonParams(params);
  params.m_depthLayer = DepthLayer::OverlayLayer;
  params.m_depthTestEnabled = false;
  params.m_posZ = m_posZ;

  auto & titleDecl = params.m_titleDecl;

  dp::FontDecl decl;
  CaptionDefProtoToFontDecl(primaryProto, decl);
  titleDecl.m_primaryTextFont = decl;
  titleDecl.m_anchor = GetAnchor(primaryProto->offset_x(), primaryProto->offset_y());
  // TODO(pastk) : remove offsets processing as de-facto "text-offset: *" is used to define anchors only.
  titleDecl.m_primaryOffset = GetOffset(primaryProto->offset_x(), primaryProto->offset_y());
  titleDecl.m_primaryOptional = primaryProto->is_optional();

  if (!titleDecl.m_secondaryText.empty())
  {
    ASSERT(secondaryProto != nullptr, ());
    dp::FontDecl auxDecl;
    CaptionDefProtoToFontDecl(secondaryProto, auxDecl);
    titleDecl.m_secondaryTextFont = auxDecl;
    // Secondary captions are optional always.
    titleDecl.m_secondaryOptional = true;
  }
}

double BaseApplyFeature::PriorityToDepth(int priority, drule::TypeT ruleType, double areaDepth) const
{
  double depth = priority;

  if (ruleType == drule::area || ruleType == drule::line)
  {
    if (depth < drule::kBasePriorityBgTop)
    {
      ASSERT(ruleType == drule::area, (m_f.DebugString()));
      ASSERT_GREATER_OR_EQUAL(depth, drule::kBasePriorityBgBySize, (m_f.DebugString()));
      // Prioritize BG-by-size areas by their bbox sizes instead of style-set priorities.
      ASSERT_GREATER_OR_EQUAL(areaDepth, drule::kBaseDepthBgBySize, (m_f.DebugString()));
      depth = areaDepth;
    }
    else if (depth < drule::kBasePriorityFg)
    {
      // Adjust BG-top features depth range so that it sits just above the BG-by-size range.
      depth = drule::kBaseDepthBgTop + (depth - drule::kBasePriorityBgTop) * drule::kBgTopRangeFraction;
    }

    // Shift the depth according to layer=* value.
    int8_t const layer = m_f.GetLayer();
    if (layer != feature::LAYER_EMPTY)
      depth += layer * drule::kLayerPriorityRange;
  }
  else
  {
    // Note we don't adjust priorities of "point-styles" according to layer=*,
    // because their priorities are used for displacement logic only.
    /// @todo(pastk) we might want to hide e.g. a trash bin under man_made=bridge or a bench on underground railway station?

    // Check overlays priorities range.
    ASSERT(-drule::kOverlaysMaxPriority <= depth && depth < drule::kOverlaysMaxPriority, (depth, m_f.DebugString()));
  }

  // Check no features are clipped by the depth range constraints.
  ASSERT(dp::kMinDepth <= depth && depth <= dp::kMaxDepth, (depth, m_f.DebugString()));
  return depth;
}

ApplyPointFeature::ApplyPointFeature(TileKey const & tileKey, TInsertShapeFn const & insertShape, FeatureType & f,
                                     CaptionDescription const & captions)
  : TBase(tileKey, insertShape, f, captions)
{}

void ApplyPointFeature::ProcessPointRules(SymbolRuleProto const * symbolRule, CaptionRuleProto const * captionRule,
                                          CaptionRuleProto const * houseNumberRule, m2::PointD const & centerPoint,
                                          ref_ptr<dp::TextureManager> texMng)
{
  // TODO: This is only one place of cross-dependency with Editor.
  auto const & editor = osm::Editor::Instance();
  auto const featureStatus = editor.GetFeatureStatus(m_f.GetID());
  bool const createdByEditor = featureStatus == FeatureStatus::Created;
  bool const obsoleteInEditor = featureStatus == FeatureStatus::Obsolete;

  m2::PointF symbolSize(0.0f, 0.0f);

  if (symbolRule)
  {
    PoiSymbolViewParams params;
    FillCommonParams(params);
    params.m_depthLayer = DepthLayer::OverlayLayer;
    params.m_depthTestEnabled = false;
    params.m_depth = PriorityToDepth(symbolRule->priority(), drule::symbol, 0);
    params.m_symbolName = symbolRule->name();
    ASSERT_GREATER_OR_EQUAL(symbolRule->min_distance(), 0, ());
    auto const & vp = df::VisualParams::Instance();
    params.m_extendingSize = static_cast<uint32_t>(vp.GetVisualScale() * symbolRule->min_distance() *
                                                   vp.GetPoiExtendScale());
    params.m_posZ = m_posZ;
    params.m_hasArea = HasArea();
    params.m_prioritized = createdByEditor;
    if (obsoleteInEditor)
      params.m_maskColor = kPoiDeletedMaskColor;

    dp::TextureManager::SymbolRegion region;
    texMng->GetSymbolRegion(params.m_symbolName, region);
    symbolSize = region.GetPixelSize();

    if (region.IsValid())
      m_insertShape(make_unique_dp<PoiSymbolShape>(centerPoint, params, m_tileKey, 0 /* textIndex */));
    else
      LOG(LERROR, ("Style error. Symbol name must be valid for feature", m_f.GetID()));
  }

  if (captionRule)
  {
    TextViewParams params;
    CaptionDefProto const * capRule = &captionRule->primary();
    CaptionDefProto const * auxRule = captionRule->has_secondary() ? &captionRule->secondary() : nullptr;

    params.m_titleDecl.m_primaryText = m_captions.GetMainText();
    if (auxRule)
      params.m_titleDecl.m_secondaryText = m_captions.GetAuxText();
    ASSERT(!params.m_titleDecl.m_primaryText.empty(), ());

    ExtractCaptionParams(capRule, auxRule, params);
    params.m_depth = PriorityToDepth(captionRule->priority(), drule::caption, 0);
    params.m_hasArea = HasArea();
    params.m_createdByEditor = createdByEditor;

    ASSERT(!(symbolRule && params.m_titleDecl.m_anchor == dp::Anchor::Center),
           ("A `text-offset: *` is not set in styles.", m_f.GetID(), m_f.DebugString()));
    if (houseNumberRule && params.m_titleDecl.m_anchor == dp::Anchor::Center)
      params.m_titleDecl.m_anchor = GetAnchor(0, 1);

    params.m_startOverlayRank = symbolRule ? dp::OverlayRank1 : dp::OverlayRank0;
    auto shape = make_unique_dp<TextShape>(centerPoint, params, m_tileKey, symbolSize,
                                           m2::PointF(0.0f, 0.0f) /* symbolOffset */,
                                           dp::Center /* symbolAnchor */, 0 /* textIndex */);
    m_insertShape(std::move(shape));
  }

  if (houseNumberRule)
  {
    TextViewParams params;
    CaptionDefProto const * capRule = &houseNumberRule->primary();

    params.m_titleDecl.m_primaryText = m_captions.GetHouseNumberText();
    ASSERT(!params.m_titleDecl.m_primaryText.empty(), ());

    ExtractCaptionParams(capRule, nullptr, params);
    params.m_depth = PriorityToDepth(houseNumberRule->priority(), drule::caption, 0);
    params.m_hasArea = HasArea();
    params.m_createdByEditor = createdByEditor;

    // If icon or main text exists then put housenumber above them.
    if (symbolRule || captionRule)
    {
      params.m_titleDecl.m_anchor = GetAnchor(0, -1);
      if (symbolRule)
      {
        params.m_titleDecl.m_primaryOptional = true;
        params.m_startOverlayRank = dp::OverlayRank1;
      }
    }
    m_insertShape(make_unique_dp<TextShape>(centerPoint, params, m_tileKey, symbolSize,
                                            m2::PointF(0.0f, 0.0f) /* symbolOffset */,
                                            dp::Center /* symbolAnchor */, 0 /* textIndex */));
  }
}

ApplyAreaFeature::ApplyAreaFeature(TileKey const & tileKey, TInsertShapeFn const & insertShape,
                                   FeatureType & f, double currentScaleGtoP, bool isBuilding,
                                   float minPosZ, float posZ,
                                   CaptionDescription const & captions)
  : TBase(tileKey, insertShape, f, captions)
  , m_minPosZ(minPosZ)
  , m_isBuilding(isBuilding)
  , m_currentScaleGtoP(currentScaleGtoP)
{
  m_posZ = posZ;
}

void ApplyAreaFeature::operator()(m2::PointD const & p1, m2::PointD const & p2, m2::PointD const & p3)
{
  if (m_isBuilding)
  {
    /// @todo I suppose that we don't intersect triangles with tile rect because of _simple_
    /// 3D and outline algo. It makes sense only if buildings have _not many_ triangles.
    ProcessBuildingPolygon(p1, p2, p3);
    return;
  }

  m2::PointD const v1 = p2 - p1;
  m2::PointD const v2 = p3 - p1;
  //TODO(pastk) : degenerate triangles filtering should be done in the generator.
  // ASSERT(!v1.IsAlmostZero() && !v2.IsAlmostZero(), ());
  if (v1.IsAlmostZero() || v2.IsAlmostZero())
    return;

  double const crossProduct = m2::CrossProduct(v1.Normalize(), v2.Normalize());
  double constexpr kEps = 1e-7;
  // ASSERT_GREATER_OR_EQUAL(fabs(crossProduct), kEps, (fabs(crossProduct), p1, p2, p3, m_f.DebugString()));
  // TODO(pastk) : e.g. a landuse-meadow has a following triangle with two identical points:
  // m2::Point<d>(8.5829683287662987823, 53.929641499591184584)
  // m2::Point<d>(8.5830675705005887721, 53.930025055483156393)
  // m2::Point<d>(8.5830675705005887721, 53.930025055483156393)
  if (fabs(crossProduct) < kEps)
    return;

  auto const clipFunctor = [this](m2::PointD const & p1, m2::PointD const & p2, m2::PointD const & p3)
  {
    m_triangles.push_back(p1);
    m_triangles.push_back(p2);
    m_triangles.push_back(p3);
  };

  if (crossProduct < 0)
    m2::ClipTriangleByRect(m_tileRect, p1, p2, p3, clipFunctor);
  else
    m2::ClipTriangleByRect(m_tileRect, p1, p3, p2, clipFunctor);
}

void ApplyAreaFeature::ProcessBuildingPolygon(m2::PointD const & p1, m2::PointD const & p2,
                                              m2::PointD const & p3)
{
  // For building we must filter degenerate polygons because now we have to reconstruct
  // building outline by bunch of polygons.
  // TODO(pastk) : filter degenerates in the generator.(see a TODO above).
  m2::PointD const v1 = p2 - p1;
  m2::PointD const v2 = p3 - p1;
  if (v1.IsAlmostZero() || v2.IsAlmostZero())
    return;

  double const crossProduct = m2::CrossProduct(v1.Normalize(), v2.Normalize());
  double constexpr kEps = 0.01;
  if (fabs(crossProduct) < kEps)
    return;

  // Triangles near to degenerate are drawn two-side, because we can't strictly determine
  // vertex traversal direction.
  double constexpr kTwoSideEps = 0.05;
  bool const isTwoSide = fabs(crossProduct) < kTwoSideEps;

  auto const i1 = GetIndex(p1);
  auto const i2 = GetIndex(p2);
  auto const i3 = GetIndex(p3);
  if (crossProduct < 0)
  {
    m_triangles.push_back(p1);
    m_triangles.push_back(p2);
    m_triangles.push_back(p3);
    BuildEdges(i1, i2, i3, isTwoSide);
  }
  else
  {
    m_triangles.push_back(p1);
    m_triangles.push_back(p3);
    m_triangles.push_back(p2);
    BuildEdges(i1, i3, i2, isTwoSide);
  }
}

int ApplyAreaFeature::GetIndex(m2::PointD const & pt)
{
  for (size_t i = 0; i < m_points.size(); i++)
  {
    // TODO(pastk) : should be possible to use exact match.
    if (pt.EqualDxDy(m_points[i], mercator::kPointEqualityEps))
      return static_cast<int>(i);
  }
  m_points.push_back(pt);
  return static_cast<int>(m_points.size()) - 1;
}

bool ApplyAreaFeature::IsDuplicatedEdge(Edge const & edge)
{
  for (auto & e : m_edges)
  {
    if (e.m_edge == edge)
    {
      e.m_internalVertexIndex = -1;
      return true;
    }
  }
  return false;
}

m2::PointD ApplyAreaFeature::CalculateNormal(m2::PointD const & p1, m2::PointD const & p2,
                                             m2::PointD const & p3) const
{
  m2::PointD const tangent = (p2 - p1).Normalize();
  m2::PointD normal = m2::PointD(-tangent.y, tangent.x);
  m2::PointD const v = ((p1 + p2) * 0.5 - p3).Normalize();
  if (m2::DotProduct(normal, v) < 0.0)
    normal = -normal;

  return normal;
}

void ApplyAreaFeature::BuildEdges(int vertexIndex1, int vertexIndex2, int vertexIndex3, bool twoSide)
{
  // Check if triangle is degenerate.
  // TODO(pastk) : filter degenerates in the generator.
  if (vertexIndex1 == vertexIndex2 || vertexIndex2 == vertexIndex3 || vertexIndex1 == vertexIndex3)
    return;

  auto edge1 = Edge(vertexIndex1, vertexIndex2);
  if (!IsDuplicatedEdge(edge1))
    m_edges.emplace_back(std::move(edge1), vertexIndex3, twoSide);

  auto edge2 = Edge(vertexIndex2, vertexIndex3);
  if (!IsDuplicatedEdge(edge2))
    m_edges.emplace_back(std::move(edge2), vertexIndex1, twoSide);

  auto edge3 = Edge(vertexIndex3, vertexIndex1);
  if (!IsDuplicatedEdge(edge3))
    m_edges.emplace_back(std::move(edge3), vertexIndex2, twoSide);
}

void ApplyAreaFeature::CalculateBuildingOutline(bool calculateNormals, BuildingOutline & outline)
{
  // Make sure that you called this fuction once! per feature.
  outline.m_vertices = std::move(m_points);
  outline.m_indices.reserve(m_edges.size() * 2);
  if (calculateNormals)
    outline.m_normals.reserve(m_edges.size());

  for (auto const & e : m_edges)
  {
    if (e.m_internalVertexIndex < 0)
      continue;

    outline.m_indices.push_back(e.m_edge.m_startIndex);
    outline.m_indices.push_back(e.m_edge.m_endIndex);
    if (e.m_twoSide)
    {
      outline.m_indices.push_back(e.m_edge.m_endIndex);
      outline.m_indices.push_back(e.m_edge.m_startIndex);
    }

    if (calculateNormals)
    {
      outline.m_normals.emplace_back(CalculateNormal(outline.m_vertices[e.m_edge.m_startIndex],
                                                     outline.m_vertices[e.m_edge.m_endIndex],
                                                     outline.m_vertices[e.m_internalVertexIndex]));
      if (e.m_twoSide)
        outline.m_normals.push_back(outline.m_normals.back());
    }
  }
}

void ApplyAreaFeature::ProcessAreaRules(AreaRuleProto const * areaRule, AreaRuleProto const * hatchingRule)
{
  ASSERT(areaRule || hatchingRule, ());
  ASSERT(HasGeometry(), ());

  double areaDepth = drule::kBaseDepthBgBySize - 1;

  if (hatchingRule)
  {
    ASSERT_GREATER_OR_EQUAL(hatchingRule->priority(), drule::kBasePriorityFg, (m_f.DebugString()));
    ProcessRule(*hatchingRule, areaDepth, true /* isHatching */);
  }

  if (areaRule)
  {
    // Calculate areaDepth for BG-by-size areas only.
    if (areaRule->priority() < drule::kBasePriorityBgTop)
      areaDepth = drule::CalcAreaBySizeDepth(m_f);
    ProcessRule(*areaRule, areaDepth, false /* isHatching */);
  }
}

void ApplyAreaFeature::ProcessRule(AreaRuleProto const & areaRule, double areaDepth, bool isHatching)
{
  AreaViewParams params;
  params.m_tileCenter = m_tileRect.Center();
  params.m_depth = PriorityToDepth(areaRule.priority(), drule::area, areaDepth);
  params.m_color = ToDrapeColor(areaRule.color());
  params.m_rank = m_f.GetRank();
  params.m_minPosZ = m_minPosZ;
  params.m_posZ = m_posZ;
  params.m_hatching = isHatching;
  params.m_baseGtoPScale = m_currentScaleGtoP;

  BuildingOutline outline;
  if (m_isBuilding && !isHatching)
  {
    /// @todo Make borders work for non-building areas too.
    outline.m_generateOutline = areaRule.has_border() &&
                                areaRule.color() != areaRule.border().color() &&
                                areaRule.border().width() > 0.0;
    if (outline.m_generateOutline)
      params.m_outlineColor = ToDrapeColor(areaRule.border().color());

    bool const calculateNormals = m_posZ > 0.0;
    if (calculateNormals || outline.m_generateOutline)
      CalculateBuildingOutline(calculateNormals, outline);

    params.m_is3D = !outline.m_indices.empty() && calculateNormals;
  }

  // see ProcessAreaRules: isHatching first, - !isHatching last.
  m_insertShape(make_unique_dp<AreaShape>(!isHatching ? std::move(m_triangles) : m_triangles,
                                          std::move(outline), params));
}

ApplyLineFeatureGeometry::ApplyLineFeatureGeometry(TileKey const & tileKey, TInsertShapeFn const & insertShape,
                                                   FeatureType & f, double currentScaleGtoP)
  : TBase(tileKey, insertShape, f, CaptionDescription())
  , m_currentScaleGtoP(currentScaleGtoP)
  // TODO(pastk) : calculate just once in the RuleDrawer.
  , m_minSegmentSqrLength(math::Pow2(4.0 * df::VisualParams::Instance().GetVisualScale() / currentScaleGtoP))
  , m_simplify(tileKey.m_zoomLevel >= 10 && tileKey.m_zoomLevel <= 12)
{
  m_spline.Reset(new m2::Spline(f.GetPointsCount()));
}

void ApplyLineFeatureGeometry::operator() (m2::PointD const & point)
{
#ifdef LINES_GENERATION_CALC_FILTERED_POINTS
  ++m_readCount;
#endif

  if (m_spline->IsEmpty())
  {
    m_spline->AddPoint(point);
    m_lastAddedPoint = point;
  }
  else
  {
    if (m_simplify &&
        ((m_spline->GetSize() > 1 && point.SquaredLength(m_lastAddedPoint) < m_minSegmentSqrLength) ||
          m_spline->IsProlonging(point)))
    {
      m_spline->ReplacePoint(point);
    }
    else
    {
      m_spline->AddPoint(point);
      m_lastAddedPoint = point;
    }
  }
}

void ApplyLineFeatureGeometry::ProcessLineRules(Stylist::LineRulesT const & lineRules)
{
  ASSERT(!lineRules.empty(), ());
  ASSERT(HasGeometry(), ());

  if (!ftypes::IsIsolineChecker::Instance()(m_f))
  {
    // A line crossing the tile several times will be split in several parts.
    // TODO(pastk) : use feature's pre-calculated limitRect when possible.
    m_clippedSplines = m2::ClipSplineByRect(m_tileRect, m_spline);
  }
  else
  {
    // Isolines smoothing.
    ASSERT_EQUAL(lineRules.size(), 1, ());
    m2::GuidePointsForSmooth guidePointsForSmooth;
    std::vector<std::vector<m2::PointD>> clippedPaths;
    auto extTileRect = m_tileRect;
    extTileRect.Inflate(m_tileRect.SizeX() * 0.3, m_tileRect.SizeY() * 0.3);
    m2::ClipPathByRectBeforeSmooth(extTileRect, m_spline->GetPath(), guidePointsForSmooth, clippedPaths);

    if (clippedPaths.empty())
      return;

    m2::SmoothPaths(guidePointsForSmooth, 4 /* newPointsPerSegmentCount */, m2::kCentripetalAlpha, clippedPaths);

    ASSERT(m_clippedSplines.empty(), ());
    std::function<void (m2::SharedSpline &&)> inserter = base::MakeBackInsertFunctor(m_clippedSplines);
    for (auto & path : clippedPaths)
      m2::ClipPathByRect(m_tileRect, std::move(path), inserter);
  }

  if (m_clippedSplines.empty())
    return;

  for (LineRuleProto const * r : lineRules)
    ProcessRule(*r);

#ifdef LINES_GENERATION_CALC_FILTERED_POINTS
  LinesStat::Get().InsertLine(m_f.GetID(), m_tileKey.m_zoomLevel, m_readCount, static_cast<int>(m_spline->GetSize()));
#endif
}

void ApplyLineFeatureGeometry::ProcessRule(LineRuleProto const & lineRule)
{
  double const depth = PriorityToDepth(lineRule.priority(), drule::line, 0);

  if (lineRule.has_pathsym())
  {
    PathSymProto const & symRule = lineRule.pathsym();
    PathSymbolViewParams params;
    params.m_tileCenter = m_tileRect.Center();
    params.m_depth = depth;
    params.m_rank = m_f.GetRank();
    params.m_symbolName = symRule.name();
    double const mainScale = df::VisualParams::Instance().GetVisualScale();
    params.m_offset = static_cast<float>(symRule.offset() * mainScale);
    params.m_step = static_cast<float>(symRule.step() * mainScale);
    params.m_baseGtoPScale = m_currentScaleGtoP;

    for (auto const & spline : m_clippedSplines)
      m_insertShape(make_unique_dp<PathSymbolShape>(spline, params));
  }
  else
  {
    LineViewParams params;
    params.m_tileCenter = m_tileRect.Center();
    ExtractLineParams(lineRule, params);
    params.m_depth = depth;
    params.m_rank = m_f.GetRank();
    params.m_baseGtoPScale = m_currentScaleGtoP;
    params.m_zoomLevel = m_tileKey.m_zoomLevel;

    for (auto const & spline : m_clippedSplines)
      m_insertShape(make_unique_dp<LineShape>(spline, params));
  }
}

ApplyLineFeatureAdditional::ApplyLineFeatureAdditional(TileKey const & tileKey, TInsertShapeFn const & insertShape,
                                                       FeatureType & f, double currentScaleGtoP,
                                                       CaptionDescription const & captions,
                                                       std::vector<m2::SharedSpline> && clippedSplines)
  : TBase(tileKey, insertShape, f, captions)
  , m_clippedSplines(std::move(clippedSplines))
  , m_currentScaleGtoP(currentScaleGtoP)
{
  ASSERT(!m_clippedSplines.empty(), ());
}

void ApplyLineFeatureAdditional::GetRoadShieldsViewParams(ref_ptr<dp::TextureManager> texMng,
                                                          ftypes::RoadShield const & shield,
                                                          uint8_t shieldIndex, uint8_t shieldCount,
                                                          TextViewParams & textParams,
                                                          ColoredSymbolViewParams & symbolParams,
                                                          PoiSymbolViewParams & poiParams,
                                                          m2::PointD & shieldPixelSize)
{
  ASSERT(m_shieldRule, ());

  std::string const & roadNumber = shield.m_name;
  double const mainScale = df::VisualParams::Instance().GetVisualScale();
  double const fontScale = df::VisualParams::Instance().GetFontScale();
  auto const anchor = GetShieldAnchor(shieldIndex, shieldCount);
  m2::PointF const shieldOffset = GetShieldOffset(anchor, 2.0, 2.0);
  double const paddingWidth = 5.0 * mainScale;
  double const paddingHeight = 1.5 * mainScale;
  m2::PointF const shieldTextOffset = GetShieldOffset(anchor, paddingWidth, paddingHeight);

  dp::FontDecl font;
  ShieldRuleProtoToFontDecl(m_shieldRule, font);
  UpdateRoadShieldTextFont(font, shield);

  FillCommonParams(textParams);
  textParams.m_depthLayer = DepthLayer::OverlayLayer;
  textParams.m_depthTestEnabled = false;
  textParams.m_depth = m_shieldDepth;
  textParams.m_titleDecl.m_anchor = anchor;
  textParams.m_titleDecl.m_primaryText = roadNumber;
  textParams.m_titleDecl.m_primaryTextFont = font;
  textParams.m_titleDecl.m_primaryOffset = shieldOffset + shieldTextOffset;
  textParams.m_titleDecl.m_primaryOptional = false;
  textParams.m_titleDecl.m_secondaryOptional = false;
  textParams.m_startOverlayRank = dp::OverlayRank1;

  auto const textMetrics = texMng->ShapeSingleTextLine(dp::kBaseFontSizePixels, roadNumber, nullptr);
  float const textRatio = font.m_size * fontScale / dp::kBaseFontSizePixels;
  float const textWidthInPixels = textMetrics.m_lineWidthInPixels * textRatio;
  float const textHeightInPixels = textMetrics.m_maxLineHeightInPixels * textRatio;

  // Calculate width and height of a shield.
  shieldPixelSize.x = textWidthInPixels + 2.0 * paddingWidth;
  shieldPixelSize.y = textHeightInPixels + 2.0 * paddingHeight;
  textParams.m_limitedText = true;
  textParams.m_limits = shieldPixelSize * 0.9;

  // A colored box road shield.
  if (IsColoredRoadShield(shield))
  {
    FillCommonParams(symbolParams);
    symbolParams.m_depthLayer = DepthLayer::OverlayLayer;
    symbolParams.m_depthTestEnabled = true;
    symbolParams.m_depth = m_shieldDepth;
    symbolParams.m_anchor = anchor;
    symbolParams.m_offset = shieldOffset;
    symbolParams.m_shape = ColoredSymbolViewParams::Shape::RoundedRectangle;
    symbolParams.m_radiusInPixels = static_cast<float>(2.5 * mainScale);
    symbolParams.m_color = ToDrapeColor(m_shieldRule->color());
    if (m_shieldRule->stroke_color() != m_shieldRule->color())
    {
      symbolParams.m_outlineColor = ToDrapeColor(m_shieldRule->stroke_color());
      symbolParams.m_outlineWidth = static_cast<float>(1.0 * mainScale);
    }
    symbolParams.m_sizeInPixels = shieldPixelSize;
    symbolParams.m_outlineWidth = GetRoadShieldOutlineWidth(symbolParams.m_outlineWidth, shield);
    symbolParams.m_color = GetRoadShieldColor(symbolParams.m_color, shield);
  }
  // A road shield using an icon.
  else if (IsSymbolRoadShield(shield))
  {
    FillCommonParams(poiParams);
    poiParams.m_depthLayer = DepthLayer::OverlayLayer;
    poiParams.m_depthTestEnabled = false;
    poiParams.m_depth = m_shieldDepth;
    poiParams.m_symbolName = GetRoadShieldSymbolName(shield, fontScale);
    poiParams.m_maskColor.clear();
    poiParams.m_anchor = anchor;
    poiParams.m_offset = GetShieldOffset(anchor, 0.5, 0.5);

    dp::TextureManager::SymbolRegion region;
    texMng->GetSymbolRegion(poiParams.m_symbolName, region);
    float const symBorderWidth = (region.GetPixelSize().x - textWidthInPixels) * 0.5f;
    float const symBorderHeight = (region.GetPixelSize().y - textHeightInPixels) * 0.5f;
    textParams.m_titleDecl.m_primaryOffset = poiParams.m_offset + GetShieldOffset(anchor, symBorderWidth, symBorderHeight);
    shieldPixelSize = region.GetPixelSize();
  }

  if (!shield.m_additionalText.empty() && (anchor & dp::Top || anchor & dp::Center))
  {
    auto & titleDecl = textParams.m_titleDecl;
    titleDecl.m_secondaryText = shield.m_additionalText;
    titleDecl.m_secondaryTextFont = titleDecl.m_primaryTextFont;
    titleDecl.m_secondaryTextFont.m_color = df::GetColorConstant(kRoadShieldBlackTextColor);
    titleDecl.m_secondaryTextFont.m_outlineColor = df::GetColorConstant(kRoadShieldWhiteTextColor);
    titleDecl.m_secondaryTextFont.m_size *= 0.9f;
    titleDecl.m_secondaryOffset = m2::PointD(0.0f, 3.0 * mainScale);
  }
}

bool ApplyLineFeatureAdditional::CheckShieldsNearby(m2::PointD const & shieldPos,
                                                    m2::PointD const & shieldPixelSize,
                                                    uint32_t minDistanceInPixels,
                                                    std::vector<m2::RectD> & shields)
{
  // Here we calculate extended rect to skip the same shields nearby.
  m2::PointD const skippingArea(2 * minDistanceInPixels, 2 * minDistanceInPixels);
  m2::PointD const extendedPixelSize = shieldPixelSize + skippingArea;
  m2::PointD const shieldMercatorHalfSize = extendedPixelSize / m_currentScaleGtoP * 0.5;
  m2::RectD shieldRect(shieldPos - shieldMercatorHalfSize, shieldPos + shieldMercatorHalfSize);
  for (auto const & r : shields)
  {
    if (r.IsIntersect(shieldRect))
      return false;
  }
  shields.push_back(shieldRect);
  return true;
}

void ApplyLineFeatureAdditional::ProcessAdditionalLineRules(PathTextRuleProto const * pathtextRule,
                                                            ShieldRuleProto const * shieldRule,
                                                            ref_ptr<dp::TextureManager> texMng,
                                                            ftypes::RoadShieldsSetT const & roadShields,
                                                            GeneratedRoadShields & generatedRoadShields)
{
  ASSERT(pathtextRule || shieldRule, ());

  auto const vs = static_cast<float>(df::VisualParams::Instance().GetVisualScale());

  std::vector<m2::PointD> shieldPositions;
  ASSERT((shieldRule && !roadShields.empty()) || !(shieldRule && !roadShields.empty()),
         (roadShields.empty(), shieldRule == nullptr));
  if (shieldRule)
  {
    m_shieldRule = shieldRule;
    m_shieldDepth = PriorityToDepth(shieldRule->priority(), drule::shield, 0);
    shieldPositions.reserve(m_clippedSplines.size() * 3);
  }

  if (pathtextRule)
  {
    ASSERT(!m_captions.GetMainText().empty(), ());
    m_captionRule = &pathtextRule->primary();
    ASSERT_GREATER_OR_EQUAL(m_captionRule->height(), kMinVisibleFontSize / df::kMaxVisualScale, ());
    m_captionDepth = PriorityToDepth(pathtextRule->priority(), drule::pathtext, 0);

    dp::FontDecl fontDecl;
    CaptionDefProtoToFontDecl(m_captionRule, fontDecl);
    PathTextViewParams params;
    FillCommonParams(params);
    params.m_depthLayer = DepthLayer::OverlayLayer;
    params.m_depthTestEnabled = false;
    params.m_depth = m_captionDepth;
    params.m_mainText = m_captions.GetMainText();
    params.m_auxText = m_captions.GetAuxText();
    params.m_textFont = fontDecl;
    params.m_baseGtoPScale = m_currentScaleGtoP;

    uint32_t textIndex = kPathTextBaseTextIndex;
    for (auto const & spline : m_clippedSplines)
    {
      PathTextViewParams p = params;
      auto shape = make_unique_dp<PathTextShape>(spline, p, m_tileKey, textIndex);

      if (!shape->CalculateLayout(texMng))
        continue;

      // Position shields inbetween captions.
      // If there is only one center position then the shield and the caption will compete for it.
      if (m_shieldRule)
        CalculateRoadShieldPositions(shape->GetOffsets(), spline, shieldPositions);

      m_insertShape(std::move(shape));
      textIndex++;
    }
  }
  else if (m_shieldRule)
  {
    // Position shields without captions.
    for (auto const & spline : m_clippedSplines)
    {
      double const pixelLength = 300.0 * vs;
      std::vector<double> offsets;
      PathTextLayout::CalculatePositions(spline->GetLength(), m_currentScaleGtoP,
                                         pixelLength, offsets);
      if (!offsets.empty())
        CalculateRoadShieldPositions(offsets, spline, shieldPositions);
    }
  }

  if (shieldPositions.empty())
    return;

  // Set default shield's icon min distance.
  int minDistance = m_shieldRule->min_distance();
  ASSERT_GREATER_OR_EQUAL(minDistance, 0, ());
  if (minDistance <= 0)
    minDistance = 50;

  uint32_t const scaledMinDistance = static_cast<uint32_t>(vs * minDistance);

  uint8_t shieldIndex = 0;
  uint32_t textIndex = kShieldBaseTextIndex;
  for (ftypes::RoadShield const & shield : roadShields)
  {
    TextViewParams textParams;
    ColoredSymbolViewParams symbolParams;
    PoiSymbolViewParams poiParams;
    m2::PointD shieldPixelSize;
    GetRoadShieldsViewParams(texMng, shield, shieldIndex, static_cast<uint8_t>(roadShields.size()),
                             textParams, symbolParams, poiParams, shieldPixelSize);

    auto & generatedShieldRects = generatedRoadShields[shield];
    generatedShieldRects.reserve(10);
    for (auto const & shieldPos : shieldPositions)
    {
      if (!CheckShieldsNearby(shieldPos, shieldPixelSize, scaledMinDistance, generatedShieldRects))
        continue;

      m_insertShape(make_unique_dp<TextShape>(shieldPos, textParams, m_tileKey,
                                              m2::PointF(0.0f, 0.0f) /* symbolSize */,
                                              m2::PointF(0.0f, 0.0f) /* symbolOffset */,
                                              dp::Center /* symbolAnchor */, textIndex));
      if (IsColoredRoadShield(shield))
        m_insertShape(make_unique_dp<ColoredSymbolShape>(shieldPos, symbolParams, m_tileKey, textIndex));
      else if (IsSymbolRoadShield(shield))
        m_insertShape(make_unique_dp<PoiSymbolShape>(shieldPos, poiParams, m_tileKey, textIndex));
      textIndex++;
    }
    shieldIndex++;
  }
}
}  // namespace df
