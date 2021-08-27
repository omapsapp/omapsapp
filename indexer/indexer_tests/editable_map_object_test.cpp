#include "testing/testing.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/editable_map_object.hpp"
#include "indexer/feature.hpp"

#include <string>
#include <vector>

using namespace std;

namespace
{
using osm::EditableMapObject;

int8_t GetLangCode(char const * ch)
{
  return StringUtf8Multilang::GetLangIndex(ch);
}

struct ExpectedName
{
  string m_lang;
  string m_value;
};

string DebugPrint(ExpectedName const & expectedName)
{
  return expectedName.m_lang + ", " + expectedName.m_value;
}

void CheckExpectations(StringUtf8Multilang const & s, vector<ExpectedName> const & expectations)
{
  size_t counter = 0;
  s.ForEach([&expectations, &counter](int8_t const code, string const & name) {
    auto const it = find_if(expectations.begin(), expectations.end(), [&code](ExpectedName const & item)
    {
      return GetLangCode(item.m_lang.c_str()) == code;
    });

    if (it == expectations.end())
      TEST(false, ("Unexpected language code: ", code, ". Expectations: ", expectations));

    TEST_EQUAL(name, it->m_value, ());
    ++counter;
  });

  TEST_EQUAL(counter, expectations.size(), ("Unexpected count of names, expected ", expectations.size(),
                                            ", but turned out ", counter, ". Expectations: ", expectations));
}
  
UNIT_TEST(EditableMapObject_SetWebsite)
{
  EditableMapObject emo;
  emo.SetWebsite("https://some.thing.org");
  TEST_EQUAL(emo.GetWebsite(), "https://some.thing.org", ());

  emo.SetWebsite("http://some.thing.org");
  TEST_EQUAL(emo.GetWebsite(), "http://some.thing.org", ());

  emo.SetWebsite("some.thing.org");
  TEST_EQUAL(emo.GetWebsite(), "http://some.thing.org", ());

  emo.SetWebsite("");
  TEST_EQUAL(emo.GetWebsite(), "", ());
}

UNIT_TEST(EditableMapObject_ValidateBuildingLevels)
{
  TEST(EditableMapObject::ValidateBuildingLevels(""), ());
  TEST(EditableMapObject::ValidateBuildingLevels("7"), ());
  TEST(EditableMapObject::ValidateBuildingLevels("17"), ());
  TEST(EditableMapObject::ValidateBuildingLevels("25"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("0"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("005"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("26"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("22a"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("a22"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("2a22"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels("ab"), ());
  TEST(!EditableMapObject::ValidateBuildingLevels(
      "2345534564564453645534545345534564564453645"), ());
}

UNIT_TEST(EditableMapObject_ValidateHouseNumber)
{
  TEST(EditableMapObject::ValidateHouseNumber(""), ());
  TEST(EditableMapObject::ValidateHouseNumber("qwer7ty"), ());
  TEST(EditableMapObject::ValidateHouseNumber("12345678"), ());

  // House number must contain at least one number.
  TEST(!EditableMapObject::ValidateHouseNumber("qwerty"), ());
  // House number is too long.
  TEST(!EditableMapObject::ValidateHouseNumber("1234567890123456"), ());
}

UNIT_TEST(EditableMapObject_ValidateFlats)
{
  TEST(EditableMapObject::ValidateFlats(""), ());
  TEST(EditableMapObject::ValidateFlats("123"), ());
  TEST(EditableMapObject::ValidateFlats("123a"), ());
  TEST(EditableMapObject::ValidateFlats("a"), ());
  TEST(EditableMapObject::ValidateFlats("123-456;a-e"), ());
  TEST(EditableMapObject::ValidateFlats("123-456"), ());
  TEST(EditableMapObject::ValidateFlats("123-456; 43-45"), ());
  TEST(!EditableMapObject::ValidateFlats("123-456, 43-45"), ());
  TEST(!EditableMapObject::ValidateFlats("234-234 124"), ());
  TEST(!EditableMapObject::ValidateFlats("123-345-567"), ());
  TEST(!EditableMapObject::ValidateFlats("234-234;234("), ());
  TEST(!EditableMapObject::ValidateFlats("-;"), ());
}

// See search_tests/postcodes_matcher_test.cpp
// UNIT_TEST(EditableMapObject_ValidatePostCode)
// {
// }

UNIT_TEST(EditableMapObject_ValidatePhoneList)
{
  TEST(EditableMapObject::ValidatePhoneList(""), ());
  TEST(EditableMapObject::ValidatePhoneList("+7 000 000 00 00"), ());
  TEST(EditableMapObject::ValidatePhoneList("+7 (000) 000 00 00"), ());
  TEST(EditableMapObject::ValidatePhoneList("+7 0000000000"), ());
  TEST(EditableMapObject::ValidatePhoneList("+7 0000 000 000"), ());
  TEST(EditableMapObject::ValidatePhoneList("8 0000-000-000"), ());

  TEST(EditableMapObject::ValidatePhoneList("000 00 00"), ());
  TEST(EditableMapObject::ValidatePhoneList("000 000 00"), ());
  TEST(EditableMapObject::ValidatePhoneList("+00 0000 000 000"), ());

  TEST(EditableMapObject::ValidatePhoneList("+7 000 000 00 00; +7 000 000 00 00"), ());
  TEST(EditableMapObject::ValidatePhoneList("+7 (000) 000 00 00, +7 (000) 000 00 00"), ());
  TEST(EditableMapObject::ValidatePhoneList("+7 0000000000;+7 0000000000"), ());
  TEST(EditableMapObject::ValidatePhoneList("+7 0000 000 000,+7 0000 000 000"), ());
  TEST(EditableMapObject::ValidatePhoneList("8 0000-000-000; 8 0000-000-000"), ());

  TEST(EditableMapObject::ValidatePhoneList("+7 00 00;7 (0)00 0, 800-00-0; 000000000000000,12345"), ());

  TEST(!EditableMapObject::ValidatePhoneList("+00 0000 000 0000 000"), ());
  TEST(!EditableMapObject::ValidatePhoneList("00 00"), ());
  TEST(!EditableMapObject::ValidatePhoneList("acb"), ());
  TEST(!EditableMapObject::ValidatePhoneList("000 000 00b"), ());
  TEST(!EditableMapObject::ValidatePhoneList(";"), ());
  TEST(!EditableMapObject::ValidatePhoneList(","), ());
  TEST(!EditableMapObject::ValidatePhoneList(";;;;;;"), ());

  // Now it is possible to specify the following incorrect phone numbers.
  // TODO: replace current implementation of ValidatePhoneList by a correct one.
  TEST(EditableMapObject::ValidatePhoneList("7+ 10 10"), ());
  TEST(EditableMapObject::ValidatePhoneList("+7 )10( 10"), ());
  TEST(EditableMapObject::ValidatePhoneList("+7 )10 10"), ());
  TEST(EditableMapObject::ValidatePhoneList("+7 10 (---) 10"), ());
}

UNIT_TEST(EditableMapObject_ValidateWebsite)
{
  TEST(EditableMapObject::ValidateWebsite(""), ());
  TEST(EditableMapObject::ValidateWebsite("qwe.rty"), ());
  TEST(EditableMapObject::ValidateWebsite("http://websit.e"), ());
  TEST(EditableMapObject::ValidateWebsite("https://websit.e"), ());

  TEST(!EditableMapObject::ValidateWebsite("qwerty"), ());
  TEST(!EditableMapObject::ValidateWebsite(".qwerty"), ());
  TEST(!EditableMapObject::ValidateWebsite("qwerty."), ());
  TEST(!EditableMapObject::ValidateWebsite(".qwerty."), ());
  TEST(!EditableMapObject::ValidateWebsite("w..com"), ());
  TEST(!EditableMapObject::ValidateWebsite("http://.websit.e"), ());
  TEST(!EditableMapObject::ValidateWebsite("https://.websit.e"), ());
  TEST(!EditableMapObject::ValidateWebsite("http://"), ());
  TEST(!EditableMapObject::ValidateWebsite("https://"), ());
}

UNIT_TEST(EditableMapObject_ValidateEmail)
{
  TEST(EditableMapObject::ValidateEmail(""), ());
  TEST(EditableMapObject::ValidateEmail("e@ma.il"), ());
  TEST(EditableMapObject::ValidateEmail("e@ma.i.l"), ());
  TEST(EditableMapObject::ValidateEmail("e-m.ail@dot.com.gov"), ());
  TEST(EditableMapObject::ValidateEmail("#$%&'*+-/=?^`_{}|~.@dot.qw.com.gov"), ());

  TEST(!EditableMapObject::ValidateEmail("e.ma.il"), ());
  TEST(!EditableMapObject::ValidateEmail("e@ma@il"), ());
  TEST(!EditableMapObject::ValidateEmail("e@ma@i.l"), ());
  TEST(!EditableMapObject::ValidateEmail("e@mail"), ());
  TEST(!EditableMapObject::ValidateEmail("@email.a"), ());
  TEST(!EditableMapObject::ValidateEmail("emai.l@"), ());
  TEST(!EditableMapObject::ValidateEmail("emai@l."), ());
  TEST(!EditableMapObject::ValidateEmail("e mai@l.com"), ());
  TEST(!EditableMapObject::ValidateEmail("emai@.l"), ());
  TEST(!EditableMapObject::ValidateEmail("emai@_l.ab"), ());
  TEST(!EditableMapObject::ValidateEmail("emai@l_.ab"), ());
  TEST(!EditableMapObject::ValidateEmail("email@e#$%&'*+-/=?^`_{}|~.com"), ());
}

UNIT_TEST(EditableMapObject_ValidateFacebookPage)
{
  TEST(EditableMapObject::ValidateFacebookPage(""), ());
  TEST(EditableMapObject::ValidateFacebookPage("facebook.com/OpenStreetMap"), ());
  TEST(EditableMapObject::ValidateFacebookPage("www.facebook.com/OpenStreetMap"), ());
  TEST(EditableMapObject::ValidateFacebookPage("http://facebook.com/OpenStreetMap"), ());
  TEST(EditableMapObject::ValidateFacebookPage("https://facebook.com/OpenStreetMap"), ());
  TEST(EditableMapObject::ValidateFacebookPage("http://www.facebook.com/OpenStreetMap"), ());
  TEST(EditableMapObject::ValidateFacebookPage("https://www.facebook.com/OpenStreetMap"), ());
  TEST(EditableMapObject::ValidateFacebookPage("https://en-us.facebook.com/OpenStreetMap"), ());
  TEST(EditableMapObject::ValidateFacebookPage("OpenStreetMap"), ());
  TEST(EditableMapObject::ValidateFacebookPage("some.good.page"), ());

  TEST(!EditableMapObject::ValidateFacebookPage("instagram.com/openstreetmapus"), ());
  TEST(!EditableMapObject::ValidateFacebookPage("https://instagram.com/openstreetmapus"), ());
  TEST(!EditableMapObject::ValidateFacebookPage("OpenStreetMap.com"), ());
  TEST(!EditableMapObject::ValidateFacebookPage("osm"), ());
  TEST(!EditableMapObject::ValidateFacebookPage("osm.net"), ());
}

UNIT_TEST(EditableMapObject_ValidateInstagramPage)
{
  TEST(EditableMapObject::ValidateInstagramPage(""), ());
  TEST(EditableMapObject::ValidateInstagramPage("instagram.com/openstreetmapus"), ());
  TEST(EditableMapObject::ValidateInstagramPage("www.instagram.com/openstreetmapus"), ());
  TEST(EditableMapObject::ValidateInstagramPage("http://instagram.com/openstreetmapus"), ());
  TEST(EditableMapObject::ValidateInstagramPage("https://instagram.com/openstreetmapus"), ());
  TEST(EditableMapObject::ValidateInstagramPage("http://www.instagram.com/openstreetmapus"), ());
  TEST(EditableMapObject::ValidateInstagramPage("https://www.instagram.com/openstreetmapus"), ());
  TEST(EditableMapObject::ValidateInstagramPage("https://en-us.instagram.com/openstreetmapus"), ());
  TEST(EditableMapObject::ValidateInstagramPage("openstreetmapus"), ());
  TEST(EditableMapObject::ValidateInstagramPage("open.street.map.us"), ());
  TEST(EditableMapObject::ValidateInstagramPage("open_street_map_us"), ());
  TEST(EditableMapObject::ValidateInstagramPage("_osm_"), ());

  TEST(!EditableMapObject::ValidateInstagramPage("facebook.com/osm_us"), ());
  TEST(!EditableMapObject::ValidateInstagramPage("https://facebook.com/osm_us"), ());
  TEST(!EditableMapObject::ValidateInstagramPage(".osm"), ());
  TEST(!EditableMapObject::ValidateInstagramPage("osm."), ());
  TEST(!EditableMapObject::ValidateInstagramPage(".dots_not_allowed."), ());
  TEST(!EditableMapObject::ValidateInstagramPage("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), ());
}

UNIT_TEST(EditableMapObject_ValidateTwitterPage)
{
  TEST(EditableMapObject::ValidateTwitterPage(""), ());
  TEST(EditableMapObject::ValidateTwitterPage("twitter.com/osm_tech"), ());
  TEST(EditableMapObject::ValidateTwitterPage("www.twitter.com/osm_tech"), ());
  TEST(EditableMapObject::ValidateTwitterPage("http://twitter.com/osm_tech"), ());
  TEST(EditableMapObject::ValidateTwitterPage("https://twitter.com/osm_tech"), ());
  TEST(EditableMapObject::ValidateTwitterPage("http://www.twitter.com/osm_tech"), ());
  TEST(EditableMapObject::ValidateTwitterPage("https://www.twitter.com/osm_tech"), ());
  TEST(EditableMapObject::ValidateTwitterPage("osm_tech"), ());
  TEST(EditableMapObject::ValidateTwitterPage("_osm_tech_"), ());
  TEST(EditableMapObject::ValidateTwitterPage("1"), ());

  TEST(!EditableMapObject::ValidateTwitterPage("instagram.com/osm_tech"), ());
  TEST(!EditableMapObject::ValidateTwitterPage("https://instagram.com/osm_tech"), ());
  TEST(!EditableMapObject::ValidateTwitterPage("dots.not.allowed"), ());
  TEST(!EditableMapObject::ValidateTwitterPage("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"), ());
}

UNIT_TEST(EditableMapObject_ValidateVkPage)
{
  TEST(EditableMapObject::ValidateVkPage(""), ());
  TEST(EditableMapObject::ValidateVkPage("vk.com/id404"), ());
  TEST(EditableMapObject::ValidateVkPage("www.vk.com/id404"), ());
  TEST(EditableMapObject::ValidateVkPage("http://vk.com/id404"), ());
  TEST(EditableMapObject::ValidateVkPage("https://vk.com/id404"), ());
  TEST(EditableMapObject::ValidateVkPage("http://www.vk.com/id404"), ());
  TEST(EditableMapObject::ValidateVkPage("https://www.vk.com/id404"), ());

  TEST(!EditableMapObject::ValidateVkPage("instagram.com/hello_world"), ());
  TEST(!EditableMapObject::ValidateVkPage("https://instagram.com/hello_world"), ());
  TEST(!EditableMapObject::ValidateVkPage("usernames_not_allowed"), ());
}

UNIT_TEST(EditableMapObject_ValidateName)
{
  vector<string> correctNames = {"abc", "абв", "ᆺᆯㅕ", "꫞ꪺꫀꪸ", "a b?c", "a!b.c", "a(b)c", "a,b.c",
                                 "a$bc", "a%bc", "a#bc", "a№bc", "c&a"};
  vector<string> incorrectNames = {"a^bc", "a~bc", "a§bc", "a>bc", "a<bc", "a{bc", "a[bc", "*",
                                   "a*bc", "a=bc", "a_bc", "a±bc", "a\nbc", "a\tbc", "a\rbc",
                                   "a\vbc", "a\fbc", "a|bc", "N√", "Hello World!\U0001F600",
                                   "Exit →", "∫0dx = C", "\U0001210A"};

  for (auto const & name : correctNames)
  {
    TEST(EditableMapObject::ValidateName(name), ());
  }

  for (auto const & name : incorrectNames)
  {
    TEST(!EditableMapObject::ValidateName(name), ());
  }
}

UNIT_TEST(EditableMapObject_CanUseAsDefaultName)
{
  EditableMapObject emo;
  vector<int8_t> const nativeMwmLanguages {GetLangCode("de"), GetLangCode("fr")};

  TEST(EditableMapObject::CanUseAsDefaultName(GetLangCode("de"), nativeMwmLanguages),
       ("Check possibility to use Mwm language code"));
  TEST(EditableMapObject::CanUseAsDefaultName(GetLangCode("fr"), nativeMwmLanguages),
       ("Check possibility to use Mwm language code"));
  TEST(!EditableMapObject::CanUseAsDefaultName(GetLangCode("int_name"), nativeMwmLanguages),
       ("Check possibility to use international language code"));
  TEST(!EditableMapObject::CanUseAsDefaultName(100, nativeMwmLanguages),
       ("Incorrect language code is not supported"));
  TEST(!EditableMapObject::CanUseAsDefaultName(GetLangCode("en"), {GetLangCode("abcd")}),
       ("Incorrect Mwm language name is not supported"));
  TEST(!EditableMapObject::CanUseAsDefaultName(GetLangCode("en"), nativeMwmLanguages),
       ("Can not to use language which not Mwm language or international"));
  TEST(!EditableMapObject::CanUseAsDefaultName(GetLangCode("ru"), nativeMwmLanguages),
       ("Check possibility to use user`s language code"));

  // Trying to use language codes in reverse priority.
  StringUtf8Multilang names;
  names.AddString(GetLangCode("fr"), "second mwm language");
  emo.SetName(names);

  TEST(EditableMapObject::CanUseAsDefaultName(GetLangCode("fr"), nativeMwmLanguages),
       ("It is possible to fix typo"));

  names.AddString(GetLangCode("de"), "first mwm language");
  emo.SetName(names);

  TEST(EditableMapObject::CanUseAsDefaultName(GetLangCode("de"), nativeMwmLanguages),
       ("It is possible to fix typo"));
  TEST(EditableMapObject::CanUseAsDefaultName(GetLangCode("fr"), nativeMwmLanguages),
       ("It is possible to fix typo"));
}

UNIT_TEST(EditableMapObject_GetNamesDataSource)
{
  EditableMapObject emo;
  StringUtf8Multilang names;

  names.AddString(GetLangCode("default"), "Default name");
  names.AddString(GetLangCode("en"), "Eng name");
  names.AddString(GetLangCode("int_name"), "Int name");
  names.AddString(GetLangCode("de"), "De name");
  names.AddString(GetLangCode("ru"), "Ru name");
  names.AddString(GetLangCode("sv"), "Sv name");
  names.AddString(GetLangCode("be"), "By name");
  names.AddString(GetLangCode("ko"), "Ko name");
  names.AddString(GetLangCode("it"), "It name");
  emo.SetName(names);

  vector<int8_t> nativeMwmLanguages = {GetLangCode("de"), GetLangCode("fr")};

  auto const namesDataSource = EditableMapObject::GetNamesDataSource(
      emo.GetNameMultilang(), nativeMwmLanguages, GetLangCode("ko"));

  TEST_EQUAL(namesDataSource.names.size(), 9, ("All names except the default should be pushed into "
                                           "data source plus empty mandatory names"));
  TEST_EQUAL(namesDataSource.mandatoryNamesCount, 4,
             ("Mandatory names count should be equal to Mwm languages + user`s language"));
  TEST_EQUAL(namesDataSource.names[0].m_code, GetLangCode("de"),
             ("German must be first because it is first in the list of languages for MWM"));
  TEST_EQUAL(namesDataSource.names[1].m_code, GetLangCode("fr"),
             ("French must be second because it is second in the list of languages for MWM"));
  TEST_EQUAL(namesDataSource.names[2].m_code, GetLangCode("en"),
             ("English name should be placed after Mwm languages"));
  TEST_EQUAL(namesDataSource.names[3].m_code, GetLangCode("ko"),
             ("Korean should be fourth because the user’s langue must be followed by English."));

  {
    vector<int8_t> nativeMwmLanguages = {GetLangCode("de"), GetLangCode("fr")};

    auto const namesDataSource = EditableMapObject::GetNamesDataSource(
        emo.GetNameMultilang(), nativeMwmLanguages, GetLangCode("fr"));
    TEST_EQUAL(namesDataSource.names.size(), 9,
               ("All names + empty mandatory names should be pushed into "
                "the data source, except the default one."));
    TEST_EQUAL(namesDataSource.mandatoryNamesCount, 3,
               ("Mandatory names count should be equal to MWM languages + "
                "The English language + user`s language. Excluding repetiton"));
  }
  {
    vector<int8_t> nativeMwmLanguages = {GetLangCode("fr"), GetLangCode("en")};

    auto const namesDataSource = EditableMapObject::GetNamesDataSource(
        emo.GetNameMultilang(), nativeMwmLanguages, GetLangCode("fr"));
    TEST_EQUAL(namesDataSource.names.size(), 9,
               ("All names + empty mandatory names should be pushed into "
                "the data source, except the default one."));
    TEST_EQUAL(namesDataSource.mandatoryNamesCount, 2,
               ("Mandatory names count should be equal to MWM languages + "
                "The English language + user`s language. Excluding repetiton"));
  }
  {
    vector<int8_t> nativeMwmLanguages = {GetLangCode("en"), GetLangCode("en")};

    auto const namesDataSource = EditableMapObject::GetNamesDataSource(
        emo.GetNameMultilang(), nativeMwmLanguages, GetLangCode("en"));
    TEST_EQUAL(namesDataSource.names.size(), 8,
               ("All names + empty mandatory names should be pushed into "
                "the data source, except the default one."));
    TEST_EQUAL(namesDataSource.mandatoryNamesCount, 1,
               ("Mandatory names count should be equal to MWM languages + "
                "The English language + user`s language. Excluding repetiton"));
  }
}

UNIT_TEST(EditableMapObject_SetInternet)
{
  classificator::Load();

  EditableMapObject emo;
  auto const wifiType = classif().GetTypeByPath({"internet_access", "wlan"});
  emo.SetType(wifiType);

  auto types = emo.GetTypes();
  TEST(types.Has(wifiType), ());

  auto const setInternetAndCheck = [wifiType](EditableMapObject & emo, osm::Internet internet, bool hasWifi)
  {
    emo.SetInternet(internet);

    TEST_EQUAL(emo.GetInternet(), internet, ());
    auto const & types = emo.GetTypes();
    TEST_EQUAL(types.Has(wifiType), hasWifi, ());
  };

  setInternetAndCheck(emo, osm::Internet::No, false);
  setInternetAndCheck(emo, osm::Internet::Yes, false);
  setInternetAndCheck(emo, osm::Internet::Wired, false);
  setInternetAndCheck(emo, osm::Internet::Wlan, true);
  setInternetAndCheck(emo, osm::Internet::Unknown, false);

  EditableMapObject bunkerEmo;
  bunkerEmo.SetType(classif().GetTypeByPath({"military", "bunker"}));
  types = bunkerEmo.GetTypes();
  TEST(!types.Has(wifiType), ());

  setInternetAndCheck(bunkerEmo, osm::Internet::Wlan, true);
  setInternetAndCheck(bunkerEmo, osm::Internet::No, false);
  setInternetAndCheck(bunkerEmo, osm::Internet::Wlan, true);
  setInternetAndCheck(bunkerEmo, osm::Internet::Yes, false);
  setInternetAndCheck(bunkerEmo, osm::Internet::Wlan, true);
  setInternetAndCheck(bunkerEmo, osm::Internet::Wired, false);
  setInternetAndCheck(bunkerEmo, osm::Internet::Wlan, true);
  setInternetAndCheck(bunkerEmo, osm::Internet::Unknown, false);
  setInternetAndCheck(bunkerEmo, osm::Internet::Wlan, true);
  setInternetAndCheck(bunkerEmo, osm::Internet::Wlan, true);
}

UNIT_TEST(EditableMapObject_RemoveFakeNames)
{
  EditableMapObject emo;
  StringUtf8Multilang name;
  osm::FakeNames fakeNames;

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "Default name");
  name.AddString(GetLangCode("en"), "Default name");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Default name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Default name"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "Changed name");
  name.AddString(GetLangCode("en"), "Default name");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Default name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Default name"}, {"ru", "Changed name"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "Default name");
  name.AddString(GetLangCode("en"), "Changed name");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Default name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Default name"}, {"en", "Changed name"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "Changed name");
  name.AddString(GetLangCode("en"), "Changed name");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Default name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Changed name"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "Changed name ru");
  name.AddString(GetLangCode("en"), "Changed name en");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Default name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Changed name ru"}, {"en", "Changed name en"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Changed by other logic");
  name.AddString(GetLangCode("ru"), "Default name");
  name.AddString(GetLangCode("en"), "Changed name en");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Default name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Changed by other logic"}, {"en", "Changed name en"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Changed by other logic");
  name.AddString(GetLangCode("ru"), "Changed name ru");
  name.AddString(GetLangCode("en"), "Changed name en");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Default name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Changed name ru"}, {"en", "Changed name en"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Changed by other logic");
  name.AddString(GetLangCode("ru"), "Default name");
  name.AddString(GetLangCode("en"), "Default name");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Default name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Changed by other logic"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "");
  name.AddString(GetLangCode("en"), "Changed name en");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Default name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Changed name en"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "");
  name.AddString(GetLangCode("en"), "");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Default name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Default name"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Changed by other logic");
  name.AddString(GetLangCode("ru"), "");
  name.AddString(GetLangCode("en"), "");
  name.AddString(GetLangCode("de"), "Deutch name");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Default name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Deutch name"}, {"de", "Deutch name"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "Test name");
  name.AddString(GetLangCode("en"), "Default name");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Test name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Default name"}, {"ru", "Test name"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "Test name changed");
  name.AddString(GetLangCode("en"), "Default name changed");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Test name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Default name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Test name changed"}, {"en", "Default name changed"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "Test name");
  name.AddString(GetLangCode("en"), "Second test name changed");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Test name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Second test name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Default name"}, {"ru", "Test name"}, {"en", "Second test name changed"}});

  name.Clear();
  fakeNames.Clear();

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "");
  name.AddString(GetLangCode("en"), "Second test name changed");
  fakeNames.m_names.push_back({GetLangCode("ru"), "Test name"});
  fakeNames.m_names.push_back({GetLangCode("en"), "Second test name"});
  fakeNames.m_defaultName = "Default name";

  EditableMapObject::RemoveFakeNames(fakeNames, name);

  CheckExpectations(name, {{"default", "Second test name changed"}});
}

UNIT_TEST(EditableMapObject_RemoveBlankNames)
{
  auto const getCountOfNames = [](StringUtf8Multilang const & names) {
    size_t counter = 0;
    names.ForEach([&counter](int8_t const, string const &) { ++counter; });

    return counter;
  };

  StringUtf8Multilang name;

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "Ru name");
  name.AddString(GetLangCode("en"), "En name");
  name.AddString(GetLangCode("de"), "De name");

  EditableMapObject emo;
  emo.SetName(name);
  emo.RemoveBlankAndDuplicationsForDefault();

  TEST_EQUAL(getCountOfNames(emo.GetNameMultilang()), 4, ());

  name.Clear();

  name.AddString(GetLangCode("default"), "");
  name.AddString(GetLangCode("ru"), "Ru name");
  name.AddString(GetLangCode("en"), "En name");
  name.AddString(GetLangCode("de"), "");

  emo.SetName(name);
  emo.RemoveBlankAndDuplicationsForDefault();

  TEST_EQUAL(getCountOfNames(emo.GetNameMultilang()), 2, ());

  name.Clear();

  name.AddString(GetLangCode("default"), "Default name");
  name.AddString(GetLangCode("ru"), "");
  name.AddString(GetLangCode("en"), "");
  name.AddString(GetLangCode("de"), "");

  emo.SetName(name);
  emo.RemoveBlankAndDuplicationsForDefault();

  TEST_EQUAL(getCountOfNames(emo.GetNameMultilang()), 1, ());

  name.Clear();

  name.AddString(GetLangCode("default"), "");
  name.AddString(GetLangCode("ru"), "");
  name.AddString(GetLangCode("en"), "");
  name.AddString(GetLangCode("de"), "De name");

  emo.SetName(name);
  emo.RemoveBlankAndDuplicationsForDefault();

  TEST_EQUAL(getCountOfNames(emo.GetNameMultilang()), 1, ());
}

UNIT_TEST(EditableMapObject_FromFeatureType)
{
  classificator::Load();

  EditableMapObject emo;
  auto const wifiType = classif().GetTypeByPath({"internet_access", "wlan"});
  auto const cafeType = classif().GetTypeByPath({"amenity", "cafe"});
  feature::TypesHolder types;
  types.Add(wifiType);
  types.Add(cafeType);
  emo.SetTypes(types);

  emo.SetHouseNumber("1");

  StringUtf8Multilang names;

  names.AddString(GetLangCode("default"), "Default name");
  names.AddString(GetLangCode("ru"), "Ru name");

  emo.SetWebsite("https://some.thing.org");

  emo.SetName(names);

  emo.SetPointType();
  emo.SetMercator(m2::PointD(1.0, 1.0));

  FeatureType ft(emo);
  EditableMapObject emo2;
  emo2.SetFromFeatureType(ft);
  TEST(emo.GetTypes().Equals(emo2.GetTypes()), ());
  TEST_EQUAL(emo.GetNameMultilang(), emo2.GetNameMultilang(), ());
  TEST_EQUAL(emo.GetHouseNumber(), emo2.GetHouseNumber(), ());
  TEST_EQUAL(emo.GetMercator(), emo2.GetMercator(), ());
  TEST_EQUAL(emo.GetWebsite(), emo2.GetWebsite(), ());
  TEST(emo.IsPointType(), ());
  TEST(emo2.IsPointType(), ());
}
}  // namespace
