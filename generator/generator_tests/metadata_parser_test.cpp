#include "testing/testing.hpp"

#include "generator/generator_tests_support/test_with_classificator.hpp"
#include "generator/osm2meta.hpp"

#include "indexer/classificator.hpp"

#include "base/logging.hpp"

using namespace generator::tests_support;

using feature::Metadata;

UNIT_TEST(Metadata_ValidateAndFormat_stars)
{
  FeatureBuilderParams params;
  MetadataTagProcessor p(params);
  Metadata & md = params.GetMetadata();

  // Ignore incorrect values.
  p("stars", "0");
  TEST(md.Empty(), ());
  p("stars", "-1");
  TEST(md.Empty(), ());
  p("stars", "aasdasdas");
  TEST(md.Empty(), ());
  p("stars", "8");
  TEST(md.Empty(), ());
  p("stars", "10");
  TEST(md.Empty(), ());
  p("stars", "910");
  TEST(md.Empty(), ());
  p("stars", "100");
  TEST(md.Empty(), ());

  // Check correct values.
  p("stars", "1");
  TEST_EQUAL(md.Get(Metadata::FMD_STARS), "1", ());
  md.Drop(Metadata::FMD_STARS);

  p("stars", "2");
  TEST_EQUAL(md.Get(Metadata::FMD_STARS), "2", ());
  md.Drop(Metadata::FMD_STARS);

  p("stars", "3");
  TEST_EQUAL(md.Get(Metadata::FMD_STARS), "3", ());
  md.Drop(Metadata::FMD_STARS);

  p("stars", "4");
  TEST_EQUAL(md.Get(Metadata::FMD_STARS), "4", ());
  md.Drop(Metadata::FMD_STARS);

  p("stars", "5");
  TEST_EQUAL(md.Get(Metadata::FMD_STARS), "5", ());
  md.Drop(Metadata::FMD_STARS);

  p("stars", "6");
  TEST_EQUAL(md.Get(Metadata::FMD_STARS), "6", ());
  md.Drop(Metadata::FMD_STARS);

  p("stars", "7");
  TEST_EQUAL(md.Get(Metadata::FMD_STARS), "7", ());
  md.Drop(Metadata::FMD_STARS);

  // Check almost correct values.
  p("stars", "4+");
  TEST_EQUAL(md.Get(Metadata::FMD_STARS), "4", ());
  md.Drop(Metadata::FMD_STARS);

  p("stars", "5s");
  TEST_EQUAL(md.Get(Metadata::FMD_STARS), "5", ());
  md.Drop(Metadata::FMD_STARS);

}

UNIT_CLASS_TEST(TestWithClassificator, Metadata_ValidateAndFormat_operator)
{
  uint32_t const typeAtm = classif().GetTypeByPath({"amenity", "atm"});
  uint32_t const typeFuel = classif().GetTypeByPath({"amenity", "fuel"});
  uint32_t const typeCarSharing = classif().GetTypeByPath({"amenity", "car_sharing"});
  uint32_t const typeCarRental = classif().GetTypeByPath({"amenity", "car_rental"});

  FeatureBuilderParams params;
  MetadataTagProcessor p(params);
  Metadata & md = params.GetMetadata();

  // Ignore tag 'operator' if feature have inappropriate type.
  p("operator", "Some");
  TEST(md.Empty(), ());

  params.SetType(typeAtm);
  p("operator", "Some1");
  TEST_EQUAL(md.Get(Metadata::FMD_OPERATOR), "Some1", ());

  params.SetType(typeFuel);
  p("operator", "Some2");
  TEST_EQUAL(md.Get(Metadata::FMD_OPERATOR), "Some2", ());

  params.SetType(typeCarSharing);
  params.AddType(typeCarRental);
  p("operator", "Some3");
  TEST_EQUAL(md.Get(Metadata::FMD_OPERATOR), "Some3", ());
}

