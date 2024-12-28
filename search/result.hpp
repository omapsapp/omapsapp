#pragma once

#include "search/bookmarks/results.hpp"
#include "search/ranking_info.hpp"

#include "indexer/feature_decl.hpp"

#include "editor/yes_no_unknown.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/checked_cast.hpp"
#include "base/buffer_vector.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

// Define this option to DebugPrint provenance.
#ifdef DEBUG
//#define SEARCH_USE_PROVENANCE
#endif

namespace search
{
// Search result. Search returns a list of them, ordered by score.
//
// TODO (@y, @m): split this class to small classes by type, replace
// Results::m_results by vectors corresponding to types.
class Result
{
public:
  enum class Type
  {
    Feature,
    LatLon,
    PureSuggest,
    SuggestFromFeature,
    Postcode
  };

  // Search results details. Considered valid if GetResultType() == Type::Feature.
  struct Details
  {
    osm::YesNoUnknown m_isOpenNow = osm::Unknown;

    uint16_t m_minutesUntilOpen = 0;

    uint16_t m_minutesUntilClosed = 0;

    std::string m_description;

    bool m_isInitialized = false;
  };

  // Min distance to search result when popularity label has a higher priority (in meters).
  static auto constexpr kPopularityHighPriorityMinDistance = 50000.0;

  Result(m2::PointD const & pt, std::string const & name) : m_center(pt), m_str(name) {}
  void FromFeature(FeatureID const & id, uint32_t mainType, uint32_t matchedType, Details const & details);

  void SetAddress(std::string && address) { m_address = std::move(address); }
  void SetType(Result::Type type) { m_resultType = type; }

  // For Type::PureSuggest.
  Result(std::string str, std::string && suggest);

  // For Type::SuggestFromFeature.
  Result(Result && res, std::string && suggest);

  Type GetResultType() const { return m_resultType; }

  std::string const & GetString() const { return m_str; }
  std::string const & GetAddress() const { return m_address; }
  std::string const & GetDescription() const { return m_details.m_description; }

  osm::YesNoUnknown IsOpenNow() const { return m_details.m_isOpenNow; }
  uint16_t GetMinutesUntilOpen() const { return m_details.m_minutesUntilOpen; }
  uint16_t GetMinutesUntilClosed() const { return m_details.m_minutesUntilClosed; }

  bool IsSuggest() const;
  bool HasPoint() const;

  // Feature id in mwm.
  // Precondition: GetResultType() == Type::Feature.
  FeatureID const & GetFeatureID() const;

  // Precondition: GetResultType() == Type::Feature.
  uint32_t GetFeatureType() const;
  bool IsSameType(uint32_t type) const;

  std::string GetLocalizedFeatureType() const;
  // Secondary title for the result.
  std::string GetFeatureDescription() const;

  // Center point of a feature.
  // Precondition: HasPoint() == true.
  m2::PointD GetFeatureCenter() const;

  // String to write in the search box.
  // Precondition: IsSuggest() == true.
  std::string const & GetSuggestionString() const;

  bool IsEqualSuggest(Result const & r) const;
  bool IsEqualFeature(Result const & r) const;

  void AddHighlightRange(std::pair<uint16_t, uint16_t> const & range);
  void AddDescHighlightRange(const std::pair<uint16_t, uint16_t> & range);
  std::pair<uint16_t, uint16_t> const & GetHighlightRange(size_t idx) const;
  size_t GetHighlightRangesCount() const { return m_hightlightRanges.size(); }
  
  //returns ranges to hightlight in address
  std::pair<uint16_t, uint16_t> const & GetDescHighlightRange(size_t idx) const;
  size_t GetDescHighlightRangesCount() const { return m_descHightlightRanges.size(); }

  void PrependCity(std::string_view name);

  int32_t GetPositionInResults() const { return m_positionInResults; }
  void SetPositionInResults(int32_t pos) { m_positionInResults = pos; }

  /// @name Used for debug logs and tests only.
  /// @{
  RankingInfo const & GetRankingInfo() const
  {
    CHECK(m_dbgInfo, ());
    return *m_dbgInfo;
  }
  void SetRankingInfo(std::shared_ptr<RankingInfo> info) { m_dbgInfo = std::move(info); }
  /// @}

#ifdef SEARCH_USE_PROVENANCE
  template <typename Prov>
  void SetProvenance(Prov && prov)
  {
    m_provenance = std::forward<Prov>(prov);
  }
#endif

  // Returns a representation of this result that is sent to the
  // statistics servers and later used to measure the quality of our
  // search engine.
  std::string ToStringForStats() const;

  friend std::string DebugPrint(search::Result const & result);

private:
  Type m_resultType;

  FeatureID m_id;
  m2::PointD m_center;
  std::string m_str;
  std::string m_address;
  uint32_t m_mainType = 0;
  uint32_t m_matchedType = 0;
  std::string m_suggestionStr;
  buffer_vector<std::pair<uint16_t, uint16_t>, 4> m_hightlightRanges;
  buffer_vector<std::pair<uint16_t, uint16_t>, 4> m_descHightlightRanges;

  std::shared_ptr<RankingInfo> m_dbgInfo;   // used in debug logs and tests, nullptr in production

  // The position that this result occupied in the vector returned by
  // a search query. -1 if undefined.
  int32_t m_positionInResults = -1;

#ifdef SEARCH_USE_PROVENANCE
  std::vector<ResultTracer::Branch> m_provenance;
#endif

public:
  // Careful when moving: the order of destructors is important.
  Details m_details;
};

std::string DebugPrint(search::Result::Type type);

class Results
{
public:
  using ConstIter = std::vector<Result>::const_iterator;

  enum class Type
  {
    Default,
    Hotels
  };

  Results();

  bool IsEndMarker() const { return m_status != Status::None; }
  bool IsEndedNormal() const { return m_status == Status::EndedNormal; }
  bool IsEndedCancelled() const { return m_status == Status::EndedCancelled; }

  void SetEndMarker(bool cancelled)
  {
    m_status = cancelled ? Status::EndedCancelled : Status::EndedNormal;
  }

  // Used for results in the list.
  bool AddResult(Result && result);

  // Fast version of AddResult() that doesn't do any checks for duplicates.
  // Used for results in the viewport.
  void AddResultNoChecks(Result && result);

  void AddBookmarkResult(bookmarks::Result const & result);

  void Clear();

  ConstIter begin() const { return m_results.cbegin(); }
  ConstIter end() const { return m_results.cend(); }

  size_t GetCount() const { return m_results.size(); }
  size_t GetSuggestsCount() const;

  Result const & operator[](size_t i) const
  {
    ASSERT_LESS(i, m_results.size(), ());
    return m_results[i];
  }

  bookmarks::Results const & GetBookmarksResults() const;

  /// @deprecated Fucntion is obsolete (used in tests) and doesn't take into account bookmarks.
  template <typename Fn>
  void SortBy(Fn && comparator)
  {
    std::sort(m_results.begin(), m_results.end(), comparator);
    for (size_t i = 0; i < m_results.size(); ++i)
      m_results[i].SetPositionInResults(base::asserted_cast<uint32_t>(i));
  }

private:
  enum class Status
  {
    None,
    EndedCancelled,
    EndedNormal
  };

  // Inserts |result| in |m_results| at position denoted by |where|.
  //
  // *NOTE* all iterators, references and pointers to |m_results| are
  // invalid after the call.
  void InsertResult(std::vector<Result>::iterator where, Result && result);

  std::vector<Result> m_results;
  bookmarks::Results m_bookmarksResults;
  Status m_status;
};

std::string DebugPrint(search::Results const & results);
}  // namespace search
