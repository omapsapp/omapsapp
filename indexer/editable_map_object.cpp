#include "indexer/editable_map_object.hpp"

#include "indexer/classificator.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/postcodes_matcher.hpp"
#include "indexer/validate_and_format_contacts.hpp"
#include "indexer/edit_journal.hpp"
#include "indexer/edit_journal.cpp"

#include "editor/xml_feature.hpp"

#include "platform/preferred_languages.hpp"

#include "base/control_flow.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>

namespace osm
{
using namespace std;

namespace
{
bool ExtractName(StringUtf8Multilang const & names, int8_t const langCode,
                 vector<osm::LocalizedName> & result)
{
  if (StringUtf8Multilang::kUnsupportedLanguageCode == langCode)
  {
    return false;
  }

  // Exclude languages that are already present.
  auto const it = base::FindIf(result, [langCode](osm::LocalizedName const & localizedName)
  {
    return localizedName.m_code == langCode;
  });

  if (result.end() != it)
    return false;

  string_view name;
  names.GetString(langCode, name);
  result.emplace_back(langCode, name);

  return true;
}
}  // namespace

// LocalizedName -----------------------------------------------------------------------------------

LocalizedName::LocalizedName(int8_t const code, string_view name)
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

bool EditableMapObject::IsNameEditable() const { return m_editableProperties.m_name; }
bool EditableMapObject::IsAddressEditable() const { return m_editableProperties.m_address; }

vector<MapObject::MetadataID> EditableMapObject::GetEditableProperties() const
{
  auto props = m_editableProperties.m_metadata;

  if (m_editableProperties.m_cuisine)
  {
    // Props are already sorted by Metadata::EType value.
    auto insertBefore = props.begin();
    if (insertBefore != props.end() && *insertBefore == MetadataID::FMD_OPEN_HOURS)
      ++insertBefore;
    props.insert(insertBefore, MetadataID::FMD_CUISINE);
  }

  return props;
}

NamesDataSource EditableMapObject::GetNamesDataSource()
{
  auto const mwmInfo = GetID().m_mwmId.GetInfo();

  if (!mwmInfo)
    return NamesDataSource();

  vector<int8_t> mwmLanguages;
  mwmInfo->GetRegionData().GetLanguages(mwmLanguages);

  auto const userLangCode = StringUtf8Multilang::GetLangIndex(languages::GetCurrentNorm());

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

  // Push default/native for country language.
  if (ExtractName(source, StringUtf8Multilang::kDefaultCode, names))
    ++mandatoryCount;

  // Push other languages.
  source.ForEach([&names, mandatoryCount](int8_t const code, string_view name)
  {
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

void EditableMapObject::ForEachMetadataItem(function<void(string_view tag, string_view value)> const & fn) const
{
  m_metadata.ForEach([&fn](MetadataID type, std::string_view value)
  {
    switch (type)
    {
    // Multilang description may produce several tags with different values.
    case MetadataID::FMD_DESCRIPTION:
    {
      auto const mlDescr = StringUtf8Multilang::FromBuffer(std::string(value));
      mlDescr.ForEach([&fn](int8_t code, string_view v)
      {
        if (code == StringUtf8Multilang::kDefaultCode)
          fn("description", v);
        else
          fn(string("description:").append(StringUtf8Multilang::GetLangByCode(code)), v);
      });
      break;
    }
    // Skip non-string values (they are not related to OSM anyway).
    case MetadataID::FMD_CUSTOM_IDS:
    case MetadataID::FMD_PRICE_RATES:
    case MetadataID::FMD_RATINGS:
    case MetadataID::FMD_EXTERNAL_URI:
    case MetadataID::FMD_WHEELCHAIR:    // Value is runtime only, data is taken from the classificator types, should not be used to update the OSM database
      break;
    default: fn(ToString(type), value); break;
    }
  });
}

void EditableMapObject::SetTestId(uint64_t id)
{
  m_metadata.Set(feature::Metadata::FMD_TEST_ID, std::to_string(id));
}

void EditableMapObject::MarkAsCreated(uint32_t type, feature::GeomType geomType, m2::PointD mercator)
{
  journal.MarkAsCreated(type, geomType, mercator);
}

void EditableMapObject::ClearJournal()
{
  journal.Clear();
}

EditJournal EditableMapObject::GetJournal()
{
  return journal;
}

void EditableMapObject::SetJournal(EditJournal editJournal)
{
  journal = editJournal;
}

EditingLifecycle EditableMapObject::GetEditingLifecycle()
{
  return journal.GetEditingLifecycle();
}

void EditableMapObject::SetEditableProperties(osm::EditableProperties const & props)
{
  m_editableProperties = props;
}

void EditableMapObject::SetName(StringUtf8Multilang const & name) {
  //TODO: Log in Journal
  m_name = name;
}

void EditableMapObject::SetName(string_view name, int8_t langCode)
{
  strings::Trim(name);
  std::string_view old_name;
  m_name.GetString(langCode, old_name);
  if (name != old_name) {
    std::string osmLangName = editor::XMLFeature::NameToOSMTag(langCode);
    journal.AddTagChange(osmLangName, std::string(old_name), std::string(name));
    m_name.AddString(langCode, name);
  }
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
void EditableMapObject::SetStreet(LocalizedStreet const & st)
{
  if (st.m_defaultName != m_street.m_defaultName)
  {
    journal.AddTagChange("addr:street", m_street.m_defaultName, st.m_defaultName);
    m_street = st;
  }
}

void EditableMapObject::SetNearbyStreets(vector<LocalizedStreet> && streets)
{
  m_nearbyStreets = std::move(streets);
}

void EditableMapObject::SetHouseNumber(string const & houseNumber)
{
  if (houseNumber != m_houseNumber) {
    journal.AddTagChange("addr:housenumber", m_houseNumber, houseNumber);
    m_houseNumber = houseNumber;
  }
}

void EditableMapObject::SetPostcode(std::string const & postcode)
{
  std::string old_postcode = std::string(m_metadata.Get(MetadataID::FMD_POSTCODE));
  if (postcode != old_postcode) {
    journal.AddTagChange(ToString(MetadataID::FMD_POSTCODE), old_postcode, postcode);
    m_metadata.Set(MetadataID::FMD_POSTCODE, postcode);
  }
}

bool EditableMapObject::IsValidMetadata(MetadataID type, std::string const & value)
{
  switch (type)
  {
  case MetadataID::FMD_WEBSITE: return ValidateWebsite(value);
  case MetadataID::FMD_WEBSITE_MENU: return ValidateWebsite(value);
  case MetadataID::FMD_CONTACT_FACEBOOK: return ValidateFacebookPage(value);
  case MetadataID::FMD_CONTACT_INSTAGRAM: return ValidateInstagramPage(value);
  case MetadataID::FMD_CONTACT_TWITTER: return ValidateTwitterPage(value);
  case MetadataID::FMD_CONTACT_VK: return ValidateVkPage(value);
  case MetadataID::FMD_CONTACT_LINE: return ValidateLinePage(value);

  case MetadataID::FMD_STARS:
  {
    uint32_t stars;
    return strings::to_uint(value, stars) && stars > 0 && stars <= feature::kMaxStarsCount;
  }
  case MetadataID::FMD_ELE:
  {
    /// @todo Reuse existing validadors in generator (osm2meta).
    double ele;
    return strings::to_double(value, ele) && ele > -11000 && ele < 9000;
  }

  case MetadataID::FMD_BUILDING_LEVELS: return ValidateBuildingLevels(value);
  case MetadataID::FMD_LEVEL: return ValidateLevel(value);
  case MetadataID::FMD_FLATS: return ValidateFlats(value);
  case MetadataID::FMD_POSTCODE: return ValidatePostCode(value);
  case MetadataID::FMD_PHONE_NUMBER: return ValidatePhoneList(value);
  case MetadataID::FMD_EMAIL: return ValidateEmail(value);

  default: return true;
  }
}

void EditableMapObject::SetMetadata(MetadataID type, std::string value)
{
  switch (type)
  {
  case MetadataID::FMD_WEBSITE: value = ValidateAndFormat_website(value); break;
  case MetadataID::FMD_WEBSITE_MENU: value = ValidateAndFormat_website(value); break;
  case MetadataID::FMD_CONTACT_FACEBOOK: value = ValidateAndFormat_facebook(value); break;
  case MetadataID::FMD_CONTACT_INSTAGRAM: value = ValidateAndFormat_instagram(value); break;
  case MetadataID::FMD_CONTACT_TWITTER: value = ValidateAndFormat_twitter(value); break;
  case MetadataID::FMD_CONTACT_VK: value = ValidateAndFormat_vk(value); break;
  case MetadataID::FMD_CONTACT_LINE: value = ValidateAndFormat_contactLine(value); break;
  default: break;
  }

  std::string old_value = std::string(m_metadata.Get(type));
  if (value != old_value) {
    journal.AddTagChange(ToString(type), old_value, value);
    m_metadata.Set(type, std::move(value));
  }
}

bool EditableMapObject::UpdateMetadataValue(string_view key, string value)
{
  MetadataID type;
  if (!feature::Metadata::TypeFromString(key, type))
    return false;

  SetMetadata(type, std::move(value));
  return true;
}

void EditableMapObject::SetOpeningHours(std::string oh)
{
  std::string old_oh = std::string(m_metadata.Get(MetadataID::FMD_OPEN_HOURS));
  if (oh != old_oh) {
    journal.AddTagChange(ToString(MetadataID::FMD_OPEN_HOURS), old_oh, oh);
    m_metadata.Set(MetadataID::FMD_OPEN_HOURS, std::move(oh));
  }
}

void EditableMapObject::SetInternet(feature::Internet internet)
{
  std::string old_internet = std::string(m_metadata.Get(MetadataID::FMD_INTERNET));
  std::string new_internet = DebugPrint(internet);
  if (new_internet != old_internet) {
    journal.AddTagChange(ToString(MetadataID::FMD_INTERNET), old_internet, new_internet);
    m_metadata.Set(MetadataID::FMD_INTERNET, new_internet);
  }

  uint32_t const wifiType = ftypes::IsWifiChecker::Instance().GetType();
  bool const hasWiFi = m_types.Has(wifiType);

  if (hasWiFi && internet != feature::Internet::Wlan)
    m_types.Remove(wifiType);
  else if (!hasWiFi && internet == feature::Internet::Wlan)
    m_types.Add(wifiType);
}

LocalizedStreet const & EditableMapObject::GetStreet() const { return m_street; }

template <class T>
void EditableMapObject::SetCuisinesImpl(vector<T> const & cuisines)
{
  FeatureParams params;

  // Ignore cuisine types as these will be set from the cuisines param
  auto const & isCuisine = ftypes::IsCuisineChecker::Instance();
  for (uint32_t const type : m_types)
  {
    if (!isCuisine(type))
      params.m_types.push_back(type);
  }

  Classificator const & cl = classif();
  for (auto const & cuisine : cuisines)
    params.m_types.push_back(cl.GetTypeByPath({string_view("cuisine"), cuisine}));

  // Move useless types to the end and resize to fit TypesHolder.
  params.FinishAddingTypes();

  m_types.Assign(params.m_types.begin(), params.m_types.end());
}

void EditableMapObject::SetCuisines(std::vector<std::string_view> const & cuisines)
{
  SetCuisinesImpl(cuisines);
}

void EditableMapObject::SetCuisines(std::vector<std::string> const & cuisines)
{
  std::vector<std::string> new_cuisines = cuisines;
  std::vector<std::string> old_cuisines = GetCuisines();

  auto const findAndErase = [] (std::vector<std::string>* cuisinesPtr, std::string s){
    auto it = std::find((*cuisinesPtr).begin(), (*cuisinesPtr).end(), s);
    if (it != (*cuisinesPtr).end()) {
      (*cuisinesPtr).erase(it);
      return "yes";
    }
    return "";
  };

  std::string new_vegetarian = findAndErase(&new_cuisines, "vegetarian");
  std::string old_vegetarian = findAndErase(&old_cuisines, "vegetarian");
  if (new_vegetarian != old_vegetarian)
    journal.AddTagChange("diet:vegetarian", old_vegetarian, new_vegetarian);

  std::string new_vegan = findAndErase(&new_cuisines, "vegan");
  std::string old_vegan = findAndErase(&old_cuisines, "vegan");
  if (new_vegan != old_vegan)
    journal.AddTagChange("diet:vegan", old_vegan, new_vegan);

  bool cuisinesModified = false;

  if (new_cuisines.size() != old_cuisines.size()) {
    cuisinesModified = true;
  }
  else {
    for (auto const new_cuisine : new_cuisines) {
      for (auto const old_cuisine : old_cuisines) {
        if (new_cuisine == old_cuisine) {
          goto value_found;
        }
      }
      cuisinesModified = true;
      value_found:;
    }
  }

  if (cuisinesModified) {
    journal.AddTagChange("cuisine", strings::JoinStrings(old_cuisines, ";"), strings::JoinStrings(new_cuisines, ";"));
  }

  SetCuisinesImpl(cuisines);
}

void EditableMapObject::SetPointType() { m_geomType = feature::GeomType::Point; }

void EditableMapObject::RemoveBlankNames()
{
  StringUtf8Multilang editedName;

  m_name.ForEach([&editedName](int8_t langCode, string_view name)
  {
    if (!name.empty())
      editedName.AddString(langCode, name);
  });

  m_name = editedName;
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
    string_view token = *it;
    strings::Trim(token);

    auto range = strings::Tokenize(token, "-");
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
bool EditableMapObject::ValidateEmail(string const & email)
{
  if (email.empty())
    return true;

  if (strings::IsASCIIString(email))
  {
    static auto const s_emailRegex = regex(R"([^@\s]+@[a-zA-Z0-9-]+(\.[a-zA-Z0-9-]+)+$)");
    return regex_match(email, s_emailRegex);
  }

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
  if (level.find('.') != string::npos && !level.ends_with(".5"))
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

  static std::u32string_view constexpr excludedSymbols = U"^§><*=_±√•÷×¶";

  using Iter = utf8::unchecked::iterator<string::const_iterator>;
  for (Iter it{name.cbegin()}; it != Iter{name.cend()}; ++it)
  {
    auto const ch = *it;
    // Exclude ASCII control characters.
    if (ch <= 0x1F)
      return false;
    // Exclude {|}~ DEL and C1 control characters.
    if (ch >= 0x7B && ch <= 0x9F)
      return false;
    // Exclude arrows, mathematical symbols, borders, geometric shapes.
    if (ch >= 0x2190 && ch <= 0x2BFF)
      return false;
    // Emoji modifiers https://en.wikipedia.org/wiki/Emoji#Emoji_versus_text_presentation
    if (ch == 0xFE0E || ch == 0xFE0F)
      return false;
    // Exclude format controls, musical symbols, emoticons, ornamental and pictographs,
    // ancient and exotic alphabets.
    if (ch >= 0xFFF0 && ch <= 0x1F9FF)
      return false;

    if (excludedSymbols.find(ch) != std::u32string_view::npos)
      return false;
  }
  return true;
}

bool AreObjectsEqualIgnoringStreet(EditableMapObject const & lhs, EditableMapObject const & rhs)
{
  feature::TypesHolder const & lhsTypes = lhs.GetTypes();
  feature::TypesHolder const & rhsTypes = rhs.GetTypes();

  if (!lhsTypes.Equals(rhsTypes))
    return false;

  if (lhs.GetHouseNumber() != rhs.GetHouseNumber())
    return false;

  if (lhs.GetCuisines() != rhs.GetCuisines())
    return false;

  if (!lhs.m_metadata.Equals(rhs.m_metadata))
    return false;

  if (lhs.GetNameMultilang() != rhs.GetNameMultilang())
    return false;

  return true;
}

}  // namespace osm
