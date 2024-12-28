#include "testing/testing.hpp"

#include "search/reverse_geocoder.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/data_source.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/mwm_set.hpp"
#include "indexer/search_string_utils.hpp"

#include "platform/preferred_languages.hpp"

#include "coding/string_utf8_multilang.hpp"

#include <memory>
#include <string>

namespace address_tests
{
using namespace search;
using namespace platform;

void TestAddress(ReverseGeocoder & coder, ms::LatLon const & ll, std::string_view street,
                 std::string const & houseNumber)
{
  ReverseGeocoder::Address addr;
  coder.GetNearbyAddress(mercator::FromLatLon(ll), addr);

  std::string const expectedKey =
      strings::ToUtf8(GetStreetNameAsKey(street, false /* ignoreStreetSynonyms */));
  std::string const resultKey =
      strings::ToUtf8(GetStreetNameAsKey(addr.m_street.m_name, false /* ignoreStreetSynonyms */));

  TEST_EQUAL(resultKey, expectedKey, (addr));
  TEST_EQUAL(houseNumber, addr.GetHouseNumber(), (addr));
}

void TestAddress(ReverseGeocoder & coder, std::shared_ptr<MwmInfo> mwmInfo, ms::LatLon const & ll,
                 StringUtf8Multilang const & streetNames, std::string const & houseNumber)
{
  feature::NameParamsOut out;
  feature::GetReadableName({ streetNames, mwmInfo->GetRegionData(), languages::GetCurrentMapLanguage(),
                             false /* allowTranslit */ }, out);

  TestAddress(coder, ll, out.primary, houseNumber);
}

UNIT_TEST(ReverseGeocoder_Smoke)
{
  classificator::Load();

  LocalCountryFile file = LocalCountryFile::MakeForTesting("minsk-pass");

  FrozenDataSource dataSource;
  auto const regResult = dataSource.RegisterMap(file);
  TEST_EQUAL(regResult.second, MwmSet::RegResult::Success, ());

  auto mwmInfo = regResult.first.GetInfo();

  TEST(mwmInfo != nullptr, ());

  ReverseGeocoder coder(dataSource);

  auto const currentLocale = languages::GetCurrentMapLanguage();

  {
    StringUtf8Multilang streetNames;
    streetNames.AddString("default", "улица Мясникова");
    streetNames.AddString("int_name", "vulica Miasnikova");
    streetNames.AddString("be", "вуліца Мяснікова");
    streetNames.AddString("ru", "улица Мясникова");
    TestAddress(coder, mwmInfo, {53.89815, 27.54265}, streetNames, "32");
  }
  {
    StringUtf8Multilang streetNames;
    streetNames.AddString("default", "улица Немига");
    streetNames.AddString("int_name", "vulica Niamiha");
    streetNames.AddString("be", "вуліца Няміга");
    streetNames.AddString("ru", "улица Немига");
    TestAddress(coder, mwmInfo, {53.8997617, 27.5429365}, streetNames, "40");
  }
  {
    StringUtf8Multilang streetNames;
    streetNames.AddString("default", "Советская улица");
    streetNames.AddString("int_name", "Savieckaja vulica");
    streetNames.AddString("be", "Савецкая вуліца");
    streetNames.AddString("ru", "Советская улица");
    TestAddress(coder, mwmInfo, {53.89666, 27.54904}, streetNames, "19");
  }
  {
    StringUtf8Multilang streetNames;
    streetNames.AddString("default", "проспект Независимости");
    streetNames.AddString("int_name", "praspiekt Niezaliežnasci");
    streetNames.AddString("be", "праспект Незалежнасці");
    streetNames.AddString("ru", "проспект Независимости");
    TestAddress(coder, mwmInfo, {53.89724, 27.54983}, streetNames, "11");
  }
  {
    StringUtf8Multilang streetNames;
    streetNames.AddString("int_name", "vulica Karla Marksa");
    streetNames.AddString("default", "улица Карла Маркса");
    streetNames.AddString("be", "вуліца Карла Маркса");
    streetNames.AddString("ru", "улица Карла Маркса");
    TestAddress(coder, mwmInfo, {53.89745, 27.55835}, streetNames, "18А");
  }
}
} // namespace address_tests
