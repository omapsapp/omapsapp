#include "search/geocoder.hpp"

#include "search/cbv.hpp"
#include "search/dummy_rank_table.hpp"
#include "search/features_filter.hpp"
#include "search/features_layer_matcher.hpp"
#include "search/house_numbers_matcher.hpp"
#include "search/house_to_street_table.hpp"
#include "search/locality_scorer.hpp"
#include "search/pre_ranker.hpp"
#include "search/retrieval.hpp"
#include "search/token_slice.hpp"
#include "search/tracer.hpp"
#include "search/utils.hpp"

#include "storage/country_info_getter.hpp"

#include "indexer/data_source.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/postcodes_matcher.hpp"
#include "indexer/rank_table.hpp"
#include "indexer/search_string_utils.hpp"
#include "indexer/utils.hpp"

#include "storage/country_info_getter.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/checked_cast.hpp"
#include "base/control_flow.hpp"
#include "base/logging.hpp"
#include "base/macros.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>

#include "defines.hpp"

#ifdef DEBUG
#include "base/timer.hpp"
#endif  // DEBUG

#include "search/features_layer_path_finder.cpp"    // template functions implementation

namespace search
{
using namespace std;
using namespace strings;

namespace
{
/// @todo Move this constants into RecommendedFilteringParams?

size_t constexpr kMaxNumCities = 10;
size_t constexpr kMaxNumStates = 10;
size_t constexpr kMaxNumVillages = 5;
size_t constexpr kMaxNumCountries = 10;

double constexpr kMaxViewportRadiusM = 50.0 * 1000;
double constexpr kMaxPostcodeRadiusM = 1000;
double constexpr kMaxSuburbRadiusM = 2000;
double constexpr kMaxNeighbourhoodRadiusM = 500;
double constexpr kMaxResidentialRadiusM = 500;

size_t constexpr kPivotRectsCacheSize = 10;
size_t constexpr kPostcodesRectsCacheSize = 10;
size_t constexpr kSuburbsRectsCacheSize = 10;
size_t constexpr kLocalityRectsCacheSize = 10;


struct ScopedMarkTokens
{
  static BaseContext::TokenType constexpr kUnused = BaseContext::TOKEN_TYPE_COUNT;

  ScopedMarkTokens(vector<BaseContext::TokenType> & tokens, BaseContext::TokenType type,
                   TokenRange const & range)
    : m_tokens(tokens), m_type(type), m_range(range)
  {
    ASSERT(m_range.IsValid(), ());
    ASSERT_LESS_OR_EQUAL(m_range.End(), m_tokens.size(), ());
#if defined(DEBUG)
    for (size_t i : m_range)
      ASSERT_EQUAL(m_tokens[i], kUnused, (i));
#endif
    fill(m_tokens.begin() + m_range.Begin(), m_tokens.begin() + m_range.End(), m_type);
  }

  ~ScopedMarkTokens()
  {
#if defined(DEBUG)
    for (size_t i : m_range)
      ASSERT_EQUAL(m_tokens[i], m_type, (i));
#endif
    fill(m_tokens.begin() + m_range.Begin(), m_tokens.begin() + m_range.End(), kUnused);
  }

  vector<search::BaseContext::TokenType> & m_tokens;
  search::BaseContext::TokenType const m_type;
  TokenRange const m_range;
};

// static
BaseContext::TokenType constexpr ScopedMarkTokens::kUnused;

class LazyRankTable : public RankTable
{
public:
  explicit LazyRankTable(MwmValue const & value) : m_value(value) {}

  uint8_t Get(uint64_t i) const override
  {
    EnsureTableLoaded();
    return m_table->Get(i);
  }

  uint64_t Size() const override
  {
    EnsureTableLoaded();
    return m_table->Size();
  }

  RankTable::Version GetVersion() const override
  {
    EnsureTableLoaded();
    return m_table->GetVersion();
  }

  void Serialize(Writer & writer) override
  {
    EnsureTableLoaded();
    m_table->Serialize(writer);
  }

private:
  void EnsureTableLoaded() const
  {
    if (m_table)
      return;
    m_table = search::RankTable::Load(m_value.m_cont, SEARCH_RANKS_FILE_TAG);
    if (!m_table)
      m_table = make_unique<search::DummyRankTable>();
  }

  MwmValue const & m_value;
  mutable unique_ptr<search::RankTable> m_table;
};

class LocalityScorerDelegate : public LocalityScorer::Delegate
{
public:
  LocalityScorerDelegate(MwmContext & context, Geocoder::Params const & params,
                         function<bool(m2::PointD const &)> const & belongsToMatchedRegionFn,
                         base::Cancellable const & cancellable)
    : m_context(context)
    , m_params(params)
    , m_cancellable(cancellable)
    , m_belongsToMatchedRegionFn(belongsToMatchedRegionFn)
    , m_retrieval(m_context, m_cancellable)
    , m_ranks(m_context.m_value)
  {
    ASSERT(m_belongsToMatchedRegionFn, ());
  }

  // LocalityScorer::Delegate overrides:
  void GetNames(uint32_t featureId, vector<string> & names) const override
  {
    auto ft = m_context.GetFeature(featureId);
    if (!ft)
      return;
    for (auto const lang : m_params.GetLangs())
    {
      string_view name = ft->GetName(lang);
      if (!name.empty())
        names.push_back(std::string(name));
    }
  }

  uint8_t GetRank(uint32_t featureId) const override { return m_ranks.Get(featureId); }

  optional<m2::PointD> GetCenter(uint32_t featureId) override
  {
    m2::PointD center;
    // m_context->GetCenter is faster but may not work for editor created features.
    if (!m_context.GetCenter(featureId, center))
    {
      auto ft = m_context.GetFeature(featureId);
      if (!ft)
        return {};

      center = feature::GetCenter(*ft);
    }

    return center;
  }

  bool BelongsToMatchedRegion(m2::PointD const & p) const override
  {
    ASSERT(m_belongsToMatchedRegionFn, ());
    return m_belongsToMatchedRegionFn(p);
  }

private:
  MwmContext & m_context;
  Geocoder::Params const & m_params;
  base::Cancellable const & m_cancellable;
  function<bool(m2::PointD const &)> m_belongsToMatchedRegionFn;

  Retrieval m_retrieval;

