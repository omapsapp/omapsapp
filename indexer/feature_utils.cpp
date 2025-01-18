#include "indexer/feature_utils.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/feature_visibility.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/scales.hpp"

#include "platform/localization.hpp"
#include "platform/distance.hpp"

#include "coding/string_utf8_multilang.hpp"
#include "coding/transliteration.hpp"

#include "base/control_flow.hpp"

#include <unordered_map>
#include <utility>

namespace feature
{
using namespace std;

namespace
{
using StrUtf8 = StringUtf8Multilang;

int8_t GetIndex(string const & lang)
{
  return StrUtf8::GetLangIndex(lang);
}

void GetMwmLangName(feature::RegionData const & regionData, StrUtf8 const & src, string_view & out)
{
  vector<int8_t> mwmLangCodes;
  regionData.GetLanguages(mwmLangCodes);

  for (auto const code : mwmLangCodes)
  {
    if (src.GetString(code, out))
      return;
  }
}

bool GetTransliteratedName(RegionData const & regionData, StrUtf8 const & src, string & out)
{
  vector<int8_t> mwmLangCodes;
  regionData.GetLanguages(mwmLangCodes);

  auto const & translator = Transliteration::Instance();

  string_view srcName;
  for (auto const code : mwmLangCodes)
  {
    if (src.GetString(code, srcName) && translator.Transliterate(srcName, code, out))
      return true;
  }

  // If default name is available, interpret it as a name for the first mwm language.
  if (!mwmLangCodes.empty() && src.GetString(StrUtf8::kDefaultCode, srcName))
    return translator.Transliterate(srcName, mwmLangCodes[0], out);

  return false;
}

bool GetBestName(StrUtf8 const & src, vector<int8_t> const & priorityList, string_view & out)
{
  size_t bestIndex = priorityList.size();

  src.ForEach([&](int8_t code, string_view name)
  {
    if (bestIndex == 0)
      return base::ControlFlow::Break;

    size_t const idx = std::distance(priorityList.begin(), find(priorityList.begin(), priorityList.end(), code));
    if (bestIndex > idx)
    {
      bestIndex = idx;
      out = name;
    }

    return base::ControlFlow::Continue;
  });

  // There are many "junk" names in Arabian island.
  if (bestIndex < priorityList.size() &&
    priorityList[bestIndex] == StrUtf8::kInternationalCode)
  {
    out = out.substr(0, out.find_first_of(','));
  }

  return bestIndex < priorityList.size();
}

vector<int8_t> GetSimilarLanguages(int8_t lang)
{
  static unordered_map<int8_t, vector<int8_t>> const kSimilarLanguages = {
    {GetIndex("be"), {GetIndex("ru")}},
    {GetIndex("ja"), {GetIndex("ja_kana"), GetIndex("ja_rm")}},
    {GetIndex("ko"), {GetIndex("ko_rm")}},
    {GetIndex("zh"), {GetIndex("zh_pinyin")}}};

  auto const it = kSimilarLanguages.find(lang);
  if (it != kSimilarLanguages.cend())
    return it->second;

  return {};
}

bool IsNativeLang(feature::RegionData const & regionData, int8_t deviceLang)
{
  if (regionData.HasLanguage(deviceLang))
    return true;

  for (auto const lang : GetSimilarLanguages(deviceLang))
  {
    if (regionData.HasLanguage(lang))
      return true;
  }

  return false;
}

vector<int8_t> MakeLanguagesPriorityList(int8_t deviceLang, bool preferDefault)
{
  vector<int8_t> langPriority = {deviceLang};
  if (preferDefault)
    langPriority.push_back(StrUtf8::kDefaultCode);

  /// @DebugNote
  // Add ru lang for descriptions/rendering tests.
  //langPriority.push_back(StrUtf8::GetLangIndex("ru"));

  auto const similarLangs = GetSimilarLanguages(deviceLang);
  langPriority.insert(langPriority.cend(), similarLangs.cbegin(), similarLangs.cend());
  langPriority.insert(langPriority.cend(), {StrUtf8::kInternationalCode, StrUtf8::kEnglishCode});

  return langPriority;
}

void GetReadableNameImpl(NameParamsIn const & in, bool preferDefault, NameParamsOut & out)
{
  auto const langPriority = MakeLanguagesPriorityList(in.deviceLang, preferDefault);

  if (GetBestName(in.src, langPriority, out.primary))
    return;

  if (in.allowTranslit && GetTransliteratedName(in.regionData, in.src, out.transliterated))
    return;

  if (!preferDefault)
  {
    if (GetBestName(in.src, {StrUtf8::kDefaultCode}, out.primary))
      return;
  }

  GetMwmLangName(in.regionData, in.src, out.primary);
}

// Filters types with |checker|, returns vector of raw type second components.
// For example for types {"cuisine-sushi", "cuisine-pizza", "cuisine-seafood"} vector
// of second components is {"sushi", "pizza", "seafood"}.
vector<string> GetRawTypeSecond(ftypes::BaseChecker const & checker, TypesHolder const & types)
{
  auto const & c = classif();
  vector<string> res;
  for (auto const t : types)
  {
    if (!checker(t))
      continue;
    auto path = c.GetFullObjectNamePath(t);
    CHECK_EQUAL(path.size(), 2, (path));
    res.push_back(std::move(path[1]));
  }
  return res;
}

vector<string> GetLocalizedTypes(ftypes::BaseChecker const & checker, TypesHolder const & types)
{
  auto const & c = classif();
  vector<string> localized;
  for (auto const t : types)
  {
    if (checker(t))
      localized.push_back(platform::GetLocalizedTypeName(c.GetReadableObjectName(t)));
  }
  return localized;
}

class FeatureEstimator
{
  template <size_t N>
  static bool IsEqual(uint32_t t, uint32_t const (&arr)[N])
  {
    for (size_t i = 0; i < N; ++i)
      if (arr[i] == t)
        return true;
    return false;
  }

