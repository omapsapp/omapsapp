#include "search/intermediate_result.hpp"

#include "search/reverse_geocoder.hpp"

#include "storage/country_info_getter.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/road_shields_parser.hpp"

#include "platform/measurement_utils.hpp"
#include "platform/localization.hpp"

#include "base/string_utils.hpp"

#include <algorithm>

#include "3party/opening_hours/opening_hours.hpp"

namespace search
{
using namespace std;

namespace
{
class SkipRegionInfo
{
  static size_t const kCount = 2;
  uint32_t m_types[kCount];

public:
  SkipRegionInfo()
  {
    base::StringIL arr[] = {
      {"place", "continent"},
      {"place", "country"}
    };
    static_assert(kCount == ARRAY_SIZE(arr), "");

    Classificator const & c = classif();
    for (size_t i = 0; i < kCount; ++i)
      m_types[i] = c.GetTypeByPath(arr[i]);
  }

  bool IsSkip(uint32_t type) const
  {
    for (uint32_t t : m_types)
    {
      if (t == type)
        return true;
    }
    return false;
  }
};
}  // namespace

// PreRankerResult ---------------------------------------------------------------------------------
PreRankerResult::PreRankerResult(FeatureID const & id, PreRankingInfo const & info,
                                 vector<ResultTracer::Branch> const & provenance)
: m_id(id), m_info(info)
, m_isRelaxed(base::IsExist(provenance, ResultTracer::Branch::Relaxed))
#ifdef SEARCH_USE_PROVENANCE
, m_provenance(provenance)
#endif
{
  ASSERT(m_id.IsValid(), ());

  m_matchedTokensNumber = 0;
  for (auto const & r : m_info.m_tokenRanges)
    m_matchedTokensNumber += r.Size();
}

// static
bool PreRankerResult::LessRankAndPopularity(PreRankerResult const & lhs, PreRankerResult const & rhs)
{
  if (lhs.m_info.m_rank != rhs.m_info.m_rank)
    return lhs.m_info.m_rank > rhs.m_info.m_rank;
  if (lhs.m_info.m_popularity != rhs.m_info.m_popularity)
    return lhs.m_info.m_popularity > rhs.m_info.m_popularity;

  /// @todo Remove this epilog when we will have _enough_ ranks and popularities in data.
  return lhs.m_info.m_distanceToPivot < rhs.m_info.m_distanceToPivot;
}

// static
bool PreRankerResult::LessDistance(PreRankerResult const & lhs, PreRankerResult const & rhs)
{
  return lhs.m_info.m_distanceToPivot < rhs.m_info.m_distanceToPivot;
}

// static
int PreRankerResult::CompareByTokensMatch(PreRankerResult const & lhs, PreRankerResult const & rhs)
{
  if (lhs.m_info.m_isCommonMatchOnly != rhs.m_info.m_isCommonMatchOnly)
    return rhs.m_info.m_isCommonMatchOnly ? -1 : 1;

  auto const & lRange = lhs.m_info.InnermostTokenRange();
  auto const & rRange = rhs.m_info.InnermostTokenRange();

  if (lRange.Size() != rRange.Size())
    return lRange.Size() > rRange.Size() ? -1 : 1;

  if (lhs.m_matchedTokensNumber != rhs.m_matchedTokensNumber)
     return lhs.m_matchedTokensNumber > rhs.m_matchedTokensNumber ? -1 : 1;

  if (lRange.Begin() != rRange.Begin())
    return lRange.Begin() < rRange.Begin() ? -1 : 1;

  return 0;
}

// static
bool PreRankerResult::LessByExactMatch(PreRankerResult const & lhs, PreRankerResult const & rhs)
{
  bool const lScore = lhs.m_info.m_exactMatch && lhs.m_info.m_allTokensUsed;
  bool const rScore = rhs.m_info.m_exactMatch && rhs.m_info.m_allTokensUsed;
  if (lScore != rScore)
    return lScore;

  return CompareByTokensMatch(lhs, rhs) == -1;
}

bool PreRankerResult::CategoriesComparator::operator()(PreRankerResult const & lhs,
                                                       PreRankerResult const & rhs) const
{
  if (m_positionIsInsideViewport)
    return lhs.GetDistance() < rhs.GetDistance();

  if (m_detailedScale)
  {
    bool const lhsInside = m_viewport.IsPointInside(lhs.GetInfo().m_center);
    bool const rhsInside = m_viewport.IsPointInside(rhs.GetInfo().m_center);
    if (lhsInside && !rhsInside)
      return true;
    if (rhsInside && !lhsInside)
      return false;
  }
  return lhs.GetPopularity() > rhs.GetPopularity();
}

std::string DebugPrint(PreRankerResult const & r)
{
  ostringstream os;
  os << "PreRankerResult "
     << "{ FID: " << r.GetId().m_index    // index is enough here for debug purpose
     << "; m_matchedTokensNumber: " << r.m_matchedTokensNumber
     << "; m_isRelaxed: " << r.m_isRelaxed
     << "; " << DebugPrint(r.m_info)
     << " }";
  return os.str();
}

// RankerResult ------------------------------------------------------------------------------------
RankerResult::RankerResult(FeatureType & ft, m2::PointD const & center,
                           string displayName, string const & fileName)
  : m_types(ft)
  , m_str(std::move(displayName))
  , m_id(ft.GetID())
  , m_resultType(ftypes::IsBuildingChecker::Instance()(m_types) ? Type::Building : Type::Feature)
  , m_geomType(ft.GetGeomType())
{
  ASSERT(m_id.IsValid(), ());
  ASSERT(!m_types.Empty(), ());

  m_types.SortBySpec();

  m_region.SetParams(fileName, center);

  FillDetails(ft, m_str, m_details);
}

RankerResult::RankerResult(FeatureType & ft, std::string const & fileName)
  : RankerResult(ft, feature::GetCenter(ft, FeatureType::WORST_GEOMETRY),
                 std::string(ft.GetReadableName()), fileName)
{
}

RankerResult::RankerResult(double lat, double lon)
  : m_str("(" + measurement_utils::FormatLatLon(lat, lon) + ")"), m_resultType(Type::LatLon)
{
  m_region.SetParams({}, mercator::FromLatLon(lat, lon));
}

RankerResult::RankerResult(m2::PointD const & coord, string_view postcode)
  : m_str(postcode), m_resultType(Type::Postcode)
{
  strings::AsciiToUpper(m_str);
  m_region.SetParams({}, coord);
}

bool RankerResult::GetCountryId(storage::CountryInfoGetter const & infoGetter, uint32_t ftype,
                                storage::CountryId & countryId) const
{
  static SkipRegionInfo const checker;
  if (checker.IsSkip(ftype))
    return false;
  return m_region.GetCountryId(infoGetter, countryId);
}

bool RankerResult::IsEqualBasic(RankerResult const & r) const
{
  return (m_geomType == r.m_geomType &&
          GetRankingInfo().m_type == r.GetRankingInfo().m_type &&
          m_str == r.m_str);
}

bool RankerResult::IsEqualCommon(RankerResult const & r) const
{
  return (IsEqualBasic(r) && GetBestType() == r.GetBestType());
}

bool RankerResult::IsStreet() const { return ftypes::IsStreetOrSquareChecker::Instance()(m_types); }

uint32_t RankerResult::GetBestType(vector<uint32_t> const * preferredTypes /* = nullptr */) const
{
  if (preferredTypes)
  {
    ASSERT(is_sorted(preferredTypes->begin(), preferredTypes->end()), ());
    for (uint32_t type : m_types)
    {
      if (binary_search(preferredTypes->begin(), preferredTypes->end(), type))
        return type;
    }
  }

  return m_types.GetBestType();
}

// RankerResult::RegionInfo ------------------------------------------------------------------------
bool RankerResult::RegionInfo::GetCountryId(storage::CountryInfoGetter const & infoGetter,
                                            storage::CountryId & countryId) const
{
  if (!m_countryId.empty())
  {
    countryId = m_countryId;
    return true;
  }

  auto const id = infoGetter.GetRegionCountryId(m_point);
  if (id != storage::kInvalidCountryId)
  {
    countryId = id;
    return true;
  }

  return false;
}

// Functions ---------------------------------------------------------------------------------------
void FillDetails(FeatureType & ft, std::string const & name, Result::Details & details)
{
  if (details.m_isInitialized)
    return;

  std::string_view airportIata = ft.GetMetadata(feature::Metadata::FMD_AIRPORT_IATA);

  std::string brand {ft.GetMetadata(feature::Metadata::FMD_BRAND)};
  if (!brand.empty())
    brand = platform::GetLocalizedBrandName(brand);

  if (name == brand)
    brand.clear();

  /// @todo Avoid temporary string when OpeningHours (boost::spirit) will allow string_view.
  std::string const openHours(ft.GetMetadata(feature::Metadata::FMD_OPEN_HOURS));
  if (!openHours.empty())
  {
    using namespace osmoh;
    OpeningHours const oh((std::string(openHours)));
    if (oh.IsValid())
    {
      /// @todo We should check closed/open time for specific feature's timezone.
      time_t const now = time(nullptr);
      auto const info = oh.GetInfo(now);
      if (info.state != RuleState::Unknown)
      {
        // In else case value is osm::Unknown, it's set in preview's constructor.
        details.m_isOpenNow = (info.state == RuleState::Open) ? osm::Yes : osm::No;

        details.m_minutesUntilOpen = (info.nextTimeOpen - now) / 60;
        details.m_minutesUntilClosed = (info.nextTimeClosed - now) / 60;
      }
    }
  }

  feature::TypesHolder const typesHolder(ft);

  std::string stars;
  uint8_t starsCount = 0;
  bool const isHotel = ftypes::IsHotelChecker::Instance()(typesHolder);
  if (isHotel && strings::to_uint(ft.GetMetadata(feature::Metadata::FMD_STARS), starsCount))
    stars = feature::FormatStars(starsCount);

  auto const cuisines = feature::GetLocalizedCuisines(typesHolder);
  auto const cuisine = strings::JoinStrings(cuisines, feature::kFieldsSeparator);

  auto const recycling = strings::JoinStrings(feature::GetLocalizedRecyclingTypes(typesHolder), feature::kFieldsSeparator);

  auto const roadShields = ftypes::GetRoadShieldsNames(ft);
  auto const roadShield = strings::JoinStrings(roadShields, feature::kFieldsSeparator);

  auto const fee = feature::GetLocalizedFeeType(typesHolder);

  auto const elevation = feature::FormatElevation(ft.GetMetadata(feature::Metadata::FMD_ELE));

  std::string description;

  auto const append = [&description](std::string_view sv)
  {
    if (sv.empty())
      return;
    if (!description.empty())
      description += feature::kFieldsSeparator;
    description += sv;
  };

  append(stars);
  append(airportIata);
  append(roadShield);
  append(brand);
  append(elevation);
  append(cuisine);
  append(recycling);
  append(fee);

  details.m_description = std::move(description);

  details.m_isInitialized = true;
}

string DebugPrint(RankerResult const & r)
{
  stringstream ss;
  ss << "RankerResult "
     << "{ FID: " << r.GetID().m_index    // index is enough here for debug purpose
     << "; Name: " << r.GetName()
     << "; Type: " << classif().GetReadableObjectName(r.GetBestType())
     << "; Linear model rank: " << r.GetLinearModelRank();

#ifdef SEARCH_USE_PROVENANCE
    if (!r.m_provenance.empty())
      ss << "; Provenance: " << ::DebugPrint(r.m_provenance);
#endif

  if (r.m_dbgInfo)
    ss << "; " << DebugPrint(*r.m_dbgInfo);
  else
    ss << "; " << DebugPrint(r.GetRankingInfo());

  ss << " }";
  return ss.str();
}
}  // namespace search