UNIT_TEST(Metadata_ValidateAndFormat_height)
{
  FeatureBuilderParams params;
  MetadataTagProcessor p(params);
  Metadata & md = params.GetMetadata();

  p("height", "0");
  TEST(md.Empty(), ());

  p("height", "0,0000");
  TEST(md.Empty(), ());

  p("height", "0.0");
  TEST(md.Empty(), ());

  p("height", "123");
  TEST_EQUAL(md.Get(Metadata::FMD_HEIGHT), "123", ());
  md.Drop(Metadata::FMD_HEIGHT);

  p("height", "123.2");
  TEST_EQUAL(md.Get(Metadata::FMD_HEIGHT), "123.2", ());
  md.Drop(Metadata::FMD_HEIGHT);

  p("height", "2 m");
  TEST_EQUAL(md.Get(Metadata::FMD_HEIGHT), "2", ());
  md.Drop(Metadata::FMD_HEIGHT);

  p("height", "3-6");
  TEST_EQUAL(md.Get(Metadata::FMD_HEIGHT), "6", ());
}

UNIT_TEST(Metadata_ValidateAndFormat_wikipedia)
{
  char const * kWikiKey = "wikipedia";

  FeatureBuilderParams params;
  MetadataTagProcessor p(params);
  Metadata & md = params.GetMetadata();

#ifdef OMIM_OS_MOBILE
  #define WIKIHOST "m.wikipedia.org"
#else
  #define WIKIHOST "wikipedia.org"
#endif

  struct Test
  {
    char const * source;
    char const * validated;
    char const * url;
  };
  constexpr Test tests[] = {
    {"en:Bad %20Data", "en:Bad %20Data", "https://en." WIKIHOST "/wiki/Bad_%20Data"},
    {"ru:Это тест_со знаками %, ? (и скобками)", "ru:Это тест со знаками %, ? (и скобками)", "https://ru." WIKIHOST "/wiki/Это_тест_со_знаками_%,_%3F_(и_скобками)"},
    {"https://be-tarask.wikipedia.org/wiki/Вялікае_Княства_Літоўскае", "be-tarask:Вялікае Княства Літоўскае", "https://be-tarask." WIKIHOST "/wiki/Вялікае_Княства_Літоўскае"},
    // Final link points to https and mobile version.
    {"http://en.wikipedia.org/wiki/A#id", "en:A#id", "https://en." WIKIHOST "/wiki/A#id"},
  };

  for (auto [source, validated, url] : tests)
  {
    p(kWikiKey, source);
    TEST_EQUAL(md.Get(Metadata::FMD_WIKIPEDIA), validated, (source));
    TEST_EQUAL(md.GetWikiURL(), url, (source));
    md.Drop(Metadata::FMD_WIKIPEDIA);
  }

  p(kWikiKey, "invalid_input_without_language_and_colon");
  TEST(md.Empty(), (md.Get(Metadata::FMD_WIKIPEDIA)));

  p(kWikiKey, "https://en.wikipedia.org/wiki/");
  TEST(md.Empty(), (md.Get(Metadata::FMD_WIKIPEDIA)));

  p(kWikiKey, "http://wikipedia.org/wiki/Article");
  TEST(md.Empty(), (md.Get(Metadata::FMD_WIKIPEDIA)));

  p(kWikiKey, "http://somesite.org");
  TEST(md.Empty(), (md.Get(Metadata::FMD_WIKIPEDIA)));

  p(kWikiKey, "http://www.spamsitewithaslash.com/");
  TEST(md.Empty(), (md.Get(Metadata::FMD_WIKIPEDIA)));

  p(kWikiKey, "http://.wikipedia.org/wiki/Article");
  TEST(md.Empty(), (md.Get(Metadata::FMD_WIKIPEDIA)));

  // Ignore incorrect prefixes.
  p(kWikiKey, "ht.tps://en.wikipedia.org/wiki/Whuh");
  TEST_EQUAL(md.Get(Metadata::FMD_WIKIPEDIA), "en:Whuh", ());
  md.Drop(Metadata::FMD_WIKIPEDIA);

  p(kWikiKey, "http://ru.google.com/wiki/wutlol");
  TEST(md.Empty(), ("Not a wikipedia site."));

#undef WIKIHOST
}

