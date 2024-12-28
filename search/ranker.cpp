#include "search/ranker.hpp"

#include "search/emitter.hpp"
#include "search/geometry_utils.hpp"
#include "search/highlighting.hpp"
#include "search/model.hpp"
#include "search/pre_ranking_info.hpp"
#include "search/ranking_utils.hpp"
#include "search/token_slice.hpp"

#include "indexer/brands_holder.hpp"
#include "indexer/data_source.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/road_shields_parser.hpp"
#include "indexer/search_string_utils.hpp"

#include "platform/localization.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <memory>
#include <optional>

namespace search
{
using namespace std;

namespace
{
template <typename Slice>
void UpdateNameScores(string_view name, uint8_t lang, Slice const & slice, NameScores & bestScores)
{
  if (StringUtf8Multilang::IsAltOrOldName(lang))
  {
    strings::Tokenize(name, ";", [&](string_view n)
    {
      bestScores.UpdateIfBetter(GetNameScores(n, lang, slice));
    });
  }
  else
  {
    bestScores.UpdateIfBetter(GetNameScores(name, lang, slice));
  }
}

template <typename Slice>
void UpdateNameScores(TokensVector & tokens, uint8_t lang, Slice const & slice, NameScores & bestScores)
{
  bestScores.UpdateIfBetter(GetNameScores(tokens, lang, slice));
}

// This function supports only street names like "abcdstrasse"/"abcd strasse".
/// @see Also FeatureNameInserter::AddDACHNames
vector<vector<strings::UniString>> ModifyDACHStreet(vector<strings::UniString> const & streetTokens)
{
  auto const size = streetTokens.size();
  ASSERT_GREATER(size, 0, ());

  vector<vector<strings::UniString>> result;
  for (auto const & sx : GetDACHStreets())
  {
    if (!strings::EndsWith(streetTokens.back(), sx.first))
      continue;

    if (streetTokens.back() == sx.first)
    {
      if (size == 1)
        return {};

      // "Abcd strasse" -> "abcdstrasse".
      result.emplace_back(streetTokens.begin(), streetTokens.end() - 1);
      result.back().back() += sx.first;

      // "Abcd strasse" -> "abcdstr".
      result.emplace_back(streetTokens.begin(), streetTokens.end() - 1);
      result.back().back() += sx.second;
      return result;
    }

    // "Abcdstrasse" -> "abcd strasse".
    auto const name = strings::UniString(streetTokens.back().begin(), streetTokens.back().end() - sx.first.size());
    result.push_back(streetTokens);
    result.back().back() = name;
    result.back().push_back(sx.first);

    // "Abcdstrasse" -> "abcdstr".
    result.push_back(streetTokens);
    result.back().back() = name + sx.second;
  }

  return result;
}

vector<strings::UniString> RemoveStreetSynonyms(vector<strings::UniString> const & tokens)
{
  vector<strings::UniString> res;
  for (auto const & e : tokens)
  {
    if (!IsStreetSynonym(e))
      res.push_back(e);
  }
  return res;
}

NameScores GetNameScores(FeatureType & ft, Geocoder::Params const & params,
                         TokenRange const & range, Model::Type type)
{
  NameScores bestScores;

  TokenSlice const slice(params, range);
  TokenSliceNoCategories const sliceNoCategories(params, range);

  for (auto const lang : params.GetLangs())
  {
    string_view const name = ft.GetName(lang);
    if (name.empty())
      continue;

    auto const updateScore = [&](string_view name)
    {
      TokensVector vec(name);
      // Name consists of stop words only.
      if (vec.Size() == 0)
        return;

      UpdateNameScores(vec, lang, slice, bestScores);
      UpdateNameScores(vec, lang, sliceNoCategories, bestScores);

      /// @todo
      /// 1. Make sure that this conversion also happens for Address and POI results,
      /// where street is only one component.
      /// 2. Make an optimization: If there are no synonyms or "strasse", skip this step.
      /// 3. Short street synonym should be scored like full synonym ("AV.SAENZ 459" -> "Avenida SAENZ 459")
      if (type == Model::TYPE_STREET)
      {
        // Searching for "Santa Fe" should rank "Avenida Santa Fe" like FULL_MATCH or FULL_PREFIX, but not SUBSTRING.
        {
          TokensVector cleaned(RemoveStreetSynonyms(vec.GetTokens()));
          UpdateNameScores(cleaned, lang, slice, bestScores);
          UpdateNameScores(cleaned, lang, sliceNoCategories, bestScores);
        }

        for (auto & variant : ModifyDACHStreet(vec.GetTokens()))
        {
          TokensVector modified(std::move(variant));
          UpdateNameScores(modified, lang, slice, bestScores);
          UpdateNameScores(modified, lang, sliceNoCategories, bestScores);
        }
      }
    };

    if (StringUtf8Multilang::IsAltOrOldName(lang))
    {
      strings::Tokenize(name, ";", [&updateScore](string_view n)
      {
        updateScore(n);
      });
    }
    else
    {
      updateScore(name);
    }
  }

  if (type == Model::TYPE_BUILDING)
  {
    if (ft.GetGeomType() == feature::GeomType::Line)
    {
      // Sometimes we can get linear matches with postcode (instead of house number) here.
      // Because of _fake_ TYPE_BUILDING layer in MatchPOIsAndBuildings.
      if (ftypes::IsAddressInterpolChecker::Instance()(ft))
      {
        // Separate case for addr:interpolation (Building + Line).
        ASSERT(!ft.GetRef().empty(), ());
        // Just assign SUBSTRING with no errors (was checked in HouseNumbersMatch).
        bestScores.UpdateIfBetter(NameScores(NameScore::SUBSTRING, ErrorsMade(0), false, 4));
      }
    }
    else
      UpdateNameScores(ft.GetHouseNumber(), StringUtf8Multilang::kDefaultCode, sliceNoCategories, bestScores);
  }

  if (ftypes::IsAirportChecker::Instance()(ft))
  {
    auto const iata = ft.GetMetadata(feature::Metadata::FMD_AIRPORT_IATA);
    if (!iata.empty())
      UpdateNameScores(iata, StringUtf8Multilang::kDefaultCode, sliceNoCategories, bestScores);
  }

  auto const op = ft.GetMetadata(feature::Metadata::FMD_OPERATOR);
  if (!op.empty())
    UpdateNameScores(op, StringUtf8Multilang::kDefaultCode, sliceNoCategories, bestScores);

  auto const brand = ft.GetMetadata(feature::Metadata::FMD_BRAND);
  if (!brand.empty())
  {
    indexer::ForEachLocalizedBrands(brand, [&](indexer::BrandsHolder::Brand::Name const & name)
    {
      UpdateNameScores(name.m_name, name.m_locale, sliceNoCategories, bestScores);
    });
  }

  if (type == Model::TYPE_STREET)
  {
    for (auto const & shield : ftypes::GetRoadShieldsNames(ft))
      UpdateNameScores(shield, StringUtf8Multilang::kDefaultCode, sliceNoCategories, bestScores);
  }

  return bestScores;
}

void RemoveDuplicatingLinear(vector<RankerResult> & results)
{
  // "Молодечно первомайская ул"; "Тюрли первомайская ул" should remain both :)
  double constexpr kDistSameStreetMeters = 3000.0;

  /// @todo This kind of filtering doesn't work properly in 2-dimension.
  /// - less makes X, Y, Z by GetLinearModelRank
  /// - equal doesn't recognize X == Z by PointDistance because of middle Y
  /// * Update Result::IsEqualFeature when will be fixed.

  auto const lessCmp = [](RankerResult const & r1, RankerResult const & r2)
  {
    if (r1.GetGeomType() != r2.GetGeomType())
      return r1.GetGeomType() < r2.GetGeomType();

    auto const & ri1 = r1.GetRankingInfo();
    auto const & ri2 = r2.GetRankingInfo();

    if (ri1.m_type != ri2.m_type)
      return ri1.m_type < ri2.m_type;

    if (r1.GetName() != r2.GetName())
      return r1.GetName() < r2.GetName();

    if (ri1.m_type == Model::TYPE_STREET)
    {
      if (ri1.m_classifType.street != ri2.m_classifType.street)
        return ri1.m_classifType.street < ri2.m_classifType.street;
    }
    else
    {
      uint32_t const t1 = r1.GetBestType();
      uint32_t const t2 = r2.GetBestType();
      if (t1 != t2)
        return t1 < t2;
    }

    // After unique, the better feature should be kept.
    return r1.GetLinearModelRank() > r2.GetLinearModelRank();
  };

  auto const equalCmp = [](RankerResult const & r1, RankerResult const & r2)
  {
    if (r1.GetGeomType() != feature::GeomType::Line || !r1.IsEqualBasic(r2))
      return false;

    auto const & ri1 = r1.GetRankingInfo();
    auto const & ri2 = r2.GetRankingInfo();
    if (ri1.m_type == Model::TYPE_STREET)
    {
      if (ri1.m_classifType.street != ri2.m_classifType.street)
        return false;
    }
    else
    {
      if (r1.GetBestType() != r2.GetBestType())
        return false;
    }

    // Note! Do compare for distance when filtering linear objects.
    // Otherwise we will skip the results for different parts of the map.
    return PointDistance(r1.GetCenter(), r2.GetCenter()) < kDistSameStreetMeters;
  };

  base::SortUnique(results, lessCmp, equalCmp);
}

ftypes::LocalityType GetLocalityIndex(feature::TypesHolder const & types)
{
  using namespace ftypes;

  // Inner logic of SearchAddress expects COUNTRY, STATE and CITY only.
  LocalityType const type = IsLocalityChecker::Instance().GetType(types);
  switch (type)
  {
  case LocalityType::None:
  case LocalityType::Country:
  case LocalityType::State:
  case LocalityType::City: return type;
  case LocalityType::Town: return LocalityType::City;
  case LocalityType::Village: return LocalityType::None;
  case LocalityType::Count: return type;
  }
  UNREACHABLE();
}

// TODO: Format street and house number according to local country's rules.
string FormatStreetAndHouse(ReverseGeocoder::Address const & addr)
{
  return addr.GetStreetName() + ", " + addr.GetHouseNumber();
}

// TODO: Share common formatting code for search results and place page.
string FormatFullAddress(ReverseGeocoder::Address const & addr, string const & region)
{
  /// @todo Print "near" for not exact addresses.
  /// Add some threshold for addr:interpolation or refactor ReverseGeocoder?
  if (addr.GetDistance() != 0)
    return region;

  return FormatStreetAndHouse(addr) + (region.empty() ? "" : ", ") + region;
}

}  // namespace

class RankerResultMaker
{
  ftypes::IsWayChecker const & m_wayChecker;
  ftypes::IsCapitalChecker const & m_capitalChecker;
  ftypes::IsCountryChecker const & m_countryChecker;

public:
  RankerResultMaker(Ranker & ranker, DataSource const & dataSource,
                    storage::CountryInfoGetter const & infoGetter,
                    ReverseGeocoder const & reverseGeocoder, Geocoder::Params const & params)
    : m_wayChecker(ftypes::IsWayChecker::Instance())
    , m_capitalChecker(ftypes::IsCapitalChecker::Instance())
    , m_countryChecker(ftypes::IsCountryChecker::Instance())
    , m_ranker(ranker)
    , m_dataSource(dataSource)
    , m_infoGetter(infoGetter)
    , m_reverseGeocoder(reverseGeocoder)
    , m_params(params)
    , m_isViewportMode(m_params.m_mode == Mode::Viewport)
  {
  }

