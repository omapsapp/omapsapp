#include "testing/testing.hpp"

#include "routing/routing_callbacks.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "geometry/mercator.hpp"


namespace bicycle_route_test
{
using namespace integration;
using namespace routing;
using namespace routing::turns;

UNIT_TEST(RussiaMoscowSevTushinoParkPreferingBicycleWay)
{
  // Prefer a good quality dedicated cycleway over bad quality path + footway.
  /// @todo: replace with a better case that prefers a longer cycleway to e.g. shorter track of same quality.
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(55.87445, 37.43711), {0., 0.},
      mercator::FromLatLon(55.87203, 37.44274), 460.0);
}

UNIT_TEST(RussiaMoscowNahimovskyLongRoute)
{
  // Get onto a secondary and follow it. Same as GraphHopper.
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(55.66151, 37.63320), {0., 0.},
      mercator::FromLatLon(55.67695, 37.56220), 5670.0);
}

UNIT_TEST(RussiaDomodedovoSteps)
{
  // Use the steps as the detour is way too long.
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(55.4403114, 37.7740223), {0., 0.},
      mercator::FromLatLon(55.439703, 37.7725059), 139.058);
}

UNIT_TEST(SwedenStockholmCyclewayPriority)
{
  /// @todo(pastk): DELETE: the cycleway is the shortest and the only obvious option here anyway, what's the value of this test?
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(59.33151, 18.09347), {0., 0.},
      mercator::FromLatLon(59.33052, 18.09391), 113.0);
}

UNIT_TEST(NetherlandsAmsterdamBicycleNo)
{
  // Snap start/finish point to the closest suitable road.
  // The closest road here has a bicycle=no tag so its ignored and the next closest cycleway is used.
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(52.32716, 5.05932), {0., 0.},
      mercator::FromLatLon(52.32587, 5.06121), 338.0);
}

UNIT_TEST(NetherlandsAmsterdamBicycleYes)
{
  // Test that a highway=unclassified gets a significant boost due to presence of bicycle=yes tag.
  /// @todo(pastk): it shouldn't as there is no cycling infra (cycleway:both=no https://www.openstreetmap.org/way/214196820)
  /// and bicycle=yes means "its legal", not "its fast", see https://github.com/organicmaps/organicmaps/issues/9593
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Bicycle),
                     mercator::FromLatLon(52.32872, 5.07527), {0.0, 0.0},
                     mercator::FromLatLon(52.33853, 5.08941));

  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());
  TestRouteTime(*routeResult.first, 284.38);
}

UNIT_TEST(Netherlands_Amsterdam_OnewaySt_CyclewayOpposite)
{
  // Bicycles can go against the car traffic on oneway=yes roads if there is a cycleway=opposite tag.
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(52.37571, 4.88591), {0., 0.},
      mercator::FromLatLon(52.37736, 4.88744), 212.8);
}

UNIT_TEST(RussiaMoscowKashirskoe16ToCapLongRoute)
{
  // There is no dedicated bicycle infra, except short end part. All OSM routers give different results,
  // OM yields a short and logical route shortcutting via some service roads and footways (allowed in Russia).
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(55.66230, 37.63214), {0., 0.},
      mercator::FromLatLon(55.68927, 37.70356), 6968.64);
}

UNIT_TEST(RussiaMoscowServicePassThrough)
{
  // Passing through living_street and service is allowed in Russia.
  TRouteResult route =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Bicycle),
                                  mercator::FromLatLon(55.79649, 37.53738), {0., 0.},
                                  mercator::FromLatLon(55.79618, 37.54071));
  TEST_EQUAL(route.second, RouterResultCode::NoError, ());
}

UNIT_TEST(RussiaKerchStraitFerryRoute)
{
  // Use a cross-mwm ferry.
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(45.4167, 36.7658), {0.0, 0.0},
      mercator::FromLatLon(45.3653, 36.6161), 17151.4);
}

UNIT_TEST(SwedenStockholmBicyclePastFerry)
{
  // Several consecutive ferries.
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(59.4725, 18.51355), {0.0, 0.0},
      mercator::FromLatLon(59.42533, 18.35991), 14338.0);
}

UNIT_TEST(CrossMwmKaliningradRegionToLiepaja)
{
  // A cross mwm route (3 regions). Includes a ferry.
  integration::CalculateRouteAndTestRouteLength(
      integration::GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(54.63519, 21.80749), {0., 0.},
      mercator::FromLatLon(56.51119, 21.01847), 266992);
}

