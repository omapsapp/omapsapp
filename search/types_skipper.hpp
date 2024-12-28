#pragma once

#include "indexer/feature_data.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "base/buffer_vector.hpp"


namespace search
{
// Helper functions to determine if a feature should be indexed for search.
class TypesSkipper
{
public:
  TypesSkipper();

  // Removes types which shouldn't be searchable in case there is no feature name.
  void SkipEmptyNameTypes(feature::TypesHolder & types) const;

  // Always skip feature for search index even it has name and other useful types.
  // Useful for mixed features, sponsored objects.
  bool SkipAlways(feature::TypesHolder const & types) const;

  // Skip "entrance" only features if they have no name or a number ref only.
  bool SkipSpecialNames(feature::TypesHolder const & types, std::string_view defName) const;

private:
  using Cont = buffer_vector<uint32_t, 16>;

  static bool HasType(Cont const & v, uint32_t t);

  // Array index (0, 1) means type level for checking (1, 2).
  Cont m_skipIfEmptyName[2];
  Cont m_skipAlways[1];
  Cont m_skipSpecialNames[1];

  ftypes::TwoLevelPOIChecker m_isPoi;
};
}  // namespace search