UNIT_TEST(Metadata_ValidateAndFormat_wikimedia_commons)
{
  char const * kWikiKey = "wikimedia_commons";

  FeatureBuilderParams params;
  MetadataTagProcessor p(params);
  Metadata & md = params.GetMetadata();

  p(kWikiKey, "File:Boğaz (105822801).jpeg");
  TEST_EQUAL(md.Get(Metadata::FMD_WIKIMEDIA_COMMONS), "File:Boğaz (105822801).jpeg", ());

  p(kWikiKey, "Category:Bosphorus");
  TEST_EQUAL(md.Get(Metadata::FMD_WIKIMEDIA_COMMONS), "Category:Bosphorus", ());

  TEST_EQUAL(md.ToWikimediaCommonsURL("Category:What If? (Bonn)"), "https://commons.wikimedia.org/wiki/Category:What_If%3F_(Bonn)", ());

  md.Drop(Metadata::FMD_WIKIMEDIA_COMMONS);
  p(kWikiKey, "incorrect_wikimedia_content");
  TEST(md.Get(Metadata::FMD_WIKIMEDIA_COMMONS).empty(), ());
}

// Look at: https://wiki.openstreetmap.org/wiki/Key:duration for details
// about "duration" format.

UNIT_CLASS_TEST(TestWithClassificator, Metadata_ValidateAndFormat_duration)
{
  FeatureBuilderParams params;
  params.AddType(classif().GetTypeByPath({"route", "ferry"}));
  MetadataTagProcessor p(params);
  Metadata & md = params.GetMetadata();

  auto const test = [&](std::string const & osm, std::string const & expected) {
    p("duration", osm);

    if (expected.empty())
    {
      TEST(md.Empty(), ());
    }
    else
    {
      TEST_EQUAL(md.Get(Metadata::FMD_DURATION), expected, ());
      md.Drop(Metadata::FMD_DURATION);
    }
  };

  // "10" - 10 minutes ~ 0.16667 hours
  test("10", "0.16667");
  // 10:00 - 10 hours
  test("10:00", "10");
  test("QWE", "");
  // 1:1:1 - 1 hour + 1 minute + 1 second
  test("1:1:1", "1.0169");
  // 10 hours and 30 minutes
  test("10:30", "10.5");
  test("30", "0.5");
  test("60", "1");
  test("120", "2");
  test("35:10", "35.167");

  test("35::10", "");
  test("", "");
  test("0", "");
  test("asd", "");
  test("10 minutes", "");
  test("01:15 h", "");
  test("08:00;07:00;06:30", "");
  test("3-4 minutes", "");
  test("5:00 hours", "");
  test("12 min", "");

  // means 20 seconds
  test("PT20S", "0.0055556");
  // means 7 minutes
  test("PT7M", "0.11667");
  // means 10 minutes and 40 seconds
  test("PT10M40S", "0.17778");
  test("PT50M", "0.83333");
  // means 2 hours
  test("PT2H", "2");
  // means 7 hours and 50 minutes
  test("PT7H50M", "7.8333");
  test("PT60M", "1");
  test("PT15M", "0.25");

  // means 1000 years, but we don't support such duration.
  test("PT1000Y", "");
  test("PTPT", "");
  // means 4 day, but we don't support such duration.
  test("P4D", "");
  test("PT50:20", "");
}


