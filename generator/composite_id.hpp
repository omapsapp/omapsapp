#pragma once

#include "base/geo_object_id.hpp"
#include "base/math.hpp"

#include <string>

namespace generator
{
// This struct represents a composite Id.
// This will be useful if we want to distinguish between polygons in a multipolygon.
struct CompositeId
{
  CompositeId() = default;
  explicit CompositeId(std::string const & str);
  explicit CompositeId(base::GeoObjectId mainId, base::GeoObjectId additionalId);
  explicit CompositeId(base::GeoObjectId mainId);

  bool operator<(CompositeId const & other) const;
  bool operator==(CompositeId const & other) const;
  bool operator!=(CompositeId const & other) const;

  std::string ToString() const;

  base::GeoObjectId m_mainId;
  base::GeoObjectId m_additionalId;
};

std::string DebugPrint(CompositeId const & id);
}  // namespace generator

namespace std
{
template <>
struct hash<generator::CompositeId>
{
  size_t operator()(generator::CompositeId const & id) const
  {
    return math::Hash(id.m_mainId, id.m_additionalId);
  }
};
}  // namespace std
