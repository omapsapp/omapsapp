#include "indexer/ftypes_matcher.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"

#include "base/assert.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <map>
#include <sstream>


namespace ftypes
{
using namespace std;

namespace
{
class HighwayClasses
{
  map<uint32_t, ftypes::HighwayClass> m_map;

public:
  HighwayClasses()
  {
    auto const & c = classif();
    m_map[c.GetTypeByPath({"route", "ferry"})] = ftypes::HighwayClass::Transported;
    m_map[c.GetTypeByPath({"route", "shuttle_train"})] = ftypes::HighwayClass::Transported;

    m_map[c.GetTypeByPath({"highway", "motorway"})] = ftypes::HighwayClass::Trunk;
    m_map[c.GetTypeByPath({"highway", "motorway_link"})] = ftypes::HighwayClass::Trunk;
    m_map[c.GetTypeByPath({"highway", "trunk"})] = ftypes::HighwayClass::Trunk;
    m_map[c.GetTypeByPath({"highway", "trunk_link"})] = ftypes::HighwayClass::Trunk;

    m_map[c.GetTypeByPath({"highway", "primary"})] = ftypes::HighwayClass::Primary;
    m_map[c.GetTypeByPath({"highway", "primary_link"})] = ftypes::HighwayClass::Primary;

    m_map[c.GetTypeByPath({"highway", "secondary"})] = ftypes::HighwayClass::Secondary;
    m_map[c.GetTypeByPath({"highway", "secondary_link"})] = ftypes::HighwayClass::Secondary;

    m_map[c.GetTypeByPath({"highway", "tertiary"})] = ftypes::HighwayClass::Tertiary;
    m_map[c.GetTypeByPath({"highway", "tertiary_link"})] = ftypes::HighwayClass::Tertiary;

    m_map[c.GetTypeByPath({"highway", "unclassified"})] = ftypes::HighwayClass::LivingStreet;
    m_map[c.GetTypeByPath({"highway", "residential"})] = ftypes::HighwayClass::LivingStreet;
    m_map[c.GetTypeByPath({"highway", "living_street"})] = ftypes::HighwayClass::LivingStreet;
    m_map[c.GetTypeByPath({"highway", "road"})] = ftypes::HighwayClass::LivingStreet;

    m_map[c.GetTypeByPath({"highway", "service"})] = ftypes::HighwayClass::Service;
    m_map[c.GetTypeByPath({"highway", "track"})] = ftypes::HighwayClass::Service;
    m_map[c.GetTypeByPath({"highway", "busway"})] = ftypes::HighwayClass::Service;
    m_map[c.GetTypeByPath({"man_made", "pier"})] = ftypes::HighwayClass::Service;

    m_map[c.GetTypeByPath({"highway", "pedestrian"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "footway"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "bridleway"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "steps"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "cycleway"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "path"})] = ftypes::HighwayClass::Pedestrian;
    m_map[c.GetTypeByPath({"highway", "construction"})] = ftypes::HighwayClass::Pedestrian;
  }

