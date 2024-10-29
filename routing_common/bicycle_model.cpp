#include "routing_common/bicycle_model.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"

namespace bicycle_model
{
using namespace routing;

// See model specifics in different countries here:
//   https://wiki.openstreetmap.org/wiki/OSM_tags_for_routing/Access-Restrictions
// Document contains proposals for some countries, but we assume that some kinds of roads are ready for bicycle routing,
// but not listed in tables in the document. For example, steps are not listed, paths, roads and services features also
// can be treated as ready for bicycle routing. These road types were added to lists below.

// See road types here:
//   https://wiki.openstreetmap.org/wiki/Key:highway

// Heuristics:
// For less bicycle roads we add fine by setting smaller value of weight speed, and for more bicycle roads we
// set greater values of weight speed. Algorithm picks roads with greater weight speed first,
// preferencing a more bicycle roads over less bicycle.
// As result of such heuristic road is not totally the shortest, but it avoids non bicycle roads, which were
// not marked as "hwtag=nobicycle" in OSM.

HighwayBasedFactors const kDefaultFactors = GetOneFactorsForBicycleAndPedestrianModel();

SpeedKMpH constexpr kSpeedOffroadKMpH = {1.5 /* weight */, 3.0 /* eta */};
SpeedKMpH constexpr kSpeedDismountKMpH = {2.0 /* weight */, 4.0 /* eta */};
// Applies only to contries where cycling is allowed on footways (by default the above dismount speed is used).
SpeedKMpH constexpr kSpeedOnFootwayKMpH = {8.0 /* weight */, 10.0 /* eta */};

HighwayBasedSpeeds const kDefaultSpeeds = {
    // {highway class : InOutCitySpeedKMpH(in city(weight, eta), out city(weight eta))}
    // Note that roads with hwtag=yesbicycle get high speed of 0.9 * Cycleway.
    /// @see Russia_UseTrunk test for Trunk weights.
    {HighwayType::HighwayTrunk, InOutCitySpeedKMpH(SpeedKMpH(7.0, 17.0), SpeedKMpH(9.0, 19.0))},
    // Presence of link roads usually means that connected roads are high traffic.
    // And complex intersections themselves are not nice for cyclists. We can't
    // easily extrapolate this to the main roads, but at least penalize the link roads a bit.
    // https://github.com/organicmaps/organicmaps/pull/9692#discussion_r1851442568
    {HighwayType::HighwayTrunkLink, InOutCitySpeedKMpH(SpeedKMpH(6.0, 17.0), SpeedKMpH(8.0, 19.0))},
    {HighwayType::HighwayPrimary, InOutCitySpeedKMpH(SpeedKMpH(10.0, 17.0), SpeedKMpH(12.0, 19.0))},
    {HighwayType::HighwayPrimaryLink, InOutCitySpeedKMpH(SpeedKMpH(8.0, 17.0), SpeedKMpH(11.0, 19.0))},
    {HighwayType::HighwaySecondary, InOutCitySpeedKMpH(SpeedKMpH(13.0, 17.0), SpeedKMpH(15.0, 19.0))},
    {HighwayType::HighwaySecondaryLink, InOutCitySpeedKMpH(SpeedKMpH(11.0, 17.0), SpeedKMpH(13.0, 19.0))},
    {HighwayType::HighwayTertiary, InOutCitySpeedKMpH(SpeedKMpH(14.0, 17.0), SpeedKMpH(17.0, 19.0))},
    {HighwayType::HighwayTertiaryLink, InOutCitySpeedKMpH(SpeedKMpH(13.0, 17.0), SpeedKMpH(16.0, 19.0))},
    {HighwayType::HighwayUnclassified, InOutCitySpeedKMpH(SpeedKMpH(13.0, 17.0), SpeedKMpH(15.0, 19.0))},
    {HighwayType::HighwayResidential, InOutCitySpeedKMpH(SpeedKMpH(12.0, 14.0), SpeedKMpH(14.0, 17.0))},
    {HighwayType::HighwayService, InOutCitySpeedKMpH(SpeedKMpH(13.0, 15.0), SpeedKMpH(15.0, 17.0))},
    {HighwayType::HighwayRoad, InOutCitySpeedKMpH(SpeedKMpH(11.0, 15.0), SpeedKMpH(14.0, 17.0))},

    {HighwayType::HighwayTrack, InOutCitySpeedKMpH(SpeedKMpH(8.0, 12.0), SpeedKMpH(10.0, 14.0))},
    {HighwayType::HighwayPath, InOutCitySpeedKMpH(SpeedKMpH(6.0, 10.0), SpeedKMpH(7.0, 12.0))},
    {HighwayType::HighwayBridleway, InOutCitySpeedKMpH(SpeedKMpH(4.0, 10.0), SpeedKMpH(5.0, 12.0))},

    {HighwayType::HighwayCycleway, InOutCitySpeedKMpH(SpeedKMpH(21.0, 18.0), SpeedKMpH(23.0, 20.0))},
    {HighwayType::HighwayLivingStreet, InOutCitySpeedKMpH(SpeedKMpH(12.0, 10.0), SpeedKMpH(14.0, 12.0))},
    // Steps have obvious inconvenience of a bike in hands.
    {HighwayType::HighwaySteps, InOutCitySpeedKMpH(SpeedKMpH(1.0, 1.0))},
    {HighwayType::HighwayPedestrian, InOutCitySpeedKMpH(kSpeedDismountKMpH)},
    {HighwayType::HighwayFootway, InOutCitySpeedKMpH(kSpeedDismountKMpH)},
    {HighwayType::ManMadePier, InOutCitySpeedKMpH(kSpeedOnFootwayKMpH)},
    /// @todo A car ferry has {10, 10}. Weight = 9 is 60% from reasonable 15 average speed.
    {HighwayType::RouteFerry, InOutCitySpeedKMpH(SpeedKMpH(9.0, 20.0))},
};

// Default, no bridleway.
VehicleModel::LimitsInitList const kDefaultOptions = {
    // {HighwayType, passThroughAllowed}
    {HighwayType::HighwayTrunk, true},
    {HighwayType::HighwayTrunkLink, true},
    {HighwayType::HighwayPrimary, true},
    {HighwayType::HighwayPrimaryLink, true},
    {HighwayType::HighwaySecondary, true},
    {HighwayType::HighwaySecondaryLink, true},
    {HighwayType::HighwayTertiary, true},
    {HighwayType::HighwayTertiaryLink, true},
    {HighwayType::HighwayService, true},
    {HighwayType::HighwayUnclassified, true},
    {HighwayType::HighwayRoad, true},
    {HighwayType::HighwayTrack, true},
    {HighwayType::HighwayPath, true},
    // HighwayBridleway is missing
    {HighwayType::HighwayCycleway, true},
    {HighwayType::HighwayResidential, true},
    {HighwayType::HighwayLivingStreet, true},
    {HighwayType::HighwaySteps, true},
    {HighwayType::HighwayPedestrian, true},
    {HighwayType::HighwayFootway, true},
    {HighwayType::ManMadePier, true},
    {HighwayType::RouteFerry, true}
};

// Same as defaults except trunk and trunk_link are not allowed
VehicleModel::LimitsInitList NoTrunk()
{
  VehicleModel::LimitsInitList res;
  res.reserve(kDefaultOptions.size() - 2);
  for (auto const & e : kDefaultOptions)
  {
    if (e.m_type != HighwayType::HighwayTrunk && e.m_type != HighwayType::HighwayTrunkLink)
      res.push_back(e);
  }
  return res;
}

// Same as defaults except pedestrian is allowed
HighwayBasedSpeeds NormalPedestrianSpeed()
{
  HighwayBasedSpeeds res = kDefaultSpeeds;
  res.Replace(HighwayType::HighwayPedestrian, InOutCitySpeedKMpH(kSpeedOnFootwayKMpH));
  return res;
}

// Same as defaults except bridleway is allowed
VehicleModel::LimitsInitList AllAllowed()
{
  auto res = kDefaultOptions;
  res.push_back({HighwayType::HighwayBridleway, true});
  return res;
}

// Same as defaults except pedestrian and footway are allowed
HighwayBasedSpeeds NormalPedestrianAndFootwaySpeed()
{
  HighwayBasedSpeeds res = kDefaultSpeeds;
  InOutCitySpeedKMpH const footSpeed(kSpeedOnFootwayKMpH);
  res.Replace(HighwayType::HighwayPedestrian, footSpeed);
  res.Replace(HighwayType::HighwayFootway, footSpeed);
  return res;
}

HighwayBasedSpeeds DismountPathSpeed()
{
  HighwayBasedSpeeds res = kDefaultSpeeds;
  res.Replace(HighwayType::HighwayPath, InOutCitySpeedKMpH(kSpeedDismountKMpH));
  return res;
}

HighwayBasedSpeeds PreferFootwaysToRoads()
{
  HighwayBasedSpeeds res = kDefaultSpeeds;

  // Decrease secondary/tertiary weight speed (-20% from default).
  InOutCitySpeedKMpH roadSpeed = InOutCitySpeedKMpH(SpeedKMpH(11.0, 17.0), SpeedKMpH(16.0, 19.0));
  res.Replace(HighwayType::HighwaySecondary, roadSpeed);
  res.Replace(HighwayType::HighwaySecondaryLink, roadSpeed);
  res.Replace(HighwayType::HighwayTertiary, roadSpeed);
  res.Replace(HighwayType::HighwayTertiaryLink, roadSpeed);

  // Increase footway speed to make bigger than other roads (+20% from default roads).
  InOutCitySpeedKMpH footSpeed = InOutCitySpeedKMpH(SpeedKMpH(17.0, 12.0), SpeedKMpH(20.0, 15.0));
  res.Replace(HighwayType::HighwayPedestrian, footSpeed);
  res.Replace(HighwayType::HighwayFootway, footSpeed);

  return res;
}

// No trunk, No pass through living_street and service
VehicleModel::LimitsInitList UkraineOptions()
{
  auto res = NoTrunk();
  for (auto & e : res)
  {
    if (e.m_type == HighwayType::HighwayLivingStreet || e.m_type == HighwayType::HighwayService)
      e.m_isPassThroughAllowed = false;
  }
  return res;
}

VehicleModel::SurfaceInitList const kBicycleSurface = {
  // {{surfaceType}, {weightFactor, etaFactor}}
  {{"psurface", "paved_good"}, {1.0, 1.0}},
  {{"psurface", "paved_bad"}, {0.8, 0.8}},
  {{"psurface", "unpaved_good"}, {0.9, 0.9}},
  {{"psurface", "unpaved_bad"}, {0.3, 0.3}},
  // no dedicated cycleway, doesn't mean that bicycle is not allowed, just lower weight
  // But why? If nocycleway is tagged explicitly means there is no cycling infra for sure.
  // Otherwise there is a small chance cycling infra is present though not mapped?
  /// @todo(pastk): this heuristic is controversial, maybe remove completely?
  {{"hwtag", "nocycleway"}, {0.95, 0.95}},
};
}  // namespace bicycle_model

