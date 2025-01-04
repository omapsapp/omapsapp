#pragma once

#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/map_shape.hpp"
#include "drape_frontend/render_state_extension.hpp"
#include "drape_frontend/shape_view_params.hpp"

#include "drape/render_bucket.hpp"
#include "drape/utils/vertex_decl.hpp"
#include "drape/pointers.hpp"

#include "traffic/speed_groups.hpp"

#include "geometry/polyline2d.hpp"

#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace df
{
double const kArrowSize = 0.0008;

// Constants below depend on arrow texture.
double const kArrowTextureWidth = 74.0;
double const kArrowTextureHeight = 44.0;
double const kArrowBodyHeight = 24.0;
double const kArrowHeadTextureWidth = 32.0;
double const kArrowTailTextureWidth = 4.0;

double const kArrowHeadSize = kArrowHeadTextureWidth / kArrowTextureWidth;
float const kArrowHeadFactor = static_cast<float>(2.0 * kArrowHeadTextureWidth / kArrowTextureHeight);
double const kArrowTailSize = kArrowTailTextureWidth / kArrowTextureWidth;
float const kArrowTailFactor = static_cast<float>(2.0 * kArrowTailTextureWidth / kArrowTextureHeight);
double const kArrowHeightFactor = kArrowTextureHeight / kArrowBodyHeight;
double const kArrowAspect = kArrowTextureWidth / kArrowTextureHeight;

extern std::array<float, 20> const kRouteHalfWidthInPixelCar;
extern std::array<float, 20> const kRouteHalfWidthInPixelTransit;
extern std::array<float, 20> const kRouteHalfWidthInPixelOthers;

enum class RouteType : uint8_t
{
  Car,
  Pedestrian,
  Bicycle,
  Taxi,
  Transit,
  Ruler
};

struct RoutePattern
{
  RoutePattern() = default;

  RoutePattern(double dashLength, double gapLength)
    : m_isDashed(true)
    , m_dashLength(dashLength)
    , m_gapLength(gapLength)
  {}

  bool operator == (RoutePattern const & pattern) const
  {
    double const kEps = 1e-5;
    return m_isDashed == pattern.m_isDashed &&
           std::fabs(m_dashLength - pattern.m_dashLength) < kEps &&
           std::fabs(m_gapLength - pattern.m_gapLength) < kEps;
  }

  bool m_isDashed = false;
  double m_dashLength = 0.0;
  double m_gapLength = 0.0;
};

enum class SubrouteStyleType
{
  Single = 0,
  Multiple
};

struct SubrouteStyle
{
  SubrouteStyle() = default;
  explicit SubrouteStyle(df::ColorConstant const & color)
    : m_color(color)
    , m_outlineColor(color)
  {}
  SubrouteStyle(df::ColorConstant const & color, df::ColorConstant const & outlineColor)
    : m_color(color)
    , m_outlineColor(outlineColor)
  {}
  SubrouteStyle(df::ColorConstant const & color, df::RoutePattern const & pattern)
    : m_color(color)
    , m_outlineColor(color)
    , m_pattern(pattern)
  {}
  SubrouteStyle(df::ColorConstant const & color, df::ColorConstant const & outlineColor,
                df::RoutePattern const & pattern)
    : m_color(color)
    , m_outlineColor(outlineColor)
    , m_pattern(pattern)
  {}

  bool operator==(SubrouteStyle const & style) const
  {
    return m_color == style.m_color && m_outlineColor == style.m_outlineColor &&
           m_pattern == style.m_pattern;
  }

  bool operator!=(SubrouteStyle const & style) const { return !operator==(style); }

  df::ColorConstant m_color;
  df::ColorConstant m_outlineColor;
  df::RoutePattern m_pattern;
  size_t m_startIndex = 0;
  size_t m_endIndex = 0;
};

// Colored circle on the subroute.
struct SubrouteMarker
{
  // Position in mercator.
  m2::PointD m_position = {};
  // Distance from the beginning of route.
  double m_distance = 0.0;
  // Array of colors in range [0;2].
  std::vector<df::ColorConstant> m_colors;
  // Color of inner circle.
  df::ColorConstant m_innerColor;
  // Normalized up vector to determine rotation of circle.
  m2::PointD m_up = m2::PointD(0.0, 1.0);
  // Scale (1.0 when the radius is equal to route line half-width).
  float m_scale = 1.0f;
};

struct Subroute
{
  void AddStyle(SubrouteStyle const & style);

  df::RouteType m_routeType;
  m2::PolylineD m_polyline;
  std::vector<double> m_turns;
  std::vector<traffic::SpeedGroup> m_traffic;
  std::vector<float> m_slopes;
  double m_baseDistance = 0.0;
  double m_baseDepthIndex = 0.0;
  float m_maxPixelWidth = -1.0f;

  SubrouteStyleType m_styleType = SubrouteStyleType::Single;
  std::vector<SubrouteStyle> m_style;

  std::vector<SubrouteMarker> m_markers;

  double m_headFakeDistance = 0.0;
  double m_tailFakeDistance = 0.0;
};

using SubrouteConstPtr = std::shared_ptr<Subroute const>;

struct RouteRenderProperty
{
  RouteRenderProperty()
    : m_state(CreateRenderState(gpu::Program::Route, DepthLayer::GeometryLayer))
  {}

  dp::RenderState m_state;
  std::vector<drape_ptr<dp::RenderBucket>> m_buckets;
  std::vector<m2::RectD> m_boundingBoxes;
};

struct BaseSubrouteData
{
  dp::DrapeID m_subrouteId = 0;
  m2::PointD m_pivot = m2::PointD(0.0, 0.0);
  int m_recacheId = -1;
  RouteRenderProperty m_renderProperty;
};

struct SubrouteData : public BaseSubrouteData
{
  SubrouteConstPtr m_subroute;
  size_t m_startPointIndex = 0;
  size_t m_endPointIndex = 0;
  size_t m_styleIndex = 0;
  double m_distanceOffset = 0.0;
};

struct SubrouteArrowsData : public BaseSubrouteData {};

struct SubrouteMarkersData : public BaseSubrouteData {};

struct ArrowBorders
{
  double m_startDistance = 0.0;
  double m_endDistance = 0.0;
  int m_groupIndex = 0;
};

class RouteShape
{
public:
  using RV = gpu::RouteVertex;
  using GeometryBuffer = gpu::VBUnknownSizeT<RV>;
  using AV = gpu::SolidTexturingVertex;
  using ArrowGeometryBuffer = gpu::VBUnknownSizeT<AV>;
  using MV = gpu::RouteMarkerVertex;
  using MarkersGeometryBuffer = gpu::VBUnknownSizeT<MV>;

  static drape_ptr<df::SubrouteData> CacheRoute(ref_ptr<dp::GraphicsContext> context,
                                                dp::DrapeID subrouteId, SubrouteConstPtr subroute,
                                                size_t styleIndex, int recacheId,
                                                ref_ptr<dp::TextureManager> textures);

  static drape_ptr<df::SubrouteMarkersData> CacheMarkers(ref_ptr<dp::GraphicsContext> context,
                                                         dp::DrapeID subrouteId,
                                                         SubrouteConstPtr subroute, int recacheId,
                                                         ref_ptr<dp::TextureManager> textures);

  static void CacheRouteArrows(ref_ptr<dp::GraphicsContext> context,
                               ref_ptr<dp::TextureManager> mng, m2::PolylineD const & polyline,
                               std::vector<ArrowBorders> const & borders, double baseDepthIndex,
                               SubrouteArrowsData & routeArrowsData);

private:
  template<typename GeometryBufferType>
  struct GeometryBufferData
  {
    GeometryBufferData()
      : m_boundingBox(m2::RectD::GetEmptyRect())
    {}

    GeometryBufferType m_geometry;
    GeometryBufferType m_joinsGeometry;
    m2::RectD m_boundingBox;
  };
  static void PrepareGeometry(std::vector<m2::PointD> const & path, m2::PointD const & pivot,
                              std::vector<glsl::vec4> const & segmentsColors, float baseDepth,
                              std::vector<GeometryBufferData<GeometryBuffer>> & geometryBufferData);
  static void PrepareArrowGeometry(std::vector<m2::PointD> const & path, m2::PointD const & pivot,
                                   m2::RectF const & texRect, float depthStep, float depth,
                                   GeometryBufferData<ArrowGeometryBuffer> & geometryBufferData);
  static void PrepareMarkersGeometry(std::vector<SubrouteMarker> const & markers,
                                     m2::PointD const & pivot, float baseDepth,
                                     MarkersGeometryBuffer & geometry);

  static void BatchGeometry(ref_ptr<dp::GraphicsContext> context, dp::RenderState const & state,
                            ref_ptr<void> geometry, uint32_t geomSize, ref_ptr<void> joinsGeometry,
                            uint32_t joinsGeomSize, m2::RectD const & boundingBox,
                            dp::BindingInfo const & bindingInfo, RouteRenderProperty & property);
};
}  // namespace df