  ftypes::HighwayClass Get(uint32_t t) const
  {
    auto const it = m_map.find(t);
    if (it == m_map.cend())
      return ftypes::HighwayClass::Undefined;
    return it->second;
  }
};

char const * HighwayClassToString(ftypes::HighwayClass const cls)
{
  switch (cls)
  {
  case ftypes::HighwayClass::Undefined: return "Undefined";
  case ftypes::HighwayClass::Transported: return "Transported";
  case ftypes::HighwayClass::Trunk: return "Trunk";
  case ftypes::HighwayClass::Primary: return "Primary";
  case ftypes::HighwayClass::Secondary: return "Secondary";
  case ftypes::HighwayClass::Tertiary: return "Tertiary";
  case ftypes::HighwayClass::LivingStreet: return "LivingStreet";
  case ftypes::HighwayClass::Service: return "Service";
  case ftypes::HighwayClass::Pedestrian: return "Pedestrian";
  case ftypes::HighwayClass::Count: return "Count";
  }
  ASSERT(false, ());
  return "";
}
}  // namespace

string DebugPrint(HighwayClass const cls)
{
  stringstream out;
  out << "[ " << HighwayClassToString(cls) << " ]";
  return out.str();
}

string DebugPrint(LocalityType const localityType)
{
  switch (localityType)
  {
  case LocalityType::None: return "None";
  case LocalityType::Country: return "Country";
  case LocalityType::State: return "State";
  case LocalityType::City: return "City";
  case LocalityType::Town: return "Town";
  case LocalityType::Village: return "Village";
  case LocalityType::Count: return "Count";
  }
  UNREACHABLE();
}

HighwayClass GetHighwayClass(feature::TypesHolder const & types)
{
  static HighwayClasses highwayClasses;

  for (auto t : types)
  {
    ftype::TruncValue(t, 2);
    HighwayClass const hc = highwayClasses.Get(t);
    if (hc != HighwayClass::Undefined)
      return hc;
  }

  return HighwayClass::Undefined;
}

uint32_t BaseChecker::PrepareToMatch(uint32_t type, uint8_t level)
{
  ftype::TruncValue(type, level);
  return type;
}

bool BaseChecker::IsMatched(uint32_t type) const
{
  return base::IsExist(m_types, PrepareToMatch(type, m_level));
}

void BaseChecker::ForEachType(function<void(uint32_t)> const & fn) const
{
  for (auto const & t : m_types)
    fn(t);
}

bool BaseChecker::operator()(feature::TypesHolder const & types) const
{
  for (uint32_t t : types)
  {
    if (IsMatched(t))
      return true;
  }

  return false;
}

bool BaseChecker::operator()(FeatureType & ft) const
{
  return this->operator()(feature::TypesHolder(ft));
}

bool BaseChecker::operator()(vector<uint32_t> const & types) const
{
  for (uint32_t t : types)
  {
    if (IsMatched(t))
      return true;
  }

  return false;
}

IsPeakChecker::IsPeakChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"natural", "peak"}));
}

IsATMChecker::IsATMChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"amenity", "atm"}));
}

IsSpeedCamChecker::IsSpeedCamChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"highway", "speed_camera"}));
}

IsPostBoxChecker::IsPostBoxChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"amenity", "post_box"}));
}

IsPostPoiChecker::IsPostPoiChecker()
{
  Classificator const & c = classif();
  for (char const * val : {"post_office", "post_box", "parcel_locker"})
    m_types.push_back(c.GetTypeByPath({"amenity", val}));
}

IsOperatorOthersPoiChecker::IsOperatorOthersPoiChecker()
{
  Classificator const & c = classif();
  for (char const * val : {"bicycle_rental", "bureau_de_change", "car_sharing", "car_rental", "fuel", "charging_station", 
                           "parking", "motorcycle_parking", "bicycle_parking", "payment_terminal", "university", "vending_machine"})
    m_types.push_back(c.GetTypeByPath({"amenity", val}));
}

IsRecyclingCentreChecker::IsRecyclingCentreChecker() : BaseChecker(3 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"amenity", "recycling", "centre"}));
}

uint32_t IsRecyclingCentreChecker::GetType() const { return m_types[0]; }

IsRecyclingContainerChecker::IsRecyclingContainerChecker() : BaseChecker(3 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"amenity", "recycling", "container"}));
  // Treat default type also as a container, see https://taginfo.openstreetmap.org/keys/recycling_type#values
  m_types.push_back(c.GetTypeByPath({"amenity", "recycling"}));
}

uint32_t IsRecyclingContainerChecker::GetType() const { return m_types[0]; }

IsRailwayStationChecker::IsRailwayStationChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"railway", "station"}));
  m_types.push_back(c.GetTypeByPath({"building", "train_station"}));
}

IsSubwayStationChecker::IsSubwayStationChecker() : BaseChecker(3 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"railway", "station", "subway"}));
}

IsAirportChecker::IsAirportChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"aeroway", "aerodrome"}));
}

IsSquareChecker::IsSquareChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"place", "square"}));
}

IsSuburbChecker::IsSuburbChecker()
{
  Classificator const & c = classif();
  base::StringIL const types[] = {{"landuse", "residential"},
                                  {"place", "neighbourhood"},
                                  {"place", "suburb"}};
  for (auto const & e : types)
    m_types.push_back(c.GetTypeByPath(e));

  // `types` order should match next indices.
  static_assert(static_cast<size_t>(SuburbType::Residential) == 0, "");
  static_assert(static_cast<size_t>(SuburbType::Neighbourhood) == 1, "");
  static_assert(static_cast<size_t>(SuburbType::Suburb) == 2, "");
}

SuburbType IsSuburbChecker::GetType(uint32_t t) const
{
  ftype::TruncValue(t, 2);
  for (size_t i = 0; i < m_types.size(); ++i)
  {
    if (m_types[i] == t)
      return static_cast<SuburbType>(i);
  }
  return SuburbType::Count;
}