namespace routing
{
BicycleModel::BicycleModel()
  : BicycleModel(bicycle_model::kDefaultOptions)
{
}

BicycleModel::BicycleModel(VehicleModel::LimitsInitList const & limits)
  : BicycleModel(limits, bicycle_model::kDefaultSpeeds)
{
}

BicycleModel::BicycleModel(VehicleModel::LimitsInitList const & limits, HighwayBasedSpeeds const & speeds)
  : VehicleModel(classif(), limits, bicycle_model::kBicycleSurface, {speeds, bicycle_model::kDefaultFactors})
{
  using namespace bicycle_model;

  // No bridleway in default.
  ASSERT_EQUAL(kDefaultOptions.size(), kDefaultSpeeds.size() - 1, ());

  std::vector<std::string> hwtagYesBicycle = {"hwtag", "yesbicycle"};

  auto const & cl = classif();
  m_noType = cl.GetTypeByPath({"hwtag", "nobicycle"});
  m_yesType = cl.GetTypeByPath(hwtagYesBicycle);
  m_bidirBicycleType = cl.GetTypeByPath({"hwtag", "bidir_bicycle"});
  m_onedirBicycleType = cl.GetTypeByPath({"hwtag", "onedir_bicycle"});

  // Assign 90% of max cycleway speed for bicycle=yes to keep choosing most preferred cycleway.
  auto const yesSpeed = kDefaultSpeeds.Get(HighwayType::HighwayCycleway).m_inCity * 0.9;
  AddAdditionalRoadTypes(cl, {{ std::move(hwtagYesBicycle), InOutCitySpeedKMpH(yesSpeed) }});

  // Update max speed with possible ferry transfer and bicycle speed downhill.
  // See EdgeEstimator::CalcHeuristic, GetBicycleClimbPenalty.
  SpeedKMpH constexpr kMaxBicycleSpeedKMpH(100.0);
  CHECK_LESS(m_maxModelSpeed, kMaxBicycleSpeedKMpH, ());
  m_maxModelSpeed = kMaxBicycleSpeedKMpH;
}

bool BicycleModel::IsBicycleBidir(feature::TypesHolder const & types) const
{
  return types.Has(m_bidirBicycleType);
}

bool BicycleModel::IsBicycleOnedir(feature::TypesHolder const & types) const
{
  return types.Has(m_onedirBicycleType);
}

SpeedKMpH BicycleModel::GetSpeed(FeatureTypes const & types, SpeedParams const & speedParams) const
{
  return GetTypeSpeedImpl(types, speedParams, false /* isCar */);
}

bool BicycleModel::IsOneWay(FeatureTypes const & types) const
{
  if (IsBicycleOnedir(types))
    return true;

  if (IsBicycleBidir(types))
    return false;

  return VehicleModel::IsOneWay(types);
}

SpeedKMpH const & BicycleModel::GetOffroadSpeed() const { return bicycle_model::kSpeedOffroadKMpH; }

// If one of feature types will be disabled for bicycles, features of this type will be simplified
// in generator. Look FeatureBuilder1::IsRoad() for more details.
// static
BicycleModel const & BicycleModel::AllLimitsInstance()
{
  static BicycleModel const instance(bicycle_model::AllAllowed(), bicycle_model::NormalPedestrianAndFootwaySpeed());
  return instance;
}

// static
SpeedKMpH BicycleModel::DismountSpeed()
{
  return bicycle_model::kSpeedDismountKMpH;
}

BicycleModelFactory::BicycleModelFactory(
    CountryParentNameGetterFn const & countryParentNameGetterFn)
  : VehicleModelFactory(countryParentNameGetterFn)
{
  using namespace bicycle_model;
  using std::make_shared;

  // Names must be the same with country names from countries.txt
  m_models[""] = make_shared<BicycleModel>(kDefaultOptions);

  m_models["Australia"] = make_shared<BicycleModel>(AllAllowed(), NormalPedestrianAndFootwaySpeed());
  m_models["Austria"] = make_shared<BicycleModel>(NoTrunk(), DismountPathSpeed());
  // Belarus law demands to use footways for bicycles where possible.
  m_models["Belarus"] = make_shared<BicycleModel>(kDefaultOptions, PreferFootwaysToRoads());
  m_models["Belgium"] = make_shared<BicycleModel>(NoTrunk(), NormalPedestrianSpeed());
  m_models["Brazil"] = make_shared<BicycleModel>(AllAllowed());
  m_models["Denmark"] = make_shared<BicycleModel>(NoTrunk());
  m_models["France"] = make_shared<BicycleModel>(NoTrunk(), NormalPedestrianSpeed());
  m_models["Finland"] = make_shared<BicycleModel>(kDefaultOptions, NormalPedestrianSpeed());
  m_models["Hungary"] = make_shared<BicycleModel>(NoTrunk());
  m_models["Iceland"] = make_shared<BicycleModel>(AllAllowed(), NormalPedestrianAndFootwaySpeed());
  m_models["Ireland"] = make_shared<BicycleModel>(AllAllowed());
  m_models["Italy"] = make_shared<BicycleModel>(kDefaultOptions, NormalPedestrianSpeed());
  m_models["Netherlands"] = make_shared<BicycleModel>(NoTrunk());
  m_models["Norway"] = make_shared<BicycleModel>(AllAllowed(), NormalPedestrianAndFootwaySpeed());
  m_models["Oman"] = make_shared<BicycleModel>(AllAllowed());
  m_models["Philippines"] = make_shared<BicycleModel>(AllAllowed(), NormalPedestrianSpeed());
  m_models["Poland"] = make_shared<BicycleModel>(NoTrunk());
  m_models["Romania"] = make_shared<BicycleModel>(AllAllowed());
  // Note. Despite the fact that according to https://wiki.openstreetmap.org/wiki/OSM_tags_for_routing/Access-Restrictions
  // passing through service and living_street with a bicycle is prohibited it's allowed according to Russian traffic rules.
  m_models["Russian Federation"] = make_shared<BicycleModel>(kDefaultOptions, NormalPedestrianAndFootwaySpeed());
  m_models["Slovakia"] = make_shared<BicycleModel>(NoTrunk());
  m_models["Spain"] = make_shared<BicycleModel>(NoTrunk(), NormalPedestrianSpeed());
  m_models["Sweden"] = make_shared<BicycleModel>(kDefaultOptions, NormalPedestrianSpeed());
  m_models["Switzerland"] = make_shared<BicycleModel>(NoTrunk(), NormalPedestrianAndFootwaySpeed());
  m_models["Ukraine"] = make_shared<BicycleModel>(UkraineOptions());
  m_models["United Kingdom"] = make_shared<BicycleModel>(AllAllowed());
  m_models["United States of America"] = make_shared<BicycleModel>(AllAllowed(), NormalPedestrianSpeed());
}
}  // routing