  optional<RankerResult> operator()(PreRankerResult const & preResult)
  {
    m2::PointD center;
    string name;
    string country;

    auto ft = LoadFeature(preResult.GetId(), center, name, country);
    if (!ft)
      return {};

    RankerResult res(*ft, center, std::move(name), country);

    RankingInfo info;
    InitRankingInfo(*ft, center, preResult, info);

    if (info.m_type == Model::TYPE_STREET)
    {
      uint32_t const bestType = res.GetBestType(&m_params.m_preferredTypes);
      info.m_classifType.street = m_wayChecker.GetSearchRank(bestType);

      /// @see Arbat_Address test.
      // "2" is a NameScore::FULL_PREFIX for "2-й Обыденский переулок", which is *very* high,
      // and suppresses building's rank, matched by house number.
      if (info.m_nameScore > NameScore::SUBSTRING)
      {
        auto const & range = info.m_tokenRanges[info.m_type];
        if (range.Size() == 1 && m_params.IsNumberTokens(range))
          info.m_nameScore = NameScore::SUBSTRING;
      }
    }
    else if (m_params.IsCategorialRequest() && Model::IsPoiOrBuilding(info.m_type))
    {
      // Update info.m_classifType.poi with the _best preferred_ type. Important for categorial request,
      // when the Feature maybe a restaurant and a toilet simultaneously.
      uint32_t const bestType = res.GetBestType(&m_params.m_preferredTypes);
      feature::TypesHolder typesHolder;
      typesHolder.Assign(bestType);
      info.m_classifType.poi = GetPoiType(typesHolder);

      // We do not compare result name and request for categorial requests, but we prefer named features
      // for Eat, Hotel or Shop categories. Toilets, stops, defibrillators, ... are equal w/wo names.

      if (info.m_classifType.poi != PoiType::Eat &&
          info.m_classifType.poi != PoiType::Hotel &&
          info.m_classifType.poi != PoiType::ShopOrAmenity)
      {
        info.m_hasName = false;
      }
    }

    info.m_rank = NormalizeRank(info.m_rank, info.m_type, center, country,
                                m_capitalChecker(*ft), !info.m_allTokensUsed);

    if (preResult.GetInfo().m_isCommonMatchOnly)
    {
      // Count tokens in Feature's name.
      auto normalized = NormalizeAndSimplifyString(res.GetName());
      PreprocessBeforeTokenization(normalized);
      int count = 0;
      SplitUniString(normalized, [&count](strings::UniString const &)
      {
        ++count;
      }, Delimiters());

      // Factor is a number of the rest, not common matched tokens in Feature' name. Bigger is worse.
      // Example when count == 0: UTH airport has empty name, but "ut" is a _common_ token.
      info.m_commonTokensFactor = min(3, std::max(0, count - int(info.m_tokenRanges[info.m_type].Size())));
    }

    res.SetRankingInfo(info, m_isViewportMode);
    if (m_params.m_useDebugInfo)
      res.m_dbgInfo = std::make_shared<RankingInfo>(std::move(info));

#ifdef SEARCH_USE_PROVENANCE
    res.m_provenance = preResult.GetProvenance();
#endif

    return res;
  }

private:
  bool IsSameLoader(FeatureID const & id) const
  {
    return (m_loader && m_loader->GetId() == id.m_mwmId);
  }