SuburbType IsSuburbChecker::GetType(feature::TypesHolder const & types) const
{
  auto smallestType = SuburbType::Count;
  for (uint32_t t : types)
  {
    auto const type = GetType(t);
    if (type < smallestType)
      smallestType = type;
  }
  return smallestType;
}

SuburbType IsSuburbChecker::GetType(FeatureType & f) const
{
  feature::TypesHolder types(f);
  return GetType(types);
}

IsWayChecker::IsWayChecker()
{
  Classificator const & c = classif();
  std::pair<char const *, SearchRank> const types[] = {
      // type           rank
      {"cycleway",      Cycleway},
      {"footway",       Pedestrian},
      {"living_street", Residential},
      {"motorway",      Motorway},
      {"motorway_link", Motorway},
      {"path",          Outdoor},
      {"pedestrian",    Pedestrian},
      {"primary",       Regular},
      {"primary_link",  Regular},
      {"residential",   Residential},
      {"road",          Minors},
      {"secondary",     Regular},
      {"secondary_link",Regular},
      {"service",       Minors},
      {"steps",         Pedestrian},
      {"tertiary",      Regular},
      {"tertiary_link", Regular},
      {"track",         Outdoor},
      {"trunk",         Motorway},
      {"trunk_link",    Motorway},
      {"unclassified",  Minors},
  };

  m_ranks.Reserve(std::size(types));
  for (auto const & e : types)
  {
    uint32_t const type = c.GetTypeByPath({"highway", e.first});
    m_types.push_back(type);
    m_ranks.Insert(type, e.second);
  }
}

IsWayChecker::SearchRank IsWayChecker::GetSearchRank(uint32_t type) const
{
  ftype::TruncValue(type, 2);
  if (auto const * res = m_ranks.Find(type))
    return *res;
  return Default;
}

IsStreetOrSquareChecker::IsStreetOrSquareChecker()
{
  for (auto const t : IsWayChecker::Instance().GetTypes())
    m_types.push_back(t);
  for (auto const t : IsSquareChecker::Instance().GetTypes())
    m_types.push_back(t);
}

IsAddressObjectChecker::IsAddressObjectChecker() : BaseChecker(1 /* level */)
{
  m_types = OneLevelPOIChecker().GetTypes();

  Classificator const & c = classif();
  for (auto const * p : {"addr:interpolation", "building", "entrance"})
    m_types.push_back(c.GetTypeByPath({p}));
}

IsVillageChecker::IsVillageChecker()
{
  // TODO (@y, @m, @vng): this list must be up-to-date with
  // data/categories.txt, so, it's worth it to generate or parse it
  // from that file.
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"place", "village"}));
  m_types.push_back(c.GetTypeByPath({"place", "hamlet"}));
}

IsOneWayChecker::IsOneWayChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"hwtag", "oneway"}));
}

IsRoundAboutChecker::IsRoundAboutChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"junction", "circular"}));
  m_types.push_back(c.GetTypeByPath({"junction", "roundabout"}));
}

IsLinkChecker::IsLinkChecker()
{
  Classificator const & c = classif();
  base::StringIL const types[] = {{"highway", "motorway_link"},
                                  {"highway", "trunk_link"},
                                  {"highway", "primary_link"},
                                  {"highway", "secondary_link"},
                                  {"highway", "tertiary_link"}};

  for (auto const & e : types)
    m_types.push_back(c.GetTypeByPath(e));
}

IsBuildingChecker::IsBuildingChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"building"}));
}

IsBuildingPartChecker::IsBuildingPartChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"building:part"}));
}

IsBuildingHasPartsChecker::IsBuildingHasPartsChecker() : BaseChecker(2 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"building", "has_parts"}));
}

IsIsolineChecker::IsIsolineChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"isoline"}));
}

IsPisteChecker::IsPisteChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"piste:type"}));
}


OneLevelPOIChecker::OneLevelPOIChecker() : ftypes::BaseChecker(1 /* level */)
{
  Classificator const & c = classif();

  for (auto const * path : {"amenity",  "craft", "healthcare", "historic", "leisure", "office", "railway",
                            "shop", "sport", "tourism"})
    m_types.push_back(c.GetTypeByPath({path}));
}

