#include "indexer/editable_map_object.hpp"

#include "indexer/classificator.hpp"
#include "indexer/cuisines.hpp"
#include "indexer/postcodes_matcher.hpp"

#include "platform/preferred_languages.hpp"

#include "base/control_flow.hpp"
#include "base/macros.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <codecvt>
#include <regex>
#include <sstream>

using namespace std;

namespace
{
size_t const kFakeNamesCount = 2;

bool ExtractName(StringUtf8Multilang const & names, int8_t const langCode,
                 vector<osm::LocalizedName> & result)
{
  if (StringUtf8Multilang::kUnsupportedLanguageCode == langCode ||
      StringUtf8Multilang::kDefaultCode == langCode)
  {
    return false;
  }

  // Exclude languages that are already present.
  auto const it =
      find_if(result.begin(), result.end(), [langCode](osm::LocalizedName const & localizedName) {
        return localizedName.m_code == langCode;
      });

  if (result.end() != it)
    return false;

  string name;
  names.GetString(langCode, name);
  result.emplace_back(langCode, name);

  return true;
}

size_t PushMwmLanguages(StringUtf8Multilang const & names, vector<int8_t> const & mwmLanguages,
                        vector<osm::LocalizedName> & result)
{
  size_t count = 0;
  static size_t const kMaxCountMwmLanguages = 2;

  for (size_t i = 0; i < mwmLanguages.size() && count < kMaxCountMwmLanguages; ++i)
  {
    if (ExtractName(names, mwmLanguages[i], result))
      ++count;
  }

  return count;
}

char const * const kWebsiteProtocols[] = {"http://", "https://"};
size_t const kWebsiteProtocolDefaultIndex = 0;

size_t GetProtocolNameLength(string const & website)
{
  for (auto const & protocol : kWebsiteProtocols)
  {
    if (strings::StartsWith(website, protocol))
      return strlen(protocol);
  }
  return 0;
}

bool IsProtocolSpecified(string const & website)
{
  return GetProtocolNameLength(website) > 0;
}

osm::FakeNames MakeFakeSource(StringUtf8Multilang const & source,
                              vector<int8_t> const & mwmLanguages, StringUtf8Multilang & fakeSource)
{
  string defaultName;
  // Fake names works for mono language (official) speaking countries-only.
  if (mwmLanguages.size() != 1 || !source.GetString(StringUtf8Multilang::kDefaultCode, defaultName))
  {
    return {};
  }

  osm::FakeNames fakeNames;
  // Mwm name has higher priority then English name.
  array<int8_t, kFakeNamesCount> fillCandidates = {{mwmLanguages.front(), StringUtf8Multilang::kEnglishCode}};
  fakeSource = source;

  string tempName;
  for (auto const code : fillCandidates)
  {
    if (!source.GetString(code, tempName))
    {
      tempName = defaultName;
      fakeSource.AddString(code, defaultName);
    }

    fakeNames.m_names.emplace_back(code, tempName);
  }

  fakeNames.m_defaultName = defaultName;

  return fakeNames;
}

// Tries to set default name from the localized name. Returns false when there's no such localized name.
bool TryToFillDefaultNameFromCode(int8_t const code, StringUtf8Multilang & names)
{
  string newDefaultName;
  if (code != StringUtf8Multilang::kUnsupportedLanguageCode)
    names.GetString(code, newDefaultName);

  // Default name can not be empty.
  if (!newDefaultName.empty())
  {
    names.AddString(StringUtf8Multilang::kDefaultCode, newDefaultName);
    return true;
  }

  return false;
}

// Tries to set default name to any non-empty localized name.
// This is the case when fake names were cleared.
void TryToFillDefaultNameFromAnyLanguage(StringUtf8Multilang & names)
{
  names.ForEach([&names](int8_t langCode, string const & name)
  {
    if (name.empty() || langCode == StringUtf8Multilang::kDefaultCode)
      return base::ControlFlow::Continue;

    names.AddString(StringUtf8Multilang::kDefaultCode, name);
    return base::ControlFlow::Break;
  });
}

void RemoveFakesFromName(osm::FakeNames const & fakeNames, StringUtf8Multilang & name)
{
  vector<int8_t> codesToExclude;
  string defaultName;
  name.GetString(StringUtf8Multilang::kDefaultCode, defaultName);

  for (auto const & item : fakeNames.m_names)
  {
    string tempName;
    if (!name.GetString(item.m_code, tempName))
      continue;
    // No need to save in case when name is empty, duplicate of default name or was not changed.
    if (tempName.empty() || tempName == defaultName ||
        (tempName == item.m_filledName && tempName == fakeNames.m_defaultName))
    {
      codesToExclude.push_back(item.m_code);
    }
  }

  if (codesToExclude.empty())
    return;

  StringUtf8Multilang nameWithoutFakes;
  name.ForEach([&codesToExclude, &nameWithoutFakes](int8_t langCode, string const & value)
  {
    auto const it = find(codesToExclude.begin(), codesToExclude.end(), langCode);
    if (it == codesToExclude.end())
      nameWithoutFakes.AddString(langCode, value);
  });

  name = nameWithoutFakes;
}
}  // namespace