UNIT_TEST(Lithuania_Avoid_Ferry_Long_Route)
{
  // Avoid ferry Dreverna-Juodkrantė-Klaipėda.
  RoutingOptionSetter optionsGuard(RoutingOptions::Ferry);

  // GraphHopper makes a detour (via unpaved), OSRM goes straight with highway=primary,
  // Valhalla uses a part of the primary. A user says that GraphHopper is the best option.
  // https://www.openstreetmap.org/directions?engine=graphhopper_bicycle&route=55.340%2C21.459%3B55.715%2C21.135
  // OM uses a much shorter (56km vs 64km) route with bicycle=yes tracks and a short path section.
  /// @todo(pastk): the route goes through a landuse=military briefly and OM doesn't account for that.
  /// And too much preference is given to bicycle=yes track, see https://github.com/organicmaps/organicmaps/issues/9593
  integration::CalculateRouteAndTestRouteLength(
      integration::GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(55.3405073, 21.4595925), {0., 0.},
      mercator::FromLatLon(55.7140174, 21.1365445), 56243.2);
}

UNIT_TEST(SpainTenerifeAdejeVilaflor)
{
  // Test on riding up from Adeje (sea level) to Vilaflor (altitude 1400 meters).
  // A long ETA due to going uphill.
  TRouteResult const res = CalculateRoute(GetVehicleComponents(VehicleType::Bicycle),
                              mercator::FromLatLon(28.11984, -16.72592), {0., 0.},
                              mercator::FromLatLon(28.15865, -16.63704));
  TEST_EQUAL(res.second, RouterResultCode::NoError, ());

  TestRouteLength(*res.first, 26401);
  TestRouteTime(*res.first, 10716);
}

UNIT_TEST(SpainTenerifeVilaflorAdeje)
{
  // Test on riding down from Vilaflor (altitude 1400 meters) to Adeje (sea level).
  // A short ETA going downhill.
  TRouteResult const res = CalculateRoute(GetVehicleComponents(VehicleType::Bicycle),
                              mercator::FromLatLon(28.15865, -16.63704), {0., 0.},
                              mercator::FromLatLon(28.11984, -16.72592));
  TEST_EQUAL(res.second, RouterResultCode::NoError, ());

  TestRouteLength(*res.first, 24582);
  TestRouteTime(*res.first, 4459);
}

// Two tests on not building route against traffic on road with oneway:bicycle=yes.
UNIT_TEST(Munich_OnewayBicycle1)
{
  /// @todo Should combine TurnSlightLeft, TurnLeft, TurnLeft into UTurnLeft?
  integration::CalculateRouteAndTestRouteLength(
      integration::GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(48.1601673, 11.5630245), {0.0, 0.0},
      mercator::FromLatLon(48.1606349, 11.5631699), 264.042 /* expectedRouteMeters */);
}

UNIT_TEST(Munich_OnewayBicycle2)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(48.17819, 11.57286), {0.0, 0.0},
      mercator::FromLatLon(48.17867, 11.57303), 201.532 /* expectedRouteMeters */);
}

UNIT_TEST(London_GreenwichTunnel)
{
  // Use the bicycle=dismount foot tunnel https://www.openstreetmap.org/way/4358990
  // as a detour is way too long.
  // https://github.com/organicmaps/organicmaps/issues/8028

  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(51.4817397, -0.0100070258), {0.0, 0.0},
      mercator::FromLatLon(51.4883739, -0.00809729298), 1183.12 /* expectedRouteMeters */);
}

UNIT_TEST(Batumi_AvoidServiceDetour)
{
  // Go straight via a residential without a short detour via service road.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(41.6380014, 41.6269446), {0.0, 0.0},
      mercator::FromLatLon(41.6392113, 41.6260084), 160.157 /* expectedRouteMeters */);
}

UNIT_TEST(Gdansk_AvoidLongCyclewayDetour)
{
  /// @todo(pastk): DELETE: the preferred route goes by a shared cycleway also - replace with a better case (the next one!)
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(54.2632738, 18.6771661), {0.0, 0.0},
      mercator::FromLatLon(54.2698882, 18.6765837), 760.749 /* expectedRouteMeters */);
}

UNIT_TEST(Belarus_StraightFootway)
{
  // Prefer footways over roads in Belarus due to local laws.
  // https://github.com/organicmaps/organicmaps/issues/4145
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(53.8670285, 30.3162749), {0.0, 0.0},
      mercator::FromLatLon(53.876436, 30.3348084), 1613.34 /* expectedRouteMeters */);
}

UNIT_TEST(Spain_Madrid_ElevationsDetour)
{
  /// @todo(pastk): DELETE: there are no detours here atm, the second route is just a tad longer
  /// and is of the same alt difference, so this test is currently useless
  TRouteResult const r1 = CalculateRoute(GetVehicleComponents(VehicleType::Bicycle),
                              mercator::FromLatLon(40.459616, -3.690031), {0., 0.},
                              mercator::FromLatLon(40.4408171, -3.69261893));
  TEST_EQUAL(r1.second, RouterResultCode::NoError, ());

  TRouteResult const r2 = CalculateRoute(GetVehicleComponents(VehicleType::Bicycle),
                              mercator::FromLatLon(40.459616, -3.690031), {0., 0.},
                              mercator::FromLatLon(40.4403523, -3.69267444));
  TEST_EQUAL(r2.second, RouterResultCode::NoError, ());

  TEST(r1.first && r2.first, ());

  TEST_LESS(r1.first->GetTotalDistanceMeters(), r2.first->GetTotalDistanceMeters(), ());
  TEST_LESS(r1.first->GetTotalTimeSec(), r2.first->GetTotalTimeSec(), ());
}