TwoLevelPOIChecker::TwoLevelPOIChecker() : ftypes::BaseChecker(2 /* level */)
{
  Classificator const & c = classif();
  base::StringIL arr[] = {
      {"aeroway", "terminal"},
      {"aeroway", "gate"},
      {"building", "train_station"},
      {"emergency", "defibrillator"},
      {"emergency", "fire_hydrant"},
      {"emergency", "phone"},
      {"highway", "bus_stop"},
      {"highway", "elevator"},
      {"highway", "ford"},
      {"highway", "raceway"},
      {"highway", "rest_area"},
      {"highway", "services"},
      {"highway", "speed_camera"},
      {"man_made", "lighthouse"},
      {"man_made", "water_tap"},
      {"man_made", "water_well"},
      {"natural", "beach"},
      {"natural", "geyser"},
      {"natural", "cave_entrance"},
      {"natural", "spring"},
      {"natural", "volcano"},
      {"waterway", "waterfall"}
  };

  for (auto const & path : arr)
    m_types.push_back(c.GetTypeByPath(path));
}


IsAmenityChecker::IsAmenityChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"amenity"}));
}

AttractionsChecker::AttractionsChecker() : BaseChecker(2 /* level */)
{
  base::StringIL const primaryAttractionTypes[] = {
      {"amenity", "arts_centre"},
      {"amenity", "grave_yard"},
      {"amenity", "fountain"},
      {"amenity", "place_of_worship"},
      {"amenity", "theatre"},
      {"amenity", "townhall"},
      {"amenity", "university"},
      {"boundary", "national_park"},
      {"building", "train_station"},
      {"highway", "pedestrian"},
      {"historic", "archaeological_site"},
      {"historic", "boundary_stone"},
      {"historic", "castle"},
      {"historic", "city_gate"},
      {"historic", "citywalls"},
      {"historic", "fort"},
      {"historic", "gallows"},
      {"historic", "memorial"},
      {"historic", "monument"},
      {"historic", "pillory"},
      {"historic", "ruins"},
      {"historic", "ship"},
      {"historic", "tomb"},
      {"historic", "wayside_cross"},
      {"historic", "wayside_shrine"},
      {"landuse", "cemetery"},
      {"leisure", "beach_resort"},
      {"leisure", "garden"},
      {"leisure", "marina"},
      {"leisure", "nature_reserve"},
      {"leisure", "park"},
      {"leisure", "water_park"},
      {"man_made", "lighthouse"},
      {"man_made", "windmill"},
      {"natural", "beach"},
      {"natural", "cave_entrance"},
      {"natural", "geyser"},
      {"natural", "glacier"},
      {"natural", "hot_spring"},
      {"natural", "peak"},
      {"natural", "volcano"},
      {"place", "square"},
      {"tourism", "aquarium"},
      {"tourism", "artwork"},
      {"tourism", "museum"},
      {"tourism", "gallery"},
      {"tourism", "zoo"},
      {"tourism", "theme_park"},
      {"waterway", "waterfall"},
  };

  Classificator const & c = classif();

  for (auto const & e : primaryAttractionTypes)
    m_types.push_back(c.GetTypeByPath(e));
  sort(m_types.begin(), m_types.end());
  m_additionalTypesStart = m_types.size();

  // Additional types are worse in "hierarchy" priority.
  base::StringIL const additionalAttractionTypes[] = {
      {"tourism", "viewpoint"},
      {"tourism", "attraction"},
  };

  for (auto const & e : additionalAttractionTypes)
    m_types.push_back(c.GetTypeByPath(e));
  sort(m_types.begin() + m_additionalTypesStart, m_types.end());
}

uint32_t AttractionsChecker::GetBestType(FeatureParams::Types const & types) const
{
  auto additionalType = ftype::GetEmptyValue();
  auto const itAdditional = m_types.begin() + m_additionalTypesStart;

  for (auto type : types)
  {
    type = PrepareToMatch(type, m_level);
    if (binary_search(m_types.begin(), itAdditional, type))
      return type;

    if (binary_search(itAdditional, m_types.end(), type))
      additionalType = type;
  }

  return additionalType;
}

IsPlaceChecker::IsPlaceChecker() : BaseChecker(1 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"place"}));
}

IsBridgeOrTunnelChecker::IsBridgeOrTunnelChecker() : BaseChecker(3 /* level */) {}

