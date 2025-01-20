#include "indexer/feature_decl.hpp"

#include "std/boost_container_hash.hpp"

#include <sstream>

namespace feature
{
std::string DebugPrint(GeomType type)
{
  return ToString(type);
}

std::string ToString(GeomType type)
{
  switch (type)
  {
    case GeomType::Undefined: return "Undefined";
    case GeomType::Point: return "Point";
    case GeomType::Line: return "Line";
    case GeomType::Area: return "Area";
  }
  UNREACHABLE();
}

GeomType TypeFromString(std::string type)
{
  if (type == "Point")
    return GeomType::Point;
  else if (type == "Line")
    return GeomType::Line;
  else if (type == "Area")
    return GeomType::Area;
  else
    return GeomType::Undefined;
}
}  // namespace feature

std::string DebugPrint(FeatureID const & id)
{
  return "{ " + DebugPrint(id.m_mwmId) + ", " + std::to_string(id.m_index) + " }";
}


std::string FeatureID::GetMwmName() const
{
  return IsValid() ? m_mwmId.GetInfo()->GetCountryName() : std::string();
}

int64_t FeatureID::GetMwmVersion() const
{
  return IsValid() ? m_mwmId.GetInfo()->GetVersion() : -1;
}

bool FeatureID::IsEqualCountry(base::StringIL const & lst) const
{
  if (!IsValid())
    return false;

  auto const & name = m_mwmId.GetInfo()->GetCountryName();
  for (char const * e : lst)
  {
    if (name.starts_with(e))
      return true;
  }
  return false;
}

bool FeatureID::IsWorld() const
{
  return m_mwmId.GetInfo()->GetType() == MwmInfo::MwmTypeT::WORLD;
}

size_t std::hash<FeatureID>::operator()(FeatureID const & fID) const
{
  size_t seed = 0;
  boost::hash_combine(seed, fID.m_mwmId.GetInfo());
  boost::hash_combine(seed, fID.m_index);
  return seed;
}