  static bool InSubtree(uint32_t t, uint32_t const orig)
  {
    ftype::TruncValue(t, ftype::GetLevel(orig));
    return t == orig;
  }

public:
  FeatureEstimator()
  {
    auto const & cl = classif();

    m_TypeContinent   = cl.GetTypeByPath({"place", "continent"});
    m_TypeCountry     = cl.GetTypeByPath({"place", "country"});

    m_TypeState       = cl.GetTypeByPath({"place", "state"});
    m_TypeCounty[0]   = cl.GetTypeByPath({"place", "region"});
    m_TypeCounty[1]   = cl.GetTypeByPath({"place", "county"});

    m_TypeCity        = cl.GetTypeByPath({"place", "city"});
    m_TypeTown        = cl.GetTypeByPath({"place", "town"});

    m_TypeVillage[0]  = cl.GetTypeByPath({"place", "village"});
    m_TypeVillage[1]  = cl.GetTypeByPath({"place", "suburb"});

    m_TypeSmallVillage[0]  = cl.GetTypeByPath({"place", "hamlet"});
    m_TypeSmallVillage[1]  = cl.GetTypeByPath({"place", "locality"});
    m_TypeSmallVillage[2]  = cl.GetTypeByPath({"place", "farm"});
  }

  void CorrectScaleForVisibility(TypesHolder const & types, int & scale) const
  {
    pair<int, int> const scaleR = GetDrawableScaleRangeForRules(types, RULE_ANY_TEXT);
    ASSERT_LESS_OR_EQUAL ( scaleR.first, scaleR.second, () );

    // Result types can be without visible texts (matched by category).
    if (scaleR.first != -1)
    {
      if (scale < scaleR.first)
        scale = scaleR.first;
      else if (scale > scaleR.second)
        scale = scaleR.second;
    }
  }

  int GetViewportScale(TypesHolder const & types) const
  {
    int scale = GetDefaultScale();

    if (types.GetGeomType() == GeomType::Point)
      for (uint32_t t : types)
        scale = min(scale, GetScaleForType(t));

    CorrectScaleForVisibility(types, scale);
    return scale;
  }

private:
  static int GetDefaultScale() { return scales::GetUpperComfortScale(); }

  int GetScaleForType(uint32_t const type) const
  {
    if (type == m_TypeContinent)
      return 2;

    /// @todo Load countries bounding rects.
    if (type == m_TypeCountry)
      return 4;

    if (type == m_TypeState)
      return 6;

    if (IsEqual(type, m_TypeCounty))
      return 7;

    if (InSubtree(type, m_TypeCity))
      return 9;

    if (type == m_TypeTown)
      return 9;

    if (IsEqual(type, m_TypeVillage))
      return 12;

    if (IsEqual(type, m_TypeSmallVillage))
      return 14;

    return GetDefaultScale();
  }