namespace osm
{
// LocalizedName -----------------------------------------------------------------------------------

LocalizedName::LocalizedName(int8_t const code, string const & name)
  : m_code(code)
  , m_lang(StringUtf8Multilang::GetLangByCode(code))
  , m_langName(StringUtf8Multilang::GetLangNameByCode(code))
  , m_name(name)
{
}

LocalizedName::LocalizedName(string const & langCode, string const & name)
  : m_code(StringUtf8Multilang::GetLangIndex(langCode))
  , m_lang(StringUtf8Multilang::GetLangByCode(m_code))
  , m_langName(StringUtf8Multilang::GetLangNameByCode(m_code))
  , m_name(name)
{
}

// EditableMapObject -------------------------------------------------------------------------------

// static
int8_t const EditableMapObject::kMaximumLevelsEditableByUsers = 25;

bool EditableMapObject::IsNameEditable() const { return m_editableProperties.m_name; }
bool EditableMapObject::IsAddressEditable() const { return m_editableProperties.m_address; }

vector<Props> EditableMapObject::GetEditableProperties() const
{
  auto props = MetadataToProps(m_editableProperties.m_metadata);
  if (m_editableProperties.m_cuisine)
  {
    props.push_back(Props::Cuisine);
    base::SortUnique(props);
  }

  return props;
}

vector<feature::Metadata::EType> const & EditableMapObject::GetEditableFields() const
{
  return m_editableProperties.m_metadata;
}

NamesDataSource EditableMapObject::GetNamesDataSource(bool needFakes /* = true */)
{
  auto const mwmInfo = GetID().m_mwmId.GetInfo();

  if (!mwmInfo)
    return NamesDataSource();

  vector<int8_t> mwmLanguages;
  mwmInfo->GetRegionData().GetLanguages(mwmLanguages);

  auto const userLangCode = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());

  if (needFakes)
  {
    StringUtf8Multilang fakeSource;
    m_fakeNames = MakeFakeSource(m_name, mwmLanguages, fakeSource);

    if (!m_fakeNames.m_names.empty())
      return GetNamesDataSource(fakeSource, mwmLanguages, userLangCode);
  }
  else
  {
    RemoveFakeNames(m_fakeNames, m_name);
  }

  return GetNamesDataSource(m_name, mwmLanguages, userLangCode);
}

// static
NamesDataSource EditableMapObject::GetNamesDataSource(StringUtf8Multilang const & source,
                                                      vector<int8_t> const & mwmLanguages,
                                                      int8_t const userLangCode)
{
  NamesDataSource result;
  auto & names = result.names;
  auto & mandatoryCount = result.mandatoryNamesCount;
  // Push Mwm languages.
  mandatoryCount = PushMwmLanguages(source, mwmLanguages, names);

  // Push english name.
  if (ExtractName(source, StringUtf8Multilang::kEnglishCode, names))
    ++mandatoryCount;

  // Push user's language.
  if (ExtractName(source, userLangCode, names))
    ++mandatoryCount;

  // Push other languages.
  source.ForEach([&names, mandatoryCount](int8_t const code, string const & name) {
    // Exclude default name.
    if (StringUtf8Multilang::kDefaultCode == code)
      return;

    auto const mandatoryNamesEnd = names.begin() + mandatoryCount;
    // Exclude languages which are already in container (languages with top priority).
    auto const it = find_if(
        names.begin(), mandatoryNamesEnd,
        [code](LocalizedName const & localizedName) { return localizedName.m_code == code; });

    if (mandatoryNamesEnd == it)
      names.emplace_back(code, name);
  });

  return result;
}