UNIT_CLASS_TEST(TestWithClassificator, ValidateAndFormat_facebook)
{
  FeatureBuilderParams params;
  MetadataTagProcessor p(params);
  Metadata & md = params.GetMetadata();

  p("contact:facebook", "");
  TEST(md.Empty(), ());

  p("contact:facebook", "osm.us");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_FACEBOOK), "osm.us", ());
  md.Drop(Metadata::FMD_CONTACT_FACEBOOK);

  p("contact:facebook", "@vtbgroup");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_FACEBOOK), "vtbgroup", ());
  md.Drop(Metadata::FMD_CONTACT_FACEBOOK);

  p("contact:facebook", "https://www.facebook.com/pyaterochka");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_FACEBOOK), "pyaterochka", ());
  md.Drop(Metadata::FMD_CONTACT_FACEBOOK);

  p("contact:facebook", "facebook.de/mcdonaldsbonn/");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_FACEBOOK), "mcdonaldsbonn", ());
  md.Drop(Metadata::FMD_CONTACT_FACEBOOK);

  p("contact:facebook", "https://facebook.com/238702340219158/posts/284664265622965");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_FACEBOOK), "238702340219158/posts/284664265622965", ());
  md.Drop(Metadata::FMD_CONTACT_FACEBOOK);

  p("contact:facebook", "https://facebook.com/238702340219158/posts/284664265622965");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_FACEBOOK), "238702340219158/posts/284664265622965", ());
  md.Drop(Metadata::FMD_CONTACT_FACEBOOK);

  p("contact:facebook", "https://fr-fr.facebook.com/people/Paillote-Lgm/100012630853826/");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_FACEBOOK), "people/Paillote-Lgm/100012630853826", ());
  md.Drop(Metadata::FMD_CONTACT_FACEBOOK);

  p("contact:facebook", "https://www.sandwichparlour.com.au/");
  TEST(md.Empty(), ());
}

UNIT_CLASS_TEST(TestWithClassificator, ValidateAndFormat_instagram)
{
  FeatureBuilderParams params;
  MetadataTagProcessor p(params);
  Metadata & md = params.GetMetadata();

  p("contact:instagram", "");
  TEST(md.Empty(), ());

  p("contact:instagram", "instagram.com/openstreetmapus");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_INSTAGRAM), "openstreetmapus", ());
  md.Drop(Metadata::FMD_CONTACT_INSTAGRAM);

  p("contact:instagram", "www.instagram.com/openstreetmapus");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_INSTAGRAM), "openstreetmapus", ());
  md.Drop(Metadata::FMD_CONTACT_INSTAGRAM);

  p("contact:instagram", "https://instagram.com/openstreetmapus");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_INSTAGRAM), "openstreetmapus", ());
  md.Drop(Metadata::FMD_CONTACT_INSTAGRAM);

  p("contact:instagram", "https://en-us.instagram.com/openstreetmapus/");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_INSTAGRAM), "openstreetmapus", ());
  md.Drop(Metadata::FMD_CONTACT_INSTAGRAM);

  p("contact:instagram", "@open.street.map.us");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_INSTAGRAM), "open.street.map.us", ());
  md.Drop(Metadata::FMD_CONTACT_INSTAGRAM);

  p("contact:instagram", "_osm_");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_INSTAGRAM), "_osm_", ());
  md.Drop(Metadata::FMD_CONTACT_INSTAGRAM);

  p("contact:instagram", "https://www.instagram.com/explore/locations/358536820/trivium-sport-en-dance/");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_INSTAGRAM), "explore/locations/358536820/trivium-sport-en-dance", ());
  md.Drop(Metadata::FMD_CONTACT_INSTAGRAM);

  p("contact:instagram", "https://www.instagram.com/explore/tags/boojum/");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_INSTAGRAM), "explore/tags/boojum", ());
  md.Drop(Metadata::FMD_CONTACT_INSTAGRAM);

  p("contact:instagram", "https://www.instagram.com/p/BvkgKZNDbqN");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_INSTAGRAM), "p/BvkgKZNDbqN", ());
  md.Drop(Metadata::FMD_CONTACT_INSTAGRAM);

  p("contact:instagram", "dharampura road");
  TEST(md.Empty(), ());

  p("contact:instagram", "https://twitter.com/theuafpub");
  TEST(md.Empty(), ());

  p("contact:instagram", ".dots_not_allowed.");
  TEST(md.Empty(), ());
}

