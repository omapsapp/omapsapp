#pragma once

#include "search/pre_ranking_info.hpp"
#include "search/ranking_info.hpp"
#include "search/result.hpp"
#include "search/tracer.hpp"

#include "storage/storage_defines.hpp"

#include "indexer/feature_data.hpp"

#include "geometry/point2d.hpp"

#include <string>
#include <vector>

class FeatureType;

namespace storage
{
class CountryInfoGetter;
struct CountryInfo;
}

namespace search
{
class ReverseGeocoder;

// First pass results class. Objects are created during search in trie.
// Works fast because it does not load features.
class PreRankerResult
{
public:
  PreRankerResult(FeatureID const & id, PreRankingInfo const & info,
                  std::vector<ResultTracer::Branch> const & provenance);

  /// @name Compare functions.
  /// @return true (-1) if lhs is better (less in sort) than rhs.
  /// @{
  static bool LessRankAndPopularity(PreRankerResult const & lhs, PreRankerResult const & rhs);
  static bool LessDistance(PreRankerResult const & lhs, PreRankerResult const & rhs);
  static int CompareByTokensMatch(PreRankerResult const & lhs, PreRankerResult const & rhs);
  static bool LessByExactMatch(PreRankerResult const & lhs, PreRankerResult const & rhs);
  /// @}

  struct CategoriesComparator
  {
    bool operator()(PreRankerResult const & lhs, PreRankerResult const & rhs) const;

    m2::RectD m_viewport;
    bool m_positionIsInsideViewport = false;
    bool m_detailedScale = false;
  };

  FeatureID const & GetId() const { return m_id; }
  double GetDistance() const { return m_info.m_distanceToPivot; }
  uint8_t GetRank() const { return m_info.m_rank; }
  uint8_t GetPopularity() const { return m_info.m_popularity; }
  PreRankingInfo const & GetInfo() const { return m_info; }

#ifdef SEARCH_USE_PROVENANCE
  std::vector<ResultTracer::Branch> const & GetProvenance() const { return m_provenance; }
#endif

  //size_t GetInnermostTokensNumber() const { return m_info.InnermostTokenRange().Size(); }
  //size_t GetMatchedTokensNumber() const { return m_matchedTokensNumber; }
  bool IsNotRelaxed() const { return !m_isRelaxed; }

  bool SkipForViewportSearch(size_t queryTokensNumber) const
  {
    return m_isRelaxed || m_matchedTokensNumber + 1 < queryTokensNumber;
  }

  void SetRank(uint8_t rank) { m_info.m_rank = rank; }
  void SetPopularity(uint8_t popularity) { m_info.m_popularity = popularity; }
  void SetDistanceToPivot(double distance) { m_info.m_distanceToPivot = distance; }
  void SetCenter(m2::PointD const & center)
  {
    m_info.m_center = center;
    m_info.m_centerLoaded = true;
  }

  friend std::string DebugPrint(PreRankerResult const & r);

private:
  FeatureID m_id;
  PreRankingInfo m_info;

  size_t m_matchedTokensNumber;
  bool m_isRelaxed;

#ifdef SEARCH_USE_PROVENANCE
  // The call path in the Geocoder that leads to this result.
  std::vector<ResultTracer::Branch> m_provenance;
#endif
};

// Second result class. Objects are created during reading of features.
// Read and fill needed info for ranking and getting final results.
class RankerResult
{
public:
  enum class Type : uint8_t
  {
    LatLon = 0,
    Feature,
    Building,  //!< Buildings are not filtered out in duplicates filter.
    Postcode
  };

  /// For Type::Feature and Type::Building.
  RankerResult(FeatureType & ft, m2::PointD const & center,
               std::string displayName, std::string const & fileName);
  RankerResult(FeatureType & ft, std::string const & fileName);

  /// For Type::LatLon.
  RankerResult(double lat, double lon);

  /// For Type::Postcode.
  RankerResult(m2::PointD const & coord, std::string_view postcode);

  bool IsStreet() const;

  StoredRankingInfo const & GetRankingInfo() const { return m_info; }
  void SetRankingInfo(RankingInfo const & info, bool viewportMode)
  {
    m_finalRank = info.GetLinearModelRank(viewportMode);
    m_info = info;
  }

  FeatureID const & GetID() const { return m_id; }
  std::string const & GetName() const { return m_str; }
  feature::TypesHolder const & GetTypes() const { return m_types; }
  Type GetResultType() const { return m_resultType; }
  m2::PointD GetCenter() const { return m_region.m_point; }
  feature::GeomType GetGeomType() const { return m_geomType; }
  Result::Details GetDetails() const { return m_details; }

  double GetDistanceToPivot() const { return m_info.m_distanceToPivot; }
  double GetLinearModelRank() const { return m_finalRank; }

  bool GetCountryId(storage::CountryInfoGetter const & infoGetter, uint32_t ftype,
                    storage::CountryId & countryId) const;

  bool IsEqualBasic(RankerResult const & r) const;
  bool IsEqualCommon(RankerResult const & r) const;

  uint32_t GetBestType(std::vector<uint32_t> const * preferredTypes = nullptr) const;

#ifdef SEARCH_USE_PROVENANCE
  std::vector<ResultTracer::Branch> const & GetProvenance() const { return m_provenance; }
#endif

  friend std::string DebugPrint(RankerResult const & r);

private:
  friend class RankerResultMaker;
  friend class Ranker;

  struct RegionInfo
  {
    storage::CountryId m_countryId;
    m2::PointD m_point;

    void SetParams(storage::CountryId const & countryId, m2::PointD const & point)
    {
      m_countryId = countryId;
      m_point = point;
    }

    bool GetCountryId(storage::CountryInfoGetter const & infoGetter,
                      storage::CountryId & countryId) const;
  };

  RegionInfo m_region;
  feature::TypesHolder m_types;
  std::string m_str;
  Result::Details m_details;

  StoredRankingInfo m_info;
  std::shared_ptr<RankingInfo> m_dbgInfo;   // used in debug logs and tests, nullptr in production

  FeatureID m_id;
  double m_finalRank;

  Type m_resultType;
  feature::GeomType m_geomType = feature::GeomType::Undefined;

#ifdef SEARCH_USE_PROVENANCE
  // The call path in the Geocoder that leads to this result.
  std::vector<ResultTracer::Branch> m_provenance;
#endif
};

void FillDetails(FeatureType & ft, std::string const & name, Result::Details & details);
}  // namespace search