vector<LocalizedStreet> const & EditableMapObject::GetNearbyStreets() const { return m_nearbyStreets; }

string EditableMapObject::GetPostcode() const
{
  return m_metadata.Get(feature::Metadata::FMD_POSTCODE);
}

string EditableMapObject::GetWikipedia() const
{
  return m_metadata.Get(feature::Metadata::FMD_WIKIPEDIA);
}

void EditableMapObject::ForEachMetadataItem(
    bool skipSponsored, function<void(string const & tag, string const & value)> const & fn) const
{
  for (auto const type : m_metadata.GetKeys())
  {
    if (skipSponsored && m_metadata.IsSponsoredType(type))
      continue;
    auto const attributeName = ToString(type);
    fn(attributeName, m_metadata.Get(type));
  }
}

uint64_t EditableMapObject::GetTestId() const
{
  istringstream iss(m_metadata.Get(feature::Metadata::FMD_TEST_ID));
  uint64_t id;
  iss >> id;
  return id;
}

void EditableMapObject::SetTestId(uint64_t id)
{
  ostringstream oss;
  oss << id;
  m_metadata.Set(feature::Metadata::FMD_TEST_ID, oss.str());
}

void EditableMapObject::SetEditableProperties(osm::EditableProperties const & props)
{
  m_editableProperties = props;
}

void EditableMapObject::SetName(StringUtf8Multilang const & name) { m_name = name; }

void EditableMapObject::SetName(string name, int8_t langCode)
{
  strings::Trim(name);

  if (m_namesAdvancedMode)
  {
    m_name.AddString(langCode, name);
    return;
  }

  if (!name.empty() && !m_name.HasString(StringUtf8Multilang::kDefaultCode))
  {
    const auto mwmInfo = GetID().m_mwmId.GetInfo();

    if (mwmInfo)
    {
      vector<int8_t> mwmLanguages;
      mwmInfo->GetRegionData().GetLanguages(mwmLanguages);

      if (CanUseAsDefaultName(langCode, mwmLanguages))
      {
        m_name.AddString(StringUtf8Multilang::kDefaultCode, name);
        return;
      }
    }
  }

  m_name.AddString(langCode, name);
}

// static
bool EditableMapObject::CanUseAsDefaultName(int8_t const lang, vector<int8_t> const & mwmLanguages)
{
  for (auto const & mwmLang : mwmLanguages)
  {
    if (StringUtf8Multilang::kUnsupportedLanguageCode == mwmLang)
      continue;

    if (lang == mwmLang)
      return true;
  }

  return false;
}

// static
void EditableMapObject::RemoveFakeNames(FakeNames const & fakeNames, StringUtf8Multilang & name)
{
  if (fakeNames.m_names.empty())
    return;

  int8_t newDefaultNameCode = StringUtf8Multilang::kUnsupportedLanguageCode;
  size_t changedCount = 0;
  string defaultName;
  name.GetString(StringUtf8Multilang::kDefaultCode, defaultName);

  // New default name calculation priority: 1. name on mwm language, 2. english name.
  for (auto it = fakeNames.m_names.rbegin(); it != fakeNames.m_names.rend(); ++it)
  {
    string tempName;
    if (!name.GetString(it->m_code, tempName))
      continue;

    if (tempName != it->m_filledName)
    {
      if (!tempName.empty())
        newDefaultNameCode = it->m_code;

      ++changedCount;
    }
  }

  // If all previously filled fake names were changed - try to change the default name.
  if (changedCount == fakeNames.m_names.size())
  {
    if (!TryToFillDefaultNameFromCode(newDefaultNameCode, name))
      TryToFillDefaultNameFromAnyLanguage(name);
  }

  RemoveFakesFromName(fakeNames, name);
}

