#include "search/search_quality/matcher.hpp"

#include "search/feature_loader.hpp"
#include "search/house_numbers_matcher.hpp"

#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/search_string_utils.hpp"

#include "base/assert.hpp"
#include "base/control_flow.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <algorithm>

namespace search
{
namespace
{
template <typename Iter>
bool StartsWithHouseNumber(Iter beg, Iter end)
{
  using namespace search::house_numbers;

  std::string s;
  for (auto it = beg; it != end; ++it)
  {
    if (!s.empty())
      s.append(" ");
    s.append(*it);
    if (LooksLikeHouseNumber(s, false /* isPrefix */))
      return true;
  }
  return false;
}

// todo(@m) This function looks very slow.
template <typename Iter>
bool EndsWithHouseNumber(Iter beg, Iter end)
{
  using namespace search::house_numbers;

  if (beg == end)
    return false;

  std::string s;
  for (auto it = --end;; --it)
  {
    if (s.empty())
      s = *it;
    else
      s = *it + " " + s;
    if (LooksLikeHouseNumber(s, false /* isPrefix */))
      return true;
    if (it == beg)
      break;
  }
  return false;
}

bool StreetMatches(std::string_view name, std::vector<std::string> const & queryTokens)
{
  auto const nameTokens = search::NormalizeAndTokenizeAsUtf8(name);

  if (nameTokens.empty())
    return false;

  for (size_t i = 0; i + nameTokens.size() <= queryTokens.size(); ++i)
  {
    bool found = true;
    for (size_t j = 0; j < nameTokens.size(); ++j)
    {
      if (queryTokens[i + j] != nameTokens[j])
      {
        found = false;
        break;
      }
    }

    if (!found)
      continue;

    if (!EndsWithHouseNumber(queryTokens.begin(), queryTokens.begin() + i) &&
        !StartsWithHouseNumber(queryTokens.begin() + i + nameTokens.size(), queryTokens.end()))
    {
      return true;
    }
  }

  return false;
}
}  // namespace

Matcher::Matcher(FeatureLoader & loader) : m_loader(loader) {}

void Matcher::Match(Sample const & goldenSample, std::vector<Result> const & actual,
                    std::vector<size_t> & goldenMatching, std::vector<size_t> & actualMatching)
{
  auto const & golden = goldenSample.m_results;

  auto const n = golden.size();
  auto const m = actual.size();

  goldenMatching.assign(n, kInvalidId);
  actualMatching.assign(m, kInvalidId);

  // TODO (@y, @m): use Kuhn algorithm here for maximum matching.
  for (size_t i = 0; i < n; ++i)
  {
    if (goldenMatching[i] != kInvalidId)
      continue;
    auto const & g = golden[i];

    for (size_t j = 0; j < m; ++j)
    {
      if (actualMatching[j] != kInvalidId)
        continue;

      auto const & a = actual[j];
      if (Matches(goldenSample.m_query, g, a))
      {
        goldenMatching[i] = j;
        actualMatching[j] = i;
        break;
      }
    }
  }
}

bool Matcher::Matches(strings::UniString const & query, Sample::Result const & golden,
                      search::Result const & actual)
{
  if (actual.GetResultType() != Result::Type::Feature)
    return false;

  auto ft = m_loader.Load(actual.GetFeatureID());
  if (!ft)
    return false;

  return Matches(query, golden, *ft);
}

bool Matcher::Matches(strings::UniString const & query, Sample::Result const & golden,
                      FeatureType & ft)
{
  static double constexpr kToleranceMeters = 50;

  auto const houseNumber = ft.GetHouseNumber();

  auto const queryTokens = NormalizeAndTokenizeAsUtf8(ToUtf8(query));

  bool nameMatches = false;

  // The golden result may have an empty name. What is more likely, though, is that the
  // sample was not obtained as a result of a previous run of our search_quality tools.
  // Probably it originates from a third-party source.
  if (golden.m_name.empty())
  {
    if (ft.GetGeomType() == feature::GeomType::Line)
    {
      nameMatches = StreetMatches(ft.GetRoadNumber(), queryTokens);
    }
    else
    {
      // Don't try to guess: it's enough to match by distance.
      // |ft| with GeomType::Point here is usually a POI and |ft| with GeomType::Area is a building.
      nameMatches = true;
    }
  }

  ft.ForEachName([&queryTokens, &ft, &golden, &nameMatches](int8_t /* lang */, std::string_view name)
  {
    if (NormalizeAndSimplifyString(ToUtf8(golden.m_name)) == NormalizeAndSimplifyString(name))
    {
      nameMatches = true;
      return base::ControlFlow::Break;
    }

    if (golden.m_name.empty() && ft.GetGeomType() == feature::GeomType::Line &&
        StreetMatches(name, queryTokens))
    {
      nameMatches = true;
      return base::ControlFlow::Break;
    }

    return base::ControlFlow::Continue;
  });

  bool houseNumberMatches = true;
  if (!golden.m_houseNumber.empty() && !houseNumber.empty())
    houseNumberMatches = golden.m_houseNumber == houseNumber;

  return (nameMatches && houseNumberMatches &&
          feature::GetMinDistanceMeters(ft, golden.m_pos) < kToleranceMeters);
}
}  // namespace search