  LazyRankTable m_ranks;
};

void JoinQueryTokens(QueryParams const & params, TokenRange const & range, UniString const & sep,
                     UniString & res)
{
  ASSERT(range.IsValid(), (range));
  for (size_t i : range)
  {
    res.append(params.GetToken(i).GetOriginal());
    if (i + 1 != range.End())
      res.append(sep);
  }
}

/// @todo Can't change on string_view now, because of unordered_map<string> Affiliations.
[[nodiscard]] bool GetAffiliationName(FeatureType & ft, string & affiliation)
{
  string_view name = ft.GetName(StringUtf8Multilang::kDefaultCode);
  if (name.empty())
  {
    // As a best effort, we try to read an english name if default name is absent.
    name = ft.GetName(StringUtf8Multilang::kEnglishCode);
    if (name.empty())
    {
      affiliation.clear();
      return false;
    }
  }

  affiliation = name;
  return true;
}

double Area(m2::RectD const & rect) { return rect.IsValid() ? rect.SizeX() * rect.SizeY() : 0; }

/// @brief Computes the average similarity between |rect| and |pivot|.
/// By similarity between two rects we mean a fraction of the area of
/// rects intersection to the area of the smallest rect.
/// @return [0, 1]
double GetSimilarity(m2::RectD const & pivot, m2::RectD const & rect)
{
  double const area = min(Area(pivot), Area(rect));
  if (area == 0.0)
    return 0.0;
  m2::RectD p = pivot;
  if (!p.Intersect(rect))
    return 0.0;
  return Area(p) / area;
}

// Returns shortest distance from the |pivot| to the |rect|.
//
// *NOTE* calculations below are incorrect, because shortest distance
// on the Mercator's plane is not the same as shortest distance on the
// Earth. But we assume that it is not an issue here.
double GetDistanceMeters(m2::PointD const & pivot, m2::RectD const & rect)
{
  if (rect.IsPointInside(pivot))
    return 0.0;

  double distance = numeric_limits<double>::max();

  rect.ForEachSide([&](m2::PointD const & a, m2::PointD const & b) {
    m2::ParametrizedSegment<m2::PointD> segment(a, b);
    distance = min(distance, mercator::DistanceOnEarth(pivot, segment.ClosestPointTo(pivot)));
  });

  return distance;
}

unique_ptr<MwmContext> GetWorldContext(DataSource const & dataSource)
{
  vector<shared_ptr<MwmInfo>> infos;
  dataSource.GetMwmsInfo(infos);
  MwmSet::MwmHandle handle = indexer::FindWorld(dataSource, infos);
  if (handle.IsAlive())
    return make_unique<MwmContext>(std::move(handle));
  return {};
}

#define TRACE(branch)                                      \
  m_resultTracer.CallMethod(ResultTracer::Branch::branch); \
  SCOPE_GUARD(tracerGuard, [&] { m_resultTracer.LeaveMethod(ResultTracer::Branch::branch); })
}  // namespace


// Geocoder::LocalitiesCaches ----------------------------------------------------------------------
Geocoder::LocalitiesCaches::LocalitiesCaches(base::Cancellable const & cancellable)
  : m_countries(cancellable)
  , m_states(cancellable)
  , m_citiesTownsOrVillages(cancellable)
  , m_villages(cancellable)
{
}

void Geocoder::LocalitiesCaches::Clear()
{
  m_countries.Clear();
  m_states.Clear();
  m_citiesTownsOrVillages.Clear();
  m_villages.Clear();
}

// Geocoder::Geocoder ------------------------------------------------------------------------------
Geocoder::Geocoder(DataSource const & dataSource, storage::CountryInfoGetter const & infoGetter,
                   CategoriesHolder const & categories,
                   CitiesBoundariesTable const & citiesBoundaries, PreRanker & preRanker,
                   LocalitiesCaches & localitiesCaches, base::Cancellable const & cancellable)
  : m_dataSource(dataSource)
  , m_infoGetter(infoGetter)
  , m_categories(categories)
  , m_streetsCache(cancellable)
  , m_suburbsCache(cancellable)
  , m_localitiesCaches(localitiesCaches)
  , m_hotelsCache(cancellable)
  , m_foodCache(cancellable)
  , m_cuisineFilter(m_foodCache)
  , m_cancellable(cancellable)
  , m_citiesBoundaries(citiesBoundaries)
  , m_pivotRectsCache(kPivotRectsCacheSize, m_cancellable, kMaxViewportRadiusM)
  , m_postcodesRectsCache(kPostcodesRectsCacheSize, m_cancellable, kMaxPostcodeRadiusM)
  , m_suburbsRectsCache(kSuburbsRectsCacheSize, m_cancellable, kMaxSuburbRadiusM)
  , m_localityRectsCache(kLocalityRectsCacheSize, m_cancellable)
  , m_filter(nullptr)
  , m_matcher(nullptr)
  , m_finder(m_cancellable)
  , m_preRanker(preRanker)
{
}

Geocoder::~Geocoder() {}

void Geocoder::SetParams(Params const & params)
{
  if (params.IsCategorialRequest())
  {
    SetParamsForCategorialSearch(params);
    return;
  }

  m_params = params;

  auto const MakeRequest = [this](size_t i, auto & request)
  {
    FillRequestFromToken(m_params.GetToken(i), request);
    for (auto const & index : m_params.GetTypeIndices(i))
      request.m_categories.emplace_back(FeatureTypeToString(index));
    request.SetLangs(m_params.GetLangs());
  };

  m_tokenRequests.clear();
  m_prefixTokenRequest.Clear();
  for (size_t i = 0; i < m_params.GetNumTokens(); ++i)
  {
    if (!m_params.IsPrefixToken(i))
    {
      m_tokenRequests.emplace_back();
      MakeRequest(i, m_tokenRequests.back());
    }
    else
    {
      MakeRequest(i, m_prefixTokenRequest);
    }
  }

  m_resultTracer.Clear();

  LOG(LDEBUG, (static_cast<QueryParams const &>(m_params)));
}

void Geocoder::GoEverywhere()
{
// TODO (@y): remove following code as soon as Geocoder::Go() will
// work fast for most cases (significantly less than 1 second).
#if defined(DEBUG)
  base::Timer timer;
  SCOPE_GUARD(printDuration, [&timer]() {
    LOG(LINFO, ("Total geocoding time:", timer.ElapsedSeconds(), "seconds"));
  });
#endif

  TRACE(GoEverywhere);

  if (m_params.GetNumTokens() == 0)
    return;

  vector<MwmInfoPtr> infos;
  m_dataSource.GetMwmsInfo(infos);

  GoImpl(infos, false /* inViewport */);
}

void Geocoder::GoInViewport()
{
  TRACE(GoInViewport);

  if (m_params.GetNumTokens() == 0)
    return;

  vector<MwmInfoPtr> infos;
  m_dataSource.GetMwmsInfo(infos);

  base::EraseIf(infos, [this](MwmInfoPtr const & info) {
    return !m_params.m_pivot.IsIntersect(info->m_bordersRect);
  });

  GoImpl(infos, true /* inViewport */);
}

void Geocoder::Finish(bool cancelled)
{
  m_preRanker.Finish(cancelled);
}

void Geocoder::ClearCaches()
{
  m_pivotRectsCache.Clear();
  m_localityRectsCache.Clear();

  m_matchersCache.clear();
  m_streetsCache.Clear();
  m_hotelsCache.Clear();
  m_foodCache.Clear();
  m_cuisineFilter.ClearCaches();
  m_postcodePointsCache.Clear();
  m_postcodes.Clear();
}

void Geocoder::SetParamsForCategorialSearch(Params const & params)
{
  m_params = params;

  m_tokenRequests.clear();
  m_prefixTokenRequest.Clear();

  LOG(LDEBUG, (static_cast<QueryParams const &>(m_params)));
}

Geocoder::ExtendedMwmInfos Geocoder::OrderCountries(bool inViewport, vector<MwmInfoPtr> const & infos)
{
  set<storage::CountryId> mwmsWithCities;
  set<storage::CountryId> mwmsWithStates;
  if (!inViewport)
  {
    for (auto const & p : m_cities)
    {
      for (auto const & city : p.second)
        mwmsWithCities.insert(m_infoGetter.GetRegionCountryId(city.m_rect.Center()));
    }
    for (auto const & p : m_regions[Region::TYPE_STATE])
    {
      for (auto const & state : p.second)
      {
        mwmsWithStates.insert(m_infoGetter.GetRegionCountryId(state.m_center));
      }
    }
  }

  auto const hasMatchedCity = [&mwmsWithCities](auto const & i)
  {
    return mwmsWithCities.count(i->GetCountryName()) != 0;
  };
  auto const hasMatchedState = [&mwmsWithStates](auto const & i)
  {
    return mwmsWithStates.count(i->GetCountryName()) != 0;
  };

  std::string locationMwm;
  if (m_params.m_position)
    locationMwm = m_infoGetter.GetRegionCountryId(*m_params.m_position);

  auto const viewportCenter = m_params.m_pivot.Center();
  std::string const viewportMwm = m_infoGetter.GetRegionCountryId(viewportCenter);

  ExtendedMwmInfos res;
  res.m_infos.reserve(infos.size());
  for (auto const & info : infos)
  {
    ExtendedMwmInfos::ExtendedMwmInfo ei;
    ei.m_info = info;

    auto const & rect = info->m_bordersRect;
    ei.m_type.m_viewportIntersected = m_params.m_pivot.IsIntersect(rect);
    ei.m_type.m_containsUserPosition = m_params.m_position && rect.IsPointInside(*m_params.m_position);
    ei.m_type.m_containsMatchedCity = hasMatchedCity(info);
    ei.m_type.m_containsMatchedState = hasMatchedState(info);

    // Order MWMs like:
    // - World
    // - containing viewport center
    // - containing user's position (except viewport search mode)
    // - less by viewport center or rect similarity

    if (info->GetType() == MwmInfo::WORLD)
      ei.m_score = -6;
    else if (info->GetCountryName() == viewportMwm)
      ei.m_score = -4;
    else if (!inViewport && ei.m_type.m_containsUserPosition)
    {
      ei.m_score = 0;
      if (info->GetCountryName() == locationMwm)
        ei.m_score = -2;
    }
    else
      ei.m_score = GetDistanceMeters(viewportCenter, rect);

    // Subtract [0, 1] similarity from the score in case of equal viewport distances.
    ei.m_score -= GetSimilarity(m_params.m_pivot, rect);

    res.m_infos.push_back(std::move(ei));
  }

  sort(res.m_infos.begin(), res.m_infos.end());

  auto const sep = stable_partition(res.m_infos.begin(), res.m_infos.end(), [&](auto const & extendedInfo)
  {
    return extendedInfo.m_type.IsFirstBatchMwm(inViewport);
  });
  res.m_firstBatchSize = distance(res.m_infos.begin(), sep);

  return res;
}

void Geocoder::GoImpl(vector<MwmInfoPtr> const & infos, bool inViewport)
{
  // base::PProf pprof("/tmp/geocoder.prof");

  // Tries to find world and fill localities table.
  {
    m_cities.clear();
    for (auto & regions : m_regions)
      regions.clear();
    MwmSet::MwmHandle handle = indexer::FindWorld(m_dataSource, infos);
    if (handle.IsAlive())
    {
      auto & value = *handle.GetValue();

      // All MwmIds are unique during the application lifetime, so
      // it's ok to save MwmId.
      m_worldId = handle.GetId();
      m_context = make_unique<MwmContext>(std::move(handle));

      if (value.HasSearchIndex())
      {
        BaseContext ctx;
        InitBaseContext(ctx);
        FillLocalitiesTable(ctx);
      }

      m_context.reset();
    }
  }

  // Orders countries by distance from viewport center and position.
  // This order is used during MatchAroundPivot() stage - we try to
  // match as many features as possible without trying to match
  // locality (COUNTRY or CITY), and only when there are too many
  // features, viewport and position vicinity filter is used.  To
  // prevent full search in all mwms, we need to limit somehow a set
  // of mwms for MatchAroundPivot(), so, we always call
  // MatchAroundPivot() on maps intersecting with pivot rect, other
  // maps are ordered by distance from pivot, and we stop to call
  // MatchAroundPivot() on them as soon as at least one feature is
  // found.
  auto const infosWithType = OrderCountries(inViewport, infos);

  // MatchAroundPivot() should always be matched in mwms
  // intersecting with position and viewport.
  auto processCountry = [&](unique_ptr<MwmContext> context, bool updatePreranker) {
    ASSERT(context, ());
    m_context = std::move(context);

    SCOPE_GUARD(cleanup, [&]() {
      LOG(LDEBUG, (m_context->GetName(), "geocoding complete."));
      m_matcher->OnQueryFinished();
      m_matcher = nullptr;
      m_context.reset();
    });

    auto it = m_matchersCache.find(m_context->GetId());
    if (it == m_matchersCache.end())
    {
      it = m_matchersCache
               .insert(make_pair(m_context->GetId(),
                                 std::make_unique<FeaturesLayerMatcher>(m_dataSource, m_cancellable)))
               .first;
    }
    m_matcher = it->second.get();
    m_matcher->SetContext(m_context.get());

    BaseContext ctx;
    InitBaseContext(ctx);

    if (inViewport)
    {
      auto const viewportCBV =
          RetrieveGeometryFeatures(*m_context, m_params.m_pivot, RectId::Pivot);
      for (auto & features : ctx.m_features)
        features = features.Intersect(viewportCBV);
    }

    ctx.m_villages = m_localitiesCaches.m_villages.Get(*m_context);

    auto const citiesFromWorld = m_cities;
    FillVillageLocalities(ctx);
    SCOPE_GUARD(remove_villages, [&]() { m_cities = citiesFromWorld; });

    if (m_params.IsCategorialRequest())
    {
      MatchCategories(ctx, m_context->GetType().m_viewportIntersected /* aroundPivot */);
    }
    else
    {
      MatchRegions(ctx, Region::TYPE_COUNTRY);

      // Probably, we should process all MWMs "until the end" but I left some _better-than-before_
      // reasonable criteria (ContinueSearch) not to hang a lot.
      auto const & mwmType = m_context->GetType();
      if (mwmType.m_viewportIntersected || mwmType.m_containsUserPosition || m_preRanker.ContinueSearch())
        MatchAroundPivot(ctx);
    }

    if (updatePreranker)
      m_preRanker.UpdateResults(false /* lastUpdate */);

    if (m_preRanker.IsFull())
      return base::ControlFlow::Break;

    return base::ControlFlow::Continue;
  };

  // Iterates through all alive mwms and performs geocoding.
  ForEachCountry(infosWithType, processCountry);
}

void Geocoder::InitBaseContext(BaseContext & ctx)
{
  Retrieval retrieval(*m_context, m_cancellable);

  size_t const numTokens = m_params.GetNumTokens();
  ctx.m_tokens.assign(numTokens, BaseContext::TOKEN_TYPE_COUNT);
  ctx.m_features.resize(numTokens);
  for (size_t i = 0; i < numTokens; ++i)
  {
    if (m_params.IsCategorialRequest())
    {
      // Implementation-wise, the simplest way to match a feature by
      // its category bypassing the matching by name is by using a CategoriesCache.
      CategoriesCache cache(m_params.m_preferredTypes, m_cancellable);
      ctx.m_features[i] = Retrieval::ExtendedFeatures(cache.Get(*m_context));
    }
    else if (m_params.IsPrefixToken(i))
    {
      ctx.m_features[i] = retrieval.RetrieveAddressFeatures(m_prefixTokenRequest);
    }
    else
    {
      ctx.m_features[i] = retrieval.RetrieveAddressFeatures(m_tokenRequests[i]);
    }
  }

  ctx.m_cuisineFilter = m_cuisineFilter.MakeScopedFilter(*m_context, m_params.m_cuisineTypes);
}

void Geocoder::InitLayer(Model::Type type, TokenRange const & tokenRange, FeaturesLayer & layer)
{
  layer.Clear();
  layer.m_type = type;
  layer.m_tokenRange = tokenRange;

  JoinQueryTokens(m_params, layer.m_tokenRange, UniString::kSpace /* sep */, layer.m_subQuery);
  layer.m_lastTokenIsPrefix =
      !layer.m_tokenRange.Empty() && m_params.IsPrefixToken(layer.m_tokenRange.End() - 1);
}

void Geocoder::FillLocalityCandidates(BaseContext const & ctx, CBV const & filter,
                                      size_t const maxNumLocalities,
                                      vector<Locality> & preLocalities)
{
  // todo(@m) "food moscow" should be a valid categorial request.
  if (m_params.IsCategorialRequest())
  {
    preLocalities.clear();
    return;
  }

  storage::CountryInfoGetter::RegionIdVec ids;
  for (auto const & type : m_regions)
  {
    for (auto const & ranges : type)
    {
      for (auto const & regions : ranges.second)
        ids.insert(ids.end(), regions.m_ids.begin(), regions.m_ids.end());
    }
  }
  base::SortUnique(ids);

  auto const belongsToMatchedRegion = [&](m2::PointD const & point) {
    return m_infoGetter.BelongsToAnyRegion(point, ids);
  };

  LocalityScorerDelegate delegate(*m_context, m_params, belongsToMatchedRegion, m_cancellable);
  LocalityScorer scorer(m_params, m_params.m_pivot.Center(), delegate);
  scorer.GetTopLocalities(m_context->GetId(), ctx, filter, maxNumLocalities, preLocalities);
}

void Geocoder::CacheWorldLocalities()
{
  if (auto context = GetWorldContext(m_dataSource))
  {
    // Get only world localities
    UNUSED_VALUE(m_localitiesCaches.m_countries.Get(*context));
    UNUSED_VALUE(m_localitiesCaches.m_states.Get(*context));
    UNUSED_VALUE(m_localitiesCaches.m_citiesTownsOrVillages.Get(*context));
  }
  else
  {
    // This is strange situation, anyway.
    LOG(LWARNING, ("Can't find World map file."));
  }
}

void Geocoder::FillLocalitiesTable(BaseContext const & ctx)
{
  auto addRegionMaps = [this](FeatureType & ft, Locality && l, Region::Type type)
  {
    if (ft.GetGeomType() != feature::GeomType::Point)
      return;

    string affiliation;
    if (!GetAffiliationName(ft, affiliation))
      return;

    Region region(std::move(l), type);
    region.m_center = ft.GetCenter();

    LOG(LDEBUG, ("Region =", ft.GetName(StringUtf8Multilang::kDefaultCode)));

    m_infoGetter.GetMatchedRegions(affiliation, region.m_ids);
    m_regions[type][region.m_tokenRange].push_back(std::move(region));
  };

  vector<Locality> preLocalities;

  CBV filter = m_localitiesCaches.m_countries.Get(*m_context);
  FillLocalityCandidates(ctx, filter, kMaxNumCountries, preLocalities);
  for (auto & l : preLocalities)
  {
    auto ft = m_context->GetFeature(l.GetFeatureIndex());
    if (!ft)
    {
      LOG(LWARNING, ("Failed to get country", l.m_featureId));
      continue;
    }

    addRegionMaps(*ft, std::move(l), Region::TYPE_COUNTRY);
  }

  filter = m_localitiesCaches.m_states.Get(*m_context);
  FillLocalityCandidates(ctx, filter, kMaxNumStates, preLocalities);
  for (auto & l : preLocalities)
  {
    auto ft = m_context->GetFeature(l.GetFeatureIndex());
    if (!ft)
    {
      LOG(LWARNING, ("Failed to get state", l.m_featureId));
      continue;
    }

    addRegionMaps(*ft, std::move(l), Region::TYPE_STATE);
  }

  filter = m_localitiesCaches.m_citiesTownsOrVillages.Get(*m_context);
  FillLocalityCandidates(ctx, filter, kMaxNumCities, preLocalities);
  for (auto & l : preLocalities)
  {
    auto ft = m_context->GetFeature(l.GetFeatureIndex());
    if (!ft)
    {
      LOG(LWARNING, ("Failed to get city", l.m_featureId));
      continue;
    }

    // We transform all cities into point Features on generator stage.
    ASSERT_EQUAL(ft->GetGeomType(), feature::GeomType::Point, (ft->GetID()));
    {
      City city(std::move(l), Model::TYPE_CITY);

      auto const center = ft->GetCenter();

      CitiesBoundariesTable::Boundaries boundaries;
      bool haveBoundary = false;
      if (m_citiesBoundaries.Get(ft->GetID(), boundaries))
      {
        city.m_rect = boundaries.GetLimitRect();
        if (city.m_rect.IsValid())
        {
          /// @todo Replace with assert in future. Now make additional check for compatibility with old "buggy" World.
          if (city.m_rect.IsPointInside(center))
            haveBoundary = true;
          //else
          //  ASSERT(false, (city.m_rect, center, ft->GetID()));
        }
      }

      if (!haveBoundary)
      {
        auto const population = ftypes::GetPopulation(*ft);
        auto const radius = ftypes::GetRadiusByPopulation(population);
        city.m_rect = mercator::RectByCenterXYAndSizeInMeters(center, radius);
      }

      LOG(LDEBUG,
          ("City =", ft->GetName(StringUtf8Multilang::kDefaultCode), "ll =", mercator::ToLatLon(center),
           "rect =", mercator::ToLatLon(city.m_rect), "rect source:", haveBoundary ? "table" : "population",
           "sizeX =", mercator::DistanceOnEarth(city.m_rect.LeftTop(), city.m_rect.RightTop()),
           "sizeY =", mercator::DistanceOnEarth(city.m_rect.LeftTop(), city.m_rect.LeftBottom())));

      m_cities[city.m_tokenRange].push_back(std::move(city));
    }
  }
}

void Geocoder::FillVillageLocalities(BaseContext const & ctx)
{
  vector<Locality> preLocalities;
  FillLocalityCandidates(ctx, ctx.m_villages, kMaxNumVillages, preLocalities);

  for (auto & l : preLocalities)
  {
    auto ft = m_context->GetFeature(l.GetFeatureIndex());
    if (!ft)
      continue;

    if (m_model.GetType(*ft) != Model::TYPE_VILLAGE)
      continue;

    ASSERT_EQUAL(ft->GetGeomType(), feature::GeomType::Point, (ft->GetID()));

    auto const center = ft->GetCenter();

    // Always grab top kMaxNumVillages, despite of the distance.
    /*
    vector<m2::PointD> pivotPoints = {m_params.m_pivot.Center()};
    if (m_params.m_position)
      pivotPoints.push_back(*m_params.m_position);
    if (!m_context->GetType().m_containsMatchedState &&
        all_of(pivotPoints.begin(), pivotPoints.end(), [&](auto const & p) {
          return mercator::DistanceOnEarth(center, p) > m_params.m_filteringParams.m_villageSearchRadiusM;
        }))
    {
      continue;
    }
    */

    City village(std::move(l), Model::TYPE_VILLAGE);

    auto const population = ftypes::GetPopulation(*ft);
    auto const radius = ftypes::GetRadiusByPopulation(population);
    village.m_rect = mercator::RectByCenterXYAndSizeInMeters(center, radius);

    LOG(LDEBUG, ("Village =", ft->GetName(StringUtf8Multilang::kDefaultCode),
                 "ll =", mercator::ToLatLon(center), "radius =", radius));

    m_cities[village.m_tokenRange].push_back(std::move(village));
  }
}

bool Geocoder::CityHasPostcode(BaseContext const & ctx) const
{
  if (!ctx.m_city)
    return false;

  return m_postcodes.Has(ctx.m_city->GetFeatureIndex(), ctx.m_city->m_featureId.IsWorld());
}

template <typename Fn>
void Geocoder::ForEachCountry(ExtendedMwmInfos const & extendedInfos, Fn && fn)
{
  for (size_t i = 0; i < extendedInfos.m_infos.size(); ++i)
  {
    auto const & info = extendedInfos.m_infos[i].m_info;
    if (info->GetType() != MwmInfo::COUNTRY && info->GetType() != MwmInfo::WORLD)
      continue;
    if (info->GetType() == MwmInfo::COUNTRY && m_params.m_mode == Mode::Downloader)
      continue;

    auto handle = m_dataSource.GetMwmHandleById(MwmSet::MwmId(info));
    if (!handle.IsAlive())
      continue;
    auto & value = *handle.GetValue();
    if (!value.HasSearchIndex() || !value.HasGeometryIndex())
      continue;
    bool const updatePreranker = i + 1 >= extendedInfos.m_firstBatchSize;
    auto const & mwmType = extendedInfos.m_infos[i].m_type;
    if (fn(make_unique<MwmContext>(std::move(handle), mwmType), updatePreranker) ==
        base::ControlFlow::Break)
    {
      break;
    }
  }
}

void Geocoder::MatchCategories(BaseContext & ctx, bool aroundPivot)
{
  TRACE(MatchCategories);

  auto features = ctx.m_features[0];

  if (aroundPivot)
  {
    auto const pivotFeatures =
        RetrieveGeometryFeatures(*m_context, m_params.m_pivot, RectId::Pivot);
    ViewportFilter filter(pivotFeatures, m_preRanker.Limit() /* threshold */);
    features.m_features = filter.Filter(features.m_features);
    features.m_exactMatchingFeatures =
        features.m_exactMatchingFeatures.Intersect(features.m_features);
  }

  // Features have been retrieved from the search index
  // using the exact (non-fuzzy) matching and intersected
  // with viewport, if needed. Every such feature is relevant.
  features.ForEach([&](uint32_t featureId, bool exactMatch)
  {
    Model::Type type;
    if (!GetTypeInGeocoding(ctx, featureId, type))
      return;

    EmitResult(ctx, {m_context->GetId(), featureId}, type, TokenRange(0, ctx.NumTokens()),
               nullptr /* geoParts */, true /* allTokensUsed */, exactMatch);
 });
}

void Geocoder::MatchRegions(BaseContext & ctx, Region::Type type)
{
  TRACE(MatchRegions);

  switch (type)
  {
  case Region::TYPE_STATE:
    // Tries to skip state matching and go to cities matching.
    // Then, performs states matching.
    MatchCities(ctx);
    break;
  case Region::TYPE_COUNTRY:
    // Tries to skip country matching and go to states matching.
    // Then, performs countries matching.
    MatchRegions(ctx, Region::TYPE_STATE);
    break;
  case Region::TYPE_COUNT: ASSERT(false, ("Invalid region type.")); return;
  }

  auto const & regions = m_regions[type];

  auto const & fileName = m_context->GetName();
  bool const isWorld = m_context->GetInfo()->GetType() == MwmInfo::WORLD;

  // Try to match regions.
  for (auto const & p : regions)
  {
    BailIfCancelled();

    auto const & tokenRange = p.first;
    if (ctx.HasUsedTokensInRange(tokenRange))
      continue;

    for (auto const & region : p.second)
    {
      bool matches = false;

      // On the World.mwm we need to check that CITY - STATE - COUNTRY
      // form a nested sequence.  Otherwise, as mwm borders do not
      // intersect state or country boundaries, it's enough to check
      // mwm that is currently being processed belongs to region.
      if (isWorld)
      {
        matches = ctx.m_regions.empty() ||
                  m_infoGetter.BelongsToAnyRegion(region.m_center, ctx.m_regions.back()->m_ids);
      }
      else
      {
        matches = m_infoGetter.BelongsToAnyRegion(fileName, region.m_ids);
      }

      if (!matches)
        continue;

      ctx.m_regions.push_back(&region);
      SCOPE_GUARD(cleanup, [&ctx]() { ctx.m_regions.pop_back(); });

      ScopedMarkTokens mark(ctx.m_tokens, BaseContext::FromRegionType(type), tokenRange);

      if (ctx.AllTokensUsed())
      {
        bool exactMatch = true;
        for (auto const & region : ctx.m_regions)
        {
          if (!region->m_exactMatch)
            exactMatch = false;
        }

        // Region matches to search query, we need to emit it as is.
        EmitResult(ctx, region, tokenRange, true /* allTokensUsed */, exactMatch);
        continue;
      }

      switch (type)
      {
      case Region::TYPE_STATE: MatchCities(ctx); break;
      case Region::TYPE_COUNTRY: MatchRegions(ctx, Region::TYPE_STATE); break;
      case Region::TYPE_COUNT: ASSERT(false, ("Invalid region type.")); break;
      }
    }
  }
}

void Geocoder::MatchCities(BaseContext & ctx)
{
  TRACE(MatchCities);

  ASSERT(!ctx.m_city, ());

  // Localities are ordered by (m_startToken, m_endToken) pairs.
  for (auto const & p : m_cities)
  {
    auto const & tokenRange = p.first;
    if (ctx.HasUsedTokensInRange(tokenRange))
      continue;

    for (auto const & city : p.second)
    {
      BailIfCancelled();

      if (!ctx.m_regions.empty() &&
          !m_infoGetter.BelongsToAnyRegion(city.m_rect.Center(), ctx.m_regions.back()->m_ids))
      {
        continue;
      }

      ScopedMarkTokens mark(ctx.m_tokens, BaseContext::FromModelType(city.m_type), tokenRange);
      ctx.m_city = &city;
      SCOPE_GUARD(cleanup, [&ctx]() { ctx.m_city = nullptr; });

      if (ctx.AllTokensUsed())
      {
        // City matches to search query, we need to emit it as is.
        EmitResult(ctx, city, tokenRange, true /* allTokensUsed */);
        continue;
      }

      // No need to search features in the World map.
      if (m_context->GetInfo()->GetType() == MwmInfo::WORLD)
        continue;

      auto cityFeatures = RetrieveGeometryFeatures(*m_context, city.m_rect, RectId::Locality);

      if (cityFeatures.IsEmpty())
        continue;

      LocalityFilter filter(cityFeatures);

      size_t const numEmitted = ctx.m_numEmitted;

      CentersFilter centers;
      centers.Add(city.m_rect.Center());
      LimitedSearch(ctx, filter, centers);

      /// @todo A bit controversial here. We don't emit relaxed city if it was matched with some address.
      /// But do emit others (less relevant) relaxed cities. Can't say for sure is it ok or not, but
      /// we don't emit less relevant relaxed streets (@see CreateStreetsLayerAndMatchLowerLayers).
      if (numEmitted == ctx.m_numEmitted && !ctx.AllTokensUsed())
      {
        TRACE(Relaxed);
        EmitResult(ctx, *ctx.m_city, ctx.m_city->m_tokenRange, false /* allTokensUsed */);
      }
    }
  }
}

void Geocoder::MatchAroundPivot(BaseContext & ctx)
{
  TRACE(MatchAroundPivot);

  ViewportFilter filter(CBV::GetFull(), m_preRanker.Limit() /* threshold */);

  CentersFilter centers;
  auto const & mwmType = m_context->GetType();
  if (mwmType.m_containsUserPosition)
  {
    CHECK(m_params.m_position, ());
    if (mwmType.m_viewportIntersected)
      centers.Add(m_params.m_pivot.Center());
    centers.Add(*m_params.m_position);
  }
  else
    centers.Add(m_params.m_pivot.Center());

  LimitedSearch(ctx, filter, centers);
}

void Geocoder::LimitedSearch(BaseContext & ctx, FeaturesFilter const & filter,
                             CentersFilter const & centers)
{
  m_filter = &filter;
  SCOPE_GUARD(resetFilter, [&]() { m_filter = nullptr; });

  if (!ctx.m_streets)
    ctx.m_streets = m_streetsCache.Get(*m_context);

  if (!ctx.m_suburbs)
    ctx.m_suburbs = m_suburbsCache.Get(*m_context);

  MatchUnclassified(ctx, 0 /* curToken */);

  auto const search = [this, &ctx, &centers]() {
    GreedilyMatchStreets(ctx, centers);
    MatchPOIsAndBuildings(ctx, 0 /* curToken */, CBV::GetFull());
  };

  WithPostcodes(ctx, search);
  search();
}

template <typename Fn>
void Geocoder::WithPostcodes(BaseContext & ctx, Fn && fn)
{
  TRACE(WithPostcodes);

  size_t const maxPostcodeTokens = GetMaxNumTokensInPostcode();
  auto worldContext = GetWorldContext(m_dataSource);

  size_t const numTokens = ctx.NumTokens();
  for (size_t startToken = 0; startToken != numTokens; ++startToken)
  {
    size_t endToken = startToken;
    for (size_t n = 1; startToken + n <= numTokens && n <= maxPostcodeTokens; ++n)
    {
      if (ctx.IsTokenUsed(startToken + n - 1))
        break;

      TokenSlice slice(m_params, TokenRange(startToken, startToken + n));
      auto const isPrefix = startToken + n == numTokens;
      if (LooksLikePostcode(QuerySlice(slice), isPrefix))
        endToken = startToken + n;
    }
    if (startToken == endToken)
      continue;

    TokenRange const tokenRange(startToken, endToken);

    // Find features by exact postcode match from OSM data.
    auto postcodes = RetrievePostcodeFeatures(*m_context, TokenSlice(m_params, tokenRange));

    // Get _around postcode_ features from additional _external postcodes_ section.
    // Do not emit _around postcode_ only features if we don't have some other tokens to match (avoid lags).
    if (tokenRange.Size() < numTokens && m_context->m_value.m_cont.IsExist(POSTCODE_POINTS_FILE_TAG))
    {
      auto & postcodePoints = m_postcodePointsCache.Get(*m_context);
      UniString postcodeQuery;
      JoinQueryTokens(m_params, tokenRange, UniString::kSpace /* sep */, postcodeQuery);
      vector<m2::PointD> points;
      postcodePoints.Get(postcodeQuery, points);

      // PostcodePoints::Get returns many points for "BN1" or one point for "BN1 3LJ".
      // Do aggregate rects for a _wide_ postcode prefix match.
      m2::RectD rect;
      for (auto const & p : points)
        rect.Add(mercator::RectByCenterXYAndOffset(p, postcodePoints.GetRadius()));

      postcodes = postcodes.Union(RetrieveGeometryFeatures(*m_context, rect, RectId::Postcode));
    }

    SCOPE_GUARD(cleanup, [&]() { m_postcodes.Clear(); });

    if (!postcodes.IsEmpty())
    {
      ScopedMarkTokens mark(ctx.m_tokens, BaseContext::TOKEN_TYPE_POSTCODE, tokenRange);

      m_postcodes.Clear();

      if (worldContext)
      {
        m_postcodes.m_worldFeatures =
            RetrievePostcodeFeatures(*worldContext, TokenSlice(m_params, tokenRange));
      }

      m_postcodes.m_tokenRange = tokenRange;
      m_postcodes.m_countryFeatures = std::move(postcodes);

      if (ctx.AllTokensUsed() && CityHasPostcode(ctx))
      {
        EmitResult(ctx, *ctx.m_city, ctx.m_city->m_tokenRange, true /* allTokensUsed */);
        continue;
      }

      fn();
    }
  }
}

void Geocoder::GreedilyMatchStreets(BaseContext & ctx, CentersFilter const & centers)
{
  TRACE(GreedilyMatchStreets);

  ProcessStreets(ctx, centers, ctx.m_streets);

  GreedilyMatchStreetsWithSuburbs(ctx, centers);
}

void Geocoder::ProcessStreets(BaseContext & ctx, CentersFilter const & centers, CBV const & streets)
{
  using PredictionT = StreetsMatcher::Prediction;
  vector<PredictionT> predictions;
  StreetsMatcher::Go(ctx, streets, *m_filter, m_params, predictions);

  // Iterating from best to worst predictions here. Make "Relaxed" results for the best probability.
  for (size_t i = 0; i < predictions.size(); ++i)
  {
    CreateStreetsLayerAndMatchLowerLayers(ctx, predictions[i], centers,
                                          predictions[0].SameForRelaxedMatch(predictions[i]) /* makeRelaxed */);
  }
}

void Geocoder::GreedilyMatchStreetsWithSuburbs(BaseContext & ctx, CentersFilter const & centers)
{
  TRACE(GreedilyMatchStreetsWithSuburbs);
  vector<StreetsMatcher::Prediction> suburbs;
  StreetsMatcher::Go(ctx, ctx.m_suburbs, *m_filter, m_params, suburbs);

  auto const & suburbChecker = ftypes::IsSuburbChecker::Instance();
  for (auto const & suburb : suburbs)
  {
    ScopedMarkTokens mark(ctx.m_tokens, BaseContext::TOKEN_TYPE_SUBURB, suburb.m_tokenRange);

    suburb.m_features.ForEach([&](uint64_t suburbId)
    {
      auto ft = m_context->GetFeature(base::asserted_cast<uint32_t>(suburbId));
      if (!ft)
        return;

      auto & layers = ctx.m_layers;
      ASSERT(layers.empty(), ());
      layers.emplace_back();
      SCOPE_GUARD(cleanupGuard, [&layers]{ layers.pop_back(); });

      auto & layer = layers.back();
      InitLayer(Model::TYPE_SUBURB, suburb.m_tokenRange, layer);
      vector<uint32_t> suburbFeatures = {ft->GetID().m_index};
      layer.m_sortedFeatures = &suburbFeatures;

      auto const suburbType = suburbChecker.GetType(*ft);
      double radius = 0.0;
      switch (suburbType)
      {
      case ftypes::SuburbType::Residential: radius = kMaxResidentialRadiusM; break;
      case ftypes::SuburbType::Neighbourhood: radius = kMaxNeighbourhoodRadiusM; break;
      case ftypes::SuburbType::Quarter: radius = kMaxNeighbourhoodRadiusM; break;
      case ftypes::SuburbType::Suburb: radius = kMaxSuburbRadiusM; break;
      default: CHECK(false, ("Bad suburb type:", base::Underlying(suburbType)));
      }

      auto const rect = mercator::RectByCenterXYAndSizeInMeters(feature::GetCenter(*ft), radius);
      auto const suburbCBV = RetrieveGeometryFeatures(*m_context, rect, RectId::Suburb);
      auto const suburbStreets = ctx.m_streets.Intersect(suburbCBV);

      layer.m_getFeatures = [&suburbCBV]() { return suburbCBV; };

      ProcessStreets(ctx, centers, suburbStreets);

      MatchPOIsAndBuildings(ctx, 0 /* curToken */, suburbCBV);
    });
  }
}

template <class FnT>
void Geocoder::CentersFilter::ClusterizeStreets(std::vector<uint32_t> & streets,
                                                Geocoder const & geocoder, FnT && fn) const
{
  std::vector<std::tuple<double, m2::PointD, uint32_t>> loadedStreets;
  loadedStreets.reserve(streets.size());

  // Calculate {distance, center, feature id}.
  for (uint32_t fid : streets)
  {
    m2::PointD ftCenter;
    if (geocoder.m_context->GetCenter(fid, ftCenter))
    {
      double minDist = std::numeric_limits<double>::max();
      for (auto const & c : m_centers)
        minDist = std::min(minDist, ftCenter.Length(c));
      loadedStreets.emplace_back(minDist, ftCenter, fid);
    }
    else
    {
      // In general, we don't have centers for newly created features, but editor doesn't support streets now.
      ASSERT(false, ("Street feature without table's center"));
    }
  }

  // Sort by distance.
  std::sort(loadedStreets.begin(), loadedStreets.end(), [](auto const & t1, auto const & t2)
  {
    return std::get<0>(t1) < std::get<0>(t2);
  });

  buffer_vector<m2::RectD, 2> rects(m_centers.size());
  for (size_t i = 0; i < rects.size(); ++i)
  {
    rects[i] = mercator::RectByCenterXYAndSizeInMeters(
          m_centers[i], geocoder.m_params.m_filteringParams.m_streetSearchRadiusM);
  }

  // Find the first (after m_maxStreetsCount) street that is out of the rect's bounds
  size_t count = std::min(loadedStreets.size(), geocoder.m_params.m_filteringParams.m_maxStreetsCount);
  for (; count < loadedStreets.size(); ++count)
  {
    bool const outside = std::all_of(rects.begin(), rects.end(),
                                     [pt = std::get<1>(loadedStreets[count])](m2::RectD const & rect)
                                     {
                                       return !rect.IsPointInside(pt);
                                     });
    if (outside)
      break;
  }

  // Emit results.
  streets.clear();
  if (count == 0)
    return;

  double firstDist = std::get<0>(loadedStreets[0]);
  for (size_t i = 0; i < count; ++i)
  {
    double const currDist = std::get<0>(loadedStreets[i]);
    if (currDist - firstDist > geocoder.m_params.m_filteringParams.m_streetClusterRadiusMercator)
    {
      std::sort(streets.begin(), streets.end());
      fn();
      streets.clear();
      firstDist = currDist;
    }
    streets.push_back(std::get<2>(loadedStreets[i]));
  }
  std::sort(streets.begin(), streets.end());
  fn();
}

void Geocoder::CreateStreetsLayerAndMatchLowerLayers(BaseContext & ctx,
                                                     StreetsMatcher::Prediction const & prediction,
                                                     CentersFilter const & centers, bool makeRelaxed)
{
  auto & layers = ctx.m_layers;

  layers.emplace_back();
  SCOPE_GUARD(cleanupGuard, [&]{ layers.pop_back(); });

  auto & layer = layers.back();
  InitLayer(Model::TYPE_STREET, prediction.m_tokenRange, layer);

  vector<uint32_t> sortedFeatures;
  layer.m_sortedFeatures = &sortedFeatures;
  sortedFeatures.reserve(base::asserted_cast<size_t>(prediction.m_features.PopCount()));
  prediction.m_features.ForEach([&](uint64_t bit)
  {
    sortedFeatures.push_back(base::asserted_cast<uint32_t>(bit));
  });

  centers.ClusterizeStreets(sortedFeatures, *this, [&]()
  {
    ScopedMarkTokens mark(ctx.m_tokens, BaseContext::TOKEN_TYPE_STREET, prediction.m_tokenRange);
    size_t const numEmitted = ctx.m_numEmitted;

    MatchPOIsAndBuildings(ctx, 0 /* curToken */, CBV::GetFull());

    // A relaxed best effort parse: at least show the street if we can find one.
    if (makeRelaxed && numEmitted == ctx.m_numEmitted && !ctx.AllTokensUsed())
    {
      TRACE(Relaxed);
      FindPaths(ctx);
    }
  });
}

void Geocoder::MatchPOIsAndBuildings(BaseContext & ctx, size_t curToken, CBV const & filter)
{
  TRACE(MatchPOIsAndBuildings);

  BailIfCancelled();

  auto & layers = ctx.m_layers;

  size_t const numTokens = ctx.NumTokens();
  curToken = ctx.SkipUsedTokens(curToken);
  if (curToken == numTokens)
  {
    // All tokens were consumed, find paths through layers, emit features.
    if (m_postcodes.IsEmpty() || CityHasPostcode(ctx))
      return FindPaths(ctx);

    // When there are no layers but user entered a postcode, we have
    // to emit all features matching to the postcode.
    if (layers.size() == 0)
    {
      CBV filtered = m_postcodes.m_countryFeatures;
      if (m_filter->NeedToFilter(m_postcodes.m_countryFeatures))
        filtered = m_filter->Filter(m_postcodes.m_countryFeatures);
      filtered.ForEach([&](uint64_t bit)
      {
        auto const featureId = base::asserted_cast<uint32_t>(bit);
        Model::Type type;
        if (GetTypeInGeocoding(ctx, featureId, type))
        {
          EmitResult(ctx, {m_context->GetId(), featureId}, type, m_postcodes.m_tokenRange,
                     nullptr /* geoParts */, true /* allTokensUsed */, true /* exactMatch */);
        }
      });
      return;
    }

    if (!(layers.size() == 1 && layers[0].m_type == Model::TYPE_STREET))
      return FindPaths(ctx);

    // If there's only one street layer but user also entered a
    // postcode, we need to emit all features matching to postcode on
    // the given street, including the street itself.

    // Following code emits streets matched by postcodes, because
    // GreedilyMatchStreets() doesn't (and shouldn't) perform
    // postcodes matching.
    {
      for (auto const & id : *layers.back().m_sortedFeatures)
      {
        if (!m_postcodes.Has(id))
          continue;
        EmitResult(ctx, {m_context->GetId(), id}, Model::TYPE_STREET, layers.back().m_tokenRange,
                   nullptr /* geoParts */, true /* allTokensUsed */, true /* exactMatch */);
      }
    }

    // Following code creates a fake TYPE_BUILDING layer and intersects it with the streets layer.
    // Controversial: we can emit streets like buildings here. Filtered in IsFakeBuildingButStreet().
    layers.emplace_back();
    SCOPE_GUARD(cleanupGuard, [&]{ layers.pop_back(); });

    auto & layer = layers.back();
    InitLayer(Model::TYPE_BUILDING, m_postcodes.m_tokenRange, layer);

    vector<uint32_t> features;
    m_postcodes.m_countryFeatures.ForEach([&features](uint64_t bit)
    {
      features.push_back(base::asserted_cast<uint32_t>(bit));
    });
    layer.m_sortedFeatures = &features;
    return FindPaths(ctx);
  }

  layers.emplace_back();
  SCOPE_GUARD(cleanupGuard, [&]{ layers.pop_back(); });

  // Clusters of features by search type. Each cluster is a sorted
  // list of ids.
  size_t const kNumClusters = Model::TYPE_BUILDING + 1;
  vector<uint32_t> clusters[kNumClusters];

  // Appends |featureId| to the end of the corresponding cluster, if
  // any.
  auto const needPostcodes = !m_postcodes.IsEmpty() && !CityHasPostcode(ctx);
  auto clusterize = [&](uint64_t bit)
  {
    auto const featureId = base::asserted_cast<uint32_t>(bit);
    Model::Type type;
    if (!GetTypeInGeocoding(ctx, featureId, type))
      return;

    // All TYPE_CITY features were filtered in MatchCities().  All
    // TYPE_STREET features were filtered in GreedilyMatchStreets().
    if (type < kNumClusters)
    {
      if (!needPostcodes || m_postcodes.Has(featureId))
        clusters[type].push_back(featureId);
    }
  };

  Retrieval::ExtendedFeatures features(filter);

  // Try to consume [curToken, m_numTokens) tokens range.
  for (size_t idx = curToken; idx < numTokens && !ctx.IsTokenUsed(idx); ++idx)
  {
    // At this point |features| is the intersection of
    // m_addressFeatures[curToken], m_addressFeatures[curToken + 1],
    // ..., m_addressFeatures[curToken + n - 2].

    BailIfCancelled();

    size_t const endToken = idx + 1;
    {
      auto & layer = layers.back();
      InitLayer(layer.m_type, TokenRange(curToken, endToken), layer);
    }

    features = features.Intersect(ctx.m_features[idx]);

    CBV filtered = features.m_features;
    if (m_filter->NeedToFilter(features.m_features))
      filtered = m_filter->Filter(features.m_features);

    bool const looksLikeHouseNumber = house_numbers::LooksLikeHouseNumber(
        layers.back().m_subQuery, layers.back().m_lastTokenIsPrefix);
    if (filtered.IsEmpty() && !looksLikeHouseNumber)
      break;

    if (idx == curToken)
    {
      filtered.ForEach(clusterize);
    }
    else
    {
      auto noFeature = [&filtered](uint64_t bit) -> bool
      {
        return !filtered.HasBit(bit);
      };
      for (auto & cluster : clusters)
        base::EraseIf(cluster, noFeature);

      size_t curs[kNumClusters] = {};
      size_t ends[kNumClusters];
      for (size_t i = 0; i < kNumClusters; ++i)
        ends[i] = clusters[i].size();
      filtered.ForEach([&](uint64_t bit)
                       {
                         auto const featureId = base::asserted_cast<uint32_t>(bit);
                         bool found = false;
                         for (size_t i = 0; i < kNumClusters && !found; ++i)
                         {
                           size_t & cur = curs[i];
                           size_t const end = ends[i];
                           while (cur != end && clusters[i][cur] < featureId)
                             ++cur;
                           if (cur != end && clusters[i][cur] == featureId)
                             found = true;
                         }
                         if (!found)
                           clusterize(featureId);
                       });
      for (size_t i = 0; i < kNumClusters; ++i)
        inplace_merge(clusters[i].begin(), clusters[i].begin() + ends[i], clusters[i].end());
    }

    for (size_t i = 0; i < kNumClusters; ++i)
    {
      // ATTENTION: DO NOT USE layer after recursive calls to
      // MatchPOIsAndBuildings().  This may lead to use-after-free.
      auto & layer = layers.back();
      layer.m_sortedFeatures = &clusters[i];

      if (i == Model::TYPE_BUILDING)
      {
        if (layer.m_sortedFeatures->empty() && !looksLikeHouseNumber)
          continue;
      }
      else if (layer.m_sortedFeatures->empty() ||
              /// @todo The crutch, but can't invent a better solution now. Should refactor layers iteration.
              /// @see ProcessorTest_Smoke and Numeric_POI_Name tests.
               (house_numbers::LooksLikeHouseNumberStrict(layer.m_subQuery) && numTokens > 1))
      {
        continue;
      }

      layer.m_type = static_cast<Model::Type>(i);
      ScopedMarkTokens mark(ctx.m_tokens, BaseContext::FromModelType(layer.m_type),
                            TokenRange(curToken, endToken));
      if (IsLayerSequenceSane(layers))
        MatchPOIsAndBuildings(ctx, endToken, filter);
    }
  }
}

bool Geocoder::IsLayerSequenceSane(vector<FeaturesLayer> const & layers) const
{
  ASSERT(!layers.empty(), ());
  static_assert(Model::TYPE_COUNT <= 32, "Select a wider type to represent search types mask.");
  uint32_t mask = 0;
  size_t buildingIndex = layers.size();
  size_t streetIndex = layers.size();
  size_t poiIndex = layers.size();

  // Following loop returns false iff there are two different layers
  // of the same search type.
  for (size_t i = 0; i < layers.size(); ++i)
  {
    auto const & layer = layers[i];
    ASSERT_NOT_EQUAL(layer.m_type, Model::TYPE_COUNT, ());

    // TODO (@y): probably it's worth to check belongs-to-locality here.
    uint32_t bit = 1U << layer.m_type;
    if (mask & bit)
      return false;
    mask |= bit;

    if (layer.m_type == Model::TYPE_BUILDING)
      buildingIndex = i;
    else if (layer.m_type == Model::TYPE_STREET)
      streetIndex = i;
    else if (Model::IsPoi(layer.m_type))
      poiIndex = i;
  }

  bool const hasBuildings = buildingIndex != layers.size();
  bool const hasStreets = streetIndex != layers.size();
  bool const hasPois = poiIndex != layers.size();

  // Checks that building and street layers are neighbours.
  if (hasBuildings && hasStreets)
  {
    auto const & buildings = layers[buildingIndex];
    auto const & streets = layers[streetIndex];
    if (!buildings.m_tokenRange.AdjacentTo(streets.m_tokenRange))
      return false;
  }
  if (hasBuildings && hasPois && !hasStreets)
  {
    return false;
  }

  return true;
}

uint32_t Geocoder::MatchWorld2Country(FeatureID const & id) const
{
  if (m_context->m_handle.GetId() == id.m_mwmId)
    return id.m_index;

  ASSERT(id.IsWorld(), ());
  // Get source Feature's name and center.
  std::string name;
  m2::PointD pt;
  m_dataSource.ReadFeature([&](FeatureType & ft)
  {
    name = ft.GetName(StringUtf8Multilang::kDefaultCode);
    pt = feature::GetCenter(ft);
  }, id);

  // Match name and center in the current MwmContext.
  uint32_t resID = kInvalidFeatureId;
  m_context->ForEachFeature({pt, pt}, [&](FeatureType & ft)
  {
    if (resID == kInvalidFeatureId &&
        ft.GetName(StringUtf8Multilang::kDefaultCode) == name &&
        // Relaxed points comparison because geometry coding in the World and in a Country is different.
        feature::GetCenter(ft).EqualDxDy(pt, kMwmPointAccuracy * 100))
    {
      resID = ft.GetID().m_index;
    }
  });

  return resID;
}

void Geocoder::FindPaths(BaseContext & ctx)
{
  auto const & layers = ctx.m_layers;
  if (layers.empty())
    return;

  FeaturesLayer cityLayer;
  vector<uint32_t> cityFeature;

  if (ctx.m_city)
  {
    bool hasBuilding = false;
    bool hasStreetOrSuburb = false;
    for (auto const & l : layers)
    {
      switch (l.m_type)
      {
      case Model::TYPE_BUILDING:
        // Same condition as in MatchPOIsAndBuildings.
        hasBuilding = l.m_sortedFeatures->empty() || house_numbers::LooksLikeHouseNumberStrict(l.m_subQuery);
        break;
      case Model::TYPE_STREET:
      case Model::TYPE_SUBURB:
        hasStreetOrSuburb = true;
        break;
      case Model::TYPE_CITY:
      case Model::TYPE_COMPLEX_POI:
      case Model::TYPE_COUNT:
      case Model::TYPE_COUNTRY:
      case Model::TYPE_STATE:
      case Model::TYPE_SUBPOI:
      case Model::TYPE_UNCLASSIFIED:
      case Model::TYPE_VILLAGE:
        // TODO: These types could be processed in a different way in the future...
        break;
      }
    }

    if (hasBuilding && !hasStreetOrSuburb)
    {
      InitLayer(Model::TYPE_CITY, ctx.m_city->m_tokenRange, cityLayer);

      // Convert Feature's index, because a city maybe from World, but should match with Country MWM index.
      if (uint32_t fid = MatchWorld2Country(ctx.m_city->m_featureId); fid != kInvalidFeatureId)
        cityFeature.push_back(fid);
      cityLayer.m_sortedFeatures = &cityFeature;
      cityLayer.m_getFeatures = [this, &ctx]()
      {
        return RetrieveGeometryFeatures(*m_context, ctx.m_city->m_rect, RectId::Locality);
      };
    }
  }

  // Layers ordered by search type.
  vector<FeaturesLayer const *> sortedLayers;
  sortedLayers.reserve(layers.size() + 1);
  for (auto const & layer : layers)
    sortedLayers.push_back(&layer);

  if (!cityFeature.empty())
    sortedLayers.push_back(&cityLayer);

  sort(sortedLayers.begin(), sortedLayers.end(), base::LessBy(&FeaturesLayer::m_type));

  auto const & innermostLayer = *sortedLayers.front();

  if (m_postcodes.IsEmpty() || CityHasPostcode(ctx))
    m_matcher->SetPostcodes(nullptr);
  else
    m_matcher->SetPostcodes(&m_postcodes.m_countryFeatures);

  auto const isExactMatch = [&ctx](IntersectionResult const & result)
  {
    bool haveRegion = false;
    for (size_t i = 0; i < ctx.m_tokens.size(); ++i)
    {
      auto const tokenType = ctx.m_tokens[i];
      auto id = IntersectionResult::kInvalidId;

      if (tokenType == BaseContext::TokenType::TOKEN_TYPE_SUBPOI)
        id = result.m_subpoi;
      if (tokenType == BaseContext::TokenType::TOKEN_TYPE_COMPLEX_POI)
        id = result.m_complexPoi;
      if (tokenType == BaseContext::TokenType::TOKEN_TYPE_STREET)
        id = result.m_street;

      if (id != IntersectionResult::kInvalidId && ctx.m_features[i].m_features.HasBit(id) &&
          !ctx.m_features[i].m_exactMatchingFeatures.HasBit(id))
      {
        return false;
      }

      auto const isCityOrVillage = tokenType == BaseContext::TokenType::TOKEN_TYPE_CITY ||
                                   tokenType == BaseContext::TokenType::TOKEN_TYPE_VILLAGE;
      if (isCityOrVillage && ctx.m_city && !ctx.m_city->m_exactMatch)
        return false;

      auto const isRegion = tokenType == BaseContext::TokenType::TOKEN_TYPE_STATE ||
                            tokenType == BaseContext::TokenType::TOKEN_TYPE_COUNTRY;
      if (isRegion)
        haveRegion = true;
    }
    if (haveRegion)
    {
      for (auto const & region : ctx.m_regions)
      {
        if (!region->m_exactMatch)
          return false;
      }
    }

    return true;
  };

  m_finder.ForEachReachableVertex(*m_matcher, sortedLayers, [&](IntersectionResult const & result)
  {
    ASSERT(result.IsValid(), ());
    if (result.IsFakeBuildingButStreet())
      return;

    EmitResult(ctx, {m_context->GetId(), result.InnermostResult()}, innermostLayer.m_type,
               innermostLayer.m_tokenRange, &result, ctx.AllTokensUsed(),
               isExactMatch(result));
  });
}

void Geocoder::TraceResult(Tracer & tracer, BaseContext const & ctx, MwmSet::MwmId const & mwmId,
                           uint32_t ftId, Model::Type type, TokenRange const & tokenRange)
{
  SCOPE_GUARD(emitParse, [&]() { tracer.EmitParse(ctx.m_tokens); });

  if (!Model::IsPoiOrBuilding(type))
    return;

  if (mwmId != m_context->GetId())
    return;

  auto ft = m_context->GetFeature(ftId);
  if (!ft)
    return;

  feature::TypesHolder holder(*ft);
  CategoriesInfo catInfo(holder, TokenSlice(m_params, tokenRange), m_params.m_categoryLocales,
                         m_categories);

  emitParse.release();
  tracer.EmitParse(ctx.m_tokens, catInfo.IsPureCategories());
}

void Geocoder::EmitResult(BaseContext & ctx, FeatureID const & id,
                          Model::Type type, TokenRange const & tokenRange,
                          IntersectionResult const * geoParts, bool allTokensUsed, bool exactMatch)
{
  double matchedFraction = 1.0;
  // For categorial requests |allTokensUsed| == true and matchedFraction can not be calculated from |ctx|.
  if (!allTokensUsed)
  {
    size_t length = 0;
    size_t matchedLength = 0;
    size_t const numTokens = ctx.NumTokens();
    TokenSlice slice(m_params, TokenRange(0, numTokens));
    for (size_t tokenIdx = 0; tokenIdx < numTokens; ++tokenIdx)
    {
      auto const tokenLength = slice.Get(tokenIdx).GetOriginal().size();
      length += tokenLength;
      if (ctx.IsTokenUsed(tokenIdx))
        matchedLength += tokenLength;
    }
    CHECK_NOT_EQUAL(length, 0, ());

    matchedFraction = static_cast<double>(matchedLength) / static_cast<double>(length);
  }

  // In our samples the least value for matched fraction for relevant result is 0.116.
  // It is "Горнолыжный комплекс Ключи" feature for "Спортивно стрелковый комплекс Ключи
  // Новосибирск" query. It is relevant not found result (search does not find it, but it's
  // relevant). The least matched fraction for found relevant result is 0.241935, for found vital
  // result is 0.269231.
  if (matchedFraction <= 0.1)
    return;

  if (ctx.m_cuisineFilter && !ctx.m_cuisineFilter->Matches(id))
    return;

  if (m_params.m_tracer)
    TraceResult(*m_params.m_tracer, ctx, id.m_mwmId, id.m_index, type, tokenRange);

  // Distance and rank will be filled at the end, for all results at once.
  //
  // TODO (@y, @m): need to skip zero rank features that are too
  // distant from the pivot when there are enough results close to the
  // pivot.
  PreRankingInfo info(type, tokenRange);

  info.m_isCommonMatchOnly = true;
  for (size_t i : tokenRange)
  {
    if (!m_params.IsCommonToken(i))
    {
      info.m_isCommonMatchOnly = false;
      break;
    }
  }

  for (auto const & layer : ctx.m_layers)
    info.m_tokenRanges[layer.m_type] = layer.m_tokenRange;

  for (auto const * region : ctx.m_regions)
  {
    auto const regionType = Region::ToModelType(region->m_type);
    ASSERT_NOT_EQUAL(regionType, Model::TYPE_COUNT, ());
    info.m_tokenRanges[regionType] = region->m_tokenRange;
  }

  if (ctx.m_city)
  {
    auto const & city = *ctx.m_city;
    info.m_tokenRanges[city.m_type] = city.m_tokenRange;
    info.m_cityId = city.m_featureId;
  }

  if (geoParts)
    info.m_geoParts = *geoParts;

  info.m_allTokensUsed = allTokensUsed;
  info.m_exactMatch = exactMatch;

  m_preRanker.Emplace(id, info, m_resultTracer.GetProvenance());

  ++ctx.m_numEmitted;
}

void Geocoder::EmitResult(BaseContext & ctx, Region const & region, TokenRange const & tokenRange,
                          bool allTokensUsed, bool exactMatch)
{
  auto const type = Region::ToModelType(region.m_type);
  EmitResult(ctx, region.m_featureId, type, tokenRange, nullptr /* geoParts */,
             allTokensUsed, exactMatch);
}

void Geocoder::EmitResult(BaseContext & ctx, City const & city, TokenRange const & tokenRange,
                          bool allTokensUsed)
{
  EmitResult(ctx, city.m_featureId, city.m_type, tokenRange,
             nullptr /* geoParts */, allTokensUsed, city.m_exactMatch);
}

void Geocoder::MatchUnclassified(BaseContext & ctx, size_t curToken)
{
  TRACE(MatchUnclassified);

  ASSERT(ctx.m_layers.empty(), ());

  // We need to match all unused tokens to UNCLASSIFIED features,
  // therefore unused tokens must be adjacent to each other.  For
  // example, as parks are UNCLASSIFIED now, it's ok to match "London
  // Hyde Park", because London will be matched as a city and rest
  // adjacent tokens will be matched to "Hyde Park", whereas it's not
  // ok to match something to "Park London Hyde", because tokens
  // "Park" and "Hyde" are not adjacent.
  if (ctx.NumUnusedTokenGroups() != 1)
    return;

  Retrieval::ExtendedFeatures allFeatures;
  allFeatures.SetFull();

  curToken = ctx.SkipUsedTokens(curToken);
  auto startToken = curToken;
  for (; curToken < ctx.NumTokens() && !ctx.IsTokenUsed(curToken); ++curToken)
  {
    allFeatures = allFeatures.Intersect(ctx.m_features[curToken]);
  }

  if (m_filter->NeedToFilter(allFeatures.m_features))
  {
    allFeatures.m_features = m_filter->Filter(allFeatures.m_features);
    allFeatures.m_exactMatchingFeatures = m_filter->Filter(allFeatures.m_exactMatchingFeatures);
  }

  auto emitUnclassified = [&](uint32_t featureId, bool exactMatch)
  {
    Model::Type type;
    if (!GetTypeInGeocoding(ctx, featureId, type))
      return;
    if (type == Model::TYPE_UNCLASSIFIED)
    {
      auto const tokenRange = TokenRange(startToken, curToken);
      ScopedMarkTokens mark(ctx.m_tokens, BaseContext::TOKEN_TYPE_UNCLASSIFIED, tokenRange);
      EmitResult(ctx, {m_context->GetId(), featureId}, type, tokenRange,
                 nullptr /* geoParts */, true /* allTokensUsed */, exactMatch);
    }
  };
  allFeatures.ForEach(emitUnclassified);
}

CBV Geocoder::RetrievePostcodeFeatures(MwmContext const & context, TokenSlice const & slice)
{
  Retrieval retrieval(context, m_cancellable);
  return CBV(retrieval.RetrievePostcodeFeatures(slice));
}

CBV Geocoder::RetrieveGeometryFeatures(MwmContext const & context, m2::RectD const & rect,
                                       RectId id)
{
  switch (id)
  {
  case RectId::Pivot: return m_pivotRectsCache.Get(context, rect, m_params.m_scale);
  case RectId::Postcode: return m_postcodesRectsCache.Get(context, rect, m_params.m_scale);
  case RectId::Locality: return m_localityRectsCache.Get(context, rect, m_params.m_scale);
  case RectId::Suburb: return m_localityRectsCache.Get(context, rect, m_params.m_scale);
  case RectId::Count: ASSERT(false, ("Invalid RectId.")); return CBV();
  }
  UNREACHABLE();
}

bool Geocoder::GetTypeInGeocoding(BaseContext const & ctx, uint32_t featureId, Model::Type & type)
{
  if (ctx.m_streets.HasBit(featureId))
  {
    type = Model::TYPE_STREET;
    return true;
  }
  if (ctx.m_suburbs.HasBit(featureId))
  {
    type = Model::TYPE_SUBURB;
    return true;
  }
  if (ctx.m_villages.HasBit(featureId))
  {
    type = Model::TYPE_VILLAGE;
    return true;
  }

  auto feature = m_context->GetFeature(featureId);
  if (!feature)
    return false;

  type = m_model.GetType(*feature);
  return true;
}
}  // namespace search

#undef TRACE