void EditableMapObject::SetMercator(m2::PointD const & center) { m_mercator = center; }

void EditableMapObject::SetType(uint32_t featureType)
{
  if (m_types.GetGeomType() == feature::GeomType::Undefined)
  {
    // Support only point type for newly created features.
    m_types = feature::TypesHolder(feature::GeomType::Point);
    m_types.Assign(featureType);
  }
  else
  {
    // Correctly replace "main" type in cases when feature holds more types.
    ASSERT(!m_types.Empty(), ());
    feature::TypesHolder copy = m_types;
    // TODO(mgsergio): Replace by correct sorting from editor's config.
    copy.SortBySpec();
    m_types.Remove(*copy.begin());
    m_types.Add(featureType);
  }
}

void EditableMapObject::SetTypes(feature::TypesHolder const & types) { m_types = types; }

void EditableMapObject::SetID(FeatureID const & fid) { m_featureID = fid; }
void EditableMapObject::SetStreet(LocalizedStreet const & st) { m_street = st; }

void EditableMapObject::SetNearbyStreets(vector<LocalizedStreet> && streets)
{
  m_nearbyStreets = move(streets);
}

void EditableMapObject::SetHouseNumber(string const & houseNumber)
{
  m_houseNumber = houseNumber;
}

bool EditableMapObject::UpdateMetadataValue(string const & key, string const & value)
{
  feature::Metadata::EType mdType;
  if (!feature::Metadata::TypeFromString(key, mdType))
    return false;
  m_metadata.Set(mdType, value);
  return true;
}

void EditableMapObject::SetPostcode(string const & postcode)
{
  m_metadata.Set(feature::Metadata::FMD_POSTCODE, postcode);
}

void EditableMapObject::SetPhone(string const & phone)
{
  m_metadata.Set(feature::Metadata::FMD_PHONE_NUMBER, phone);
}

void EditableMapObject::SetFax(string const & fax)
{
  m_metadata.Set(feature::Metadata::FMD_FAX_NUMBER, fax);
}

void EditableMapObject::SetEmail(string const & email)
{
  m_metadata.Set(feature::Metadata::FMD_EMAIL, email);
}

void EditableMapObject::SetWebsite(string website)
{
  if (!website.empty() && !IsProtocolSpecified(website))
    website = kWebsiteProtocols[kWebsiteProtocolDefaultIndex] + website;

  m_metadata.Set(feature::Metadata::FMD_WEBSITE, website);
  m_metadata.Drop(feature::Metadata::FMD_URL);
}

void EditableMapObject::SetFacebookPage(string facebookPage)
{
  m_metadata.Set(feature::Metadata::FMD_FACEBOOK_PAGE, facebookPage);
}

void EditableMapObject::SetInstagramPage(string instagramPage)
{
  m_metadata.Set(feature::Metadata::FMD_INSTAGRAM_PAGE, instagramPage);
}

void EditableMapObject::SetTwitterPage(string twitterPage)
{
  m_metadata.Set(feature::Metadata::FMD_TWITTER_PAGE, twitterPage);
}

void EditableMapObject::SetVkPage(string vkPage)
{
  m_metadata.Set(feature::Metadata::FMD_VK_PAGE, vkPage);
}

void EditableMapObject::SetInternet(Internet internet)
{
  m_metadata.Set(feature::Metadata::FMD_INTERNET, DebugPrint(internet));

  static auto const wifiType = classif().GetTypeByPath({"internet_access", "wlan"});

  if (m_types.Has(wifiType) && internet != Internet::Wlan)
    m_types.Remove(wifiType);
  else if (!m_types.Has(wifiType) && internet == Internet::Wlan)
    m_types.Add(wifiType);
}

