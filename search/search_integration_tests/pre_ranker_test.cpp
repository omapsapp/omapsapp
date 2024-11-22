#include "testing/testing.hpp"

#include "search/categories_cache.hpp"
#include "search/cities_boundaries_table.hpp"
#include "search/emitter.hpp"
#include "search/intermediate_result.hpp"
#include "search/model.hpp"
#include "search/pre_ranker.hpp"
#include "search/ranker.hpp"
#include "search/search_tests_support/helpers.hpp"
#include "search/search_tests_support/test_search_engine.hpp"
#include "search/suggest.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/features_vector.hpp"
#include "indexer/mwm_set.hpp"
#include "indexer/scales.hpp"

#include "generator/generator_tests_support/test_feature.hpp"
#include "generator/generator_tests_support/test_mwm_builder.hpp"

#include "geometry/mercator.hpp"

#include "platform/country_defines.hpp"
#include "platform/local_country_file.hpp"

#include "base/assert.hpp"
#include "base/cancellable.hpp"
#include "base/math.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <iterator>
#include <vector>

namespace pre_ranker_test
{
using namespace generator::tests_support;
using namespace search;
using namespace std;

class TestRanker : public Ranker
{
public:
  TestRanker(DataSource & dataSource, storage::CountryInfoGetter & infoGetter,
             CitiesBoundariesTable const & boundariesTable, KeywordLangMatcher & keywordsScorer,
             Emitter & emitter, vector<Suggest> const & suggests, VillagesCache & villagesCache,
             base::Cancellable const & cancellable, size_t limit, vector<PreRankerResult> & results)
    : Ranker(dataSource, boundariesTable, infoGetter, keywordsScorer, emitter,
             GetDefaultCategories(), suggests, villagesCache, cancellable)
    , m_results(results)
  {
    Ranker::Params rankerParams;
    Geocoder::Params geocoderParams;
    rankerParams.m_limit = limit;
    Init(rankerParams, geocoderParams);
  }

  inline bool Finished() const { return m_finished; }

  // Ranker overrides:
  void AddPreRankerResults(vector<PreRankerResult> && preRankerResults) override
  {
    CHECK(!Finished(), ());
    std::move(preRankerResults.begin(), preRankerResults.end(), back_inserter(m_results));
    preRankerResults.clear();
  }

  void UpdateResults(bool lastUpdate) override
  {
    CHECK(!Finished(), ());
    if (lastUpdate)
      m_finished = true;
  }

private:
  vector<PreRankerResult> & m_results;
  bool m_finished = false;
};

class PreRankerTest : public search::tests_support::SearchTest
{
public:
  vector<Suggest> m_suggests;
  base::Cancellable m_cancellable;
};

UNIT_CLASS_TEST(PreRankerTest, Smoke)
{
  // Tests that PreRanker correctly computes distances to pivot when
  // number of results is larger than batch size, and that PreRanker
  // emits results nearest to the pivot.

  m2::PointD const kPivot(0, 0);
  m2::RectD const kViewport(-5, -5, 5, 5);

  vector<TestPOI> pois;
  for (int x = -5; x <= 5; ++x)
  {
    for (int y = -5; y <= 5; ++y)
    {
      pois.emplace_back(m2::PointD(x, y), "cafe", "en");
      pois.back().SetTypes({{"amenity", "cafe"}});
    }
  }

  size_t const batchSize = pois.size() / 2;

  auto mwmId = BuildCountry("Cafeland", [&](TestMwmBuilder & builder)
  {
    for (auto const & poi : pois)
      builder.Add(poi);
  });

  vector<PreRankerResult> results;
  Emitter emitter;
  CitiesBoundariesTable boundariesTable(m_dataSource);
  VillagesCache villagesCache(m_cancellable);
  KeywordLangMatcher keywordsScorer(0 /* maxLanguageTiers */);

  TestRanker ranker(m_dataSource, m_engine.GetCountryInfoGetter(), boundariesTable, keywordsScorer,
                    emitter, m_suggests, villagesCache, m_cancellable, pois.size(), results);

  PreRanker preRanker(m_dataSource, ranker);
  PreRanker::Params params;
  params.m_viewport = kViewport;
  params.m_accuratePivotCenter = kPivot;
  params.m_scale = scales::GetUpperScale();
  params.m_everywhereBatchSize = batchSize;
  params.m_limit = pois.size();
  params.m_viewportSearch = false;
  preRanker.Init(params);

  vector<double> distances(pois.size());
  vector<bool> emit(pois.size());

  FeaturesVectorTest fv(mwmId.GetInfo()->GetLocalFile().GetPath(MapFileType::Map));
  fv.GetVector().ForEach([&](FeatureType & ft, uint32_t index)
  {
    FeatureID id(mwmId, index);
    ResultTracer::Provenance provenance;
    preRanker.Emplace(id, PreRankingInfo(Model::TYPE_SUBPOI, TokenRange(0, 1)), provenance);

    TEST_LESS(index, pois.size(), ());
    distances[index] = mercator::DistanceOnEarth(feature::GetCenter(ft), kPivot);
    emit[index] = true;
  });

  preRanker.UpdateResults(true /* lastUpdate */);

  TEST(all_of(emit.begin(), emit.end(), base::IdFunctor()), (emit));
  TEST(ranker.Finished(), ());

  size_t const count = results.size();
  // Depends on std::shuffle, but lets keep 6% threshold.
  TEST(count > batchSize*1.06 && count < batchSize*1.94, (count));

  vector<bool> checked(pois.size());
  for (size_t i = 0; i < count; ++i)
  {
    size_t const index = results[i].GetId().m_index;
    TEST_LESS(index, pois.size(), ());

    TEST(!checked[index], (index));
    TEST(AlmostEqualAbs(distances[index], results[i].GetDistance(), 1.0 /* 1 meter epsilon */),
         (distances[index], results[i].GetDistance()));
    checked[index] = true;
  }
}
} // namespace pre_ranker_test
