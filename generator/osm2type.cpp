#include "generator/osm2type.hpp"

#include "generator/osm2meta.hpp"
#include "generator/osm_element.hpp"
#include "generator/osm_element_helpers.hpp"
#include "generator/utils.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/country_tree_helpers.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_impl.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "platform/platform.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include "defines.hpp"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace ftype
{
using std::string;

namespace
{
template <typename ToDo>
void ForEachTag(OsmElement * p, ToDo && toDo)
{
  for (auto & e : p->m_tags)
    toDo(std::move(e.m_key), std::move(e.m_value));
}

class NamesExtractor
{
public:
  enum class LangAction
  {
    Forbid,
    Accept,
    Append,
    Replace
  };

  explicit NamesExtractor(FeatureBuilderParams & params) : m_params(params) {}

  static LangAction GetLangByKey(string const & k, string & lang)
  {
    strings::SimpleTokenizer token(k, "\t :");
    if (!token)
      return LangAction::Forbid;

    // Is this an international (latin) / old / alternative name.
    if (*token == "int_name" || *token == "old_name" || *token == "alt_name")
    {
      lang = *token;

      // Consider only pure int/alt/old name without :lang. Otherwise, feature with several
      // alt_name:lang will receive random name based on tags enumeration order.
      // For old_name we support old_name:date.
      if (++token)
      {
        if (lang != "old_name")
          return LangAction::Forbid;

        if (base::AllOf(*token, [](auto const c) { return isdigit(c) || c == '-'; }))
          return LangAction::Append;

        return LangAction::Forbid;
      }

      return lang == "old_name" ? LangAction::Append : LangAction::Accept;
    }

    if (*token != "name")
      return LangAction::Forbid;

    ++token;
    lang = (token ? *token : "default");

    // Do not consider languages with suffixes, like "en:pronunciation".
    if (++token)
      return LangAction::Forbid;

    // Replace dummy arabian tag with correct tag.
    if (lang == "ar1")
      lang = "ar";

    if (lang == "ja-Hira")
    {
      // Save "ja-Hira" if there is no "ja_kana".
      lang = "ja_kana";
    }
    else if (lang == "ja_kana")
    {
      // Prefer "ja_kana" over "ja-Hira".
      return LangAction::Replace;
    }

    return LangAction::Accept;
  }

  void operator()(string && k, string && v)
  {
    if (v.empty())
      return;

    string lang;
    switch (GetLangByKey(k, lang))
    {
    case LangAction::Forbid: return;
    case LangAction::Accept: m_names.emplace(std::move(lang), std::move(v)); break;
    case LangAction::Replace: swap(m_names[lang], v); break;
    case LangAction::Append:
      auto & name = m_names[lang];
      if (name.empty())
        swap(name, v);
      else
        name = name + ";" + v;
      break;
    }
    k.clear();
    v.clear();
  }

  void Finish()
  {
    for (auto const & kv : m_names)
      m_params.AddName(kv.first, kv.second);
    m_names.clear();
  }

private:
  std::map<string, string> m_names;
  FeatureBuilderParams & m_params;
};

class TagProcessor
{
public:
  explicit TagProcessor(OsmElement * elem) : m_element(elem) {}

  template <typename Function>
  struct Rule
  {
    char const * m_key;
    // Wildcard values:
    // * - take any values
    // ! - take only negative values
    // !r  - take only negative values for Routing
    // ~ - take only positive values
    // ~r - take only positive values for Routing
    // Note that the matching logic here is different from the one used in classificator matching,
    // see ParseMapCSS() and Matches() in generator/utils.cpp.
    char const * m_value;
    std::function<Function> m_func;
  };

  template <typename Function = void()>
  void ApplyRules(std::initializer_list<Rule<Function>> const & rules) const
  {
    for (auto & e : m_element->m_tags)
    {
      for (auto const & rule : rules)
      {
        if (e.m_key != rule.m_key)
          continue;

        bool take = false;
        if (rule.m_value[0] == '*')
          take = true;
        else if (strncmp(rule.m_value, "!r", 2) == 0)
          take = IsNegativeRouting(e.m_value);
        else if (strncmp(rule.m_value, "~r", 2) == 0)
          take = IsPositiveRouting(e.m_value);
        else if (rule.m_value[0] == '!')
          take = IsNegative(e.m_value);
        else if (rule.m_value[0] == '~')
          take = !IsNegative(e.m_value);

        if (take || e.m_value == rule.m_value)
          Call(rule.m_func, e.m_key, e.m_value);
      }
    }
  }

protected:
  static void Call(std::function<void()> const & f, string &, string &) { f(); }
  static void Call(std::function<void(string &, string &)> const & f, string & k, string & v)
  {
    f(k, v);
    k.clear();
    v.clear();
  }

private:
  static bool IsNegative(string const & value)
  {
    for (char const * s : {"no", "none", "false"})
    {
      if (value == s)
        return true;
    }
    return false;
  }
  static bool IsNegativeRouting(string const & value)
  {
    for (char const * s : {"use_sidepath", "separate"})
    {
      if (value == s)
        return true;
    }
    return IsNegative(value);
  }
  static bool IsPositiveRouting(string const & value)
  {
    // This values neither positive and neither negative.
    for (char const * s : {"unknown", "dismount"})
    {
      if (value == s)
        return false;
    }
    return !IsNegativeRouting(value);
  }

  OsmElement * m_element;
};

class CachedTypes
{
public:
  enum Type
  {
    Entrance,
    Highway,
    Address,
    OneWay,
    Private,
    Lit,
    NoFoot,
    YesFoot,
    NoSidewalk,   // no dedicated sidewalk, doesn't mean that foot is not allowed, just lower weight
    NoBicycle,
    YesBicycle,
    NoCycleway,   // no dedicated cycleway, doesn't mean that bicycle is not allowed, just lower weight
    BicycleBidir,
    SurfacePavedGood,
    SurfacePavedBad,
    SurfaceUnpavedGood,
    SurfaceUnpavedBad,
    HasParts,
    NoCar,
    YesCar,
    InternetAny,
    Wlan,
    RailwayStation,
    SubwayStation,
    WheelchairAny,
    WheelchairYes,
    BarrierGate,
    Toll,
    BicycleOnedir,
    Ferry,
    ShuttleTrain,
    Count
  };

  CachedTypes()
  {
    Classificator const & c = classif();

    static std::map<Type, std::vector<string>> const kTypeToName = {
        {Entrance,           {"entrance"}},
        {Highway,            {"highway"}},
        {Address,            {"building", "address"}},
        {OneWay,             {"hwtag", "oneway"}},
        {Private,            {"hwtag", "private"}},
        {Lit,                {"hwtag", "lit"}},
        {NoFoot,             {"hwtag", "nofoot"}},
        {YesFoot,            {"hwtag", "yesfoot"}},
        {NoSidewalk,         {"hwtag", "nosidewalk"}},
        {NoBicycle,          {"hwtag", "nobicycle"}},
        {YesBicycle,         {"hwtag", "yesbicycle"}},
        {NoCycleway,         {"hwtag", "nocycleway"}},
        {BicycleBidir,       {"hwtag", "bidir_bicycle"}},
        {SurfacePavedGood,   {"psurface", "paved_good"}},
        {SurfacePavedBad,    {"psurface", "paved_bad"}},
        {SurfaceUnpavedGood, {"psurface", "unpaved_good"}},
        {SurfaceUnpavedBad,  {"psurface", "unpaved_bad"}},
        {HasParts,           {"building", "has_parts"}},
        {NoCar,              {"hwtag", "nocar"}},
        {YesCar,             {"hwtag", "yescar"}},
        {InternetAny,        {"internet_access"}},
        {Wlan,               {"internet_access", "wlan"}},
        {RailwayStation,     {"railway", "station"}},
        {SubwayStation,      {"railway", "station", "subway"}},
        {WheelchairAny,      {"wheelchair"}},
        {WheelchairYes,      {"wheelchair", "yes"}},
        {BarrierGate,        {"barrier", "gate"}},
        {Toll,               {"hwtag", "toll"}},
        {BicycleOnedir,      {"hwtag", "onedir_bicycle"}},
        {Ferry,              {"route", "ferry"}},
        {ShuttleTrain,       {"route", "shuttle_train"}},
    };

    m_types.resize(static_cast<size_t>(Count));
    for (auto const & kv : kTypeToName)
      m_types[static_cast<size_t>(kv.first)] = c.GetTypeByPath(kv.second);
  }

  uint32_t Get(CachedTypes::Type t) const
  {
    return m_types[static_cast<size_t>(t)];
  }

  bool IsHighway(uint32_t t) const
  {
    ftype::TruncValue(t, 1);
    return t == Get(Highway);
  }

  bool IsTransporter(uint32_t t) const
  {
    // Ferry and shuttle have the same processing logic now.
    return t == Get(Ferry) || t == Get(ShuttleTrain);
  }

  bool IsRailwayStation(uint32_t t) const
  {
    return t == Get(RailwayStation);
  }

  bool IsSubwayStation(uint32_t t) const
  {
    ftype::TruncValue(t, 3);
    return t == Get(SubwayStation);
  }

private:
  buffer_vector<uint32_t, static_cast<size_t>(Count)> m_types;
};

// If first 2 (1 for short types like "building") components of pre-matched types are the same,
// then leave only the longest types (there could be a few of them). Equal arity types are kept.
// - highway-primary-bridge is left while highway-primary is removed;
// - building-garages is left while building is removed;
// - amenity-parking-underground-fee is left while amenity-parking and amenity-parking-fee is removed;
// - both amenity-charging_station-motorcar and amenity-charging_station-bicycle are left;
void LeaveLongestTypes(std::vector<generator::TypeStrings> & matchedTypes)
{
  auto const equalPrefix = [](auto const & lhs, auto const & rhs)
  {
    size_t const prefixSz = std::min(lhs.size(), rhs.size());
    return equal(lhs.begin(), lhs.begin() + std::min(size_t(2), prefixSz), rhs.begin());
  };

  auto const isBetter = [&equalPrefix](auto const & lhs, auto const & rhs)
  {
    if (equalPrefix(lhs, rhs))
    {
      // Longest type is better.
      if (lhs.size() != rhs.size())
        return lhs.size() > rhs.size();
    }

    return lhs < rhs;
  };

  auto const isEqual = [&equalPrefix](auto const & lhs, auto const & rhs)
  {
    if (equalPrefix(lhs, rhs))
    {
      // Keep longest type only, so return equal is true.
      if (lhs.size() != rhs.size())
        return true;

      return lhs == rhs;
    }
    return false;
  };

  base::SortUnique(matchedTypes, isBetter, isEqual);
}

void MatchTypes(OsmElement * p, FeatureBuilderParams & params, TypesFilterFnT const & filterType)
{
  auto static const rules = generator::ParseMapCSS(GetPlatform().GetReader(MAPCSS_MAPPING_FILE));

  std::vector<generator::TypeStrings> matchedTypes;
  for (auto const & [typeString, rule] : rules)
  {
    if (rule.Matches(p->m_tags))
      matchedTypes.push_back(typeString);
  }

  LeaveLongestTypes(matchedTypes);

  auto const & cl = classif();

  bool buswayAdded = false;
  for (size_t i = 0; i < matchedTypes.size(); ++i)
  {
    auto const & path = matchedTypes[i];
    uint32_t const type = cl.GetTypeByPath(path);
    if (!filterType(type))
      continue;

    // "busway" and "service" can be matched together, keep busway only.
    if (path[0] == "highway")
    {
      // busway goes before service, see LeaveLongestTypes.isBetter
      if (path[1] == "busway")
        buswayAdded = true;
      else if (buswayAdded && path[1] == "service")
        continue;
    }

    params.AddType(type);
  }
}

string MatchCity(ms::LatLon const & ll)
{
  // needs to be in format {minLon, minLat, maxLon, maxLat}
  // Draw boundary around metro with http://bboxfinder.com (set to Lon/Lat)
  // City name should be equal with railway-station-subway-CITY classifier types.
  static std::map<string, m2::RectD> const cities = {
      {"adana", {35.216442,36.934693,35.425525,37.065481}},
      {"algiers", {2.949538, 36.676777, 3.256914, 36.826518}},
      {"almaty", {76.7223358154, 43.1480920701, 77.123336792, 43.4299852362}},
      {"amsterdam", {4.65682983398, 52.232846171, 5.10040283203, 52.4886341706}},
      {"ankara", {32.4733, 39.723, 33.0499, 40.086}},
      {"athens", {23.518075, 37.849188, 23.993853, 38.129109}},
      {"baku", {49.7315,40.319728,49.978006,40.453619}},
      {"bangkok", {100.159606934, 13.4363737155, 100.909423828, 14.3069694978}},
      {"barcelona", {1.94458007812, 41.2489025224, 2.29614257812, 41.5414776668}},
      {"beijing", {115.894775391, 39.588757277, 117.026367187, 40.2795256688}},
      {"berlin", {13.061007, 52.290099, 13.91399, 52.760803}},
      {"boston", {-71.2676239014, 42.2117365893, -70.8879089355, 42.521711682}},
      {"bengalore", {77.393079,12.807501,77.806439,13.17014}},
      {"bilbao", {-3.129730,43.202673,-2.859879,43.420011}},
      {"brasilia", {-48.334467, -16.036627, -47.358264, -15.50321}},
      {"brescia", {10.128068,45.478792,10.312432,45.595665}},
      {"brussels", {4.2448425293, 50.761653413, 4.52499389648, 50.9497757762}},
      {"bucharest", {25.905198, 44.304636, 26.29032, 44.588137}},
      {"budapest", {18.7509155273, 47.3034470439, 19.423828125, 47.7023684666}},
      {"buenos_aires", {-58.9910888672, -35.1221551064, -57.8045654297, -34.2685661867}},
      {"bursa", {28.771425,40.151234,29.297395,40.315832}},
      {"cairo", {30.3232, 29.6163, 31.9891, 30.6445}},
      {"caracas", {-67.109144,10.317298,-66.754835,10.551623}},
      {"catania", {14.94978,37.443756,15.126343,37.540221}},
      {"changchun", {124.6144,43.6274,125.9163,44.1578}},
      {"chengdu", {103.6074,30.3755,104.4863,31.0462}},
      {"chicago", {-88.3163452148, 41.3541338721, -87.1270751953, 42.2691794924}},
      {"chongqing", {105.9873,29.1071,107.0255,30.0102}},
      {"dalian", {121.284679,38.752352,121.90678,39.167744}},
      {"delhi", {76.8026733398, 28.3914003758, 77.5511169434, 28.9240352884}},
      {"dnepro", {34.7937011719, 48.339820521, 35.2798461914, 48.6056737841}},
      {"dubai", {55.01953125, 24.9337667594, 55.637512207, 25.6068559937}},
      {"ekb", {60.3588867188, 56.6622647682, 61.0180664062, 57.0287738515}},
      {"frankfurt", {8.36334228516, 49.937079757, 8.92364501953, 50.2296379179}},
      {"fukuoka", {130.285484,33.544873,130.51757,33.70268}},
      {"glasgow", {-4.542417,55.753178,-3.943662,55.982611}},
      {"guangzhou", {112.560424805, 22.4313401564, 113.766174316, 23.5967112789}},
      {"hamburg", {9.75860595703, 53.39151869, 10.2584838867, 53.6820686709}},
      {"helsinki", {24.3237304688, 59.9989861206, 25.48828125, 60.44638186}},
      {"hiroshima", {132.256736,34.312824,132.621345,34.55182}},
      {"hongkong", {113.934086,22.150767,114.424924,22.515215}},
      {"isfahan", {51.472344,32.504644,51.865369,32.820396}},
      {"istanbul", {28.4155, 40.7172, 29.7304, 41.4335}},
      {"izmir", {26.953591,38.262044,27.318199,38.543469}},
      {"kazan", {48.8067626953, 55.6372985742, 49.39453125, 55.9153515154}},
      {"kharkiv", {36.078138, 49.854027, 36.51107, 50.141277}},
      {"kiev", {30.1354980469, 50.2050332649, 31.025390625, 50.6599083609}},
      {"kobe", {135.066888,34.617728,135.32232,34.776364}},
      {"kolkata", {88.240623,22.450324,88.458955,22.632536}},
      {"koln", {6.7943572998, 50.8445380881, 7.12669372559, 51.0810964366}},
      {"kunming", {102.0983,24.3319,103.5969,25.7119}},
      {"kyoto", {135.619598, 34.874916, 135.878442, 35.113709}},
      {"lausanne", {6.583868,46.504301,6.720813,46.602578}},
      {"lille", {2.789132,50.441626,3.329113,50.794609}},
      {"lima", {-77.2750854492, -12.3279274859, -76.7999267578, -11.7988014362}},
      {"lisboa", {-9.42626953125, 38.548165423, -8.876953125, 38.9166815364}},
      {"london", {-0.4833984375, 51.3031452592, 0.2197265625, 51.6929902115}},
      {"la", {-118.944112, 32.806553, -117.644787, 34.822766}},
      {"lyon", {4.5741, 45.5842, 5.1603, 45.9393}},
      {"madrid", {-4.00451660156, 40.1536868578, -3.32885742188, 40.6222917831}},
      {"malaga", {-5.611777,36.310352,-3.765967,37.282445}},
      {"manila", {120.936229,14.550825,121.026167,14.639547}},
      {"maracaibo", {-71.812942,10.570632,-71.581199,10.758897}},
      {"mashhad", {59.302159,36.13267,59.83225,36.530945}},
      {"mecca", {39.663307, 21.274985, 40.056236, 21.564195}},
      {"medellin", {-75.719423,6.162617,-75.473408,6.376421}},
      {"mexico", {-99.3630981445, 19.2541083164, -98.879699707, 19.5960192403}},
      {"milan", {9.02252197266, 45.341528405, 9.35760498047, 45.5813674681}},
      {"minsk", {27.2845458984, 53.777934972, 27.8393554688, 54.0271334441}},
      {"montreal", {-73.995802, 45.398482, -73.474295, 45.70479}},
      {"moscow", {36.9964599609, 55.3962717136, 38.1884765625, 56.1118730004}},
      {"mumbai", {72.7514648437, 18.8803004445, 72.9862976074, 19.2878132403}},
      {"munchen", {11.3433837891, 47.9981928195, 11.7965698242, 48.2730267576}},
      {"nagoya", {136.791969,35.025951,137.060899,35.260229}},
      {"newyork", {-74.4104003906, 40.4134960497, -73.4600830078, 41.1869224229}},
      {"nnov", {43.6431884766, 56.1608472541, 44.208984375, 56.4245355509}},
      {"novosibirsk", {82.4578857422, 54.8513152597, 83.2983398438, 55.2540770671}},
      {"osaka", {134.813232422, 34.1981730963, 136.076660156, 35.119908571}},
      {"oslo", {10.3875732422, 59.7812868211, 10.9286499023, 60.0401604652}},
      {"palma", {2.556669,39.503227,2.841284,39.670445}},
      {"panama", {-79.633827, 8.880788, -79.367367, 9.149179}},
      {"paris", {2.09014892578, 48.6637569323, 2.70538330078, 49.0414689141}},
      {"philadelphia", {-75.276761, 39.865446, -74.964493, 40.137768}},
      {"porto", {-8.758979,41.095783,-8.540001,41.378495}},
      {"pyongyang", {125.48888, 38.780932, 126.12748, 39.298738}},
      {"rennes", {-2.28897,47.934093,-1.283944,48.379636}},
      {"rio", {-43.4873199463, -23.0348745407, -43.1405639648, -22.7134898498}},
      {"roma", {12.3348999023, 41.7672146942, 12.6397705078, 42.0105298189}},
      {"rotterdam", {3.940749, 51.842118, 4.601808, 52.004528}},
      {"samara", {50.001145, 53.070867, 50.434992, 53.339216}},
      {"santiago", {-71.015625, -33.8133843291, -70.3372192383, -33.1789392606}},
      {"santo_domingo", {-70.029669,18.390645,-69.831571,18.573966}},
      {"saopaulo", {-46.9418334961, -23.8356009866, -46.2963867187, -23.3422558351}},
      {"sapporo", {141.160343,42.945651,141.577136,43.243986}},
      {"sendai", {140.469472,38.050849,141.260304,38.454699}},
      {"seoul", {126.540527344, 37.3352243593, 127.23815918, 37.6838203267}},
      {"sf", {-122.72277832, 37.1690715771, -121.651611328, 38.0307856938}},
      {"shanghai", {119.849853516, 30.5291450367, 122.102050781, 32.1523618947}},
      {"shenzhen", {113.747866,22.464779,114.477038,22.816068}},
      {"shiraz", {52.382254,29.498738,52.667513,29.840346}},
      {"singapore", {103.624420166, 1.21389843409, 104.019927979, 1.45278619819}},
      {"sofia", {23.195085, 42.574041, 23.503569, 42.835375}},
      {"spb", {29.70703125, 59.5231755354, 31.3110351562, 60.2725145948}},
      {"stockholm", {17.5726318359, 59.1336814082, 18.3966064453, 59.5565918857}},
      {"stuttgart", {9.0877532959, 48.7471343254, 9.29306030273, 48.8755544436}},
      {"sydney", {150.42755127, -34.3615762875, 151.424560547, -33.4543597895}},
      {"tabriz", {46.18432,38.015584,46.4126,38.15366}},
      {"taipei", {121.368713379, 24.9312761454, 121.716156006, 25.1608229799}},
      {"taoyuan", {110.8471,28.4085,111.6109,29.4019}},
      {"tashkent", {69.12171, 41.163421, 69.476967, 41.398638}},
      {"tbilisi", {44.596922, 41.619315, 45.019694, 41.843421}},
      {"tehran", {50.6575, 35.353216, 52.007904, 35.974672}},
      {"tianjin", {116.7022, 38.555, 118.0587, 40.252}},
      {"tokyo", {139.240722656, 35.2186974963, 140.498657227, 36.2575628263}},
      {"valencia", {-0.432551, 39.27845, -0.272521, 39.566609}},
      {"vienna", {16.0894775391, 48.0633965378, 16.6387939453, 48.3525987075}},
      {"warszawa", {20.7202148438, 52.0322181041, 21.3024902344, 52.4091212523}},
      {"washington", {-77.4920654297, 38.5954071994, -76.6735839844, 39.2216149801}},
      {"wuhan", {113.6925,29.972,115.0769,31.3622}},
      {"yerevan", {44.359899, 40.065411, 44.645352, 40.26398}},
      {"yokohama", {139.464781, 35.312501, 139.776935, 35.592738}},
  };

  m2::PointD const pt(ll.m_lon, ll.m_lat);
  for (auto const & city : cities)
  {
    if (city.second.IsPointInside(pt))
      return city.first;
  }
  return {};
}

string DetermineSurfaceAndHighwayType(OsmElement * p)
{
  string surface;
  string smoothness;
  double surfaceGrade = 2; // default is "normal"
  string highway;
  string trackGrade;

  for (auto const & tag : p->m_tags)
  {
    if (tag.m_key == "surface")
      surface = tag.m_value;
    else if (tag.m_key == "smoothness")
      smoothness = tag.m_value;
    else if (tag.m_key == "surface:grade") // discouraged, 25k usages as of 2024
      (void)strings::to_double(tag.m_value, surfaceGrade);
    else if (tag.m_key == "tracktype")
      trackGrade = tag.m_value;
    else if (tag.m_key == "highway" && tag.m_value != "ford")
      highway = tag.m_value;
    else if (tag.m_key == "4wd_only" && (tag.m_value == "yes" || tag.m_value == "recommended"))
      return "unpaved_bad";
  }

  // According to https://wiki.openstreetmap.org/wiki/Key:surface
  static base::StringIL pavedSurfaces = {
      "asphalt", "cobblestone", "chipseal", "concrete", "grass_paver", "stone",
      "metal", "paved", "paving_stones", "sett", "brick", "bricks", "unhewn_cobblestone", "wood"
  };

  // All not explicitly listed surface types are considered unpaved good, e.g. "compacted", "fine_gravel".
  static base::StringIL badSurfaces = {
      "cobblestone", "dirt", "earth", "soil", "grass", "gravel", "ground", "metal", "mud", "rock", "stone", "unpaved",
      "pebblestone", "sand", "sett", "brick", "bricks", "snow", "stepping_stones", "unhewn_cobblestone",
      "grass_paver", "wood", "woodchips"
  };

  static base::StringIL veryBadSurfaces = {
      "dirt", "earth", "soil", "grass", "ground", "mud", "rock", "sand", "snow",
      "stepping_stones", "woodchips"
  };

  // surface=tartan/artificial_turf/clay are not used for highways (but for sport pitches etc).

  static base::StringIL veryBadSmoothness = {
      "very_bad",       "horrible",        "very_horrible", "impassable",
      "robust_wheels", "high_clearance", "off_road_wheels", "rough"
  };

  static base::StringIL midSmoothness = {
      "unknown", "intermediate"
  };

  auto const Has = [](base::StringIL const & il, string const & v)
  {
    bool res = false;
    // Also matches compound values like concrete:plates, sand/dirt, etc. if a single part matches.
    strings::Tokenize(v, ";:/", [&il, &res](std::string_view sv)
    {
      if (!res)
        res = base::IsExist(il, sv);
    });
    return res;
  };

  /* Convert between highway=path/footway/cycleway depending on surface and other tags.
   * The goal is to end up with following clear types:
   *   footway - for paved/formed urban looking pedestrian paths
   *   footway + yesbicycle - same but shared with cyclists
   *   path - for unpaved paths and trails
   *   path + yesbicycle - same but explicitly shared with cyclists
   *   cycleway - dedicated for cyclists (segregated from pedestrians)
   * I.e. segregated shared paths should have both footway and cycleway types.
   */
  string const kCycleway = "cycleway";
  string const kFootway = "footway";
  string const kPath = "path";
  if (highway == kFootway || highway == kPath || highway == kCycleway)
  {
    static base::StringIL goodPathSmoothness = {
        "excellent", "good", "very_good", "intermediate"
    };
    bool const hasQuality = !smoothness.empty() || !trackGrade.empty();
    bool const isGood = (smoothness.empty() || Has(goodPathSmoothness, smoothness)) &&
                        (trackGrade.empty() || trackGrade == "grade1" || trackGrade == "grade2");
    bool const isMed = (smoothness == "intermediate" || trackGrade == "grade2");
    bool const hasTrailTags = p->HasTag("sac_scale") || p->HasTag("trail_visibility") ||
                              p->HasTag("ford") || p->HasTag("informal", "yes");
    bool const hasUrbanTags = p->HasTag("footway") || p->HasTag("segregated") ||
                              (p->HasTag("lit") && !p->HasTag("lit", "no"));

    bool isFormed = !surface.empty() && Has(pavedSurfaces, surface);
    // Treat "compacted" and "fine_gravel" as formed when in good or default quality.
    if ((surface == "compacted" || surface == "fine_gravel") && isGood)
      isFormed = true;
    // Treat pebble/gravel surfaces as formed only when it has urban tags or a certain good quality and no trail tags.
    if ((surface == "gravel" || surface == "pebblestone") && isGood && (hasUrbanTags || (hasQuality && !hasTrailTags)))
      isFormed = true;

    auto const ConvertTo = [&](string const & newType)
    {
      // TODO(@pastk): remove redundant tags, e.g. if converted to cycleway then remove "bicycle=yes".
      LOG(LDEBUG, ("Convert", DebugPrintID(*p), "to", newType, isFormed, isGood, hasTrailTags, hasUrbanTags, p->m_tags));
      p->UpdateTag("highway", newType);
    };

    auto const ConvertPathOrFootway = [&](bool const toPath)
    {
      string const toType = toPath ? kPath : kFootway;
      if (!surface.empty())
      {
        if (toPath ? !isFormed : isFormed)
          ConvertTo(toType);
      }
      else
      {
        if (hasQuality && !isMed)
        {
          if (toPath ? !isGood : isGood)
            ConvertTo(toType);
        }
        else if (toPath ? (hasTrailTags && !hasUrbanTags) : (!hasTrailTags && hasUrbanTags))
          ConvertTo(toType);
      }
    };

    if (highway == kCycleway)
    {
      static base::StringIL segregatedSidewalks = {
          "right", "left", "both"
      };
      if (p->HasTag("segregated", "yes") || Has(segregatedSidewalks, p->GetTag("sidewalk")))
      {
        LOG(LDEBUG, ("Add a separate footway to", DebugPrintID(*p), p->m_tags));
        p->AddTag("highway", kFootway);
      }
      else
      {
        string const foot = p->GetTag("foot");
        if (foot == "designated" || foot == "yes")
        {
          // A non-segregated shared footway/cycleway.
          ConvertTo(kFootway);
          p->AddTag("bicycle", "designated");
          ConvertPathOrFootway(true /* toPath */); // In case its unpaved.
        }
      }
    }
    else if (highway == kPath)
    {
      if (p->HasTag("segregated", "yes"))
      {
        ConvertTo(kCycleway);
        LOG(LDEBUG, ("Add a separate footway to", DebugPrintID(*p), p->m_tags));
        p->AddTag("highway", kFootway);
      }
      else if (p->HasTag("foot", "no") && p->HasTag("bicycle", "designated"))
        ConvertTo(kCycleway);
      else if (!p->HasTag("foot", "no"))
        ConvertPathOrFootway(false /* toPath */);
    }
    else
    {
      CHECK_EQUAL(highway, kFootway, ());
      ConvertPathOrFootway(true /* toPath */);
    }
  }

  if (highway.empty() || (surface.empty() && smoothness.empty()))
    return {};

  bool isGood = true;
  bool isPaved = true;

  // Check surface.
  if (surface.empty())
  {
    CHECK(!smoothness.empty(), ());
    // Extremely bad case.
    if (Has(veryBadSmoothness, smoothness))
      return "unpaved_bad";

    // Tracks already have low speed for cars, but this is mostly for bicycle or pedestrian.
    // If a track has mapped smoothness, obviously it is unpaved :)
    if (highway == "track" && trackGrade != "grade1")
      isPaved = false;
  }
  else
    isPaved = Has(pavedSurfaces, surface);

  // Check smoothness.
  if (!smoothness.empty())
  {
    // Middle case has some heuristics.
    /// @todo Actually, should implement separate surface and smoothness types.
    if (Has(midSmoothness, smoothness))
    {
      if (isPaved)
      {
        if (highway == "motorway" || highway == "trunk")
          return {};
        else
          isGood = false;
      }
      else
        isGood = !Has(badSurfaces, surface);
    }
    else
      isGood = (smoothness != "bad") && !Has(veryBadSmoothness, smoothness);
  }
  else if (surfaceGrade < 2)
    isGood = false;
  else if (!surface.empty() && surfaceGrade < 3)
    isGood = isPaved ? !Has(badSurfaces, surface) : !Has(veryBadSurfaces, surface);

  string psurface = isPaved ? "paved_" : "unpaved_";
  psurface += isGood ? "good" : "bad";
  return psurface;
}

string DeterminePathGrade(OsmElement * p)
{
  if (!p->HasTag("highway", "path"))
    return {};

  string scale = p->GetTag("sac_scale");
  string visibility = p->GetTag("trail_visibility");

  if (scale.empty() && visibility.empty())
    return {};

  static base::StringIL expertScales = {
      "alpine_hiking", "demanding_alpine_hiking", "difficult_alpine_hiking"
  };
  static base::StringIL difficultVisibilities = {
      "bad", "poor" // poor is not official
  };
  static base::StringIL expertVisibilities = {
      "horrible", "no", "very_bad" // very_bad is not official
  };

  if (base::IsExist(expertScales, scale) || base::IsExist(expertVisibilities, visibility))
    return "expert";
  else if (scale == "demanding_mountain_hiking" || base::IsExist(difficultVisibilities, visibility))
    return "difficult";

  // hiking & mountain_hiking scales, excellent, good, intermediate & unknown visibilities means "no grade"
  return {};
}

void PreprocessElement(OsmElement * p, CalculateOriginFnT const & calcOrg)
{
  bool hasLayer = false;
  char const * layer = nullptr;

  bool isSubway = false;
  bool isLightRail = false;
  bool isPlatform = false;
  bool isStopPosition = false;
  bool isBus = false;
  bool isTram = false;
  bool isFunicular = false;

  bool isCapital = false;

  bool isMultipolygon = false;

  /// @todo Rearrange processing code with accumulating tags for: PT, Surface, Cuisine, Artwork, Memorial, ...
  /// Process in separate components (with unit-tests), like DetermineSurface.
  TagProcessor(p).ApplyRules({
      {"bridge", "yes", [&layer] { layer = "1"; }},
      {"tunnel", "yes", [&layer] { layer = "-1"; }},
      {"layer", "*", [&hasLayer] { hasLayer = true; }},

      {"railway", "subway_entrance", [&isSubway] { isSubway = true; }},
      {"public_transport", "platform", [&isPlatform] { isPlatform = true; }},
      {"public_transport", "stop_position", [&isStopPosition] { isStopPosition = true; }},
      {"bus", "yes", [&isBus] { isBus = true; }},
      {"trolleybus", "yes", [&isBus] { isBus = true; }},
      {"tram", "yes", [&isTram] { isTram = true; }},
      {"funicular", "yes", [&isFunicular] { isFunicular = true; }},

      /// @todo Unfortunately, it's not working in many cases (route=subway, transport=subway).
      /// Actually, it's better to process subways after feature types assignment.
      {"station", "subway", [&isSubway] { isSubway = true; }},
      {"station", "light_rail", [&isLightRail] { isLightRail = true; }},

      {"capital", "yes", [&isCapital] { isCapital = true; }},

      {"type", "multipolygon", [&isMultipolygon] { isMultipolygon = true; }},
  });

  if (!hasLayer && layer)
    p->AddTag("layer", layer);

  // Append tags for Node or Way. Relation logic may be too complex to make such a straightforward tagging.
  if (p->m_type != OsmElement::EntityType::Relation)
  {
    // Tag 'city' is needed for correct selection of metro icons.
    if ((isSubway || isLightRail) && calcOrg)
    {
      auto const org = calcOrg(p);
      if (org)
      {
        string const city = MatchCity(mercator::ToLatLon(*org));
        if (!city.empty())
          p->AddTag("city", city);
      }
    }

    // Convert public_transport tags to the older schema.
    if (isPlatform && isBus)
      p->AddTag("highway", "bus_stop");

    if (isStopPosition)
    {
      if (isTram)
        p->AddTag("railway", "tram_stop");
      if (isFunicular)
      {
        p->AddTag("railway", "station");
        p->AddTag("station", "funicular");
      }
    }
  }
  else if (isMultipolygon)
  {
    p->AddTag("area", "yes");
  }

  p->AddTag("psurface", DetermineSurfaceAndHighwayType(p));

  p->AddTag("_path_grade", DeterminePathGrade(p));

  string const kCuisineKey = "cuisine";
  auto cuisines = p->GetTag(kCuisineKey);
  if (!cuisines.empty())
  {
    strings::MakeLowerCaseInplace(cuisines);
    strings::SimpleTokenizer iter(cuisines, ",;");
    auto const collapse = [](char c, string & str)
    {
      auto const comparator = [c](char lhs, char rhs) { return lhs == rhs && lhs == c; };
      str.erase(unique(str.begin(), str.end(), comparator), str.end());
    };

    bool first = true;
    while (iter)
    {
      string normalized(*iter);
      strings::Trim(normalized, " ");
      collapse(' ', normalized);
      replace(normalized.begin(), normalized.end(), ' ', '_');

      if (normalized.empty())
      {
        ++iter;
        continue;
      }

      // Avoid duplication for some cuisines.
      if (normalized == "bbq" || normalized == "barbeque")
        normalized = "barbecue";
      else if (normalized == "doughnut")
        normalized = "donut";
      else if (normalized == "steak")
        normalized = "steak_house";
      else if (normalized == "coffee")
        normalized = "coffee_shop";

      if (first)
        p->UpdateTag(kCuisineKey, normalized);
      else
        p->AddTag(kCuisineKey, normalized);

      first = false;
      ++iter;
    }
  }

  string const kAerodromeTypeKey = "aerodrome:type";
  auto aerodromeTypes = p->GetTag(kAerodromeTypeKey);
  if (!aerodromeTypes.empty())
  {
    strings::MakeLowerCaseInplace(aerodromeTypes);
    bool first = true;
    for (auto type : strings::Tokenize<string>(aerodromeTypes, ",;"))
    {
      strings::Trim(type);

      if (first)
        p->UpdateTag(kAerodromeTypeKey, type);
      else
        p->AddTag(kAerodromeTypeKey, type);

      first = false;
    }
  }

  class CountriesLoader
  {
    std::unique_ptr<storage::CountryInfoGetter> m_infoGetter;
    std::optional<storage::CountryTree> m_countryTree;
    std::array<storage::CountryId, 3> m_provinceToState;

  public:
    // In this countries place=province means place=state.
    CountriesLoader() : m_provinceToState{"Japan", "South Korea", "Turkey"}
    {
      m_infoGetter = storage::CountryInfoReader::CreateCountryInfoGetter(GetPlatform());
      CHECK(m_infoGetter, ());
      m_countryTree = storage::LoadCountriesFromFile(COUNTRIES_FILE);
      CHECK(m_countryTree, ());
    }

    bool IsTransformToState(m2::PointD const & pt) const
    {
      auto const countryId = m_infoGetter->GetRegionCountryId(pt);
      if (!countryId.empty())
      {
        auto const country = storage::GetTopmostParentFor(*m_countryTree, countryId);
        return base::IsExist(m_provinceToState, country);
      }

      LOG(LWARNING, ("CountryId not found for (lat, lon):", mercator::ToLatLon(pt)));
      return false;
    }
  };

  static CountriesLoader s_countriesChecker;

  auto const dePlace = p->GetTag("de:place");
  p->UpdateTagFn("place", [&](string & value)
  {
    // 1. Replace a value of 'place' with a value of 'de:place' because most people regard
    // places names as 'de:place' defines it.
    if (!dePlace.empty())
      value = dePlace;

    // 2. Check valid capital. We support only place-city-capital-2 (not town, village, etc).
    if (isCapital && !value.empty())
      value = "city";

    // 3. Replace 'province' with 'state'.
    if (value != "province")
      return;

    CHECK(calcOrg, ());
    auto const org = calcOrg(p);
    if (org && s_countriesChecker.IsTransformToState(*org))
      value = "state";
  });

  if (isCapital)
    p->UpdateTag("capital", "2");
}

bool IsCarDesignatedHighway(uint32_t type)
{
  switch (ftypes::IsWayChecker::Instance().GetSearchRank(type))
  {
  case ftypes::IsWayChecker::Motorway:
  case ftypes::IsWayChecker::Regular:
  case ftypes::IsWayChecker::Minors:
    return true;
  default:
    return false;
  }
}

bool IsBicycleDesignatedHighway(uint32_t type)
{
  return ftypes::IsWayChecker::Instance().GetSearchRank(type) == ftypes::IsWayChecker::Cycleway;
}

bool IsPedestrianDesignatedHighway(uint32_t type)
{
  return ftypes::IsWayChecker::Instance().GetSearchRank(type) == ftypes::IsWayChecker::Pedestrian;
}

void PostprocessElement(OsmElement * p, FeatureBuilderParams & params)
{
  static CachedTypes const types;

  auto const AddParam = [&params](ftype::CachedTypes::Type type)
  {
    params.AddType(types.Get(type));
  };

  if (!params.house.IsEmpty())
  {
    // Add "building-address" type if we have house number, but no "suitable" (building, POI, etc) types.
    // A lot in Czech, Italy or others, with individual address points (house numbers).
    bool hasSuitableType = false;
    for (uint32_t t : params.m_types)
    {
      /// @todo Make a function like HaveAddressLikeType ?
      ftype::TruncValue(t, 1);
      if (t != types.Get(CachedTypes::WheelchairAny) &&
          t != types.Get(CachedTypes::InternetAny))
      {
        hasSuitableType = true;
        break;
      }
    }

    if (!hasSuitableType)
    {
      AddParam(CachedTypes::Address);

      // https://github.com/organicmaps/organicmaps/issues/5803
      std::string_view const disusedPrefix[] = {"disused:", "abandoned:", "was:"};
      for (auto const & tag : p->Tags())
      {
        for (auto const & prefix : disusedPrefix)
        {
          if (tag.m_key.starts_with(prefix))
          {
            params.ClearPOIAttribs();
            goto exit;
          }
        }
      }
      exit:;
    }
  }

  // Process yes/no tags.
  TagProcessor(p).ApplyRules({
      {"wheelchair", "designated",
       [&AddParam] { AddParam(CachedTypes::WheelchairYes); }},
      {"wifi", "~", [&AddParam] { AddParam(CachedTypes::Wlan); }},
  });

  bool highwayDone = false;
  bool subwayDone = false;
  bool railwayDone = false;
  bool ferryDone = false;

  // Get a copy of source types, because we will modify params in the loop;
  FeatureBuilderParams::Types const vTypes = params.m_types;

  // Defined in priority order:
  // Foot attr is stronger than Sidewalk,
  // Bicycle attr is stronger than Cycleway,
  // MotorCar attr is stronger than MotorVehicle.
  struct Flags { enum Type { Foot = 0, Sidewalk, Bicycle, Cycleway, MotorCar, MotorVehicle, Count }; };
  int flags[Flags::Count] = {0}; // 1 for Yes, -1 for No, 0 for Undefined

  for (uint32_t const vType : vTypes)
  {
    if (!highwayDone && types.IsHighway(vType))
    {
      bool addOneway = false;
      bool noOneway = false;

      TagProcessor(p).ApplyRules({
          {"oneway", "yes", [&addOneway] { addOneway = true; }},
          {"oneway", "1", [&addOneway] { addOneway = true; }},
          {"oneway", "-1",
           [&addOneway, &params] {
             addOneway = true;
             params.SetReversedGeometry(true);
           }},
          {"oneway", "!", [&noOneway] { noOneway = true; }},
// Unlike "roundabout", "circular" is not assumed to force oneway=yes
// (https://wiki.openstreetmap.org/wiki/Tag:junction%3Dcircular), but!
// There are a lot of junction=circular without oneway tag, which is a mapping error (run overpass under England).
// And most of this junctions are assumed to be oneway.
          {"junction", "circular", [&addOneway] { addOneway = true; }},
          {"junction", "roundabout", [&addOneway] { addOneway = true; }},

          {"access", "private", [&AddParam] { AddParam(CachedTypes::Private); }},
          {"access", "!", [&AddParam] { AddParam(CachedTypes::Private); }},

          {"barrier", "gate", [&AddParam] { AddParam(CachedTypes::BarrierGate); }},

          {"lit", "~", [&AddParam] { AddParam(CachedTypes::Lit); }},
          {"toll", "~", [&AddParam] { AddParam(CachedTypes::Toll); }},

          {"foot", "!r", [&flags] { flags[Flags::Foot] = -1; }},
          {"foot", "~r", [&flags] { flags[Flags::Foot] = 1; }},
          {"sidewalk", "!r", [&flags] { flags[Flags::Sidewalk] = -1; }},
          {"sidewalk", "~r", [&flags] { flags[Flags::Sidewalk] = 1; }},
          {"sidewalk:both", "!r", [&flags] { flags[Flags::Sidewalk] = -1; }},
          {"sidewalk:both", "~r", [&flags] { flags[Flags::Sidewalk] = 1; }},
          /// @todo Process left && right == no ?
          {"sidewalk:left", "~r", [&flags] { flags[Flags::Sidewalk] = 1; }},
          {"sidewalk:right", "~r", [&flags] { flags[Flags::Sidewalk] = 1; }},

          {"bicycle", "!r", [&flags] { flags[Flags::Bicycle] = -1; }},
          {"bicycle", "~r", [&flags] { flags[Flags::Bicycle] = 1; }},
          {"bicycle_road", "~r", [&flags] { flags[Flags::Bicycle] = 1; }},
          {"cyclestreet", "~r", [&flags] { flags[Flags::Bicycle] = 1; }},
          {"cycleway", "!r", [&flags] { flags[Flags::Cycleway] = -1; }},
          {"cycleway", "~r", [&flags] { flags[Flags::Cycleway] = 1; }},
          {"cycleway:both", "!r", [&flags] { flags[Flags::Cycleway] = -1; }},
          {"cycleway:both", "~r", [&flags] { flags[Flags::Cycleway] = 1; }},
          /// @todo Process left && right == no ?
          {"cycleway:left", "~r", [&flags] { flags[Flags::Cycleway] = 1; }},
          {"cycleway:right", "~r", [&flags] { flags[Flags::Cycleway] = 1; }},
          {"oneway:bicycle", "!", [&AddParam] { AddParam(CachedTypes::BicycleBidir); }},
          {"oneway:bicycle", "~", [&AddParam] { AddParam(CachedTypes::BicycleOnedir); }},
          {"cycleway", "opposite", [&AddParam] { AddParam(CachedTypes::BicycleBidir); }},

          // For YesCar process only strict =yes/designated.
          {"motor_vehicle", "private", [&flags] { flags[Flags::MotorVehicle] = -1; }},
          {"motor_vehicle", "!", [&flags] { flags[Flags::MotorVehicle] = -1; }},
          {"motor_vehicle", "yes", [&flags] { flags[Flags::MotorVehicle] = 1; }},
          {"motor_vehicle", "designated", [&flags] { flags[Flags::MotorVehicle] = 1; }},
          {"motorcar", "private", [&flags] { flags[Flags::MotorCar] = -1; }},
          {"motorcar", "!", [&flags] { flags[Flags::MotorCar] = -1; }},
          {"motorcar", "yes", [&flags] { flags[Flags::MotorCar] = 1; }},
          {"motorcar", "designated", [&flags] { flags[Flags::MotorCar] = 1; }},
      });

      if (addOneway && !noOneway)
        params.AddType(types.Get(CachedTypes::OneWay));

      auto const ApplyFlag = [&flags, &AddParam](Flags::Type f, CachedTypes::Type yes,
                                                 CachedTypes::Type no0, CachedTypes::Type no1,
                                                 bool isDesignated)
      {
        if (flags[f] == 1 && !isDesignated)
          AddParam(yes);
        else if (flags[f] == -1)
          AddParam(no0);
        else if (flags[int(f) + 1] == 1 && !isDesignated)
          AddParam(yes);
        else if (flags[int(f) + 1] == -1)
          AddParam(no1);
      };

      ApplyFlag(Flags::Foot, CachedTypes::YesFoot, CachedTypes::NoFoot, CachedTypes::NoSidewalk,
                IsPedestrianDesignatedHighway(vType));
      ApplyFlag(Flags::Bicycle, CachedTypes::YesBicycle, CachedTypes::NoBicycle, CachedTypes::NoCycleway,
                IsBicycleDesignatedHighway(vType));
      ApplyFlag(Flags::MotorCar, CachedTypes::YesCar, CachedTypes::NoCar, CachedTypes::NoCar,
                IsCarDesignatedHighway(vType));

      highwayDone = true;
    }

    if (!ferryDone && types.IsTransporter(vType))
    {
      bool yesMotorFerry = false;
      bool noMotorFerry = false;

      TagProcessor(p).ApplyRules({
          {"foot", "!", [&AddParam] { AddParam(CachedTypes::NoFoot); }},
          {"foot", "~", [&AddParam] { AddParam(CachedTypes::YesFoot); }},
          {"ferry", "footway", [&AddParam] { AddParam(CachedTypes::YesFoot); }},
          {"ferry", "pedestrian", [&AddParam] { AddParam(CachedTypes::YesFoot); }},
          {"ferry", "path", [&params] {
             params.AddType(types.Get(CachedTypes::YesFoot));
             params.AddType(types.Get(CachedTypes::YesBicycle));
          }},

          {"bicycle", "!", [&AddParam] { AddParam(CachedTypes::NoBicycle); }},
          {"bicycle", "~", [&AddParam] { AddParam(CachedTypes::YesBicycle); }},

          // Check for explicit no-tag.
          {"motor_vehicle", "!", [&noMotorFerry] { noMotorFerry = true; }},
          {"motorcar", "!", [&noMotorFerry] { noMotorFerry = true; }},

          {"motor_vehicle", "yes", [&yesMotorFerry] { yesMotorFerry = true; }},
          {"motorcar", "yes", [&yesMotorFerry] { yesMotorFerry = true; }},
          {"ferry", "trunk", [&yesMotorFerry] { yesMotorFerry = true; }},
          {"ferry", "primary", [&yesMotorFerry] { yesMotorFerry = true; }},
          {"ferry", "secondary", [&yesMotorFerry] { yesMotorFerry = true; }},
          {"ferry", "tertiary", [&yesMotorFerry] { yesMotorFerry = true; }},
          {"ferry", "residential", [&yesMotorFerry] { yesMotorFerry = true; }},
          {"ferry", "service", [&yesMotorFerry] { yesMotorFerry = true; }},
          {"ferry", "unclassified", [&yesMotorFerry] { yesMotorFerry = true; }},
          {"ferry", "track", [&yesMotorFerry] { yesMotorFerry = true; }},
      });

      // Car routing for ferries should be explicitly defined.
      params.AddType(types.Get(!noMotorFerry && yesMotorFerry ? CachedTypes::YesCar : CachedTypes::NoCar));

      ferryDone = true;
    }

    /// @todo Probably, we can delete this processing because cities
    /// are matched by limit rect in MatchCity.
    if (!subwayDone && types.IsSubwayStation(vType))
    {
      TagProcessor(p).ApplyRules({
          {"network", "London Underground", [&params] { params.SetRwSubwayType("london"); }},
          {"network", "New York City Subway", [&params] { params.SetRwSubwayType("newyork"); }},
          {"network", "Московский метрополитен", [&params] { params.SetRwSubwayType("moscow"); }},
          {"network", "Петербургский метрополитен", [&params] { params.SetRwSubwayType("spb"); }},
          {"network", "Verkehrsverbund Berlin-Brandenburg",
           [&params] { params.SetRwSubwayType("berlin"); }},
          {"network", "Минский метрополитен", [&params] { params.SetRwSubwayType("minsk"); }},

          {"network", "Київський метрополітен", [&params] { params.SetRwSubwayType("kiev"); }},
          {"operator", "КП «Київський метрополітен»",
           [&params] { params.SetRwSubwayType("kiev"); }},

          {"network", "RATP", [&params] { params.SetRwSubwayType("paris"); }},
          {"network", "Metro de Barcelona", [&params] { params.SetRwSubwayType("barcelona"); }},

          {"network", "Metro de Madrid", [&params] { params.SetRwSubwayType("madrid"); }},
          {"operator", "Metro de Madrid", [&params] { params.SetRwSubwayType("madrid"); }},

          {"network", "Metropolitana di Roma", [&params] { params.SetRwSubwayType("roma"); }},
          {"network", "ATAC", [&params] { params.SetRwSubwayType("roma"); }},
      });

      subwayDone = true;
    }

    if (!subwayDone && !railwayDone && types.IsRailwayStation(vType))
    {
      TagProcessor(p).ApplyRules({
          {"network", "London Underground", [&params] { params.SetRwSubwayType("london"); }},
      });

      railwayDone = true;
    }
  }
}
}  // namespace

void GetNameAndType(OsmElement * p, FeatureBuilderParams & params,
                    TypesFilterFnT const & filterType, CalculateOriginFnT const & calcOrg)
{
  // At this point, some preprocessing could've been done to the tags already
  // in TranslatorInterface::Preprocess(), e.g. converting tags according to replaced_tags.txt.

  // Stage1: Preprocess tags.
  PreprocessElement(p, calcOrg);

  // Stage2: Process feature name on all languages.
  NamesExtractor namesExtractor(params);
  ForEachTag(p, namesExtractor);
  namesExtractor.Finish();

  // Stage3: Process base feature tags.
  std::string houseName, houseNumber, conscriptionHN, streetHN, addrPostcode;
  std::string addrCity, addrSuburb;
  feature::AddressData addr;
  TagProcessor(p).ApplyRules<void(string &, string &)>(
  {
      {"addr:housenumber", "*", [&houseNumber](string & k, string & v)
      {
        houseNumber = std::move(v);
      }},
      {"addr:conscriptionnumber", "*", [&conscriptionHN](string & k, string & v)
      {
        conscriptionHN = std::move(v);
      }},
      {"addr:provisionalnumber", "*", [&conscriptionHN](string & k, string & v)
      {
        conscriptionHN = std::move(v);
      }},
      {"addr:streetnumber", "*", [&streetHN](string & k, string & v)
      {
        streetHN = std::move(v);
      }},
      {"contact:housenumber", "*", [&houseNumber](string & k, string & v)
      {
        if (houseNumber.empty())
          houseNumber = std::move(v);
      }},
      {"addr:housename", "*", [&houseName](string & k, string & v)
      {
        houseName = std::move(v);
      }},
      {"addr:street", "*", [&addr](string & k, string & v)
      {
        addr.Set(feature::AddressData::Type::Street, std::move(v));
      }},
      {"contact:street", "*", [&addr](string & k, string & v)
      {
        addr.SetIfAbsent(feature::AddressData::Type::Street, std::move(v));
      }},
      {"addr:place", "*", [&addr](string & k, string & v)
      {
        addr.Set(feature::AddressData::Type::Place, std::move(v));
      }},
      {"addr:city", "*", [&addrCity](string & k, string & v)
       {
         addrCity = std::move(v);
      }},
      {"addr:suburb", "*", [&addrSuburb](string & k, string & v)
      {
        addrSuburb = std::move(v);
      }},
      {"addr:postcode", "*", [&addrPostcode](string & k, string & v)
      {
        addrPostcode = std::move(v);
      }},
      {"postal_code", "*", [&addrPostcode](string & k, string & v)
      {
        addrPostcode = std::move(v);
      }},
      {"contact:postcode", "*", [&addrPostcode](string & k, string & v)
      {
        if (addrPostcode.empty())
          addrPostcode = std::move(v);
      }},
      {"population", "*", [&params](string & k, string & v)
      {
        // Get population rank.
        uint64_t const population = generator::osm_element::GetPopulation(v);
        if (population != 0)
          params.rank = feature::PopulationToRank(population);
      }},
      {"ref", "*", [&params](string & k, string & v)
      {
        // Get reference; its used for selected types only, see FeatureBuilder::PreSerialize().
        params.ref = std::move(v);
      }},
      {"layer", "*", [&params](string & k, string & v)
      {
        // Get layer.
        if (params.layer == feature::LAYER_EMPTY)
        {
          // atoi error value (0) should match empty layer constant.
          static_assert(feature::LAYER_EMPTY == 0);
          params.layer = atoi(v.c_str());
          params.layer = base::Clamp(params.layer, int8_t{feature::LAYER_LOW}, int8_t{feature::LAYER_HIGH});
        }
      }},
  });

  // OSM consistency check with house numbers.
  if (!conscriptionHN.empty() || !streetHN.empty())
  {
    // Simple validity check, trust housenumber tag in other cases.

    char const * kHNLogTag = "HNLog";
    if (!conscriptionHN.empty() && !streetHN.empty())
    {
      auto i = houseNumber.find('/');
      if (i == std::string::npos)
      {
        LOG(LWARNING, (kHNLogTag, "Override housenumber for:", DebugPrintID(*p), houseNumber, conscriptionHN, streetHN));
        houseNumber = conscriptionHN + "/" + streetHN;
      }
    }
    else if (houseNumber.empty())
    {
      LOG(LWARNING, (kHNLogTag, "Assign housenumber for:", DebugPrintID(*p), houseNumber, conscriptionHN, streetHN));
      houseNumber = conscriptionHN.empty() ? streetHN : conscriptionHN;
    }

    /// @todo Remove "ev." prefix from HN?
  }

  if (!houseNumber.empty() && addr.Empty())
  {
    if (!addrSuburb.empty())
    {
      // Treat addr:suburb as addr:place (https://overpass-turbo.eu/s/1Dlz)
      addr.Set(feature::AddressData::Type::Place, std::move(addrSuburb));
    }
    else if (!addrCity.empty())
    {
      // Treat addr:city as addr:place
      class CityBBox
      {
        std::vector<m2::RectD> m_rects;
      public:
        CityBBox()
        {
          // Зеленоград
          m_rects.emplace_back(37.119113, 55.944925, 37.273608, 56.026874);

          // Add new {lon, lat} city bboxes here.
        }
        bool IsInside(m2::PointD const & pt)
        {
          auto const ll = mercator::ToLatLon(pt);
          for (auto const & r : m_rects)
          {
            if (r.IsPointInside({ll.m_lon, ll.m_lat}))
              return true;
          }
          return false;
        }
      };

      static CityBBox s_cityBBox;

      CHECK(calcOrg, ());
      auto const org = calcOrg(p);
      if (org && s_cityBBox.IsInside(*org))
        addr.Set(feature::AddressData::Type::Place, std::move(addrCity));
    }
  }

  params.SetAddress(std::move(addr));
  params.SetPostcode(std::move(addrPostcode));
  params.SetHouseNumberAndHouseName(std::move(houseNumber), std::move(houseName));

  // Fetch piste:name and piste:ref if there are no other name/ref values.
  TagProcessor(p).ApplyRules<void(string &, string &)>(
  {
      {"piste:ref", "*", [&params](string & k, string & v)
      {
        if (params.ref.empty())
          params.ref = std::move(v);
      }},
      {"piste:name", "*", [&params](string & k, string & v)
      {
        params.SetDefaultNameIfEmpty(std::move(v));
      }},
  });

  // Stage4: Match tags to classificator feature types via mapcss-mapping.csv.
  MatchTypes(p, params, filterType);

  // Stage5: Postprocess feature types.
  PostprocessElement(p, params);

  {
    std::string const typesString = params.PrintTypes();

    if (params.RemoveInconsistentTypes())
      LOG(LWARNING, ("Inconsistent types for:", DebugPrintID(*p), "Types:", typesString));

    size_t const typesCount = params.m_types.size();
    if (params.FinishAddingTypesEx() == FeatureParams::TYPES_EXCEED_MAX)
      LOG(LWARNING, ("Exceeded types count for:", DebugPrintID(*p), "Types:", typesCount, typesString));

    if (!params.house.IsEmpty() && !ftypes::IsAddressObjectChecker::Instance()(params.m_types))
      LOG(LWARNING, ("Have house number for _non-address_:", DebugPrintID(*p), "Types:", typesString));
  }

  // Stage6: Collect additional information about feature such as
  // hotel stars, opening hours, cuisine, ...
  ForEachTag(p, MetadataTagProcessor(params));
}
}  // namespace ftype