void EditableMapObject::SetStars(int stars)
{
  if (stars > 0 && stars <= 7)
    m_metadata.Set(feature::Metadata::FMD_STARS, strings::to_string(stars));
  else
    LOG(LWARNING, ("Ignored invalid value to Stars:", stars));
}

void EditableMapObject::SetOperator(string const & op)
{
  m_metadata.Set(feature::Metadata::FMD_OPERATOR, op);
}

void EditableMapObject::SetElevation(double ele)
{
  // TODO: Reuse existing validadors in generator (osm2meta).
  constexpr double kMaxElevationOnTheEarthInMeters = 10000;
  constexpr double kMinElevationOnTheEarthInMeters = -15000;
  if (ele < kMaxElevationOnTheEarthInMeters && ele > kMinElevationOnTheEarthInMeters)
    m_metadata.Set(feature::Metadata::FMD_ELE, strings::to_string_dac(ele, 1));
  else
    LOG(LWARNING, ("Ignored invalid value to Elevation:", ele));
}

void EditableMapObject::SetWikipedia(string const & wikipedia)
{
  m_metadata.Set(feature::Metadata::FMD_WIKIPEDIA, wikipedia);
}

void EditableMapObject::SetFlats(string const & flats)
{
  m_metadata.Set(feature::Metadata::FMD_FLATS, flats);
}

void EditableMapObject::SetBuildingLevels(string const & buildingLevels)
{
  m_metadata.Set(feature::Metadata::FMD_BUILDING_LEVELS, buildingLevels);
}

void EditableMapObject::SetLevel(string const & level)
{
  m_metadata.Set(feature::Metadata::FMD_LEVEL, level);
}

LocalizedStreet const & EditableMapObject::GetStreet() const { return m_street; }

void EditableMapObject::SetCuisines(vector<string> const & cuisines)
{
  FeatureParams params;
  params.m_types.insert(params.m_types.begin(), m_types.begin(), m_types.end());
  for (auto const & cuisine : cuisines)
    params.m_types.push_back(classif().GetTypeByPath({"cuisine", cuisine}));

  // Move useless types to the end and resize to fit TypesHolder.
  params.FinishAddingTypes();

  feature::TypesHolder types;
  for (auto const t : params.m_types)
    types.Add(t);

  m_types = types;
}

void EditableMapObject::SetOpeningHours(string const & openingHours)
{
  m_metadata.Set(feature::Metadata::FMD_OPEN_HOURS, openingHours);
}

void EditableMapObject::SetPointType() { m_geomType = feature::GeomType::Point; }

void EditableMapObject::RemoveBlankAndDuplicationsForDefault()
{
  StringUtf8Multilang editedName;
  string defaultName;
  m_name.GetString(StringUtf8Multilang::kDefaultCode, defaultName);

  m_name.ForEach([&defaultName, &editedName](int8_t langCode, string const & name)
  {
    auto const duplicate = langCode != StringUtf8Multilang::kDefaultCode && defaultName == name;
    if (!name.empty() && !duplicate)
      editedName.AddString(langCode, name);
  });

  m_name = editedName;
}

void EditableMapObject::RemoveNeedlessNames()
{
  if (!IsNamesAdvancedModeEnabled())
    RemoveFakeNames(m_fakeNames, m_name);

  RemoveBlankAndDuplicationsForDefault();
}

// static
bool EditableMapObject::ValidateBuildingLevels(string const & buildingLevels)
{
  if (buildingLevels.empty())
    return true;

  if (buildingLevels.size() > 18 /* max number of digits in uint_64 */)
    return false;

  if ('0' == buildingLevels.front())
    return false;

  uint64_t levels;
  return strings::to_uint64(buildingLevels, levels) && levels > 0 && levels <= kMaximumLevelsEditableByUsers;
}

