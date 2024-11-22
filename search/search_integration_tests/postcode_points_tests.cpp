#include "testing/testing.hpp"

#include "search/postcode_points.hpp"
#include "search/search_tests_support/helpers.hpp"

#include "generator/generator_tests_support/test_mwm_builder.hpp"

#include "storage/country_info_getter.hpp"

#include "indexer/search_string_utils.hpp"

#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"

#include "coding/point_coding.hpp"

#include "base/file_name_utils.hpp"


namespace postcode_points_tests
{
using namespace generator::tests_support;
using namespace platform::tests_support;
using namespace search::tests_support;
using namespace search;
using namespace std;

class PostcodePointsTest : public SearchTest
{
};

UNIT_CLASS_TEST(PostcodePointsTest, SmokeUK)
{
  string const countryName = "United Kingdom";

  Platform & platform = GetPlatform();
  auto const & writableDir = platform.WritableDir();
  string const testFile = "postcodes.csv";
  auto const postcodesRelativePath = base::JoinPath(writableDir, testFile);

  ScopedFile const osmScopedFile(testFile,
                                 "header\n"
                                 "aa11 0bb, 1.0, 1.0\n"
                                 "aa11 1bb, 2.0, 2.0\n"
                                 "aa11 2bb, 3.0, 3.0\n"
  );

  auto infoGetter = std::make_shared<storage::CountryInfoGetterForTesting>();
  infoGetter->AddCountry(storage::CountryDef(countryName,
      m2::RectD(mercator::FromLatLon(0.99, 0.99), mercator::FromLatLon(3.01, 3.01))));

  auto const id = BuildCountry(countryName, [&](TestMwmBuilder & builder)
  {
    builder.SetPostcodesData(postcodesRelativePath, indexer::PostcodePointsDatasetType::UK, infoGetter);
  });

  auto handle = m_dataSource.GetMwmHandleById(id);
  auto value = handle.GetValue();
  CHECK(value, ());
  TEST(value->m_cont.IsExist(POSTCODE_POINTS_FILE_TAG), ());

  PostcodePoints p(*value);
  {
    vector<m2::PointD> points;
    p.Get(NormalizeAndSimplifyString("aa11 0bb"), points);
    TEST_EQUAL(points.size(), 1, ());
    TEST(AlmostEqualAbs(points[0], mercator::FromLatLon(1.0, 1.0), kMwmPointAccuracy), ());
  }
  {
    vector<m2::PointD> points;
    p.Get(NormalizeAndSimplifyString("aa11 1bb"), points);
    TEST_EQUAL(points.size(), 1, ());
    TEST(AlmostEqualAbs(points[0], mercator::FromLatLon(2.0, 2.0), kMwmPointAccuracy), ());
  }
  {
    vector<m2::PointD> points;
    p.Get(NormalizeAndSimplifyString("aa11 2bb"), points);
    TEST_EQUAL(points.size(), 1, ());
    TEST(AlmostEqualAbs(points[0], mercator::FromLatLon(3.0, 3.0), kMwmPointAccuracy), ());
  }
  {
    vector<m2::PointD> points;
    p.Get(NormalizeAndSimplifyString("aa11"), points);
    TEST_EQUAL(points.size(), 3, ());
    sort(points.begin(), points.end());
    TEST(AlmostEqualAbs(points[0], mercator::FromLatLon(1.0, 1.0), kMwmPointAccuracy), ());
    TEST(AlmostEqualAbs(points[1], mercator::FromLatLon(2.0, 2.0), kMwmPointAccuracy), ());
    TEST(AlmostEqualAbs(points[2], mercator::FromLatLon(3.0, 3.0), kMwmPointAccuracy), ());
  }
}

UNIT_CLASS_TEST(PostcodePointsTest, SmokeUS)
{
  string const countryName = "United States of America";

  Platform & platform = GetPlatform();
  auto const & writableDir = platform.WritableDir();
  string const testFile = "postcodes.csv";
  auto const postcodesRelativePath = base::JoinPath(writableDir, testFile);

  ScopedFile const osmScopedFile(testFile,
                                 "header\n"
                                 R"("00601","18.18027","-66.75266","Adjuntas","PR","Puerto Rico","TRUE","","16834","100.9","72001","Adjuntas","{""72001"": 98.74, ""72141"": 1.26}","Adjuntas|Utuado","72001|72141","FALSE","FALSE","America/Puerto_Rico")"
  );

  auto infoGetter = std::make_shared<storage::CountryInfoGetterForTesting>();
  infoGetter->AddCountry(storage::CountryDef(countryName,
      m2::RectD(mercator::FromLatLon(0, -180), mercator::FromLatLon(90, 0))));

  auto const id = BuildCountry(countryName, [&](TestMwmBuilder & builder)
  {
    builder.SetPostcodesData(postcodesRelativePath, indexer::PostcodePointsDatasetType::UK, infoGetter);
  });

  auto handle = m_dataSource.GetMwmHandleById(id);
  auto value = handle.GetValue();
  CHECK(value, ());
  TEST(value->m_cont.IsExist(POSTCODE_POINTS_FILE_TAG), ());

  PostcodePoints p(*value);
  {
    vector<m2::PointD> points;
    p.Get(NormalizeAndSimplifyString("00601"), points);
    TEST_EQUAL(points.size(), 1, ());
    TEST(AlmostEqualAbs(points[0], mercator::FromLatLon(18.18027, -66.75266), kMwmPointAccuracy), ());
  }
}

UNIT_CLASS_TEST(PostcodePointsTest, SearchPostcode)
{
  string const countryName = "Wonderland";

  Platform & platform = GetPlatform();
  auto const & writableDir = platform.WritableDir();
  string const testFile = "postcodes.csv";
  auto const postcodesRelativePath = base::JoinPath(writableDir, testFile);

  // Use the same latitude for easier center calculation.
  ScopedFile const osmScopedFile(testFile,
                                 "header\n"
                                 "BA6 7JP, 5.0, 4.0\n"
                                 "BA6 8JP, 5.0, 6.0\n");

  auto infoGetter = std::make_shared<storage::CountryInfoGetterForTesting>();
  infoGetter->AddCountry(storage::CountryDef(
      countryName,
      m2::RectD(mercator::FromLatLon(3.0, 3.0), mercator::FromLatLon(7.0, 7.0))));

  auto const id = BuildCountry(countryName, [&](TestMwmBuilder & builder)
  {
    builder.SetPostcodesData(postcodesRelativePath, indexer::PostcodePointsDatasetType::UK, infoGetter);
  });

  auto test = [&](string const & query, m2::PointD const & expected) {
    auto request = MakeRequest(query);
    auto const & results = request->Results();
    TEST_EQUAL(results.size(), 1, ());

    auto const & result = results[0];
    TEST_EQUAL(result.GetResultType(), Result::Type::Postcode, ());
    TEST(result.HasPoint(), ());

    auto const actual = result.GetFeatureCenter();
    TEST(AlmostEqualAbs(expected, actual, kMwmPointAccuracy), ());
  };

  test("BA6 7JP", mercator::FromLatLon(5.0, 4.0));
  test("BA6 7JP ", mercator::FromLatLon(5.0, 4.0));
  test("BA6 8JP", mercator::FromLatLon(5.0, 6.0));
  test("BA6 8JP ", mercator::FromLatLon(5.0, 6.0));
  // Search should return center of all inward codes for outward query.
  test("BA6", mercator::FromLatLon(5.0, 5.0));
  test("BA6 ", mercator::FromLatLon(5.0, 5.0));
}

UNIT_CLASS_TEST(PostcodePointsTest, SearchStreetWithPostcode)
{
  string const countryName = "Wonderland";

  Platform & platform = GetPlatform();
  auto const & writableDir = platform.WritableDir();
  string const testFile = "postcodes.csv";
  auto const postcodesRelativePath = base::JoinPath(writableDir, testFile);

  ScopedFile const osmScopedFile(
      testFile,
      "header\n"
      "AA5 6KL, 4.0, 4.0\n"
      "BB7 8MN, 6.0, 6.0\n"
      "XX6 7KL, 4.0, 6.0\n"
      "YY8 9MN, 6.0, 4.0\n"
      // Some dummy postcodes to make postcode radius approximation not too big.
      "CC1 0AA, 5.0, 5.0\n"
      "CC1 0AB, 5.0, 5.0\n"
      "CC1 0AC, 5.0, 5.0\n"
      "CC1 0AD, 5.0, 5.0\n"
      "CC1 0AE, 5.0, 5.0\n"
      "CC1 0AF, 5.0, 5.0\n"
      "CC1 0AG, 5.0, 5.0\n"
      "CC1 0AH, 5.0, 5.0\n"
      "CC1 0AI, 5.0, 5.0\n"
      "CC1 0AJ, 5.0, 5.0\n"
      "CC1 0AK, 5.0, 5.0\n"
      "CC1 0AL, 5.0, 5.0\n"
      "CC1 0AM, 5.0, 5.0\n"
      "CC1 0AN, 5.0, 5.0\n"
      "CC1 0AO, 5.0, 5.0\n"
      "CC1 0AP, 5.0, 5.0\n"
      "CC1 0AQ, 5.0, 5.0\n"
      "CC1 0AR, 5.0, 5.0\n"
      "CC1 0AS, 5.0, 5.0\n"
      "CC1 0AT, 5.0, 5.0\n");

  auto const rect =
      m2::RectD(mercator::FromLatLon(3.99, 3.99), mercator::FromLatLon(6.01, 6.01));
  auto infoGetter = std::make_shared<storage::CountryInfoGetterForTesting>();
  infoGetter->AddCountry(storage::CountryDef(countryName, rect));

  TestStreet streetA(vector<m2::PointD>{mercator::FromLatLon(3.99, 3.99), mercator::FromLatLon(4.01, 4.01)},
                     "Garden street", "en");
  TestPOI houseA(mercator::FromLatLon(4.0, 4.0), "", "en");
  houseA.SetHouseNumber("1");
  houseA.SetStreetName(streetA.GetName("en"));
  TestStreet streetB(vector<m2::PointD>{mercator::FromLatLon(5.99, 5.99), mercator::FromLatLon(6.01, 6.01)},
                     "Garden street", "en");
  TestPOI houseB(mercator::FromLatLon(6.0, 6.0), "", "en");
  houseB.SetHouseNumber("1");
  houseB.SetStreetName(streetB.GetName("en"));
  TestStreet streetX(vector<m2::PointD>{mercator::FromLatLon(3.99, 5.99), mercator::FromLatLon(4.01, 6.01)},
                     "Main street", "en");
  TestStreet streetY(vector<m2::PointD>{mercator::FromLatLon(5.99, 3.99), mercator::FromLatLon(6.01, 4.01)},
                     "Main street", "en");

  auto const id = BuildCountry(countryName, [&](TestMwmBuilder & builder)
  {
    builder.SetPostcodesData(postcodesRelativePath, indexer::PostcodePointsDatasetType::UK, infoGetter);

    builder.Add(streetA);
    builder.Add(houseA);
    builder.Add(streetB);
    builder.Add(houseB);
    builder.Add(streetX);
    builder.Add(streetY);
  });

  auto test = [&](string const & query, TestFeature const & bestResult) {
    auto request = MakeRequest(query);
    auto const & results = request->Results();

    TEST_GREATER_OR_EQUAL(results.size(), 1, ("Unexpected number of results."));
    TEST(ResultsMatch({results[0]}, {ExactMatch(id, bestResult)}), ());
    TEST(results[0].GetRankingInfo().m_allTokensUsed, ());
    for (size_t i = 1; i < results.size(); ++i)
      TEST(!results[i].GetRankingInfo().m_allTokensUsed, ());
  };
  SetViewport(rect);
  test("Garden street 1 AA5 6KL ", houseA);
  test("Garden street 1 BB7 8MN ", houseB);
  test("Main street XX6 7KL ", streetX);
  test("Main street YY8 9MN ", streetY);
}

} // namespace postcode_points_tests
