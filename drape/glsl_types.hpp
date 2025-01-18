#pragma once

#include "drape/color.hpp"

#include "geometry/point2d.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x2.hpp>
#include <glm/mat4x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <type_traits>

namespace glsl
{
using glm::vec2;
using glm::vec3;
using glm::vec4;

using glm::dvec2;
using glm::dvec3;
using glm::dvec4;

using glm::ivec2;
using glm::ivec3;
using glm::ivec4;

using glm::uvec2;
using glm::uvec3;
using glm::uvec4;

using glm::mat3;
using glm::mat4;
using glm::mat4x2;
using glm::mat4x3;

using glm::dmat3;
using glm::dmat4;
using glm::dmat4x2;
using glm::dmat4x3;

using glm::value_ptr;

using glm::make_mat4;
using glm::make_vec2;
using glm::make_vec3;
using glm::make_vec4;

inline m2::PointF ToPoint(vec2 const & v)
{
  return m2::PointF(v.x, v.y);
}

inline vec2 ToVec2(m2::PointF const & pt)
{
  return glsl::vec2(pt.x, pt.y);
}

inline vec2 ToVec2(m2::PointD const & pt)
{
  return glsl::vec2(pt.x, pt.y);
}

inline m2::PointD FromVec2(glsl::vec2 const & pt)
{
  return m2::PointD(pt.x, pt.y);
}

inline vec3 ToVec3(dp::Color const & color)
{
  return glsl::vec3(static_cast<float>(color.GetRed()) / 255,
                    static_cast<float>(color.GetGreen()) / 255,
                    static_cast<float>(color.GetBlue()) / 255);
}

inline vec4 ToVec4(dp::Color const & color)
{
  return glsl::vec4(static_cast<float>(color.GetRed()) / 255,
                    static_cast<float>(color.GetGreen()) / 255,
                    static_cast<float>(color.GetBlue()) / 255,
                    static_cast<float>(color.GetAlpha()) / 255);
}

inline vec4 ToVec4(m2::PointD const & pt1, m2::PointD const & pt2)
{
  return glsl::vec4(pt1.x, pt1.y, pt2.x, pt2.y);
}

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value ||
                                                  std::is_floating_point<T>::value>>
inline uint8_t GetArithmeticComponentCount()
{
  return 1;
}

template <typename T>
inline uint8_t GetComponentCount()
{
  return GetArithmeticComponentCount<T>();
}

template <>
inline uint8_t GetComponentCount<vec2>()
{
  return 2;
}

template <>
inline uint8_t GetComponentCount<vec3>()
{
  return 3;
}

template <>
inline uint8_t GetComponentCount<vec4>()
{
  return 4;
}
}  // namespace glsl