// static
bool EditableMapObject::ValidateHouseNumber(string const & houseNumber)
{
  // TODO(mgsergio): Use LooksLikeHouseNumber!

  if (houseNumber.empty())
    return true;

  strings::UniString us = strings::MakeUniString(houseNumber);
  // TODO: Improve this basic limit - it was choosen by @Zverik.
  auto constexpr kMaxHouseNumberLength = 15;
  if (us.size() > kMaxHouseNumberLength)
    return false;

  // TODO: Should we allow arabic numbers like U+0661 ١	Arabic-Indic Digit One?
  strings::NormalizeDigits(us);
  for (auto const c : us)
  {
    // Valid house numbers contain at least one digit.
    if (strings::IsASCIIDigit(c))
      return true;
  }
  return false;
}

// static
bool EditableMapObject::ValidateFlats(string const & flats)
{
  for (auto it = strings::SimpleTokenizer(flats, ";"); it; ++it)
  {
    auto token = *it;
    strings::Trim(token);

    vector<string> range;
    for (auto i = strings::SimpleTokenizer(token, "-"); i; ++i)
      range.push_back(*i);
    if (range.empty() || range.size() > 2)
      return false;

    for (auto const & rangeBorder : range)
    {
      if (!all_of(begin(rangeBorder), end(rangeBorder), ::isalnum))
        return false;
    }
  }
  return true;
}

// static
bool EditableMapObject::ValidatePostCode(string const & postCode)
{
  if (postCode.empty())
    return true;
  return search::LooksLikePostcode(postCode, false /* IsPrefix */);
}

// static
bool EditableMapObject::ValidatePhoneList(string const & phone)
{
  // BNF:
  // <digit>            ::= '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'
  // <available_char>   ::= ' ' | '+' | '-' | '(' | ')'
  // <delimeter>        ::= ',' | ';'
  // <phone>            ::= (<digit> | <available_chars>)+
  // <phone_list>       ::= '' | <phone> | <phone> <delimeter> <phone_list>

  if (phone.empty())
    return true;

  auto const kMaxNumberLen = 15;
  auto const kMinNumberLen = 5;

  if (phone.size() < kMinNumberLen)
    return false;

  auto curr = phone.begin();
  auto last = phone.begin();

  do
  {
    last = find_if(curr, phone.end(), [](string::value_type const & ch)
    {
      return ch == ',' || ch == ';';
    });

    auto digitsCount = 0;
    string const symbols = "+-() ";
    for (; curr != last; ++curr)
    {
      if (!isdigit(*curr) && find(symbols.begin(), symbols.end(), *curr) == symbols.end())
        return false;

      if (isdigit(*curr))
        ++digitsCount;
    }

    if (digitsCount < kMinNumberLen || digitsCount > kMaxNumberLen)
      return false;

    curr = last;
  }
  while (last != phone.end() && ++curr != phone.end());

  return true;
}

// static
bool EditableMapObject::ValidateWebsite(string const & site)
{
  if (site.empty())
    return true;

  auto const startPos = GetProtocolNameLength(site);

  if (startPos >= site.size())
    return false;

  // Site should contain at least one dot but not at the begining/end.
  if ('.' == site[startPos] || '.' == site.back())
    return false;

  if (string::npos == site.find("."))
    return false;

  if (string::npos != site.find(".."))
    return false;

  return true;
}

//static
bool EditableMapObject::ValidateFacebookPage(string const & facebookPage)
{
  if (facebookPage.empty())
    return true;
  //check that facebookPage contains valid username. See rules: https://www.facebook.com/help/105399436216001
  if (strings::EndsWith(facebookPage, ".com") || strings::EndsWith(facebookPage, ".net"))
    return false;
  if (regex_match(facebookPage, regex(R"(^[a-zA-Z\d.]{5,}$)")))
    return true;
  if (ValidateWebsite(facebookPage))
  {
    //check domain name
    //TODO: maybe there is better way to parse URL and extract domain name
    return regex_match(facebookPage, regex(R"(^(https?:\/\/)?([^/]+\.)?facebook\.com/.+)"));
  }

  return false;
}