  uint32_t m_TypeContinent;
  uint32_t m_TypeCountry;
  uint32_t m_TypeState;
  uint32_t m_TypeCounty[2];
  uint32_t m_TypeCity;
  uint32_t m_TypeTown;
  uint32_t m_TypeVillage[2];
  uint32_t m_TypeSmallVillage[3];
};

FeatureEstimator const & GetFeatureEstimator()
{
  static FeatureEstimator const featureEstimator;
  return featureEstimator;
}
}  // namespace

static constexpr std::string_view kStarSymbol = "★";
static constexpr std::string_view kMountainSymbol= "▲";
static constexpr std::string_view kDrinkingWaterYes = "🚰";
static constexpr std::string_view kDrinkingWaterNo = "🚱";

NameParamsIn::NameParamsIn(StringUtf8Multilang const & src_, RegionData const & regionData_,
                           std::string_view deviceLang_, bool allowTranslit_)
  : NameParamsIn(src_, regionData_, StringUtf8Multilang::GetLangIndex(deviceLang_), allowTranslit_)
{
}

bool NameParamsIn::IsNativeOrSimilarLang() const
{
  return IsNativeLang(regionData, deviceLang);
}

int GetFeatureViewportScale(TypesHolder const & types)
{
  return GetFeatureEstimator().GetViewportScale(types);
}

vector<int8_t> GetSimilar(int8_t lang)
{
  vector<int8_t> langs = {lang};

  auto const similarLangs = GetSimilarLanguages(lang);
  langs.insert(langs.cend(), similarLangs.cbegin(), similarLangs.cend());
  return langs;
}

void GetPreferredNames(NameParamsIn const & in, NameParamsOut & out)
{
  out.Clear();

  if (in.src.IsEmpty())
    return;

  // When the language of the user is equal to one of the languages of the MWM
  // (or similar languages) only single name scheme is used.
  if (in.IsNativeOrSimilarLang())
    return GetReadableNameImpl(in, true /* preferDefault */, out);

  auto const primaryCodes = MakeLanguagesPriorityList(in.deviceLang, false /* preferDefault */);

  if (!GetBestName(in.src, primaryCodes, out.primary) && in.allowTranslit)
    GetTransliteratedName(in.regionData, in.src, out.transliterated);

  vector<int8_t> secondaryCodes = {StrUtf8::kDefaultCode, StrUtf8::kInternationalCode};

  vector<int8_t> mwmLangCodes;
  in.regionData.GetLanguages(mwmLangCodes);
  secondaryCodes.insert(secondaryCodes.end(), mwmLangCodes.begin(), mwmLangCodes.end());

  secondaryCodes.push_back(StrUtf8::kEnglishCode);

  GetBestName(in.src, secondaryCodes, out.secondary);

  if (out.primary.empty())
    out.primary.swap(out.secondary);
  else if (!out.secondary.empty() && out.primary.find(out.secondary) != string::npos)
    out.secondary = {};
}

void GetReadableName(NameParamsIn const & in, NameParamsOut & out)
{
  out.Clear();

  if (!in.src.IsEmpty())
    GetReadableNameImpl(in, in.IsNativeOrSimilarLang(), out);
}

/*
int8_t GetNameForSearchOnBooking(RegionData const & regionData, StringUtf8Multilang const & src, string & name)
{
  if (src.GetString(StringUtf8Multilang::kDefaultCode, name))
    return StringUtf8Multilang::kDefaultCode;

  vector<int8_t> mwmLangs;
  regionData.GetLanguages(mwmLangs);

  for (auto mwmLang : mwmLangs)
  {
    if (src.GetString(mwmLang, name))
      return mwmLang;
  }

  if (src.GetString(StringUtf8Multilang::kEnglishCode, name))
    return StringUtf8Multilang::kEnglishCode;

  name.clear();
  return StringUtf8Multilang::kUnsupportedLanguageCode;
}
*/

bool GetPreferredName(StringUtf8Multilang const & src, int8_t deviceLang, string_view & out)
{
  auto const priorityList = MakeLanguagesPriorityList(deviceLang, true /* preferDefault */);
  return GetBestName(src, priorityList, out);
}

vector<int8_t> GetDescriptionLangPriority(RegionData const & regionData, int8_t const deviceLang)
{
  bool const preferDefault = IsNativeLang(regionData, deviceLang);
  return MakeLanguagesPriorityList(deviceLang, preferDefault);
}

vector<string> GetCuisines(TypesHolder const & types)
{
  auto const & isCuisine = ftypes::IsCuisineChecker::Instance();
  return GetRawTypeSecond(isCuisine, types);
}

vector<string> GetLocalizedCuisines(TypesHolder const & types)
{
  auto const & isCuisine = ftypes::IsCuisineChecker::Instance();
  return GetLocalizedTypes(isCuisine, types);
}

vector<string> GetRecyclingTypes(TypesHolder const & types)
{
  auto const & isRecyclingType = ftypes::IsRecyclingTypeChecker::Instance();
  return GetRawTypeSecond(isRecyclingType, types);
}

vector<string> GetLocalizedRecyclingTypes(TypesHolder const & types)
{
  auto const & isRecyclingType = ftypes::IsRecyclingTypeChecker::Instance();
  return GetLocalizedTypes(isRecyclingType, types);
}

string GetLocalizedFeeType(TypesHolder const & types)
{
  auto const & isFeeType = ftypes::IsFeeTypeChecker::Instance();
  auto localized_types = GetLocalizedTypes(isFeeType, types);
  ASSERT_LESS_OR_EQUAL ( localized_types.size(), 1, () );
  if (localized_types.empty())
    return "";
  return localized_types[0];
}

string GetReadableWheelchairType(TypesHolder const & types)
{
    auto const value = ftraits::Wheelchair::GetValue(types);
    if (!value.has_value())
      return "";

    switch (*value)
    {
      case ftraits::WheelchairAvailability::No:
        return "wheelchair-no";
      case ftraits::WheelchairAvailability::Yes:
        return "wheelchair-yes";
      case ftraits::WheelchairAvailability::Limited:
        return "wheelchair-limited";
    }
    UNREACHABLE();
}

std::optional<ftraits::WheelchairAvailability> GetWheelchairType(TypesHolder const & types)
{
  return ftraits::Wheelchair::GetValue(types);
}

bool HasAtm(TypesHolder const & types)
{
  auto const & isAtmType = ftypes::IsATMChecker::Instance();
  return isAtmType(types);
}

bool HasToilets(TypesHolder const & types)
{
  auto const & isToiletsType = ftypes::IsToiletsChecker::Instance();
  return isToiletsType(types);
}

string FormatDrinkingWater(TypesHolder const & types)
{
  auto const value = ftraits::DrinkingWater::GetValue(types);
  if (!value.has_value())
    return "";

  switch (*value)
  {
    case ftraits::DrinkingWaterAvailability::No:
      return std::string{kDrinkingWaterNo};
    case ftraits::DrinkingWaterAvailability::Yes:
      return std::string{kDrinkingWaterYes};
  }
  UNREACHABLE();
}

string FormatStars(uint8_t starsCount)
{
  std::string stars;
  for (int i = 0; i < starsCount && i < kMaxStarsCount; ++i)
    stars.append(kStarSymbol);
  return stars;
}

string FormatElevation(string_view elevation)
{
  if (!elevation.empty())
  {
    double value;
    if (strings::to_double(elevation, value))
        return std::string{kMountainSymbol} + platform::Distance::FormatAltitude(value);
    else
      LOG(LWARNING, ("Invalid elevation metadata:", elevation));
  }
  return {};
}

constexpr char const * kWlan = "wlan";
constexpr char const * kWired = "wired";
constexpr char const * kTerminal = "terminal";
constexpr char const * kYes = "yes";
constexpr char const * kNo = "no";
constexpr char const * kOnly = "only";

string DebugPrint(Internet internet)
{
  switch (internet)
  {
  case Internet::No: return kNo;
  case Internet::Yes: return kYes;
  case Internet::Wlan: return kWlan;
  case Internet::Wired: return kWired;
  case Internet::Terminal: return kTerminal;
  case Internet::Unknown: break;
  }
  return {};
}

Internet InternetFromString(std::string_view inet)
{
  if (inet.empty())
    return Internet::Unknown;
  if (inet.find(kWlan) != string::npos)
    return Internet::Wlan;
  if (inet.find(kWired) != string::npos)
    return Internet::Wired;
  if (inet.find(kTerminal) != string::npos)
    return Internet::Terminal;
  if (inet == kYes)
    return Internet::Yes;
  if (inet == kNo)
    return Internet::No;
  return Internet::Unknown;
}

YesNoUnknown YesNoUnknownFromString(std::string_view str)
{
  if (str.empty())
    return Unknown;
  if (str.find(kOnly) != string::npos)
    return Yes;
  if (str.find(kYes) != string::npos)
    return Yes;
  if (str.find(kNo) != string::npos)
    return No;
  else
    return YesNoUnknown::Unknown;
}

} // namespace feature
