#pragma once

#include "indexer/mwm_set.hpp"

#include "base/stl_helpers.hpp"

#include <string>

namespace feature
{
enum class GeomType : int8_t
{
  Undefined = -1,
  Point = 0,
  Line = 1,
  Area = 2
};

std::string DebugPrint(GeomType type);

std::string ToString(GeomType type);

GeomType TypeFromString(std::string type);
}  // namespace feature

uint32_t constexpr kInvalidFeatureId = std::numeric_limits<uint32_t>::max();

struct FeatureID
{
  FeatureID() = default;
  FeatureID(MwmSet::MwmId const & mwmId, uint32_t index) : m_mwmId(mwmId), m_index(index) {}

  bool IsValid() const { return m_mwmId.IsAlive(); }

  bool operator<(FeatureID const & r) const
  {
    if (m_mwmId != r.m_mwmId)
      return m_mwmId < r.m_mwmId;
    return m_index < r.m_index;
  }

  bool operator==(FeatureID const & r) const
  {
    return m_mwmId == r.m_mwmId && m_index == r.m_index;
  }

  bool operator!=(FeatureID const & r) const { return !(*this == r); }

  std::string GetMwmName() const;
  /// @todo This function is used in Android only, but seems like useless.
  int64_t GetMwmVersion() const;
  bool IsEqualCountry(base::StringIL const & lst) const;
  bool IsWorld() const;

  MwmSet::MwmId m_mwmId;
  uint32_t m_index = 0;
};

std::string DebugPrint(FeatureID const & id);

namespace std
{
template <>
struct hash<FeatureID>
{
  size_t operator()(FeatureID const & fID) const;
};
}  // namespace std