  unique_ptr<FeatureType> LoadFeature(FeatureID const & id)
  {
    if (!IsSameLoader(id))
      m_loader = make_unique<FeaturesLoaderGuard>(m_dataSource, id.m_mwmId);
    return LoadFeatureImpl(id, *m_loader);
  }

  static unique_ptr<FeatureType> LoadFeatureImpl(FeatureID const & id, FeaturesLoaderGuard & loader)
  {
    auto ft = loader.GetFeatureByIndex(id.m_index);
    if (ft)
    {
      ASSERT(id.IsValid(), ());
      ft->SetID(id);
    }
    return ft;
  }

  bool GetExactAddress(FeatureType & ft, m2::PointD const & center, ReverseGeocoder::Address & addr) const
  {
    if (m_reverseGeocoder.GetExactAddress(ft, addr, true /* placeAsStreet */))
      return true;

    m_reverseGeocoder.GetNearbyAddress(center, 0.0 /* maxDistanceM */, addr, true /* placeAsStreet */);
    return addr.IsValid();
  }

  // For the best performance, incoming ids should be sorted by id.first (mwm file id).
  unique_ptr<FeatureType> LoadFeature(FeatureID const & id, m2::PointD & center, string & name,
                                      string & country)
  {
    auto ft = LoadFeature(id);
    if (!ft)
      return ft;

    // Country (region) name is a file name if feature isn't from World.mwm.
    ASSERT(m_loader && m_loader->GetId() == id.m_mwmId, ());
    if (m_loader->IsWorld())
      country.clear();
    else
      country = m_loader->GetCountryFileName();

    center = feature::GetCenter(*ft);
    m_ranker.GetBestMatchName(*ft, name);

    // Use brand instead of empty result name.
    if (!m_isViewportMode && name.empty())
    {
      std::string_view brand = (*ft).GetMetadata(feature::Metadata::FMD_BRAND);
      if (!brand.empty())
        name = platform::GetLocalizedBrandName(std::string{ brand });
    }

    // Insert exact address (street and house number) instead of empty result name.
    if (!m_isViewportMode && name.empty())
    {
      feature::TypesHolder featureTypes(*ft);
      featureTypes.SortBySpec();
      auto const bestType = featureTypes.GetBestType();
      auto const addressChecker = ftypes::IsAddressChecker::Instance();
    
      if (!addressChecker.IsMatched(bestType))
        return ft;

      ReverseGeocoder::Address addr;
      if (GetExactAddress(*ft, center, addr))
      {
        unique_ptr<FeatureType> streetFeature;

        // We can't change m_loader here, because of the following RankerResult. So do this trick:
        if (IsSameLoader(addr.m_street.m_id))
        {
          streetFeature = LoadFeatureImpl(addr.m_street.m_id, *m_loader);
        }
        else
        {
          auto loader = make_unique<FeaturesLoaderGuard>(m_dataSource, addr.m_street.m_id.m_mwmId);
          streetFeature = LoadFeatureImpl(addr.m_street.m_id, *loader);
        }

        if (streetFeature)
        {
          string streetName;
          m_ranker.GetBestMatchName(*streetFeature, streetName);
          name = streetName + ", " + addr.GetHouseNumber();
        }
      }
    }

    return ft;
  }

