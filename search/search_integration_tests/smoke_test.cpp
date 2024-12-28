#include "testing/testing.hpp"

#include "search/search_tests_support/helpers.hpp"
#include "search/search_tests_support/test_results_matching.hpp"
#include "search/search_tests_support/test_search_request.hpp"

#include "generator/feature_builder.hpp"
#include "generator/generator_tests_support/test_feature.hpp"
#include "generator/generator_tests_support/test_mwm_builder.hpp"

#include "search/types_skipper.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/feature_visibility.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/string_utils.hpp"

#include <string>

namespace smoke_test
{
using namespace feature;
using namespace generator::tests_support;
using namespace search;
using namespace search::tests_support;
using namespace std;

class IncorrectCountry : public TestCountry
{
public:
  IncorrectCountry(m2::PointD const & center, string const & name, string const & lang)
    : TestCountry(center, name, lang)
  {
  }

  // TestFeature overrides:
  void Serialize(FeatureBuilder & fb) const override
  {
    fb.GetMetadata().Set(Metadata::FMD_TEST_ID, strings::to_string(m_id));
    fb.SetCenter(m_center);

    m_names.ForEach([&](int8_t langCode, string_view name)
    {
      if (!name.empty())
        fb.SetName(langCode, name);
    });

    auto const & classificator = classif();
    fb.AddType(classificator.GetTypeByPath({"place", "country"}));
  }
};

class SmokeTest : public SearchTest
{
};

class AlcoShop : public TestPOI
{
public:
  AlcoShop(m2::PointD const & center, string const & name, string const & lang)
    : TestPOI(center, name, lang)
  {
    SetTypes({{"shop", "alcohol"}});
  }
};

class SubwayStation : public TestPOI
{
public:
  SubwayStation(m2::PointD const & center, string const & name, string const & lang)
    : TestPOI(center, name, lang)
  {
    SetTypes({{"railway", "station", "subway"}});
  }
};

class SubwayStationMoscow : public TestPOI
{
public:
  SubwayStationMoscow(m2::PointD const & center, string const & name, string const & lang)
    : TestPOI(center, name, lang)
  {
    SetTypes({{"railway", "station", "subway", "moscow"}});
  }
};

UNIT_CLASS_TEST(SmokeTest, Smoke)
{
  char const kCountryName[] = "BuzzTown";

  AlcoShop wineShop(m2::PointD(0, 0), "Wine shop", "en");
  AlcoShop tequilaShop(m2::PointD(1, 0), "Tequila shop", "en");
  AlcoShop brandyShop(m2::PointD(0, 1), "Brandy shop", "en");
  AlcoShop vodkaShop(m2::PointD(1, 1), "Russian vodka shop", "en");

  auto id = BuildMwm(kCountryName, DataHeader::MapType::Country, [&](TestMwmBuilder & builder)
  {
    builder.Add(wineShop);
    builder.Add(tequilaShop);
    builder.Add(brandyShop);
    builder.Add(vodkaShop);
  });

  TEST_EQUAL(4, CountFeatures(m2::RectD(m2::PointD(0, 0), m2::PointD(1, 1))), ());
  TEST_EQUAL(2, CountFeatures(m2::RectD(m2::PointD(-0.5, -0.5), m2::PointD(0.5, 1.5))), ());

  SetViewport(m2::RectD(m2::PointD(0, 0), m2::PointD(100, 100)));
  {
    Rules rules = {ExactMatch(id, wineShop)};
    TEST(ResultsMatch("wine ", rules), ());
  }

  Rules const allRule = { ExactMatch(id, wineShop), ExactMatch(id, tequilaShop),
                          ExactMatch(id, brandyShop), ExactMatch(id, vodkaShop) };

  TEST(ResultsMatch("shop ", allRule), ());
  TEST(ResultsMatch("alcohol ", allRule), ());
  TEST(CategoryMatch("алкоголь", allRule, "ru"), ());
}

UNIT_CLASS_TEST(SmokeTest, DeepCategoryTest)
{
  char const kCountryName[] = "Wonderland";

  SubwayStation redStation(m2::PointD(0, 0), "red", "en");
  SubwayStationMoscow blueStation(m2::PointD(1, 1), "blue", "en");

  auto id = BuildMwm(kCountryName, DataHeader::MapType::Country, [&](TestMwmBuilder & builder) {
    builder.Add(redStation);
    builder.Add(blueStation);
  });

  SetViewport(m2::RectD(m2::PointD(0, 0), m2::PointD(1, 1)));
  {
    Rules rules = {ExactMatch(id, redStation), ExactMatch(id, blueStation)};
    TEST(ResultsMatch("Subway ", rules), ());
  }
}

UNIT_CLASS_TEST(SmokeTest, TypesSkipperTest)
{
  TypesSkipper skipper;

  auto const & cl = classif();

  {
    base::StringIL const arr[] = {{"entrance"}};
    TypesHolder types;
    for (auto const & path : arr)
      types.Add(cl.GetTypeByPath(path));

    TEST(!skipper.SkipAlways(types), ());
    TEST(!skipper.SkipSpecialNames(types, "ETH"), ());
    TEST(skipper.SkipSpecialNames(types, "2"), ());
  }

  {
    base::StringIL const arr[] = {{"building"}};
    TypesHolder types;
    for (auto const & path : arr)
      types.Add(cl.GetTypeByPath(path));

    TEST(!skipper.SkipAlways(types), ());
    TEST(!skipper.SkipSpecialNames(types, "3"), ());
    skipper.SkipEmptyNameTypes(types);
    TEST(types.Empty(), ());
  }

  {
    base::StringIL const arr[] = {{"building"}, {"entrance"}};
    TypesHolder types;
    for (auto const & path : arr)
      types.Add(cl.GetTypeByPath(path));

    TEST(!skipper.SkipAlways(types), ());
    TEST(!skipper.SkipSpecialNames(types, "4"), ());
    skipper.SkipEmptyNameTypes(types);
    TEST_EQUAL(types.Size(), 1, ());
  }
}

UNIT_CLASS_TEST(SmokeTest, CategoriesTest)
{
  // Checks all types in categories.txt for their searchability,
  // which also depends on point drawability and presence of a name.

  auto const & cl = classif();

  /// @todo Should rewrite test
  base::StringIL const arrNotPoint[] = {
      // Area types without suitable point drawing rules.
      {"area:highway", "steps"},
      {"landuse", "basin"},
      {"natural", "glacier"},

      // Linear types.
      {"waterway", "canal"},
      {"waterway", "river"},
      {"waterway", "stream"},
      {"highway", "footway"},
      {"highway", "cycleway"},
      {"highway", "living_street"},
      {"highway", "motorway"},
      {"highway", "motorway_link"},
      {"highway", "path"},
      {"highway", "pedestrian"},
      {"highway", "primary"},
      {"highway", "primary_link"},
      {"highway", "raceway"},
      {"highway", "residential"},
      {"highway", "road"},
      {"highway", "secondary"},
      {"highway", "secondary_link"},
      {"highway", "service"},
      {"highway", "steps"},
      {"highway", "tertiary"},
      {"highway", "tertiary_link"},
      {"highway", "track"},
      {"highway", "traffic_signals"},
      {"highway", "trunk"},
      {"highway", "trunk_link"},
      {"highway", "unclassified"},
      {"historic", "citywalls"},
      {"piste:type", "downhill"},
      {"piste:type", "nordic"},
  };
  set<uint32_t> notPointTypes;
  for (auto const & tags : arrNotPoint)
    notPointTypes.insert(cl.GetTypeByPath(tags));

  // No point drawing rules for country scale range.
  base::StringIL const arrInvisible[] = {
      {"place", "continent"},
      {"place", "county"},
      {"place", "region"},
  };
  set<uint32_t> invisibleTypes;
  for (auto const & tags : arrInvisible)
    invisibleTypes.insert(cl.GetTypeByPath(tags));

  // Not indexed types for Features with empty names.
  base::StringIL const arrNoEmptyNames[] = {
      {"area:highway"},
      {"building"},
      {"highway", "motorway_junction"},
      {"landuse"},
      {"man_made", "chimney"},
      {"man_made", "flagpole"},
      {"man_made", "mast"},
      {"man_made", "tower"},
      {"man_made", "water_tower"},
      {"natural"},
      {"office"},
      {"place"},
      {"waterway"},

      /// @todo Controversial here.
      /// Don't have point drawing rules (a label only), hence type will be removed for an empty name Feature.
      {"building", "train_station"},
      {"leisure", "track"},
      {"natural", "beach"},
  };
  set<uint32_t> noEmptyNames;
  for (auto const & tags : arrNoEmptyNames)
    noEmptyNames.insert(cl.GetTypeByPath(tags));

  ftypes::TwoLevelPOIChecker isPoi;
  auto const isNoEmptyName = [&isPoi, &noEmptyNames](uint32_t t)
  {
    ftype::TruncValue(t, 2);
    if (noEmptyNames.count(t) > 0)
      return true;

    if (isPoi(t))
      return false;

    ftype::TruncValue(t, 1);
    return (noEmptyNames.count(t) > 0);
  };

  auto const & holder = GetDefaultCategories();

  uint32_t const cafeType = cl.GetTypeByPath({"amenity", "cafe"});

  auto testCategory = [&](uint32_t type, CategoriesHolder::Category const &)
  {
    if (notPointTypes.count(type) > 0)
      return;

    size_t resultIdx = 2;
    if (invisibleTypes.count(type) == 0)
    {
      if (isNoEmptyName(type))
        resultIdx = 1;
      else
        resultIdx = 0;
    }

    string const countryName = "Wonderland";

    TestPOI withName({1.0, 1.0}, "The Name", "en");
    if (IsCategoryNondrawableType(type))
      withName.SetTypes({type, cafeType});
    else
      withName.SetTypes({type});

    TestPOI withoutName({2.0, 2.0}, "", "");
    if (IsCategoryNondrawableType(type))
      withoutName.SetTypes({type, cafeType});
    else
      withoutName.SetTypes({type});

    auto id = BuildMwm(countryName, DataHeader::MapType::Country, [&](TestMwmBuilder & builder)
    {
      builder.AddSafe(withName);
      builder.AddSafe(withoutName);
    });

    Rules const rules[] = {
      {ExactMatch(id, withName), ExactMatch(id, withoutName)},
      {ExactMatch(id, withName)},
      {}
    };
    auto const query = holder.GetReadableFeatureType(type, CategoriesHolder::kEnglishCode);

    // If you have "Unsatisfied rules" error, consider:
    // - adding type to POIs here TwoLevelPOIChecker or
    // - review TypesSkipper or
    // - adding type to |arrNoEmptyNames| or |arrInvisible|
    TEST(CategoryMatch(query, rules[resultIdx]), (query, cl.GetReadableObjectName(type)));

    DeregisterMap(countryName);
  };

  holder.ForEachTypeAndCategory(testCategory);
}

UNIT_CLASS_TEST(SmokeTest, NotPrefixFreeNames)
{
  char const kCountryName[] = "ATown";

  auto id = BuildMwm(kCountryName, DataHeader::MapType::Country, [&](TestMwmBuilder & builder) {
    builder.Add(TestPOI(m2::PointD(0, 0), "a", "en"));
    builder.Add(TestPOI(m2::PointD(0, 1), "aa", "en"));
    builder.Add(TestPOI(m2::PointD(1, 1), "aa", "en"));
    builder.Add(TestPOI(m2::PointD(1, 0), "aaa", "en"));
    builder.Add(TestPOI(m2::PointD(2, 0), "aaa", "en"));
    builder.Add(TestPOI(m2::PointD(2, 1), "aaa", "en"));
  });

  TEST_EQUAL(6, CountFeatures(m2::RectD(m2::PointD(0, 0), m2::PointD(2, 2))), ());

  m2::RectD const viewport(m2::PointD(0, 0), m2::PointD(100, 100));
  {
    TestSearchRequest request(m_engine, "a ", "en", Mode::Viewport, viewport);
    request.Run();
    TEST_EQUAL(1, request.Results().size(), ());
  }
  {
    TestSearchRequest request(m_engine, "aa ", "en", Mode::Viewport, viewport);
    request.Run();
    TEST_EQUAL(2, request.Results().size(), ());
  }
  {
    TestSearchRequest request(m_engine, "aaa ", "en", search::Mode::Viewport, viewport);
    request.Run();
    TEST_EQUAL(3, request.Results().size(), ());
  }
}

UNIT_CLASS_TEST(SmokeTest, NoDefaultNameTest)
{
  char const kCountryName[] = "Wonderland";

  IncorrectCountry wonderland(m2::PointD(0, 0), kCountryName, "en");
  auto worldId = BuildWorld([&](TestMwmBuilder & builder) { builder.Add(wonderland); });

  SetViewport(m2::RectD(m2::PointD(-0.5, -0.5), m2::PointD(0.5, 0.5)));
  TEST(ResultsMatch("Wonderland", {ExactMatch(worldId, wonderland)}), ());
}

UNIT_CLASS_TEST(SmokeTest, PoiWithAddress)
{
  char const kCountryName[] = "Wonderland";
  TestStreet mainStreet({m2::PointD(0.0, 0.0), m2::PointD(1.0, 1.0), m2::PointD(2.0, 2.0)},
                        "Main Street", "en");
  TestCafe cafe(m2::PointD(1.0, 1.0), "Starbucks", "en");
  cafe.SetStreetName(mainStreet.GetName("en"));
  cafe.SetHouseNumber("27");

  auto id = BuildMwm(kCountryName, DataHeader::MapType::Country, [&](TestMwmBuilder & builder) {
    builder.Add(mainStreet);
    builder.Add(cafe);
  });

  SetViewport(m2::RectD(m2::PointD(0.0, 0.0), m2::PointD(2.0, 2.0)));
  {
    Rules rules = {ExactMatch(id, cafe)};
    TEST(ResultsMatch("Starbucks ", rules), ());
    TEST(ResultsMatch("Main street 27 ", rules), ());
    TEST(ResultsMatch("Main street 27 Starbucks ", rules), ());
    TEST(ResultsMatch("Starbucks Main street 27 ", rules), ());
  }
}
} // namespace smoke_test
