#pragma once

#include "drape/glsl_types.hpp"

#include "base/assert.hpp"

#include <type_traits>
#include <utility>

namespace df
{
#define DECLARE_SETTER(name, field)                                                                \
  template <typename T>                                                                            \
  struct Check##name                                                                               \
  {                                                                                                \
  private:                                                                                         \
    static void Detect(...);                                                                       \
    template <typename U>                                                                          \
    static decltype(std::declval<U>().field) Detect(U const &);                                    \
                                                                                                   \
  public:                                                                                          \
    static constexpr bool Value = !std::is_same<void, decltype(Detect(std::declval<T>()))>::value; \
  };                                                                                               \
                                                                                                   \
  template <typename ParamsType>                                                                   \
  std::enable_if_t<Check##name<ParamsType>::Value> name(ParamsType & params) const                 \
  {                                                                                                \
    params.field = field;                                                                          \
  }                                                                                                \
                                                                                                   \
  template <typename ParamsType>                                                                   \
  std::enable_if_t<!Check##name<ParamsType>::Value> name(ParamsType &) const                       \
  {                                                                                                \
  }

struct FrameValues
{
  glsl::mat4 m_projection;
  glsl::mat4 m_pivotTransform;
  float m_zScale = 1.0f;

  template <typename ParamsType>
  void SetTo(ParamsType & params) const
  {
    SetProjection(params);
    SetPivotTransform(params);
    SetZScale(params);
  }

private:
  DECLARE_SETTER(SetProjection, m_projection)
  DECLARE_SETTER(SetPivotTransform, m_pivotTransform)
  DECLARE_SETTER(SetZScale, m_zScale)
};
}  // namespace df