  void InitRankingInfo(FeatureType & ft, m2::PointD const & center, PreRankerResult const & res, RankingInfo & info)
  {
    auto const & preInfo = res.GetInfo();
    auto const & pivot = m_ranker.m_params.m_pivot;

    feature::TypesHolder featureTypes(ft);

    info.m_distanceToPivot = mercator::DistanceOnEarth(center, pivot);
    info.m_rank = preInfo.m_rank;
    info.m_popularity = preInfo.m_popularity;
    info.m_type = preInfo.m_type;
    if (Model::IsPoi(info.m_type))
      info.m_classifType.poi = GetPoiType(featureTypes);
    info.m_allTokensUsed = preInfo.m_allTokensUsed;
    info.m_numTokens = m_params.GetNumTokens();
    info.m_exactMatch = preInfo.m_exactMatch;
    info.m_categorialRequest = m_params.IsCategorialRequest();
    info.m_tokenRanges = preInfo.m_tokenRanges;

    size_t totalLength = 0;
    for (size_t i = 0; i < m_params.GetNumTokens(); ++i)
      totalLength += m_params.GetToken(i).GetOriginal().size();
    // Avoid division by zero.
    if (totalLength == 0)
      totalLength = 1;

    if (m_params.IsCategorialRequest())
    {
      info.m_hasName = ft.HasName();
      if (!info.m_hasName)
      {
        info.m_hasName = ft.HasMetadata(feature::Metadata::FMD_OPERATOR) ||
                         ft.HasMetadata(feature::Metadata::FMD_BRAND);
      }
    }
    else
    {
      auto const scores = GetNameScores(ft, m_params, preInfo.InnermostTokenRange(), info.m_type);

      auto nameScore = scores.m_nameScore;
      auto errorsMade = scores.m_errorsMade;
      bool isAltOrOldName = scores.m_isAltOrOldName;
      auto matchedLength = scores.m_matchedLength;

      auto const updateScoreForFeature = [&](FeatureType & ft, Model::Type type)
      {
        auto const & range = preInfo.m_tokenRanges[type];
        ASSERT(!range.Empty(), ());
        auto const scores = GetNameScores(ft, m_params, range, type);

        nameScore = std::min(nameScore, scores.m_nameScore);
        errorsMade += scores.m_errorsMade;
        if (scores.m_isAltOrOldName)
          isAltOrOldName = true;
        matchedLength += scores.m_matchedLength;
        return scores.m_nameScore;
      };

      auto const updateDependScore = [&](Model::Type type, uint32_t dependID)
      {
        if (info.m_type != type && dependID != IntersectionResult::kInvalidId)
        {
          if (auto p = LoadFeature({ ft.GetID().m_mwmId, dependID }))
            updateScoreForFeature(*p, type);
        }
      };

      updateDependScore(Model::TYPE_STREET, preInfo.m_geoParts.m_street);
      updateDependScore(Model::TYPE_SUBURB, preInfo.m_geoParts.m_suburb);
      updateDependScore(Model::TYPE_COMPLEX_POI, preInfo.m_geoParts.m_complexPoi);

      if (preInfo.m_cityId.IsValid())
      {
        if (auto city = LoadFeature(preInfo.m_cityId))
        {
          auto type = Model::TYPE_CITY;
          if (preInfo.m_tokenRanges[type].Empty())
            type = Model::TYPE_VILLAGE;
          else
          {
            /// @todo Possible match by city AND village? What will be in preInfo.m_cityId?
            ASSERT(preInfo.m_tokenRanges[Model::TYPE_VILLAGE].Empty(), ());
          }

          NameScore cityNameScore = NameScore::ZERO;
          if (Model::IsLocalityType(info.m_type))
          {
            // Hack to promote results like "Nice France".
            // Otherwise, POIs with "France" token in name around the "Nice" city will be always on top.
            if (preInfo.m_allTokensUsed && preInfo.m_cityId == res.GetId() &&
                !(preInfo.m_tokenRanges[Model::TYPE_STATE].Empty() &&
                  preInfo.m_tokenRanges[Model::TYPE_COUNTRY].Empty()))
            {
              cityNameScore = GetNameScores(ft, m_params, preInfo.m_tokenRanges[type], type).m_nameScore;
            }
          }
          else
            cityNameScore = updateScoreForFeature(*city, type);

          // Update distance with matched city pivot if we have a _good_ city name score.
          // A bit controversial distance score reset, but lets try.
          // Other option is to combine old pivot distance and city distance.
          // See 'Barcelona_Carrers' test.
          // "San Francisco" query should not update rank for "Francisco XXX" street in "San YYY" village.
          if (cityNameScore == NameScore::FULL_MATCH)
            info.m_distanceToPivot = mercator::DistanceOnEarth(center, city->GetCenter());
        }
      }

      info.m_nameScore = nameScore;
      info.m_errorsMade = errorsMade;
      info.m_isAltOrOldName = isAltOrOldName;
      info.m_matchedFraction = matchedLength / static_cast<float>(totalLength);

      /// @todo Also should add POI + nearby Street/Suburb/City.
      info.m_nearbyMatch = preInfo.m_geoParts.IsPoiAndComplexPoi();
    }

    CategoriesInfo const categoriesInfo(featureTypes,
                                        TokenSlice(m_params, preInfo.InnermostTokenRange()),
                                        m_ranker.m_params.m_categoryLocales, m_ranker.m_categories);

    info.m_pureCats = categoriesInfo.IsPureCategories();
    if (info.m_pureCats)
    {
      // Compare with previous values, in case if was assigned by street or locality.

      info.m_nameScore = NameScore::SUBSTRING;
      if (m_params.GetNumTokens() == preInfo.InnermostTokenRange().Size())
        info.m_nameScore = NameScore::FULL_PREFIX;

      ASSERT_LESS_OR_EQUAL(categoriesInfo.GetMatchedLength(), totalLength, (featureTypes));
      info.m_matchedFraction = std::max(info.m_matchedFraction,
                                        categoriesInfo.GetMatchedLength() / static_cast<float>(totalLength));
      if (!info.m_errorsMade.IsValid())
        info.m_errorsMade = ErrorsMade(0);
    }
    info.m_falseCats = categoriesInfo.IsFalseCategories();
  }