//static
bool EditableMapObject::ValidateInstagramPage(string const & instagramPage)
{
  if (instagramPage.empty())
    return true;

  //check that instagramPage contains valid username. Rules took here: https://blog.jstassen.com/2016/03/code-regex-for-instagram-username-and-hashtags/
  if (regex_match(instagramPage, regex(R"(^[A-Za-z0-9_][A-Za-z0-9_.]{0,28}[A-Za-z0-9_]$)")))
    return true;
  if (ValidateWebsite(instagramPage))
  {
    //check domain name
    //TODO: maybe there is better way to parse URL and extract domain name
    return regex_match(instagramPage, regex(R"(^(https?:\/\/)?([^/]+\.)?instagram\.com/.+)"));
  }

  return false;
}

//static
bool EditableMapObject::ValidateTwitterPage(string const & twitterPage)
{
  if (twitterPage.empty())
    return true;
  if (ValidateWebsite(twitterPage))
  {
    //check domain name
    //TODO: maybe there is better way to parse URL and extract domain name
    return regex_match(twitterPage, regex(R"(^(https?:\/\/)?([^/]+\.)?twitter\.com/.+)"));
  }
  else
  {
    //check that twitterPage contains valid username. Rules took here: https://stackoverflow.com/q/11361044
    return regex_match(twitterPage, regex(R"(^[A-Za-z0-9_]{1,15}$)"));
  }
}

//static
bool EditableMapObject::ValidateVkPage(string const & vkPage)
{
  if (vkPage.empty())
    return true;
  if (ValidateWebsite(vkPage))
  {
    //check domain name
    //TODO: maybe there is better way to parse URL and extract domain name
    return regex_match(vkPage, regex(R"(^(https?:\/\/)?([^/]+\.)?vk\.com/.+)"));
  }
  else
    return false;
}

// static
bool EditableMapObject::ValidateEmail(string const & email)
{
  if (email.empty())
    return true;

  if (strings::IsASCIIString(email))
    return regex_match(email, regex(R"([^@\s]+@[a-zA-Z0-9-]+(\.[a-zA-Z0-9-]+)+$)"));

  if ('@' == email.front() || '@' == email.back())
    return false;

  if ('.' == email.back())
    return false;

  auto const atPos = find(begin(email), end(email), '@');
  if (atPos == end(email))
    return false;

  // There should be only one '@' sign.
  if (find(next(atPos), end(email), '@') != end(email))
    return false;

  // There should be at least one '.' sign after '@'
  if (find(next(atPos), end(email), '.') == end(email))
    return false;

  return true;
}

// static
bool EditableMapObject::ValidateLevel(string const & level)
{
  if (level.empty())
    return true;

  if (level.size() > 4 /* 10.5, for example */)
    return false;

  // Allowing only half-levels.
  if (level.find('.') != string::npos && !strings::EndsWith(level, ".5"))
    return false;

  // Forbid "04" and "0.".
  if ('0' == level.front() && level.size() == 2)
    return false;

  auto constexpr kMinBuildingLevel = -9;
  double result;
  return strings::to_double(level, result) && result > kMinBuildingLevel &&
         result <= kMaximumLevelsEditableByUsers;
}

// static
bool EditableMapObject::ValidateName(string const & name)
{
  if (name.empty())
    return true;

  if (strings::IsASCIIString(name))
    return regex_match(name, regex(R"(^[ A-Za-z0-9.,?!@#$%&()\-\+:;"'`]+$)"));

  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;

  std::u32string u32name;
  try
  {
    u32name = converter.from_bytes(name);
  }
  catch (std::range_error const &)
  {
    // Cannot convert, for ex. it is possible for some emoji.
    return false;
  }

  std::u32string const excludedSymbols = U"^~§><{}[]*=_±\n\t\r\v\f|√•÷×¶°";

  for (auto const ch : u32name)
  {
    // Exclude arrows, mathematical symbols, borders, geometric shapes.
    if (ch >= U'\U00002190' && ch <= U'\U00002BFF')
      return false;
    // Exclude format controls, musical symbols, emoticons, ornamental and pictographs,
    // ancient and exotic alphabets.
    if (ch >= U'\U0000FFF0' && ch <= U'\U0001F9FF')
      return false;

    if (find(excludedSymbols.cbegin(), excludedSymbols.cend(), ch) != excludedSymbols.cend())
      return false;
  }
  return true;
}
}  // namespace osm