UNIT_TEST(Spain_Zaragoza_Fancy_NoBicycle_Crossings)
{
  // A highway=crossing node https://www.openstreetmap.org/node/258776322 on the way
  // has bicycle=no, which doesn't prevent routing along this road.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(41.6523561, -0.881151311), {0.0, 0.0},
      mercator::FromLatLon(41.6476614, -0.885694674), 649.855 /* expectedRouteMeters */);
}

UNIT_TEST(Germany_Use_Bicycle_Track)
{
  // Avoid primary and prefer smaller roads and tracks with bicycle=yes.
  /// @todo Still prefers a no-cycling-infra but bicycle=yes secondary rather than a paved track,
  /// see https://github.com/organicmaps/organicmaps/issues/1201#issuecomment-946042937
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(48.420723, 9.90350146), {0.0, 0.0},
      mercator::FromLatLon(48.4080367, 9.86597073), 3778.41 /* expectedRouteMeters */);
}

UNIT_TEST(Finland_Use_Tertiary_LowTraffic)
{
  // Detour via a tertiary to avoid a secondary.
  // https://github.com/orgs/organicmaps/discussions/5158#discussioncomment-5938807
  /// @todo(pastk): prefer roads with lower maxspeed.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(61.5445696, 23.9394003), {0.0, 0.0},
      mercator::FromLatLon(61.6153965, 23.876755), 9876.65 /* expectedRouteMeters */);
}

UNIT_TEST(Belarus_Stolbcy_Use_Unpaved)
{
  // Goes by a track, unpaved and paved streets and an unpaved_bad track in the end.
  // Closer as GraphHopper. Valhalla detours the unpaved street.
  // OSRM shortcuts through paths and a ford.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
      mercator::FromLatLon(53.471389, 26.7422186), {0.0, 0.0},
      mercator::FromLatLon(53.454082, 26.7560061), 4620.81 /* expectedRouteMeters */);
}

UNIT_TEST(Russia_UseTrunk)
{
  // Prefer riding via a straight trunk road instead of taking a long +30% detour via smaller roads.
  /// @todo(pastk): DELETE: This test is controversial as this route has sections with longer relative trunk-avoiding detours
  /// e.g. from 67.9651692,32.8685132 to 68.022093,32.9654391 its +40% longer via a teriary and its OK?
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
                                   mercator::FromLatLon(66.271, 33.048), {0.0, 0.0},
                                   mercator::FromLatLon(68.95, 33.045), 404262 /* expectedRouteMeters */);
}

UNIT_TEST(Netherlands_IgnoreCycleBarrier_WithoutAccess)
{
  // There is a barrier=cycle_barrier in the beginning of the route
  // and it doesn't affect routing as there is no explicit bicycle=no.
  // https://github.com/organicmaps/organicmaps/issues/3920
  /// @todo(pastk): such barrier should have a small penalty in routing
  /// as it slows down a cyclist.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
                                   mercator::FromLatLon(51.9960994, 5.67350176), {0.0, 0.0},
                                   mercator::FromLatLon(51.9996861, 5.67299133), 428.801);
}

UNIT_TEST(UK_ForbidGates_WithoutAccess)
{
  // A barrier=gate without explicit access leads to a long detour.
  // https://www.openstreetmap.org/node/6993853766
  /// @todo OSRM/Valhalla/GraphHopper ignore such gates,
  /// see https://www.openstreetmap.org/directions?engine=fossgis_valhalla_bicycle&route=51.3579329%2C-2.3137701%3B51.3574666%2C-2.3152644
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
                                   mercator::FromLatLon(51.3579329, -2.3137701), {0.0, 0.0},
                                   mercator::FromLatLon(51.3574666, -2.31526436), 1149.28);
}

UNIT_TEST(Australia_AvoidConstruction)
{
  // Will not work when the bridge will be finished.
  // https://github.com/organicmaps/organicmaps/issues/7257
  TRouteResult const res = CalculateRoute(GetVehicleComponents(VehicleType::Bicycle),
                                   mercator::FromLatLon(-27.4724942, 153.030171), {0.0, 0.0},
                                   mercator::FromLatLon(-27.4706626, 153.035428));
  TEST_EQUAL(res.second, RouterResultCode::NoError, ());
  TEST_GREATER(res.first->GetTotalDistanceMeters(), 2950, ());
}

UNIT_TEST(UK_Canterbury_AvoidDismount)
{
  // A shortcut via a footway is 305 meters, ETAs are similar, but cyclists prefer to ride!
  // Check the London_GreenwichTunnel case for when dismounting is reasonable as the detour is way too long.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Bicycle),
                                   mercator::FromLatLon(51.2794435, 1.05627788), {0.0, 0.0},
                                   mercator::FromLatLon(51.2818863, 1.05725286), 976);
}

} // namespace bicycle_route_test