bool IsBridgeOrTunnelChecker::IsMatched(uint32_t type) const
{
  if (ftype::GetLevel(type) != 3)
    return false;

  ClassifObject const * p = classif().GetRoot()->GetObject(ftype::GetValue(type, 0));
  if (p->GetName() != "highway")
    return false;

  p = p->GetObject(ftype::GetValue(type, 1));
  p = p->GetObject(ftype::GetValue(type, 2));
  return (p->GetName() == "bridge" || p->GetName() == "tunnel");
}

IsHotelChecker::IsHotelChecker()
{
  base::StringIL const types[] = {
    {"tourism", "alpine_hut"},
    {"tourism", "apartment"},
    {"tourism", "camp_site"},
    {"tourism", "caravan_site"},  /// @todo Sure here?
    {"tourism", "chalet"},
    {"tourism", "guest_house"},
    {"tourism", "hostel"},
    {"tourism", "hotel"},
    {"tourism", "motel"},
    {"tourism", "wilderness_hut"},
    {"leisure", "resort"},
  };

  Classificator const & c = classif();
  for (auto const & e : types)
    m_types.push_back(c.GetTypeByPath(e));
}

IsIslandChecker::IsIslandChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"place", "island"}));
  m_types.push_back(c.GetTypeByPath({"place", "islet"}));
}

IsLandChecker::IsLandChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"natural", "land"}));
}

uint32_t IsLandChecker::GetLandType() const
{
  CHECK_EQUAL(m_types.size(), 1, ());
  return m_types[0];
}

IsCoastlineChecker::IsCoastlineChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"natural", "coastline"}));
}

uint32_t IsCoastlineChecker::GetCoastlineType() const
{
  CHECK_EQUAL(m_types.size(), 1, ());
  return m_types[0];
}

IsWifiChecker::IsWifiChecker()
{
  m_types.push_back(classif().GetTypeByPath({"internet_access", "wlan"}));
}

IsEatChecker::IsEatChecker()
{
  // The order should be the same as in "enum class Type" declaration.
  /// @todo amenity=ice_cream if we already have biergarten :)
  base::StringIL const types[] = {{"amenity", "cafe"},
                                  {"amenity", "fast_food"},
                                  {"amenity", "restaurant"},
                                  {"amenity", "bar"},
                                  {"amenity", "pub"},
                                  {"amenity", "biergarten"},
                                  {"amenity", "food_court"},
  };

  Classificator const & c = classif();
//  size_t i = 0;
  for (auto const & e : types)
  {
    auto const type = c.GetTypeByPath(e);
    m_types.push_back(type);
//    m_eat2clType[i++] = type;
  }
}

//IsEatChecker::Type IsEatChecker::GetType(uint32_t t) const
//{
//  for (size_t i = 0; i < m_eat2clType.size(); ++i)
//  {
//    if (m_eat2clType[i] == t)
//      return static_cast<Type>(i);
//  }
//  return IsEatChecker::Type::Count;
//}

IsCuisineChecker::IsCuisineChecker() : BaseChecker(1 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"cuisine"}));
}

IsRecyclingTypeChecker::IsRecyclingTypeChecker() : BaseChecker(1 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"recycling"}));
}

IsFeeTypeChecker::IsFeeTypeChecker() : BaseChecker(1 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"fee"}));
}

IsToiletsChecker::IsToiletsChecker() : BaseChecker(2 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"amenity", "toilets"}));
  m_types.push_back(c.GetTypeByPath({"toilets", "yes"}));
}

IsCapitalChecker::IsCapitalChecker() : BaseChecker(3 /* level */)
{
  m_types.push_back(classif().GetTypeByPath({"place", "city", "capital"}));
}

IsPublicTransportStopChecker::IsPublicTransportStopChecker()
{
  Classificator const & c = classif();
  /// @todo Add bus station into _major_ class (like IsRailwayStationChecker)?
  m_types.push_back(c.GetTypeByPath({"amenity", "bus_station"}));
  m_types.push_back(c.GetTypeByPath({"amenity", "ferry_terminal"}));
  m_types.push_back(c.GetTypeByPath({"highway", "bus_stop"}));
  m_types.push_back(c.GetTypeByPath({"railway", "halt"}));
  m_types.push_back(c.GetTypeByPath({"railway", "tram_stop"}));
}

IsMotorwayJunctionChecker::IsMotorwayJunctionChecker()
{
  m_types.push_back(classif().GetTypeByPath({"highway", "motorway_junction"}));
}

IsWayWithDurationChecker::IsWayWithDurationChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"route", "ferry"}));
  m_types.push_back(c.GetTypeByPath({"route", "shuttle_train"}));
}