  uint16_t NormalizeRank(uint16_t rank, Model::Type type, m2::PointD const & center,
                        string const & country, bool isCapital, bool isRelaxed)
  {
    // Do not prioritize objects with population < 800. Same as RankToPopulation(rank) < 800, but faster.
    if (rank <= 70)
      return 0;

    if (isRelaxed)
      rank /= 5.0;

    switch (type)
    {
    case Model::TYPE_VILLAGE: return rank / 2.5;
    case Model::TYPE_CITY:
    {
      /// @todo Tried to reduce more (1.5), but important Famous_Cities_Rank test fails.
      if (isCapital || m_ranker.m_params.m_viewport.IsPointInside(center))
        return rank * 1.8;

      storage::CountryInfo info;
      if (country.empty())
        m_infoGetter.GetRegionInfo(center, info);
      else
        m_infoGetter.GetRegionInfo(country, info);
      if (info.IsNotEmpty() && info.m_name == m_ranker.m_params.m_pivotRegion)
        return rank * 1.7;

      // Fallthrough like "STATE" for cities without info.
    } [[fallthrough]];
    case Model::TYPE_STATE: return rank / 1.5;
    case Model::TYPE_COUNTRY: return rank;

    default: return rank;
    }
  }

  Ranker & m_ranker;
  DataSource const & m_dataSource;
  storage::CountryInfoGetter const & m_infoGetter;
  ReverseGeocoder const & m_reverseGeocoder;
  Geocoder::Params const & m_params;
  bool m_isViewportMode;

