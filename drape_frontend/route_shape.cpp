#include "drape_frontend/route_shape.hpp"
#include "drape_frontend/batcher_bucket.hpp"
#include "drape_frontend/line_shape_helper.hpp"
#include "drape_frontend/shape_view_params.hpp"
#include "drape_frontend/tile_utils.hpp"
#include "drape_frontend/traffic_generator.hpp"
#include "drape_frontend/visual_params.hpp"

#include "shaders/programs.hpp"

#include "drape/attribute_provider.hpp"
#include "drape/batcher.hpp"
#include "drape/glsl_func.hpp"
#include "drape/glsl_types.hpp"
#include "drape/texture_manager.hpp"

#include "geometry/mercator.hpp"

#include "base/logging.hpp"
#include "base/math.hpp"

namespace df
{
std::array<float, 20> const kRouteHalfWidthInPixelCar =
{
  // 1   2     3     4     5     6     7     8     9     10
  1.0f, 1.2f, 1.5f, 1.5f, 1.7f, 2.0f, 2.0f, 2.3f, 2.5f, 2.7f,
  //11   12    13    14    15   16    17    18    19     20
  3.0f, 3.5f, 4.5f, 5.5f, 7.0, 9.0f, 10.0f, 14.0f, 22.0f, 27.0f
};

std::array<float, 20> const kRouteHalfWidthInPixelTransit =
{
  // 1   2     3     4     5     6     7     8     9     10
  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.7f,
  //11   12    13    14    15   16    17    18    19     20
  1.8f, 2.1f, 2.5f, 2.8f, 3.5, 4.5f, 5.0f, 7.0f, 11.0f, 13.0f
};

std::array<float, 20> const kRouteHalfWidthInPixelOthers =
{
  // 1   2     3     4     5     6     7     8     9     10
  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.1f, 1.2f, 1.3f,
  //11   12    13    14    15   16    17    18    19     20
  1.5f, 1.7f, 2.3f, 2.7f, 3.5, 4.5f, 5.0f, 7.0f, 11.0f, 13.0f
};

namespace rs
{
float const kLeftSide = 1.0f;
float const kCenter = 0.0f;
float const kRightSide = -1.0f;

float const kRouteDepth = 99.0f;
float const kMarkersDepth = 100.0f;
float const kArrowsDepth = 200.0f;
float const kDepthPerSubroute = 200.0f;

void GetArrowTextureRegion(ref_ptr<dp::TextureManager> textures,
                           dp::TextureManager::SymbolRegion & region)
{
  textures->GetSymbolRegion("route-arrow", region);
}

void CalculatePoints(m2::PolylineD const & polyline, double start, double end,
                     std::vector<m2::PointD> & result)
{
  result.clear();

  auto addIfNotExist = [&result](m2::PointD const & pnt)
  {
    if (result.empty() || result.back() != pnt)
      result.push_back(pnt);
  };

  auto const & path = polyline.GetPoints();
  double len = 0;
  bool started = false;
  for (size_t i = 1; i < path.size(); ++i)
  {
    auto const & p1 = path[i - 1];
    auto const & p2 = path[i];
    auto const vec = p2 - p1;
    double const dist = vec.Length();
    if (dist == 0)
      continue;

    double const l = len + dist;
    if (!started && start >= len && start <= l)
    {
      double const k = (start - len) / dist;
      addIfNotExist(p1 + vec * k);
      started = true;
    }
    if (!started)
    {
      len = l;
      continue;
    }

    if (end >= len && end <= l)
    {
      double const k = (end - len) / dist;
      addIfNotExist(p1 + vec * k);
      break;
    }
    else
    {
      addIfNotExist(p2);
    }
    len = l;
  }
}

float SideByNormal(glsl::vec2 const & normal, bool isLeft)
{
  float const kEps = 1e-5;
  float const side = isLeft ? kLeftSide : kRightSide;
  return glsl::length(normal) < kEps ? kCenter : side;
}

void GenerateJoinsTriangles(glsl::vec3 const & pivot, std::vector<glsl::vec2> const & normals,
                            glsl::vec4 const & color, glsl::vec2 const & length, bool isLeft,
                            RouteShape::GeometryBuffer & joinsGeometry)
{
  size_t const trianglesCount = normals.size() / 3;
  for (size_t j = 0; j < trianglesCount; j++)
  {
    glsl::vec3 const len1 = glsl::vec3(length.x, length.y, SideByNormal(normals[3 * j], isLeft));
    glsl::vec3 const len2 = glsl::vec3(length.x, length.y, SideByNormal(normals[3 * j + 1], isLeft));
    glsl::vec3 const len3 = glsl::vec3(length.x, length.y, SideByNormal(normals[3 * j + 2], isLeft));

    joinsGeometry.emplace_back(pivot, normals[3 * j], len1, color);
    joinsGeometry.emplace_back(pivot, normals[3 * j + 1], len2, color);
    joinsGeometry.emplace_back(pivot, normals[3 * j + 2], len3, color);
  }
}

glsl::vec2 GetUV(m2::RectF const & texRect, float normU, float normV)
{
  return glsl::vec2(texRect.minX() * (1.0f - normU) + texRect.maxX() * normU,
                    texRect.minY() * (1.0f - normV) + texRect.maxY() * normV);
}

glsl::vec2 GetUV(m2::RectF const & texRect, glsl::vec2 const & uv)
{
  return GetUV(texRect, uv.x, uv.y);
}

void GenerateArrowsTriangles(glsl::vec4 const & pivot, std::vector<glsl::vec2> const & normals,
                             m2::RectF const & texRect, std::vector<glsl::vec2> const & uv,
                             bool normalizedUV, RouteShape::ArrowGeometryBuffer & joinsGeometry)
{
  size_t const trianglesCount = normals.size() / 3;
  for (size_t j = 0; j < trianglesCount; j++)
  {
    joinsGeometry.emplace_back(pivot, normals[3 * j],
                               normalizedUV ? GetUV(texRect, uv[3 * j]) : uv[3 * j]);
    joinsGeometry.emplace_back(pivot, normals[3 * j + 1],
                               normalizedUV ? GetUV(texRect, uv[3 * j + 1]) : uv[3 * j + 1]);
    joinsGeometry.emplace_back(pivot, normals[3 * j + 2],
                               normalizedUV ? GetUV(texRect, uv[3 * j + 2]) : uv[3 * j + 2]);
  }
}

glsl::vec3 MarkerNormal(float x, float y, float z, float cosAngle, float sinAngle)
{
  return glsl::vec3(x * cosAngle - y * sinAngle, x * sinAngle + y * cosAngle, z);
}
} // namespace rs

void Subroute::AddStyle(SubrouteStyle const & style)
{
  if (!m_style.empty() && m_style.back() == style)
  {
    m_style.back().m_endIndex = style.m_endIndex;
  }
  else
  {
    m_style.push_back(style);
  }
}

void RouteShape::PrepareGeometry(std::vector<m2::PointD> const & path, m2::PointD const & pivot,
                                 std::vector<glsl::vec4> const & segmentsColors, float baseDepth,
                                 std::vector<GeometryBufferData<GeometryBuffer>> & geometryBufferData)
{
  ASSERT(path.size() > 1, ());

  // Construct segments.
  std::vector<LineSegment> segments;
  segments.reserve(path.size() - 1);
  ConstructLineSegments(path, segmentsColors, segments);

  if (segments.empty())
    return;

  // Build geometry.
  float length = 0.0f;
  for (auto const & segment : segments)
    length += glsl::length(segment.m_points[EndPoint] - segment.m_points[StartPoint]);

  geometryBufferData.push_back({});

  uint32_t constexpr kMinVertices = 5000;
  double constexpr kMinExtent = mercator::Bounds::kRangeX / (1 << 10);

  float depth = baseDepth;
  float const depthStep = rs::kRouteDepth / (1 + segments.size());
  int const lastIndex = static_cast<int>(segments.size() - 1);
  for (int i = lastIndex; i >= 0; i--)
  {
    auto & geomBufferData = geometryBufferData.back();
    auto & geometry = geomBufferData.m_geometry;
    auto & joinsGeometry = geomBufferData.m_joinsGeometry;

    UpdateNormals(&segments[i], (i > 0) ? &segments[i - 1] : nullptr,
                  i < lastIndex ? &segments[i + 1] : nullptr);

    geomBufferData.m_boundingBox.Add(glsl::FromVec2(segments[i].m_points[StartPoint]));
    geomBufferData.m_boundingBox.Add(glsl::FromVec2(segments[i].m_points[EndPoint]));

    // Generate main geometry.
    m2::PointD const startPt = MapShape::ConvertToLocal(glsl::FromVec2(segments[i].m_points[StartPoint]),
                                                        pivot, kShapeCoordScalar);
    m2::PointD const endPt = MapShape::ConvertToLocal(glsl::FromVec2(segments[i].m_points[EndPoint]),
                                                      pivot, kShapeCoordScalar);

    glsl::vec3 const startPivot = glsl::vec3(glsl::ToVec2(startPt), depth);
    glsl::vec3 const endPivot = glsl::vec3(glsl::ToVec2(endPt), depth);
    depth += depthStep;

    float const startLength = length - glsl::length(segments[i].m_points[EndPoint] - segments[i].m_points[StartPoint]);

    glsl::vec2 const leftNormalStart = GetNormal(segments[i], true /* isLeft */, StartNormal);
    glsl::vec2 const rightNormalStart = GetNormal(segments[i], false /* isLeft */, StartNormal);
    glsl::vec2 const leftNormalEnd = GetNormal(segments[i], true /* isLeft */, EndNormal);
    glsl::vec2 const rightNormalEnd = GetNormal(segments[i], false /* isLeft */, EndNormal);

    float const projLeftStart = -segments[i].m_leftWidthScalar[StartPoint].y;
    float const projLeftEnd = segments[i].m_leftWidthScalar[EndPoint].y;
    float const projRightStart = -segments[i].m_rightWidthScalar[StartPoint].y;
    float const projRightEnd = segments[i].m_rightWidthScalar[EndPoint].y;

    geometry.emplace_back(startPivot, glsl::vec2(0, 0),
                          glsl::vec3(startLength, 0, rs::kCenter), segments[i].m_color);
    geometry.emplace_back(startPivot, leftNormalStart,
                          glsl::vec3(startLength, projLeftStart, rs::kLeftSide), segments[i].m_color);
    geometry.emplace_back(endPivot, glsl::vec2(0, 0),
                          glsl::vec3(length, 0, rs::kCenter), segments[i].m_color);
    geometry.emplace_back(endPivot, leftNormalEnd,
                          glsl::vec3(length, projLeftEnd, rs::kLeftSide), segments[i].m_color);

    geometry.emplace_back(startPivot, rightNormalStart,
                          glsl::vec3(startLength, projRightStart, rs::kRightSide), segments[i].m_color);
    geometry.emplace_back(startPivot, glsl::vec2(0, 0),
                          glsl::vec3(startLength, 0, rs::kCenter), segments[i].m_color);
    geometry.emplace_back(endPivot, rightNormalEnd,
                          glsl::vec3(length, projRightEnd, rs::kRightSide), segments[i].m_color);
    geometry.emplace_back(endPivot, glsl::vec2(0, 0),
                          glsl::vec3(length, 0, rs::kCenter), segments[i].m_color);

    // Generate joins.
    if (segments[i].m_generateJoin && i < lastIndex)
    {
      glsl::vec2 n1 = segments[i].m_hasLeftJoin[EndPoint] ? segments[i].m_leftNormals[EndPoint] :
                                                            segments[i].m_rightNormals[EndPoint];
      glsl::vec2 n2 = segments[i + 1].m_hasLeftJoin[StartPoint] ? segments[i + 1].m_leftNormals[StartPoint] :
                                                                  segments[i + 1].m_rightNormals[StartPoint];

      float widthScalar = segments[i].m_hasLeftJoin[EndPoint] ? segments[i].m_rightWidthScalar[EndPoint].x :
                                                                segments[i].m_leftWidthScalar[EndPoint].x;

      std::vector<glsl::vec2> normals = GenerateJoinNormals(
            dp::RoundJoin, n1, n2, 1.0f, segments[i].m_hasLeftJoin[EndPoint], widthScalar);

      rs::GenerateJoinsTriangles(endPivot, normals, segments[i].m_color, glsl::vec2(length, 0),
                                 segments[i].m_hasLeftJoin[EndPoint], joinsGeometry);
    }

    // Generate caps.
    if (i == 0)
    {
      std::vector<glsl::vec2> normals = GenerateCapNormals(
            dp::RoundCap, segments[i].m_leftNormals[StartPoint], segments[i].m_rightNormals[StartPoint],
            -segments[i].m_tangent, 1.0f, true /* isStart */);

      rs::GenerateJoinsTriangles(startPivot, normals, segments[i].m_color, glsl::vec2(startLength, 0),
                                 true, joinsGeometry);
    }

    if (i == lastIndex)
    {
      std::vector<glsl::vec2> normals = GenerateCapNormals(
            dp::RoundCap, segments[i].m_leftNormals[EndPoint], segments[i].m_rightNormals[EndPoint],
            segments[i].m_tangent, 1.0f, false /* isStart */);

      rs::GenerateJoinsTriangles(endPivot, normals, segments[i].m_color, glsl::vec2(length, 0),
                                 true, joinsGeometry);
    }

    auto const verticesCount = geomBufferData.m_geometry.size() + geomBufferData.m_joinsGeometry.size();
    auto const extent = std::max(geomBufferData.m_boundingBox.SizeX(), geomBufferData.m_boundingBox.SizeY());
    if (verticesCount > kMinVertices && extent > kMinExtent)
      geometryBufferData.push_back({});

    length = startLength;
  }
}

void RouteShape::PrepareArrowGeometry(std::vector<m2::PointD> const & path, m2::PointD const & pivot,
                                      m2::RectF const & texRect, float depthStep, float depth,
                                      GeometryBufferData<ArrowGeometryBuffer> & geometryBufferData)
{
  ASSERT(path.size() > 1, ());

  // Construct segments.
  std::vector<LineSegment> segments;
  segments.reserve(path.size() - 1);
  ConstructLineSegments(path, std::vector<glsl::vec4>(), segments);

  auto & geometry = geometryBufferData.m_geometry;
  auto & joinsGeometry = geometryBufferData.m_joinsGeometry;

  m2::RectF tr = texRect;
  tr.setMinX(static_cast<float>(texRect.minX() * (1.0 - kArrowTailSize) + texRect.maxX() * kArrowTailSize));
  tr.setMaxX(static_cast<float>(texRect.minX() * kArrowHeadSize + texRect.maxX() * (1.0 - kArrowHeadSize)));

  // Build geometry.
  float const depthInc = depthStep / (segments.size() + 1);
  for (size_t i = 0; i < segments.size(); i++)
  {
    UpdateNormals(&segments[i], (i > 0) ? &segments[i - 1] : nullptr,
                 (i < segments.size() - 1) ? &segments[i + 1] : nullptr);

    geometryBufferData.m_boundingBox.Add(glsl::FromVec2(segments[i].m_points[StartPoint]));
    geometryBufferData.m_boundingBox.Add(glsl::FromVec2(segments[i].m_points[EndPoint]));

    // Generate main geometry.
    m2::PointD const startPt = MapShape::ConvertToLocal(glsl::FromVec2(segments[i].m_points[StartPoint]),
                                                        pivot, kShapeCoordScalar);
    m2::PointD const endPt = MapShape::ConvertToLocal(glsl::FromVec2(segments[i].m_points[EndPoint]),
                                                      pivot, kShapeCoordScalar);

    glsl::vec4 const startPivot = glsl::vec4(glsl::ToVec2(startPt), depth, 1.0);
    glsl::vec4 const endPivot = glsl::vec4(glsl::ToVec2(endPt), depth, 1.0);
    depth += depthInc;

    glsl::vec2 const leftNormalStart = GetNormal(segments[i], true /* isLeft */, StartNormal);
    glsl::vec2 const rightNormalStart = GetNormal(segments[i], false /* isLeft */, StartNormal);
    glsl::vec2 const leftNormalEnd = GetNormal(segments[i], true /* isLeft */, EndNormal);
    glsl::vec2 const rightNormalEnd = GetNormal(segments[i], false /* isLeft */, EndNormal);

    glsl::vec2 const uvCenter = rs::GetUV(tr, 0.5f, 0.5f);
    glsl::vec2 const uvLeft = rs::GetUV(tr, 0.5f, 0.0f);
    glsl::vec2 const uvRight = rs::GetUV(tr, 0.5f, 1.0f);

    geometry.emplace_back(startPivot, glsl::vec2(0, 0), uvCenter);
    geometry.emplace_back(startPivot, leftNormalStart, uvLeft);
    geometry.emplace_back(endPivot, glsl::vec2(0, 0), uvCenter);
    geometry.emplace_back(endPivot, leftNormalEnd, uvLeft);

    geometry.emplace_back(startPivot, rightNormalStart, uvRight);
    geometry.emplace_back(startPivot, glsl::vec2(0, 0), uvCenter);
    geometry.emplace_back(endPivot, rightNormalEnd, uvRight);
    geometry.emplace_back(endPivot, glsl::vec2(0, 0), uvCenter);

    // Generate joins.
    if (segments[i].m_generateJoin && i < segments.size() - 1)
    {
      glsl::vec2 n1 = segments[i].m_hasLeftJoin[EndPoint] ? segments[i].m_leftNormals[EndPoint] :
                                                            segments[i].m_rightNormals[EndPoint];
      glsl::vec2 n2 = segments[i + 1].m_hasLeftJoin[StartPoint] ? segments[i + 1].m_leftNormals[StartPoint] :
                                                                  segments[i + 1].m_rightNormals[StartPoint];

      float widthScalar = segments[i].m_hasLeftJoin[EndPoint] ? segments[i].m_rightWidthScalar[EndPoint].x :
                                                                segments[i].m_leftWidthScalar[EndPoint].x;

      std::vector<glsl::vec2> uv;
      std::vector<glsl::vec2> normals = GenerateJoinNormals(
            dp::RoundJoin, n1, n2, 1.0f, segments[i].m_hasLeftJoin[EndPoint],
            widthScalar, &uv);

      ASSERT_EQUAL(normals.size(), uv.size(), ());

      rs::GenerateArrowsTriangles(endPivot, normals, tr, uv, true /* normalizedUV */, joinsGeometry);
    }

    // Generate arrow head.
    if (i == segments.size() - 1)
    {
      std::vector<glsl::vec2> normals =
      {
        segments[i].m_rightNormals[EndPoint],
        segments[i].m_leftNormals[EndPoint],
        kArrowHeadFactor * segments[i].m_tangent
      };
      float const u = 1.0f - static_cast<float>(kArrowHeadSize);
      std::vector<glsl::vec2> uv = { glsl::vec2(u, 1.0f), glsl::vec2(u, 0.0f), glsl::vec2(1.0f, 0.5f) };
      glsl::vec4 const headPivot = glsl::vec4(glsl::ToVec2(endPt), depth, 1.0);
      depth += depthInc;
      rs::GenerateArrowsTriangles(headPivot, normals, texRect, uv, true /* normalizedUV */, joinsGeometry);
    }

    // Generate arrow tail.
    if (i == 0)
    {
      glsl::vec2 const n1 = segments[i].m_leftNormals[StartPoint];
      glsl::vec2 const n2 = segments[i].m_rightNormals[StartPoint];
      glsl::vec2 const n3 = (n1 - kArrowTailFactor * segments[i].m_tangent);
      glsl::vec2 const n4 = (n2 - kArrowTailFactor * segments[i].m_tangent);
      std::vector<glsl::vec2> normals = { n2, n4, n1, n1, n4, n3 };

      m2::RectF t = texRect;
      t.setMaxX(tr.minX());
      std::vector<glsl::vec2> uv =
      {
        glsl::ToVec2(t.RightBottom()),
        glsl::ToVec2(t.LeftBottom()),
        glsl::ToVec2(t.RightTop()),
        glsl::ToVec2(t.RightTop()),
        glsl::ToVec2(t.LeftBottom()),
        glsl::ToVec2(t.LeftTop())
      };

      rs::GenerateArrowsTriangles(startPivot, normals, texRect, uv, false /* normalizedUV */, joinsGeometry);
    }
  }
}

void RouteShape::PrepareMarkersGeometry(std::vector<SubrouteMarker> const & markers,
                                        m2::PointD const & pivot, float baseDepth,
                                        MarkersGeometryBuffer & geometry)
{
  ASSERT(!markers.empty(), ());

  static float const kSqrt3 = sqrt(3.0f);
  static float const kSqrt2 = sqrt(2.0f);
  static float const kInnerRadius = 0.6f;
  static float const kOuterRadius = 1.0f;

  float const depth = baseDepth - 0.5f;
  float const innerDepth = baseDepth + 0.5f;
  for (SubrouteMarker const & marker : markers)
  {
    if (marker.m_colors.empty())
    {
      LOG(LWARNING, ("Colors have not been specified."));
      continue;
    }

    float const innerRadius = kInnerRadius * marker.m_scale;
    float const outerRadius = kOuterRadius * marker.m_scale;

    m2::PointD const pt = MapShape::ConvertToLocal(marker.m_position, pivot, kShapeCoordScalar);
    MV::TPosition outerPos(pt.x, pt.y, depth, static_cast<float>(marker.m_distance));
    MV::TPosition innerPos(pt.x, pt.y, innerDepth, static_cast<float>(marker.m_distance));

    if (marker.m_colors.size() == 1)
    {
      dp::Color const color = df::GetColorConstant(marker.m_colors[0]);
      MV::TColor const c(color.GetRedF(), color.GetGreenF(), color.GetBlueF(), color.GetAlphaF());

      // Here we use an equilateral triangle to render circle (incircle of a triangle).
      geometry.emplace_back(outerPos, MV::TNormal(-kSqrt3, -1.0f, outerRadius), c);
      geometry.emplace_back(outerPos, MV::TNormal(kSqrt3, -1.0f, outerRadius), c);
      geometry.emplace_back(outerPos, MV::TNormal(0.0f, 2.0f, outerRadius), c);
    }
    else if (marker.m_colors.size() >= 2)
    {
      dp::Color const color1 = df::GetColorConstant(marker.m_colors[0]);
      dp::Color const color2 = df::GetColorConstant(marker.m_colors[1]);
      dp::Color const innerColor = df::GetColorConstant(marker.m_innerColor);
      MV::TColor const c1(color1.GetRedF(), color1.GetGreenF(), color1.GetBlueF(), color1.GetAlphaF());
      MV::TColor const c2(color2.GetRedF(), color2.GetGreenF(), color2.GetBlueF(), color2.GetAlphaF());
      MV::TColor const ic(innerColor.GetRedF(), innerColor.GetGreenF(),
                          innerColor.GetBlueF(), innerColor.GetAlphaF());

      auto const cosAngle = static_cast<float>(m2::DotProduct(marker.m_up, m2::PointD(0.0, 1.0)));
      auto const sinAngle = static_cast<float>(m2::CrossProduct(marker.m_up, m2::PointD(0.0, 1.0)));

      // Here we use a right triangle to render half-circle.
      geometry.emplace_back(outerPos, rs::MarkerNormal(-kSqrt2, 0.0f, outerRadius, cosAngle, sinAngle), c1);
      geometry.emplace_back(outerPos, rs::MarkerNormal(0.0f, -kSqrt2, outerRadius, cosAngle, sinAngle), c1);
      geometry.emplace_back(outerPos, rs::MarkerNormal(0.0f, kSqrt2, outerRadius, cosAngle, sinAngle), c1);

      geometry.emplace_back(outerPos, rs::MarkerNormal(kSqrt2, 0.0f, outerRadius, cosAngle, sinAngle), c2);
      geometry.emplace_back(outerPos, rs::MarkerNormal(0.0f, kSqrt2, outerRadius, cosAngle, sinAngle), c2);
      geometry.emplace_back(outerPos, rs::MarkerNormal(0.0f, -kSqrt2, outerRadius, cosAngle, sinAngle), c2);
    }

    if (marker.m_colors.size() > 1 || marker.m_colors.front() != marker.m_innerColor)
    {
      dp::Color const innerColor = df::GetColorConstant(marker.m_innerColor);
      MV::TColor const ic(innerColor.GetRedF(), innerColor.GetGreenF(),
                          innerColor.GetBlueF(), innerColor.GetAlphaF());

      geometry.emplace_back(innerPos, MV::TNormal(-kSqrt3, -1.0f, innerRadius), ic);
      geometry.emplace_back(innerPos, MV::TNormal(kSqrt3, -1.0f, innerRadius), ic);
      geometry.emplace_back(innerPos, MV::TNormal(0.0f, 2.0f, innerRadius), ic);
    }
  }
}

void RouteShape::CacheRouteArrows(ref_ptr<dp::GraphicsContext> context,
                                  ref_ptr<dp::TextureManager> mng, m2::PolylineD const & polyline,
                                  std::vector<ArrowBorders> const & borders, double baseDepthIndex,
                                  SubrouteArrowsData & routeArrowsData)
{
  GeometryBufferData<ArrowGeometryBuffer> geometryData;
  dp::TextureManager::SymbolRegion region;
  rs::GetArrowTextureRegion(mng, region);
  auto state = CreateRenderState(gpu::Program::RouteArrow, DepthLayer::GeometryLayer);
  state.SetColorTexture(region.GetTexture());
  state.SetTextureIndex(region.GetTextureIndex());

  // Generate arrow geometry.
  auto depth = static_cast<float>(baseDepthIndex * rs::kDepthPerSubroute) + rs::kArrowsDepth;
  float const depthStep = (rs::kArrowsDepth - rs::kRouteDepth) / (1 + borders.size());

  std::vector<m2::PointD> points;
  points.reserve(polyline.GetSize() / 4);
  for (ArrowBorders const & b : borders)
  {
    depth -= depthStep;
    rs::CalculatePoints(polyline, b.m_startDistance, b.m_endDistance, points);
    PrepareArrowGeometry(points, routeArrowsData.m_pivot, region.GetTexRect(), depthStep, depth, geometryData);
  }

  geometryData.m_boundingBox.Scale(kBoundingBoxScale);

  BatchGeometry(context, state, make_ref(geometryData.m_geometry.data()),
                static_cast<uint32_t>(geometryData.m_geometry.size()),
                make_ref(geometryData.m_joinsGeometry.data()),
                static_cast<uint32_t>(geometryData.m_joinsGeometry.size()),
                geometryData.m_boundingBox, AV::GetBindingInfo(),
                routeArrowsData.m_renderProperty);
}

drape_ptr<df::SubrouteData> RouteShape::CacheRoute(ref_ptr<dp::GraphicsContext> context,
                                                   dp::DrapeID subrouteId, SubrouteConstPtr subroute,
                                                   size_t styleIndex, int recacheId,
                                                   ref_ptr<dp::TextureManager> textures)
{
  size_t startIndex;
  size_t endIndex;
  if (subroute->m_styleType == df::SubrouteStyleType::Single)
  {
    ASSERT_EQUAL(styleIndex, 0, ());
    startIndex = 0;
    endIndex = subroute->m_polyline.GetSize() - 1;
  }
  else
  {
    auto const & style = subroute->m_style[styleIndex];
    startIndex = style.m_startIndex;
    endIndex = style.m_endIndex;
  }

  ASSERT_LESS(startIndex, endIndex, ());

  auto const points = subroute->m_polyline.ExtractSegment(startIndex, endIndex);
  if (points.empty())
    return nullptr;

  std::vector<glsl::vec4> segmentsColors;
  if (!subroute->m_traffic.empty())
  {
    segmentsColors.reserve(endIndex - startIndex);
    for (size_t i = startIndex; i < endIndex; ++i)
    {
      auto const speedGroup = TrafficGenerator::CheckColorsSimplification(subroute->m_traffic[i]);
      auto const colorConstant = TrafficGenerator::GetColorBySpeedGroup(speedGroup, true /* route */);
      dp::Color const color = df::GetColorConstant(colorConstant);
      float const alpha = (speedGroup == traffic::SpeedGroup::G4 ||
        speedGroup == traffic::SpeedGroup::G5 ||
        speedGroup == traffic::SpeedGroup::Unknown) ? 0.0f : 1.0f;
      segmentsColors.emplace_back(color.GetRedF(), color.GetGreenF(), color.GetBlueF(), alpha);
    }
  }

  auto subrouteData = make_unique_dp<df::SubrouteData>();
  subrouteData->m_subrouteId = subrouteId;
  subrouteData->m_subroute = subroute;
  subrouteData->m_startPointIndex = startIndex;
  subrouteData->m_endPointIndex = endIndex;
  subrouteData->m_styleIndex = styleIndex;
  subrouteData->m_pivot = subroute->m_polyline.GetLimitRect().Center();
  subrouteData->m_recacheId = recacheId;
  subrouteData->m_distanceOffset = subroute->m_polyline.GetLength(startIndex);

  std::vector<GeometryBufferData<GeometryBuffer>> geometryBufferData;
  PrepareGeometry(points, subrouteData->m_pivot, segmentsColors,
                  static_cast<float>(subroute->m_baseDepthIndex * rs::kDepthPerSubroute),
                  geometryBufferData);

  auto state = CreateRenderState(subroute->m_style[styleIndex].m_pattern.m_isDashed ?
                                 gpu::Program::RouteDash : gpu::Program::Route, DepthLayer::GeometryLayer);
  state.SetColorTexture(textures->GetSymbolsTexture());

  for (auto & data : geometryBufferData)
  {
    data.m_boundingBox.Scale(kBoundingBoxScale);
    BatchGeometry(context, state, make_ref(data.m_geometry.data()),
                  static_cast<uint32_t>(data.m_geometry.size()),
                  make_ref(data.m_joinsGeometry.data()),
                  static_cast<uint32_t>(data.m_joinsGeometry.size()),
                  data.m_boundingBox, RV::GetBindingInfo(),
                  subrouteData->m_renderProperty);
  }


  return subrouteData;
}

drape_ptr<df::SubrouteMarkersData> RouteShape::CacheMarkers(ref_ptr<dp::GraphicsContext> context,
                                                            dp::DrapeID subrouteId, SubrouteConstPtr subroute,
                                                            int recacheId, ref_ptr<dp::TextureManager> textures)
{
  if (subroute->m_markers.empty())
    return nullptr;

  auto markersData = make_unique_dp<df::SubrouteMarkersData>();
  markersData->m_subrouteId = subrouteId;
  markersData->m_pivot = subroute->m_polyline.GetLimitRect().Center();
  markersData->m_recacheId = recacheId;

  MarkersGeometryBuffer geometry;
  auto const depth = static_cast<float>(subroute->m_baseDepthIndex * rs::kDepthPerSubroute + rs::kMarkersDepth);
  PrepareMarkersGeometry(subroute->m_markers, markersData->m_pivot, depth, geometry);
  if (geometry.empty())
    return nullptr;

  auto state = CreateRenderState(gpu::Program::RouteMarker, DepthLayer::GeometryLayer);
  state.SetColorTexture(textures->GetSymbolsTexture());

  // Batching.
  {
    uint32_t const kBatchSize = 200;
    dp::Batcher batcher(kBatchSize, kBatchSize);
    batcher.SetBatcherHash(static_cast<uint64_t>(BatcherBucket::Routing));
    dp::SessionGuard guard(context, batcher, [&markersData](dp::RenderState const & state,
                                                            drape_ptr<dp::RenderBucket> &&b)
    {
      markersData->m_renderProperty.m_buckets.push_back(std::move(b));
      markersData->m_renderProperty.m_state = state;
    });

    dp::AttributeProvider provider(1 /* stream count */, static_cast<uint32_t>(geometry.size()));
    provider.InitStream(0 /* stream index */, MV::GetBindingInfo(), geometry.data());
    batcher.InsertTriangleList(context, state, make_ref(&provider));
  }

  return markersData;
}

void RouteShape::BatchGeometry(ref_ptr<dp::GraphicsContext> context, dp::RenderState const & state,
                               ref_ptr<void> geometry, uint32_t geomSize,
                               ref_ptr<void> joinsGeometry, uint32_t joinsGeomSize,
                               m2::RectD const & boundingBox, dp::BindingInfo const & bindingInfo,
                               RouteRenderProperty & property)
{
  auto verticesCount = geomSize + joinsGeomSize;
  if (verticesCount == 0)
    return;

  uint32_t constexpr kMinBatchSize = 100;
  uint32_t constexpr kMaxBatchSize = 65000;
  uint32_t constexpr kIndicesScalar = 2;

  verticesCount = math::Clamp(verticesCount, kMinBatchSize, kMaxBatchSize);
  auto const indicesCount = math::Clamp(verticesCount * kIndicesScalar, kMinBatchSize, kMaxBatchSize);

  dp::Batcher batcher(indicesCount, verticesCount);
  batcher.SetBatcherHash(static_cast<uint64_t>(BatcherBucket::Routing));
  dp::SessionGuard guard(context, batcher, [&property, &boundingBox](dp::RenderState const & state,
                                                                     drape_ptr<dp::RenderBucket> && b)
  {
    property.m_buckets.push_back(std::move(b));
    property.m_boundingBoxes.push_back(boundingBox);
    property.m_state = state;
  });

  if (geomSize != 0)
  {
    dp::AttributeProvider provider(1 /* stream count */, geomSize);
    provider.InitStream(0 /* stream index */, bindingInfo, geometry);
    batcher.InsertListOfStrip(context, state, make_ref(&provider), 4);
  }

  if (joinsGeomSize != 0)
  {
    dp::AttributeProvider joinsProvider(1 /* stream count */, joinsGeomSize);
    joinsProvider.InitStream(0 /* stream index */, bindingInfo, joinsGeometry);
    batcher.InsertTriangleList(context, state, make_ref(&joinsProvider));
  }
}
}  // namespace df