LocalityType LocalityFromString(std::string_view place)
{
  if (place == "village" || place == "hamlet")
    return LocalityType::Village;
  if (place == "town")
    return LocalityType::Town;
  if (place == "city")
    return LocalityType::City;
  if (place == "state")
    return LocalityType::State;
  if (place == "country")
    return LocalityType::Country;

  return LocalityType::None;
}

IsLocalityChecker::IsLocalityChecker()
{
  Classificator const & c = classif();

  // Note! The order should be equal with constants in Type enum (add other villages to the end).
  base::StringIL const types[] = {
    { "place", "country" },
    { "place", "state" },
    { "place", "city" },
    { "place", "town" },
    { "place", "village" },
    { "place", "hamlet" }
  };

  for (auto const & e : types)
    m_types.push_back(c.GetTypeByPath(e));
}

LocalityType IsLocalityChecker::GetType(uint32_t t) const
{
  ftype::TruncValue(t, 2);

  auto j = static_cast<size_t>(LocalityType::Country);
  for (; j < static_cast<size_t>(LocalityType::Count); ++j)
    if (t == m_types[j])
      return static_cast<LocalityType>(j);

  for (; j < m_types.size(); ++j)
    if (t == m_types[j])
      return LocalityType::Village;

  return LocalityType::None;
}

LocalityType IsLocalityChecker::GetType(FeatureType & f) const
{
  feature::TypesHolder types(f);
  return GetType(types);
}

IsCountryChecker::IsCountryChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"place", "country"}));
}

IsStateChecker::IsStateChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"place", "state"}));
}

IsCityTownOrVillageChecker::IsCityTownOrVillageChecker()
{
  base::StringIL const types[] = {
    {"place", "city"},
    {"place", "town"},
    {"place", "village"},
    {"place", "hamlet"}
  };

  Classificator const & c = classif();
  for (auto const & e : types)
    m_types.push_back(c.GetTypeByPath(e));
}

IsEntranceChecker::IsEntranceChecker() : BaseChecker(1 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"entrance"}));
}

IsAerowayGateChecker::IsAerowayGateChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"aeroway", "gate"}));
}

IsRailwaySubwayEntranceChecker::IsRailwaySubwayEntranceChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"railway", "subway_entrance"}));
}

IsPlatformChecker::IsPlatformChecker()
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"railway", "platform"}));
  m_types.push_back(c.GetTypeByPath({"public_transport", "platform"}));
}

IsAddressInterpolChecker::IsAddressInterpolChecker() : BaseChecker(1 /* level */)
{
  Classificator const & c = classif();
  m_types.push_back(c.GetTypeByPath({"addr:interpolation"}));
  m_odd = c.GetTypeByPath({"addr:interpolation", "odd"});
  m_even = c.GetTypeByPath({"addr:interpolation", "even"});
}


uint64_t GetDefPopulation(LocalityType localityType)
{
  switch (localityType)
  {
  case LocalityType::Country: return 500000;
  case LocalityType::State: return 100000;
  case LocalityType::City: return 50000;
  case LocalityType::Town: return 10000;
  case LocalityType::Village: return 100;
  default: return 0;
  }
}

uint64_t GetPopulation(FeatureType & ft)
{
  uint64_t const p = ft.GetPopulation();
  return (p < 10 ? GetDefPopulation(IsLocalityChecker::Instance().GetType(ft)) : p);
}

double GetRadiusByPopulation(uint64_t p)
{
  return pow(static_cast<double>(p), 1 / 3.6) * 550.0;
}

// Look to: https://confluence.mail.ru/pages/viewpage.action?pageId=287950469
// for details about factors.
// Shortly, we assume: radius = (population ^ (1 / base)) * mult
// We knew area info about some cities|towns|villages and did grid search.
// Interval for base: [0.1, 100].
// Interval for mult: [10, 1000].
double GetRadiusByPopulationForRouting(uint64_t p, LocalityType localityType)
{
  switch (localityType)
  {
  case LocalityType::City: return pow(static_cast<double>(p), 1.0 / 2.5) * 34.0;
  case LocalityType::Town: return pow(static_cast<double>(p), 1.0 / 6.8) * 354.0;
  case LocalityType::Village: return pow(static_cast<double>(p), 1.0 / 15.1) * 610.0;
  default: UNREACHABLE();
  }
}

uint64_t GetPopulationByRadius(double r)
{
  return base::SignedRound(pow(r / 550.0, 3.6));
}

}  // namespace ftypes
