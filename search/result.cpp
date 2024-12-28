#include "search/result.hpp"

#include "search/common.hpp"
#include "search/geometry_utils.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_utils.hpp"

#include "platform/localization.hpp"

#include "geometry/mercator.hpp"

#include "base/string_utils.hpp"

#include <sstream>

namespace search
{
using namespace std;

void Result::FromFeature(FeatureID const & id, uint32_t mainType, uint32_t matchedType, Details const & details)
{
  m_id = id;
  ASSERT(m_id.IsValid(), ());

  m_mainType = mainType;
  m_matchedType = matchedType;
  m_details = details;
  m_resultType = Type::Feature;
}

Result::Result(string str, string && suggest)
  : m_resultType(Type::PureSuggest), m_str(std::move(str)), m_suggestionStr(std::move(suggest))
{
}

Result::Result(Result && res, string && suggest)
  : m_id(std::move(res.m_id))
  , m_center(res.m_center)
  , m_str(std::move(res.m_str))
  , m_address(std::move(res.m_address))
  , m_matchedType(res.m_matchedType)
  , m_suggestionStr(std::move(suggest))
  , m_hightlightRanges(std::move(res.m_hightlightRanges))
{
  m_resultType = m_id.IsValid() ? Type::SuggestFromFeature : Type::PureSuggest;
}

bool Result::IsSuggest() const
{
  return m_resultType == Type::SuggestFromFeature || m_resultType == Type::PureSuggest;
}

bool Result::HasPoint() const
{
  return m_resultType != Type::PureSuggest;
}

FeatureID const & Result::GetFeatureID() const
{
  ASSERT_EQUAL(m_resultType, Type::Feature, ());
  return m_id;
}

uint32_t Result::GetFeatureType() const
{
  ASSERT_EQUAL(m_resultType, Type::Feature, ());
  return m_mainType;
}

bool Result::IsSameType(uint32_t type) const
{
  uint8_t const level = ftype::GetLevel(type);
  for (uint32_t t : { m_mainType, m_matchedType })
  {
    ftype::TruncValue(t, level);
    if (t == type)
      return true;
  }
  return false;
}

std::string GetLocalizedTypeName(uint32_t type)
{
  return platform::GetLocalizedTypeName(classif().GetReadableObjectName(type));
}

std::string Result::GetLocalizedFeatureType() const
{
  switch (m_resultType)
  {
  case Type::Feature: return GetLocalizedTypeName(m_mainType);
  case Type::Postcode: return platform::GetLocalizedString("postal_code");
  default: return {};
  }
}

std::string Result::GetFeatureDescription() const
{
  std::string res = GetLocalizedFeatureType();
  if (res.empty())
    return res;

  auto const append = [&res](std::string_view sv)
  {
    if (!res.empty())
      res += feature::kFieldsSeparator;
    res += sv;
  };

  // Clear, because GetLocalizedFeatureType will be shown as main title.
  if (m_str.empty())
    res.clear();

  if (m_mainType != m_matchedType && m_matchedType != 0)
    append(GetLocalizedTypeName(m_matchedType));

  if (!GetDescription().empty())
    append(GetDescription());

  return res;
}

m2::PointD Result::GetFeatureCenter() const
{
  ASSERT(HasPoint(), ());
  return m_center;
}

string const & Result::GetSuggestionString() const
{
  ASSERT(IsSuggest(), ());
  return m_suggestionStr;
}

bool Result::IsEqualSuggest(Result const & r) const
{
  return m_suggestionStr == r.m_suggestionStr;
}

bool Result::IsEqualFeature(Result const & r) const
{
  /// @todo Compare TruncValue(m_matchedType) ?
  if (m_resultType != r.m_resultType || m_matchedType != r.m_matchedType)
    return false;

  ASSERT_EQUAL(m_resultType, Result::Type::Feature, ());
  ASSERT(m_id.IsValid() && r.m_id.IsValid(), ());

  /// @todo Investigate why it is happens here?
  if (m_id == r.m_id)
    return true;

  if (m_str != r.m_str)
    return false;

  if (m_id.IsWorld() != r.m_id.IsWorld())
  {
    // Filter logically duplicating results from World.mwm and Country.mwm (like cities).
    return PointDistance(m_center, r.m_center) < 500.0;
  }

  // Filter stops (bus/tram), see BA_LasHeras test.
  if (ftypes::IsPublicTransportStopChecker::Instance()(m_matchedType))
    return PointDistance(m_center, r.m_center) < 150.0;

  /// @todo Keep this check until RemoveDuplicatingLinear will be fixed.
  // Filter same streets (with 'same logical street distance' threshold).
  if (ftypes::IsWayChecker::Instance().GetSearchRank(m_matchedType) != ftypes::IsWayChecker::Default)
    return PointDistance(m_center, r.m_center) < 2000.0;

  // Filter real duplicates when say area park is present in 2 MWMs, or OSM data duplicates.
  return m_address == r.m_address && PointDistance(m_center, r.m_center) < 10.0;
}

void Result::AddHighlightRange(pair<uint16_t, uint16_t> const & range)
{
  m_hightlightRanges.push_back(range);
}

void Result::AddDescHighlightRange(pair<uint16_t, uint16_t> const & range)
{
  m_descHightlightRanges.push_back(range);
}

pair<uint16_t, uint16_t> const & Result::GetHighlightRange(size_t idx) const
{
  ASSERT(idx < m_hightlightRanges.size(), ());
  return m_hightlightRanges[idx];
}
pair<uint16_t, uint16_t> const & Result::GetDescHighlightRange(size_t idx) const
{
  ASSERT(idx < m_descHightlightRanges.size(), ());
  return m_descHightlightRanges[idx];
}

void Result::PrependCity(string_view city)
{
  // It is expected that if |m_address| is not empty,
  // it starts with the region name. Avoid duplication
  // in the case where this region name coincides with
  // the city name and prepend otherwise.
  strings::SimpleTokenizer tok(m_address, ",");
  if (tok && *tok != city)
    m_address = std::string(city) + ", " + m_address;
}

string Result::ToStringForStats() const
{
  string readableType;
  if (GetResultType() == Type::Feature)
    readableType = classif().GetReadableObjectName(m_matchedType);

  string s;
  s.append(GetString());
  s.append("|");
  s.append(readableType);
  s.append("|");
  s.append(IsSuggest() ? "1" : "0");
  s.append("|");
  s.append(to_string(mercator::YToLat(m_center.y)));
  s.append("|");
  s.append(to_string(mercator::XToLon(m_center.x)));
  return s;
}

string DebugPrint(Result::Type type)
{
  switch (type)
  {
  case Result::Type::Feature: return "Feature";
  case Result::Type::LatLon: return "LatLon";
  case Result::Type::PureSuggest: return "PureSuggest";
  case Result::Type::SuggestFromFeature: return "SuggestFromFeature";
  case Result::Type::Postcode: return "Postcode";
  }

  return "Unknown";
}

string DebugPrint(Result const & result)
{
  string readableType;
  if (result.GetResultType() == Result::Type::Feature)
    readableType = classif().GetReadableObjectName(result.GetFeatureType());

  ostringstream os;
  os << "Result [";
  os << "name: " << result.GetString();
  os << ", type: " << readableType;
  os << ", info: " << DebugPrint(result.GetRankingInfo());

#ifdef SEARCH_USE_PROVENANCE
  if (!result.m_provenance.empty())
    os << ", provenance: " << ::DebugPrint(result.m_provenance);
#endif

  os << "]";
  return os.str();
}

// Results -----------------------------------------------------------------------------------------
Results::Results()
{
  Clear();
}

bool Results::AddResult(Result && result)
{
  // Find first feature result.
  auto it = find_if(m_results.begin(), m_results.end(),
                    [](Result const & r) { return r.GetResultType() == Result::Type::Feature; });

  if (result.IsSuggest())
  {
    if (static_cast<size_t>(std::distance(m_results.begin(), it)) >= kMaxNumSuggests)
      return false;

    for (auto i = m_results.begin(); i != it; ++i)
    {
      if (result.IsEqualSuggest(*i))
        return false;
    }
    InsertResult(it, std::move(result));
  }
  else
  {
    for (; it != m_results.end(); ++it)
    {
      if (result.IsEqualFeature(*it))
        return false;
    }
    InsertResult(m_results.end(), std::move(result));
  }

  return true;
}

void Results::AddResultNoChecks(Result && result)
{
  InsertResult(m_results.end(), std::move(result));
}

void Results::AddBookmarkResult(bookmarks::Result const & result)
{
  m_bookmarksResults.push_back(result);
}

void Results::Clear()
{
  m_results.clear();
  m_bookmarksResults.clear();
  m_status = Status::None;
}

size_t Results::GetSuggestsCount() const
{
  size_t i = 0;

  // Suggests always go first, so we need to compute length of prefix
  // of suggests.
  while (i < m_results.size() && m_results[i].IsSuggest())
    ++i;
  return i;
}

bookmarks::Results const & Results::GetBookmarksResults() const
{
  return m_bookmarksResults;
}

void Results::InsertResult(vector<Result>::iterator where, Result && result)
{
  ASSERT_LESS(m_results.size(), numeric_limits<int32_t>::max(), ());

  for (auto it = where; it != m_results.end(); ++it)
  {
    auto & r = *it;
    auto const position = r.GetPositionInResults();
    r.SetPositionInResults(position + 1);
  }

  result.SetPositionInResults(static_cast<int32_t>(distance(m_results.begin(), where)));
  m_results.insert(where, std::move(result));
}

string DebugPrint(search::Results const & results)
{
  return DebugPrintSequence(results.begin(), results.end());
}
}  // namespace search