UNIT_CLASS_TEST(TestWithClassificator, ValidateAndFormat_twitter)
{
  FeatureBuilderParams params;
  MetadataTagProcessor p(params);
  Metadata & md = params.GetMetadata();

  p("contact:twitter", "");
  TEST(md.Empty(), ());

  p("contact:twitter", "https://twitter.com/hashtag/sotanosiete");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_TWITTER), "hashtag/sotanosiete", ());
  md.Drop(Metadata::FMD_CONTACT_TWITTER);

  p("contact:twitter", "twitter.com/osm_tech");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_TWITTER), "osm_tech", ());
  md.Drop(Metadata::FMD_CONTACT_TWITTER);

  p("contact:twitter", "http://twitter.com/osm_tech");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_TWITTER), "osm_tech", ());
  md.Drop(Metadata::FMD_CONTACT_TWITTER);

  p("contact:twitter", "https://twitter.com/osm_tech");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_TWITTER), "osm_tech", ());
  md.Drop(Metadata::FMD_CONTACT_TWITTER);

  p("contact:twitter", "osm_tech");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_TWITTER), "osm_tech", ());
  md.Drop(Metadata::FMD_CONTACT_TWITTER);

  p("contact:twitter", "@the_osm_tech");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_TWITTER), "the_osm_tech", ());
  md.Drop(Metadata::FMD_CONTACT_TWITTER);

  p("contact:twitter", "dharampura road");
  TEST(md.Empty(), ());

  p("contact:twitter", "http://www.facebook.com/pages/tree-house-interiors/333581653310");
  TEST(md.Empty(), ());

  p("contact:twitter", "dots.not.allowed");
  TEST(md.Empty(), ());

  p("contact:twitter", "@AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
  TEST(md.Empty(), ());
}

UNIT_CLASS_TEST(TestWithClassificator, ValidateAndFormat_vk)
{
  FeatureBuilderParams params;
  MetadataTagProcessor p(params);
  Metadata & md = params.GetMetadata();

  p("contact:vk", "");
  TEST(md.Empty(), ());

  p("contact:vk", "vk.com/osm63ru");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_VK), "osm63ru", ());
  md.Drop(Metadata::FMD_CONTACT_VK);

  p("contact:vk", "www.vk.com/osm63ru");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_VK), "osm63ru", ());
  md.Drop(Metadata::FMD_OPERATOR);

  p("contact:vk", "http://vk.com/osm63ru");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_VK), "osm63ru", ());
  md.Drop(Metadata::FMD_CONTACT_VK);

  p("contact:vk", "https://vk.com/osm63ru");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_VK), "osm63ru", ());
  md.Drop(Metadata::FMD_CONTACT_VK);

  p("contact:vk", "https://www.vk.com/osm63ru");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_VK), "osm63ru", ());
  md.Drop(Metadata::FMD_CONTACT_VK);

  p("contact:vk", "osm63ru");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_VK), "osm63ru", ());
  md.Drop(Metadata::FMD_CONTACT_VK);

  p("contact:vk", "@osm63ru");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_VK), "osm63ru", ());
  md.Drop(Metadata::FMD_CONTACT_VK);

  p("contact:vk", "@_invalid_underscores_");
  TEST(md.Empty(), ());

  p("contact:vk", "http://www.facebook.com/pages/tree-house-interiors/333581653310");
  TEST(md.Empty(), ());

  p("contact:vk", "invalid-dashes");
  TEST(md.Empty(), ());

  p("contact:vk", "@AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
  TEST(md.Empty(), ());
}

