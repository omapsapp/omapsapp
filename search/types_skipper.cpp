#include "search/types_skipper.hpp"

#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "base/stl_helpers.hpp"

#include <algorithm>

using base::StringIL;

namespace search
{
TypesSkipper::TypesSkipper()
{
  Classificator const & c = classif();

  /// @todo(pastk): why "office-*" are skipped?
  /// also could be unnamed (yet) place-hamlet/isolated_dwelling?
  /// and natural-water-pond/lake?
  // POIs like natural-spring, waterway-waterfall, highway-bus_stop are saved from skipping by the TwoLevelPOIChecker().
  StringIL const arrSkipEmptyName1[] = {
    {"area:highway"}, {"building"}, {"highway"}, {"landuse"}, {"natural"}, {"office"}, {"place"}, {"waterway"},
  };
  for (auto const & e : arrSkipEmptyName1)
    m_skipIfEmptyName[0].push_back(c.GetTypeByPath(e));

  StringIL const arrSkipEmptyName2[] = {
    {"man_made", "chimney"},
    {"man_made", "flagpole"},
    {"man_made", "mast"},
    {"man_made", "tower"},
    {"man_made", "water_tower"},
  };
  for (auto const & e : arrSkipEmptyName2)
    m_skipIfEmptyName[1].push_back(c.GetTypeByPath(e));

  m_skipAlways[0].push_back(c.GetTypeByPath({"isoline"}));

  // Do not index "entrance" only features.
  StringIL const arrSkipSpecialNames1[] = {
    {"entrance"}, {"wheelchair"},
  };
  for (auto const & e : arrSkipSpecialNames1)
    m_skipSpecialNames[0].push_back(c.GetTypeByPath(e));
}

void TypesSkipper::SkipEmptyNameTypes(feature::TypesHolder & types) const
{
  types.RemoveIf([this](uint32_t type)
  {
    if (m_isPoi(type))
      return false;

    ftype::TruncValue(type, 2);
    if (HasType(m_skipIfEmptyName[1], type))
      return true;

    ftype::TruncValue(type, 1);
    if (HasType(m_skipIfEmptyName[0], type))
      return true;

    return false;
  });
}

bool TypesSkipper::SkipAlways(feature::TypesHolder const & types) const
{
  for (auto type : types)
  {
    ftype::TruncValue(type, 1);
    if (HasType(m_skipAlways[0], type))
      return true;
  }
  return false;
}

bool TypesSkipper::SkipSpecialNames(feature::TypesHolder const & types, std::string_view defName) const
{
  // Since we assign ref tag into name for entrances, this is a crutch to avoid indexing
  // these refs (entrance numbers for CIS countries).
  /// @todo Move refs into metadata?

  uint32_t dummy;
  if (!strings::to_uint(defName, dummy))
    return false;

  for (auto type : types)
  {
    if (!HasType(m_skipSpecialNames[0], type))
      return false;
  }
  return true;
}

// static
bool TypesSkipper::HasType(Cont const & v, uint32_t t)
{
  return base::IsExist(v, t);
}
}  // namespace search
