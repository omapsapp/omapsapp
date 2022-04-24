#pragma once

#include "search/pre_ranking_info.hpp"
#include "search/ranking_info.hpp"
#include "search/ranking_utils.hpp"
#include "search/result.hpp"

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

  static bool LessRankAndPopularity(PreRankerResult const & lhs, PreRankerResult const & rhs);
  static bool LessDistance(PreRankerResult const & lhs, PreRankerResult const & rhs);
  static bool LessByExactMatch(PreRankerResult const & lhs, PreRankerResult const & rhs);

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

  size_t GetInnermostTokensNumber() const { return m_info.InnermostTokenRange().Size(); }
  size_t GetMatchedTokensNumber() const { return m_matchedTokensNumber; }
  bool IsNotRelaxed() const { return !m_isRelaxed; }

  void SetRank(uint8_t rank) { m_info.m_rank = rank; }
  void SetPopularity(uint8_t popularity) { m_info.m_popularity = popularity; }
  void SetDistanceToPivot(double distance) { m_info.m_distanceToPivot = distance; }
  void SetCenter(m2::PointD const & center)
  {
    m_info.m_center = center;
    m_info.m_centerLoaded = true;
  }

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
  enum class Type
  {
    LatLon,
    Feature,
    Building,  //!< Buildings are not filtered out in duplicates filter.
    Postcode
  };

  /// For Type::Feature and Type::Building.
  RankerResult(FeatureType & f, m2::PointD const & center, m2::PointD const & pivot,
               std::string displayName, std::string const & fileName);
  RankerResult(FeatureType & ft, m2::PointD const & pivot, std::string const & fileName);

  /// For Type::LatLon.
  RankerResult(double lat, double lon);

  /// For Type::Postcode.
  RankerResult(m2::PointD const & coord, std::string_view postcode);

  bool IsStreet() const;

  RankingInfo const & GetRankingInfo() const { return m_info; }
  void SetRankingInfo(RankingInfo & info)
  {
    // No sense to make move for RankingInfo.
    m_info = info;
  }

  FeatureID const & GetID() const { return m_id; }
  std::string const & GetName() const { return m_str; }
  feature::TypesHolder const & GetTypes() const { return m_types; }
  Type const & GetResultType() const { return m_resultType; }
  m2::PointD GetCenter() const { return m_region.m_point; }
  double GetDistance() const { return m_distance; }
  feature::GeomType GetGeomType() const { return m_geomType; }
  Result::Details GetDetails() const { return m_details; }

  double GetDistanceToPivot() const { return m_info.m_distanceToPivot; }
  double GetLinearModelRank() const { return m_info.GetLinearModelRank(); }

  bool GetCountryId(storage::CountryInfoGetter const & infoGetter, uint32_t ftype,
                    storage::CountryId & countryId) const;

  bool IsEqualCommon(RankerResult const & r) const;

  uint32_t GetBestType(std::vector<uint32_t> const & preferredTypes = {}) const;

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
  FeatureID m_id;
  feature::TypesHolder m_types;
  std::string m_str;
  double m_distance = 0.0;
  Type m_resultType;
  RankingInfo m_info = {};
  feature::GeomType m_geomType = feature::GeomType::Undefined;
  Result::Details m_details;

#ifdef SEARCH_USE_PROVENANCE
  // The call path in the Geocoder that leads to this result.
  std::vector<ResultTracer::Branch> m_provenance;
#endif
};

void FillDetails(FeatureType & ft, Result::Details & meta);
}  // namespace search