UNIT_CLASS_TEST(TestWithClassificator, ValidateAndFormat_contactLine)
{
  FeatureBuilderParams params;
  MetadataTagProcessor p(params);
  Metadata & md = params.GetMetadata();

  p("contact:line", "");
  TEST(md.Empty(), ());

  p("contact:line", "http://line.me/ti/p/mzog4fnz24");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "mzog4fnz24", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://line.me/ti/p/xnv0g02rws");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "xnv0g02rws", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://line.me/ti/p/@dgxs9r6wad");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "dgxs9r6wad", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://line.me/ti/p/%40vne5uwke17");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "vne5uwke17", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "http://line.me/R/ti/p/bfsg1a8x9u");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "bfsg1a8x9u", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://line.me/R/ti/p/gdltt7s380");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "gdltt7s380", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://line.me/R/ti/p/@sdb2pb3lsg");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "sdb2pb3lsg", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://line.me/R/ti/p/%40b30h5mdj11");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "b30h5mdj11", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "http://line.me/R/home/public/main?id=hmczqsbav5");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "hmczqsbav5", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://line.me/R/home/public/main?id=wa1gvx91jb");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "wa1gvx91jb", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "http://line.me/R/home/public/profile?id=5qll5dyqqu");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "5qll5dyqqu", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://line.me/R/home/public/profile?id=r90ck7n1rq");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "r90ck7n1rq", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://line.me/R/home/public/profile?id=r90ck7n1rq");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "r90ck7n1rq", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://page.line.me/fom5198h");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "fom5198h", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://page.line.me/qn58n8g?web=mobile");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "qn58n8g", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "https://abc.line.me/en/some/page?id=xaladqv");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "abc.line.me/en/some/page?id=xaladqv", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "@abcd");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "abcd", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "0000");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "0000", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", ".dots.are.allowed.");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), ".dots.are.allowed.", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "@.dots.are.allowed.");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), ".dots.are.allowed.", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "-hyphen-test-");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "-hyphen-test-", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "@-hyphen-test-");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "-hyphen-test-", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "under_score");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "under_score", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "@under_score");
  TEST_EQUAL(md.Get(Metadata::FMD_CONTACT_LINE), "under_score", ());
  md.Drop(Metadata::FMD_CONTACT_LINE);

  p("contact:line", "no");
  TEST(md.Empty(), ());

  p("contact:line", "yes");
  TEST(md.Empty(), ());

  p("contact:line", "No-upper-case");
  TEST(md.Empty(), ());

  p("contact:line", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  TEST(md.Empty(), ());

  p("contact:line", "https://line.com/ti/p/invalid-domain");
  TEST(md.Empty(), ());
}

UNIT_TEST(Metadata_ValidateAndFormat_ele)
{
  FeatureBuilderParams params;
  MetadataTagProcessorImpl tagProc(params);
  TEST_EQUAL(tagProc.ValidateAndFormat_ele(""), "", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("not a number"), "", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("0"), "0", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("0.0"), "0", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("0.0000000"), "0", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("22.5"), "22.5", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("-100.3"), "-100.3", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("99.0000000"), "99", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("8900.000023"), "8900", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("-300.9999"), "-301", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("-300.9"), "-300.9", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("15 m"), "15", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("15.9 m"), "15.9", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("15.9m"), "15.9", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("3000 ft"), "914.4", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("3000ft"), "914.4", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("100 feet"), "30.48", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("100feet"), "30.48", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("11'"), "3.35", ());
  TEST_EQUAL(tagProc.ValidateAndFormat_ele("11'4\""), "3.45", ());
}

UNIT_TEST(Metadata_ValidateAndFormat_building_levels)
{
  FeatureBuilderParams params;
  MetadataTagProcessorImpl tp(params);
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("４"), "4", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("４floors"), "4", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("between 1 and ４"), "", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("0"), "0", ("OSM has many zero-level buildings."));
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("0.0"), "0", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels(""), "", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("Level 1"), "", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("2.51"), "2.5", ());
  TEST_EQUAL(tp.ValidateAndFormat_building_levels("250"), "", ("Too many levels."));
}

UNIT_TEST(Metadata_ValidateAndFormat_url)
{
  std::array<std::pair<char const*, char const*>, 9> constexpr kTests =
  {{
    {"a.by", "a.by"},
    {"http://test.com", "http://test.com"},
    {"https://test.com", "https://test.com"},
    {"test.com", "test.com"},
    {"http://test.com/", "http://test.com"},
    {"https://test.com/", "https://test.com"},
    {"test.com/", "test.com"},
    {"test.com/path", "test.com/path"},
    {"test.com/path/", "test.com/path/"},
  }};

  FeatureBuilderParams params;
  MetadataTagProcessorImpl tp(params);
  for (auto const& [input, output] : kTests)
    TEST_EQUAL(tp.ValidateAndFormat_url(input), output, ());
}