  unique_ptr<FeaturesLoaderGuard> m_loader;
};

Ranker::Ranker(DataSource const & dataSource, CitiesBoundariesTable const & boundariesTable,
               storage::CountryInfoGetter const & infoGetter, KeywordLangMatcher & keywordsScorer,
               Emitter & emitter, CategoriesHolder const & categories,
               vector<Suggest> const & suggests, VillagesCache & villagesCache,
               base::Cancellable const & cancellable)
  : m_reverseGeocoder(dataSource)
  , m_cancellable(cancellable)
  , m_keywordsScorer(keywordsScorer)
  , m_localities(dataSource, boundariesTable, villagesCache)
  , m_dataSource(dataSource)
  , m_infoGetter(infoGetter)
  , m_emitter(emitter)
  , m_categories(categories)
  , m_suggests(suggests)
{
}

void Ranker::Init(Params const & params, Geocoder::Params const & geocoderParams)
{
  m_params = params;
  m_geocoderParams = geocoderParams;
  m_preRankerResults.clear();
  m_tentativeResults.clear();
}

void Ranker::Finish(bool cancelled)
{
  // The results should be updated by Geocoder before Finish call.
  m_emitter.Finish(cancelled);
}

Result Ranker::MakeResult(RankerResult const & rankerResult, bool needAddress, bool needHighlighting) const
{
  Result res(rankerResult.GetCenter(), rankerResult.m_str);

  if (needAddress)
  {
    string address = GetLocalizedRegionInfoForResult(rankerResult);

    // Format full address only for suitable results.
    if (ftypes::IsAddressObjectChecker::Instance()(rankerResult.GetTypes()))
    {
      ReverseGeocoder::Address addr;
      if (!(rankerResult.GetID().IsValid() && m_reverseGeocoder.GetExactAddress(rankerResult.GetID(), addr)))
        m_reverseGeocoder.GetNearbyAddress(rankerResult.GetCenter(), addr);

      address = FormatFullAddress(addr, address);
    }

    res.SetAddress(std::move(address));
  }

  switch (rankerResult.GetResultType())
  {
  case RankerResult::Type::Feature:
  case RankerResult::Type::Building:
    res.FromFeature(rankerResult.GetID(), rankerResult.GetBestType(), rankerResult.GetBestType(&m_params.m_preferredTypes), rankerResult.m_details);
    break;
  case RankerResult::Type::LatLon: res.SetType(Result::Type::LatLon); break;
  case RankerResult::Type::Postcode: res.SetType(Result::Type::Postcode); break;
  }

  if (needAddress && ftypes::IsLocalityChecker::Instance().GetType(rankerResult.GetTypes()) == ftypes::LocalityType::None)
  {
    m_localities.GetLocality(res.GetFeatureCenter(), [&](LocalityItem const & item)
    {
      string_view city;
      if (item.GetReadableName(city))
        res.PrependCity(city);
    });
  }

  if (needHighlighting)
    HighlightResult(m_params.m_query.m_tokens, m_params.m_query.m_prefix, res);

  res.SetRankingInfo(rankerResult.m_dbgInfo);

#ifdef SEARCH_USE_PROVENANCE
  res.SetProvenance(std::move(rankerResult.m_provenance));
#endif

  return res;
}

void Ranker::SuggestStrings()
{
  // Prefix is only empty when tokens exceeds the max allowed. No point in giving suggestions then.
  if (m_params.m_query.m_prefix.empty() || !m_params.m_suggestsEnabled)
    return;

  string const prologue = DropLastToken(m_params.m_query.m_query);

  for (auto const locale : m_params.m_categoryLocales)
    MatchForSuggestions(m_params.m_query.m_prefix, locale, prologue);
}

void Ranker::UpdateResults(bool lastUpdate)
{
  if (!lastUpdate)
    BailIfCancelled();

  MakeRankerResults();
  RemoveDuplicatingLinear(m_tentativeResults);
  if (m_tentativeResults.empty())
    return;

  if (m_params.m_viewportSearch)
  {
    // https://github.com/organicmaps/organicmaps/issues/5566
    /// @todo https://github.com/organicmaps/organicmaps/issues/5251 Should review later
    // https://github.com/organicmaps/organicmaps/issues/4325
    // https://github.com/organicmaps/organicmaps/issues/4190
    // https://github.com/organicmaps/organicmaps/issues/3677

    auto & resV = m_tentativeResults;
    auto it = std::max_element(resV.begin(), resV.end(), base::LessBy(&RankerResult::GetLinearModelRank));
    double const lowestAllowed = it->GetLinearModelRank() - RankingInfo::GetLinearRankViewportThreshold();

    it = std::partition(resV.begin(), resV.end(), [lowestAllowed](RankerResult const & r)
    {
      return r.GetLinearModelRank() >= lowestAllowed;
    });
    if (it != resV.end())
    {
      LOG(LDEBUG, ("Removed", std::distance(it, resV.end()), "viewport results."));
      resV.erase(it, resV.end());
    }
  }
  else
  {
    // Can get same Town features (from World) when searching in many MWMs.
    base::SortUnique(m_tentativeResults,
        [](RankerResult const & r1, RankerResult const & r2)
        {
          // Expect that linear rank is equal for the same features.
          return r1.GetLinearModelRank() > r2.GetLinearModelRank();
        },
        base::EqualsBy(&RankerResult::GetID));

    ProcessSuggestions(m_tentativeResults);
  }

  // Emit feature results.
  size_t count = m_emitter.GetResults().GetCount();
  size_t i = 0;
  for (; i < m_tentativeResults.size(); ++i)
  {
    if (!lastUpdate && count >= m_params.m_batchSize && !m_params.m_viewportSearch &&
        !m_params.m_categorialRequest)
    {
      break;
    }

    if (!lastUpdate)
    {
      BailIfCancelled();

      // For categorial requests, it is usual that several batches arrive
      // in the first call to UpdateResults(). Emit them as soon as they are available
      // to improve responsiveness.
      if (count != 0 && count % m_params.m_batchSize == 0)
        m_emitter.Emit();
    }

    if (count >= m_params.m_limit)
      break;

    auto const & rankerResult = m_tentativeResults[i];

    /// @DebugNote
    // Uncomment for extended ranking print.
    //if (!m_params.m_viewportSearch)
    //  LOG(LDEBUG, (rankerResult));

    // Don't make move here in case of BailIfCancelled() throw. Objects in m_tentativeResults should remain valid.
    Result result = MakeResult(rankerResult, m_params.m_needAddress, m_params.m_needHighlighting);

    if (m_params.m_viewportSearch)
    {
      m_emitter.AddResultNoChecks(std::move(result));
      ++count;
    }
    else
    {
      if (m_emitter.AddResult(std::move(result)))
        ++count;
    }
  }

  /// @todo Use deque for m_tentativeResults?
  m_tentativeResults.erase(m_tentativeResults.begin(), m_tentativeResults.begin() + i);

  // The last update must be handled by a call to Finish().
  if (!lastUpdate)
  {
    BailIfCancelled();
    m_emitter.Emit();
  }
}

void Ranker::ClearCaches() { m_localities.ClearCache(); }

void Ranker::SetLocale(string const & locale)
{
  m_regionInfoGetter.SetLocale(locale);
}

void Ranker::LoadCountriesTree() { m_regionInfoGetter.LoadCountriesTree(); }

void Ranker::MakeRankerResults()
{
  LOG(LDEBUG, ("PreRankerResults number =", m_preRankerResults.size()));

  RankerResultMaker maker(*this, m_dataSource, m_infoGetter, m_reverseGeocoder, m_geocoderParams);
  for (auto const & r : m_preRankerResults)
  {
    auto p = maker(r);
    if (!p)
      continue;

    ASSERT(m_geocoderParams.m_mode != Mode::Viewport || m_geocoderParams.m_pivot.IsPointInside(p->GetCenter()), (r));

    // Do not filter any _duplicates_ here. Leave it for high level Results class.
    m_tentativeResults.push_back(std::move(*p));
  };

  m_preRankerResults.clear();
}

void Ranker::GetBestMatchName(FeatureType & f, string & name) const
{
  int8_t bestLang = StringUtf8Multilang::kUnsupportedLanguageCode;
  KeywordLangMatcher::Score bestScore;
  auto updateScore = [&](int8_t lang, string_view s, bool force)
  {
    // Ignore name for categorial requests.
    auto const score = m_keywordsScorer.CalcScore(lang, m_params.m_categorialRequest ? "" : s);
    if (force ? bestScore <= score : bestScore < score)
    {
      bestScore = score;
      name = s;
      bestLang = lang;
    }
  };

  auto bestNameFinder = [&](int8_t lang, string_view s)
  {
    if (StringUtf8Multilang::IsAltOrOldName(lang))
    {
      strings::Tokenize(s, ";", [lang, &updateScore](std::string_view n)
      {
        updateScore(lang, n, true /* force */);
      });
    }
    else
    {
      updateScore(lang, s, true /* force */);
    }

    // Default name should be written in the regional language.
    if (lang == StringUtf8Multilang::kDefaultCode)
    {
      auto const mwmInfo = f.GetID().m_mwmId.GetInfo();
      vector<int8_t> mwmLangCodes;
      mwmInfo->GetRegionData().GetLanguages(mwmLangCodes);
      for (auto const l : mwmLangCodes)
        updateScore(l, s, false /* force */);
    }
  };
  UNUSED_VALUE(f.ForEachName(bestNameFinder));

  if (StringUtf8Multilang::IsAltOrOldName(bestLang))
  {
    string_view const readableName = f.GetReadableName();
    // Do nothing if alt/old name is the only name we have.
    if (readableName != name && !readableName.empty())
      name = std::string(readableName) + " (" + name + ")";
  }
}

void Ranker::MatchForSuggestions(strings::UniString const & token, int8_t locale,
                                 string const & prologue)
{
  for (auto const & suggest : m_suggests)
  {
    strings::UniString const & s = suggest.m_name;
    if (suggest.m_prefixLength <= token.size()
        && token != s                  // do not push suggestion if it already equals to token
        && suggest.m_locale == locale  // push suggestions only for needed language
        && strings::StartsWith(s, token))
    {
      string const utf8Str = strings::ToUtf8(s);
      Result r(utf8Str, prologue + utf8Str + " ");
      HighlightResult(m_params.m_query.m_tokens, m_params.m_query.m_prefix, r);
      m_emitter.AddResult(std::move(r));
    }
  }
}

void Ranker::ProcessSuggestions(vector<RankerResult> const & vec) const
{
  if (m_params.m_query.m_prefix.empty() || !m_params.m_suggestsEnabled)
    return;

  size_t added = 0;
  for (RankerResult const & r : vec)
  {
    if (added >= kMaxNumSuggests)
      break;

    ftypes::LocalityType const type = GetLocalityIndex(r.GetTypes());
    if (type == ftypes::LocalityType::Country || type == ftypes::LocalityType::City || r.IsStreet())
    {
      string suggestion = GetSuggestion(r.GetName(), m_params.m_query);
      if (!suggestion.empty())
      {
        // todo(@m) RankingInfo is lost here. Should it be?
        if (m_emitter.AddResult(Result(MakeResult(r, false /* needAddress */, true /* needHighlighting */),
                                       std::move(suggestion))))
        {
          ++added;
        }
      }
    }
  }
}

string Ranker::GetLocalizedRegionInfoForResult(RankerResult const & result) const
{
  auto const type = result.GetBestType(&m_params.m_preferredTypes);

  storage::CountryId id;
  if (!result.GetCountryId(m_infoGetter, type, id))
    return {};

  return m_regionInfoGetter.GetLocalizedFullName(id);
}

}  // namespace search
