#include "drape_frontend/line_shape_helper.hpp"

#include "drape/glsl_func.hpp"

#include "base/assert.hpp"

namespace df
{
namespace
{
void UpdateNormalBetweenSegments(LineSegment * segment1, LineSegment * segment2)
{
  ASSERT(segment1 != nullptr && segment2 != nullptr, ());

  float const dotProduct = glsl::dot(segment1->m_leftNormals[EndPoint],
                                     segment2->m_leftNormals[StartPoint]);
  float const absDotProduct = fabs(dotProduct);
  float const kEps = 1e-5;

  if (fabs(absDotProduct - 1.0f) < kEps)
  {
    // change nothing
    return;
  }

  float const kMaxScalar = 5;
  float const crossProduct = glsl::cross(glsl::vec3(segment1->m_tangent, 0),
                                         glsl::vec3(segment2->m_tangent, 0)).z;
  if (crossProduct < 0)
  {
    segment1->m_hasLeftJoin[EndPoint] = true;
    segment2->m_hasLeftJoin[StartPoint] = true;

    // change right-side normals
    glsl::vec2 averageNormal = glsl::normalize(segment1->m_rightNormals[EndPoint] +
                                               segment2->m_rightNormals[StartPoint]);
    float const cosAngle = glsl::dot(segment1->m_tangent, averageNormal);
    float const widthScalar = 1.0f / sqrt(1.0f - cosAngle * cosAngle);
    if (widthScalar < kMaxScalar)
    {
      segment1->m_rightNormals[EndPoint] = averageNormal;
      segment2->m_rightNormals[StartPoint] = averageNormal;
      segment1->m_rightWidthScalar[EndPoint].x = widthScalar;
      segment1->m_rightWidthScalar[EndPoint].y = widthScalar * cosAngle;
      segment2->m_rightWidthScalar[StartPoint] = segment1->m_rightWidthScalar[EndPoint];
    }
    else
    {
      segment1->m_generateJoin = false;
    }
  }
  else
  {
    segment1->m_hasLeftJoin[EndPoint] = false;
    segment2->m_hasLeftJoin[StartPoint] = false;

    // change left-side normals
    glsl::vec2 averageNormal = glsl::normalize(segment1->m_leftNormals[EndPoint] +
                                               segment2->m_leftNormals[StartPoint]);
    float const cosAngle = glsl::dot(segment1->m_tangent, averageNormal);
    float const widthScalar = 1.0f / sqrt(1.0f - cosAngle * cosAngle);
    if (widthScalar < kMaxScalar)
    {
      segment1->m_leftNormals[EndPoint] = averageNormal;
      segment2->m_leftNormals[StartPoint] = averageNormal;
      segment1->m_leftWidthScalar[EndPoint].x = widthScalar;
      segment1->m_leftWidthScalar[EndPoint].y = widthScalar * cosAngle;
      segment2->m_leftWidthScalar[StartPoint] = segment1->m_leftWidthScalar[EndPoint];
    }
    else
    {
      segment1->m_generateJoin = false;
    }
  }
}
}  // namespace

void CalculateTangentAndNormals(glsl::vec2 const & pt0, glsl::vec2 const & pt1,
                                glsl::vec2 & tangent, glsl::vec2 & leftNormal,
                                glsl::vec2 & rightNormal)
{
  tangent = glsl::normalize(pt1 - pt0);
  leftNormal = glsl::vec2(-tangent.y, tangent.x);
  rightNormal = -leftNormal;
}

void ConstructLineSegments(std::vector<m2::PointD> const & path, std::vector<glsl::vec4> const & segmentsColors,
                           std::vector<LineSegment> & segments)
{
  ASSERT_LESS(1, path.size(), ());

  if (!segmentsColors.empty())
  {
    ASSERT_EQUAL(segmentsColors.size() + 1, path.size(), ());
  }

  m2::PointD prevPoint = path[0];
  for (size_t i = 1; i < path.size(); ++i)
  {
    m2::PointF const p1 = m2::PointF(prevPoint.x, prevPoint.y);
    m2::PointF const p2 = m2::PointF(path[i].x, path[i].y);
    if (p1.EqualDxDy(p2, 1.0E-5))
      continue;

    // Important! Do emplace_back first and fill parameters later.
    // Fill parameters first and push_back later will cause ugly bug in clang 3.6 -O3 optimization.
    segments.emplace_back(glsl::ToVec2(p1), glsl::ToVec2(p2));
    LineSegment & segment = segments.back();

    CalculateTangentAndNormals(glsl::ToVec2(p1), glsl::ToVec2(p2), segment.m_tangent,
                               segment.m_leftBaseNormal, segment.m_rightBaseNormal);

    segment.m_leftNormals[StartPoint] = segment.m_leftNormals[EndPoint] = segment.m_leftBaseNormal;
    segment.m_rightNormals[StartPoint] = segment.m_rightNormals[EndPoint] = segment.m_rightBaseNormal;
    if (!segmentsColors.empty())
      segment.m_color = segmentsColors[i - 1];

    prevPoint = path[i];
  }
}

void UpdateNormals(LineSegment * segment, LineSegment * prevSegment, LineSegment * nextSegment)
{
  ASSERT(segment != nullptr, ());

  if (prevSegment != nullptr)
    UpdateNormalBetweenSegments(prevSegment, segment);

  if (nextSegment != nullptr)
    UpdateNormalBetweenSegments(segment, nextSegment);
}

std::vector<glsl::vec2> GenerateJoinNormals(
    dp::LineJoin joinType, glsl::vec2 const & normal1, glsl::vec2 const & normal2,
    float halfWidth, bool isLeft, float widthScalar, std::vector<glsl::vec2> * uv)
{
  std::vector<glsl::vec2> normals;
  float const eps = 1e-5;
  if (fabs(glsl::dot(normal1, normal2) - 1.0f) < eps)
    return normals;

  if (joinType == dp::LineJoin::BevelJoin)
  {
    glsl::vec2 const n1 = halfWidth * normal1;
    glsl::vec2 const n2 = halfWidth * normal2;

    normals = { glsl::vec2(0.0f, 0.0f), isLeft ? n1 : n2, isLeft ? n2 : n1 };

    if (uv)
    {
      *uv = { glsl::vec2(0.5f, 0.5f), isLeft ? glsl::vec2(0.5f, 0.0f) : glsl::vec2(0.5f, 1.0f),
                                      isLeft ? glsl::vec2(0.5f, 0.0f) : glsl::vec2(0.5f, 1.0f) };
    }
  }
  else if (joinType == dp::LineJoin::MiterJoin)
  {
    glsl::vec2 averageNormal = halfWidth * widthScalar * glsl::normalize(normal1 + normal2);

    glsl::vec2 const n1 = halfWidth * normal1;
    glsl::vec2 const n2 = halfWidth * normal2;

    normals = { glsl::vec2(0.0f, 0.0f), isLeft ? n1 : averageNormal, isLeft ? averageNormal : n1,
                glsl::vec2(0.0f, 0.0f), isLeft ? averageNormal : n2, isLeft ? n2 : averageNormal };

    if (uv)
    {
      *uv = { glsl::vec2(0.5f, 0.5f), isLeft ? glsl::vec2(0.5f, 0.0f) : glsl::vec2(0.5f, 1.0f),
                                      isLeft ? glsl::vec2(0.5f, 0.0f) : glsl::vec2(0.5f, 1.0f),

              glsl::vec2(0.5f, 0.5f), isLeft ? glsl::vec2(0.5f, 0.0f) : glsl::vec2(0.5f, 1.0f),
                                      isLeft ? glsl::vec2(0.5f, 0.0f) : glsl::vec2(0.5f, 1.0f) };
    }
  }
  else
  {
    double const segmentAngle = math::pi / 8.0;
    double const fullAngle = acos(glsl::dot(normal1, normal2));
    int segmentsCount = static_cast<int>(fullAngle / segmentAngle);
    if (segmentsCount == 0)
      segmentsCount = 1;

    double const angle = fullAngle / segmentsCount * (isLeft ? -1.0 : 1.0);
    glsl::vec2 const normalizedNormal = glsl::normalize(normal1);
    m2::PointD const startNormal(normalizedNormal.x, normalizedNormal.y);

    normals.reserve(segmentsCount * 3);
    if (uv)
      uv->reserve(segmentsCount * 3);

    for (int i = 0; i < segmentsCount; i++)
    {
      m2::PointD n1 = m2::Rotate(startNormal, i * angle) * halfWidth;
      m2::PointD n2 = m2::Rotate(startNormal, (i + 1) * angle) * halfWidth;

      normals.push_back(glsl::vec2(0.0f, 0.0f));
      normals.push_back(isLeft ? glsl::vec2(n1.x, n1.y) : glsl::vec2(n2.x, n2.y));
      normals.push_back(isLeft ? glsl::vec2(n2.x, n2.y) : glsl::vec2(n1.x, n1.y));

      if (uv)
      {
        uv->push_back(glsl::vec2(0.5f, 0.5f));
        uv->push_back(isLeft ? glsl::vec2(0.5f, 0.0f) : glsl::vec2(0.5f, 1.0f));
        uv->push_back(isLeft ? glsl::vec2(0.5f, 0.0f) : glsl::vec2(0.5f, 1.0f));
      }
    }
  }

  return normals;
}

std::vector<glsl::vec2> GenerateCapNormals(
    dp::LineCap capType, glsl::vec2 const & normal1, glsl::vec2 const & normal2,
    glsl::vec2 const & direction, float halfWidth, bool isStart, int segmentsCount)
{
  std::vector<glsl::vec2> normals;
  if (capType == dp::ButtCap)
    return normals;

  if (capType == dp::SquareCap)
  {
    glsl::vec2 const n1 = halfWidth * normal1;
    glsl::vec2 const n2 = halfWidth * normal2;
    glsl::vec2 const n3 = halfWidth * (normal1 + direction);
    glsl::vec2 const n4 = halfWidth * (normal2 + direction);

    normals = { n2, isStart ? n4 : n1, isStart ? n1 : n4,
                n1, isStart ? n4 : n3, isStart ? n3 : n4 };
  }
  else
  {
    double const segmentSize = math::pi / segmentsCount * (isStart ? -1.0 : 1.0);
    glsl::vec2 const normalizedNormal = glsl::normalize(normal2);
    m2::PointD const startNormal(normalizedNormal.x, normalizedNormal.y);

    normals.reserve(segmentsCount * 3);
    for (int i = 0; i < segmentsCount; i++)
    {
      m2::PointD n1 = m2::Rotate(startNormal, i * segmentSize) * halfWidth;
      m2::PointD n2 = m2::Rotate(startNormal, (i + 1) * segmentSize) * halfWidth;

      normals.push_back(glsl::vec2(0.0f, 0.0f));
      normals.push_back(isStart ? glsl::vec2(n1.x, n1.y) : glsl::vec2(n2.x, n2.y));
      normals.push_back(isStart ? glsl::vec2(n2.x, n2.y) : glsl::vec2(n1.x, n1.y));
    }
  }

  return normals;
}

glsl::vec2 GetNormal(LineSegment const & segment, bool isLeft, ENormalType normalType)
{
  if (normalType == BaseNormal)
    return isLeft ? segment.m_leftBaseNormal : segment.m_rightBaseNormal;

  int const index = (normalType == StartNormal) ? StartPoint : EndPoint;
  return isLeft ? segment.m_leftWidthScalar[index].x * segment.m_leftNormals[index]:
                  segment.m_rightWidthScalar[index].x * segment.m_rightNormals[index];
}

float GetProjectionLength(glsl::vec2 const & newPoint, glsl::vec2 const & startPoint,
                          glsl::vec2 const & endPoint)
{
  glsl::vec2 const v1 = endPoint - startPoint;
  glsl::vec2 const v2 = newPoint - startPoint;
  float const squareLen = glsl::dot(v1, v1);
  float const proj = glsl::dot(v1, v2) / squareLen;
  return sqrt(squareLen) * math::Clamp(proj, 0.0f, 1.0f);
}
}  // namespace df

