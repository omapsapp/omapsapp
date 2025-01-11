#include "testing/testing.hpp"

#include "routing/routing_callbacks.hpp"
#include "routing/routing_options.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "geometry/mercator.hpp"

#include <limits>

namespace route_test
{
using namespace routing;
using namespace integration;
using mercator::FromLatLon;

UNIT_TEST(StrangeCaseInAfrica)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(19.20789, 30.50663), {0., 0.},
      FromLatLon(19.17289, 30.47315), 7645.0);
}

UNIT_TEST(MoscowKashirskoeShosseCrossing)
{
  // OSRM agrees here: https://www.openstreetmap.org/directions?engine=fossgis_osrm_car&route=55.66216%2C37.63259%3B55.66237%2C37.63560
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.66216, 37.63259), {0., 0.},
      FromLatLon(55.66237, 37.63560), 2877.81);
}

UNIT_TEST(MoscowToSVOAirport)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.75100, 37.61790), {0.0, 0.0},
      FromLatLon(55.97310, 37.41460), 36070.1);

  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.97310, 37.41460), {0.0, 0.0},
      FromLatLon(55.75100, 37.61790), 39129.8);
}

// Restrictions tests. Check restrictions generation, if there are any errors.
UNIT_TEST(RestrictionTestNeatBaumanAndTTK)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.77398, 37.68469), {0., 0.},
      FromLatLon(55.77201, 37.68789), 1032.);
}

UNIT_TEST(RestrictionTestNearMetroShodnenskaya)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.85043, 37.43824), {0., 0.},
      FromLatLon(55.85191, 37.43910), 525.601);
}

// Strange asserts near Cupertino test
UNIT_TEST(CaliforniaCupertinoFindPhantomAssertTest)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(37.33409, -122.03458), {0., 0.},
      FromLatLon(37.33498, -122.03575), 1471.96);
}

// Path in the last map through the other map.
UNIT_TEST(RussiaUfaToUstKatavTest)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(54.7304, 55.9554), {0., 0.},
      FromLatLon(54.9228, 58.1469), 160565);
}

UNIT_TEST(RussiaMoscowNoServiceCrossing)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.77787, 37.70405), {0., 0.},
      FromLatLon(55.77682, 37.70391), 3140.);
}

UNIT_TEST(RussiaMoscowShortWayToService)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.77787, 37.70405), {0., 0.},
      FromLatLon(55.77691, 37.70428), 171.);
}

UNIT_TEST(PriceIslandLoadCrossGeometryTest)
{
  size_t constexpr kExpectedPointsNumber = 56;
  // Forward
  TRouteResult route = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(46.16255, -63.81643), {0., 0.},
                                  FromLatLon(46.25401, -63.70213));
  TEST_EQUAL(route.second, RouterResultCode::NoError, ());
  TEST(route.first, ());
  TestRoutePointsNumber(*route.first, kExpectedPointsNumber);

  // And backward case
  route = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(46.25401, -63.70213), {0., 0.},
                                  FromLatLon(46.16255, -63.81643));
  TEST_EQUAL(route.second, RouterResultCode::NoError, ());
  TEST(route.first, ());
  TestRoutePointsNumber(*route.first, kExpectedPointsNumber);
}

UNIT_TEST(NederlandLeeuwardenToDenOeverTest)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(53.2076, 5.7082), {0., 0.},
      FromLatLon(52.9337, 5.0308), 59500.);
}

UNIT_TEST(RussiaMoscowGerPanfilovtsev22SolodchaPravdiRouteTest)
{
  // OSRM agrees here to use motorways instead of city roads.
  // https://www.openstreetmap.org/directions?engine=fossgis_osrm_car&route=55.858%2C37.410%3B54.794%2C39.837
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.85792, 37.40992), {0., 0.},
      FromLatLon(54.79390, 39.83656), 263920.);
}

UNIT_TEST(RussiaMoscowBelarusMinsk)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.750650, 37.617673), {0., 0.},
      FromLatLon(53.902114, 27.562020), 712649.0);
}

UNIT_TEST(UKRugbyStIvesRouteTest)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(52.37076, -1.26530), {0., 0.},
      FromLatLon(50.21480, -5.47994), 455902.);
}

UNIT_TEST(RussiaMoscow_ItalySienaCenter_SplittedMotorway)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.79690, 37.53759), {0., 0.},
      FromLatLon(43.32677, 11.32792), 2870710.);
}

UNIT_TEST(PeruSingleRoadTest)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(-14.22061, -73.35969), {0., 0.},
      FromLatLon(-14.22389, -73.44281), 15900.);
}

UNIT_TEST(RussiaMoscowFranceParisCenterRouteTest)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.75271, 37.62618), {0., 0.}, FromLatLon(48.86123, 2.34129), 2840940.);
}

UNIT_TEST(EnglandToFranceRouteLeMansTest)
{
  TRouteResult const res = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                          FromLatLon(51.09276, 1.11369), {0., 0.},
                                          FromLatLon(50.93317, 1.82737));

  TestRouteLength(*res.first, 63877.4);
  // LeMans shuttle duration is 35 min.
  TEST_LESS(res.first->GetTotalTimeSec(), 3200, ());
}

UNIT_TEST(RussiaMoscowRegionToBelarusBorder)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.464182, 35.943947), {0.0, 0.0},
      FromLatLon(52.442467, 31.609642), 554000.);
}

UNIT_TEST(GermanyToTallinCrossMwmRoute)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(48.397416, 16.515289), {0.0, 0.0},
      FromLatLon(59.437214, 24.745355), 1650000.);
}

UNIT_TEST(Russia_Moscow_Leningradskiy39RepublicOfSouthAfricaCapeTownCenterRouteTest)
{
  /// @todo Interesting numbers here
  /// - GraphHopper: 13703 km, 153 h
  /// - Google: 15289 km, 198 h (via Europe?!)
  /// - OM: 14486 km, 185 h
  /// - OSRM, Valhalla are failed
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.79721, 37.53786), {0., 0.},
      FromLatLon(-33.9286, 18.41837), 14'493'000);
}

UNIT_TEST(AlbaniaToMontenegroCrossTest)
{
  // Road from Albania to Montenegro. Test turnaround finding at border (when start/stop
  // points are inside borders and one of segments has outside points).
  // Forward
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(42.01535, 19.40044), {0., 0.},
      FromLatLon(42.01201, 19.36286), 3749);

  // And backward case
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(42.01201, 19.36286), {0., 0.},
      FromLatLon(42.01535, 19.40044), 3753);
}

UNIT_TEST(CanadaBridgeCrossToEdwardIsland)
{
  // Forward
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(46.13418, -63.84656), {0., 0.},
      FromLatLon(46.26739, -63.63907), 23000.);

  // And backward case
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(46.26739, -63.63907), {0., 0.},
      FromLatLon(46.13418, -63.84656), 23000.);
}

UNIT_TEST(ParisCrossDestinationInForwardHeapCase)
{
  // Forward.
  // Updated after fixing primary/trunk factors. Route looks good, but it differs from OSRM/Valhalla/GraphHopper.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
      FromLatLon(49.85015, 2.24296), {0., 0.},
      FromLatLon(48.85458, 2.36291), 128749);

  // Backward.
  // OM makes the same as GraphHopper and Valhalla. OSRM makes a bit shorter route.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
      FromLatLon(48.85458, 2.36291), {0., 0.},
      FromLatLon(49.85027, 2.24283), 136653);
}

UNIT_TEST(RussiaSmolenskRussiaMoscowTimeTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(54.7998, 32.05489), {0., 0.},
                                  FromLatLon(55.753, 37.60169));

  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;
  TestRouteLength(route, 391659);

  // https://www.openstreetmap.org/directions?engine=graphhopper_car&route=54.800%2C32.055%3B55.753%2C37.602
  // Middle between GraphHopper and OSRM
  TestRouteTime(route, 18607.2);
}

UNIT_TEST(Russia_Moscow_Leningradskiy39GeroevPanfilovtsev22TimeTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(55.7971, 37.53804), {0., 0.},
                                  FromLatLon(55.8579, 37.40990));
  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;
  TestRouteLength(route, 14276.3);
  TestRouteTime(route, 1126.09);
}

UNIT_TEST(Russia_Moscow_Leningradskiy39GeroevPanfilovtsev22SubrouteTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(55.7971, 37.53804), {0., 0.},
                                  FromLatLon(55.8579, 37.40990));

  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;
  TEST_EQUAL(route.GetSubrouteCount(), 1, ());
  std::vector<RouteSegment> info;
  route.GetSubrouteInfo(0, info);
  TEST_EQUAL(route.GetPoly().GetSize(), info.size() + 1, ());
  size_t constexpr kExpectedPointsNumber = 335;
  TestRoutePointsNumber(route, kExpectedPointsNumber);
}

UNIT_TEST(USALosAnglesAriaTwentyninePalmsHighwayTimeTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(34.0739, -115.3212), {0.0, 0.0},
                                  FromLatLon(34.0928, -115.5930));
  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());
  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;
  TEST_LESS(route.GetTotalTimeSec(), std::numeric_limits<double >::max() / 2.0, ());
}

// Test on routing along features with tag man_made:pier.
UNIT_TEST(CanadaVictoriaVancouverTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(48.47831, -123.32749), {0.0, 0.0},
                                  FromLatLon(49.26242, -123.11553));
  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());
}

// Test on the route with the finish near zero length edge.
UNIT_TEST(BelarusSlonimFinishNearZeroEdgeTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(53.08279, 25.30036), {0.0, 0.0},
                                  FromLatLon(53.09443, 25.34356));
  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());
}

// Test on the route with the start near zero length edge.
UNIT_TEST(BelarusSlonimStartNearZeroEdgeTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(53.09422, 25.34411), {0.0, 0.0},
                                  FromLatLon(53.09271, 25.3467));
  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());
}

// Test on roads with tag maxspeed=none.
UNIT_TEST(GermanyBerlinMunichTimeTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(52.51172, 13.39468), {0., 0.},
                                  FromLatLon(48.13294, 11.60352));

  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;

  /// @todo New time is closer to GraphHopper timing, but still very optimistic. Compare maxspeed=none defaults.
  // https://www.openstreetmap.org/directions?engine=graphhopper_car&route=52.51172%2C13.39468%3B48.13294%2C11.60352
  TestRouteLength(route, 584960);
  TestRouteTime(route, 19173.3);
}

// Test on roads with tag route=shuttle_train. This train has defined maxspeed=100.
UNIT_TEST(GermanyShuttleTrainTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(54.78370, 8.83528), {0., 0.},
                                  FromLatLon(54.91681, 8.31346));

  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;
  TestRouteLength(route, 44517.4);
  TestRouteTime(route, 2619.62);
}

UNIT_TEST(TolyattiFeatureThatCrossSeveralMwmsTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(52.67316, 48.22478), {0., 0.},
                                  FromLatLon(53.49143, 49.52386));

  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;

  // GraphHopper and Valhalla agree here, but OSRM makes a short route via Syzran.
  TestRouteLength(route, 155734);
  TestRouteTime(route, 7958.85);
}

// Test on removing speed cameras from the route for maps from Jan 2019,
// and on the absence of speed cameras in maps for later maps for Switzerland.
UNIT_TEST(SwitzerlandNoSpeedCamerasInRouteTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(47.5194, 8.73093), {0., 0.},
                                  FromLatLon(46.80592, 7.13724));

  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;
  auto const & routeSegments = route.GetRouteSegments();
  for (auto const & routeSegment : routeSegments)
    TEST(routeSegment.GetSpeedCams().empty(), (routeSegment.GetSegment()));
}

// Test on warning about speed cameras for countries where speed cameras partly prohibited.
UNIT_TEST(GermanyWarningAboutSpeedCamerasTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(52.38465, 13.41906), {0., 0.},
                                  FromLatLon(52.67564, 13.27453));

  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;
  TEST(route.CrossMwmsPartlyProhibitedForSpeedCams(), ());
}

UNIT_TEST(Spain_RestirctionOnlyMany)
{
  // This relation https://www.openstreetmap.org/relation/7610329
  // See also Valhalla engine.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
      FromLatLon(43.38234, -5.67648), {0.0, 0.0},
      FromLatLon(43.38222, -5.69083), 8289.15);
}

UNIT_TEST(Russia_Moscow_RestirctionOnlyMany)
{
  // This relation https://www.openstreetmap.org/relation/581743
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.991986, 37.2131072), {0.0, 0.0},
      FromLatLon(55.9918083, 37.215531), 894.853);
}

// Test that fake segments are not built from start to roads with hwtag=nocar for car routing.
UNIT_TEST(SpainBilbaoAirportNoCarTest)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(43.29969, -2.91312), {0., 0.},
                                  FromLatLon(43.29904, -2.9108));
  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  Route const & route = *(routeResult.first);
  std::vector<RouteSegment> const & routeSegments = route.GetRouteSegments();
  TEST_GREATER(routeSegments.size(), 2, ());

  // Note. routeSegments[0] is a start segment(point). routeSegments[1] is a fake segment
  // which goes from the route start to a segment of the road graph.
  // routeSegments[1].GetDistFromBeginningMeters() is the length of the first fake segment.
  // Start point is located near a road with hwtag=no.
  // So if routeSegments[1].GetDistFromBeginningMeters() is long enough the segment
  // with hwtag=no is no used.
  TEST_GREATER(routeSegments[1].GetDistFromBeginningMeters(), 20.0, ());
}

// Test when start is located near mwm border. In that case it's possible that one of
// closest edges is a dead end within one mwm. The end of this dead end should
// be taken into account in |IndexGraphStarterJoints<Graph>::FindFirstJoints()|.
UNIT_TEST(EnglandLondonStartNearMwmBorderTest)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(51.603582, 0.266995), {0., 0.},
      FromLatLon(51.606785, 0.264055), 416.8);
}

// Test that toll road is not crossed by a fake edge if RouingOptions are set to Road::Toll.
// Test on necessity calling RectCoversPolyline() after DataSource::ForEachInRect() while looking for fake edges.
UNIT_TEST(RussiaMoscowNotCrossingTollRoadTest)
{
  auto & vehicleComponents = GetVehicleComponents(VehicleType::Car);

  auto const start = FromLatLon(55.93934, 37.406);
  m2::PointD finish[] = { FromLatLon(55.9414245, 37.4489627), FromLatLon(55.9421391, 37.4484832) };

  {
    // Avoid motorway toll road and build route through minor residential roads (short but slow).
    RoutingOptionSetter optionsGuard(RoutingOptions::Toll);

    // 1. End point is near the motorway toll road, but choose a minor track as end segment.
    CalculateRouteAndTestRouteLength(vehicleComponents, start, {0.0, 0.0}, finish[0], 8427.71);

    // 2. End point is near the service road via the motorway toll road, but choose a minor track as end segment.
    CalculateRouteAndTestRouteLength(vehicleComponents, start, {0.0, 0.0}, finish[1], 8361.27);
  }

  {
    // Normal route via the motorway toll road - long but fast (like Graphopper).
    // - 20595.4 is OK (Graphopper)
    // - 19203.7 is OK (OSRM)
    // - 21930.7 is OK (Valhalla)
    CalculateRouteAndTestRouteLength(vehicleComponents, start, {0.0, 0.0}, finish[0], 21930.7);
    CalculateRouteAndTestRouteLength(vehicleComponents, start, {0.0, 0.0}, finish[1], 22015.4);
  }
}

UNIT_TEST(NoCrash_RioGrandeCosmopolis)
{
  TRouteResult const route = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(-32.17641, -52.16350), {0., 0.},
                                  FromLatLon(-22.64374, -47.19720));

  TEST_EQUAL(route.second, RouterResultCode::NoError, ());
}

UNIT_TEST(AreMwmsNear_HelsinkiPiter)
{
  TRouteResult const route = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(60.87083, 26.53612), {0., 0.},
                                  FromLatLon(60.95360, 28.53979));

  TEST_EQUAL(route.second, RouterResultCode::NoError, ());
}

// Test RussiaShorterFakeEdges1 and RussiaShorterFakeEdges2 are on reducing
// |kSpeedOffroadKMpH| for car routing. This lets us make
// fake edges shorter that prevent crossing lakes, forests and so on.
UNIT_TEST(RussiaBlackLakeShorterFakeEdges1)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.62466, 39.71385), {0., 0.},
      FromLatLon(55.63114, 39.70979), 1469.54);
}

UNIT_TEST(RussiaShorterFakeEdges2)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.31103, 38.80954), {0., 0.},
      FromLatLon(55.31155, 38.8217), 2489.8);
}

// https://github.com/organicmaps/organicmaps/issues/1788
UNIT_TEST(Germany_ShortRouteWithPassThroughChanges)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(49.512076, 8.284476), {0., 0.},
      FromLatLon(49.523783, 8.288701), 2014.);
}

// https://github.com/organicmaps/organicmaps/issues/821
UNIT_TEST(Ukraine_UmanOdessa)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(48.7498, 30.2203), {0., 0.},
      FromLatLon(46.4859, 30.6837), 265163.);
}

// https://github.com/organicmaps/organicmaps/issues/1736
UNIT_TEST(Belgium_LiegeBrugge)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(50.645205, 5.573507), {0., 0.},
      FromLatLon(51.208479, 3.225558), 193436.);
}

// https://github.com/organicmaps/organicmaps/issues/1627
UNIT_TEST(Spain_MadridSevilla)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(40.415322, -3.703517), {0., 0.},
      FromLatLon(37.388667, -5.995355), 528667.);
}

UNIT_TEST(Belarus_Lithuania_MinskVilnius)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(53.902837, 27.562144), {0., 0.},
      FromLatLon(54.686821, 25.283189), 183231.);
}

// https://github.com/organicmaps/organicmaps/issues/338
UNIT_TEST(Russia_MendeleevoReutov)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(56.036866, 37.232630), {0., 0.},
      FromLatLon(55.762128, 37.856665), 66261.9);
}

// https://github.com/organicmaps/organicmaps/issues/1721
UNIT_TEST(Austria_Croatia_SalzburgZagreb)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(47.795928, 13.047597), {0., 0.},
      FromLatLon(45.812822, 15.977049), 414275);
}

// https://github.com/organicmaps/organicmaps/issues/1071
UNIT_TEST(Russia_MoscowDesnogorsk)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.715208, 37.396528), {0., 0.},
      FromLatLon(54.151853, 33.287128), 355887);
}

// https://github.com/organicmaps/organicmaps/issues/1271
UNIT_TEST(USA_DontLeaveHighway)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(34.1801345, -118.885005), {0., 0.},
      FromLatLon(34.1767471, -118.869327), 1523);
}

// https://github.com/organicmaps/organicmaps/issues/2085
UNIT_TEST(USA_NorthCarolina_CrossMWMs)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(35.6233244, -78.3917262), {0., 0.},
      FromLatLon(36.0081839, -81.5245347), 333425);
}

// https://github.com/organicmaps/organicmaps/issues/1565
UNIT_TEST(Cyprus_NoUTurnFromFake)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(34.70639, 33.1184951), {0., 0.},
      FromLatLon(34.7065239, 33.1184222), 384.238);

  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(34.7068976, 33.1199084), {0., 0.},
      FromLatLon(34.7070505, 33.1198391), 670.077);
}

UNIT_TEST(Crimea_UseGravelTertiary)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(45.362591, 36.471533), {0., 0.},
      FromLatLon(45.475055, 36.341766), 18815.6);

  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(45.470764, 36.331289), {0., 0.},
      FromLatLon(45.424964, 36.080336), 55220.2);
}

// https://github.com/organicmaps/organicmaps/issues/2475
UNIT_TEST(Spain_LinksJunction)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(38.8031, 0.0383), {0., 0.},
      FromLatLon(38.8228, 0.0357), 3479.63);
}

// https://github.com/organicmaps/organicmaps/issues/1773
UNIT_TEST(Netherlands_CrossMwm_A15)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(51.847656, 4.089189), {0., 0.},
      FromLatLon(51.651632, 4.725924), 70596.3);
}

// https://github.com/organicmaps/organicmaps/issues/2494
UNIT_TEST(Netherlands_CrossMwm_GoudaToApenheul)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(52.0181, 4.7111), {0., 0.},
      FromLatLon(52.2153, 5.9187), 103576);
}

// https://github.com/organicmaps/organicmaps/issues/2285
UNIT_TEST(Hawaii_KeepH1)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(21.277841, -157.779314), {0., 0.},
      FromLatLon(21.296098, -157.823823), 5289.31);
}

// https://github.com/organicmaps/organicmaps/issues/1668
UNIT_TEST(Russia_Moscow_KeepPrimary)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.7083688, 37.6213856), {0., 0.},
      FromLatLon(55.724623, 37.62588), 1921.88);
}

// https://github.com/organicmaps/organicmaps/issues/1727
// https://github.com/organicmaps/organicmaps/issues/2020
// https://github.com/organicmaps/organicmaps/issues/2057
UNIT_TEST(DontUseLinksWhenRidingOnMotorway)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(32.16881, 34.90656), {0., 0.},
      FromLatLon(32.1588823, 34.9330855), 2847.33);

  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(43.587808, 1.495385), {0., 0.},
      FromLatLon(43.600145, 1.490489), 1457.16);

  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(34.0175371, -84.3272339), {0., 0.},
      FromLatLon(34.0298011, -84.3182477), 1609.76);
}

UNIT_TEST(Russia_UseDonMotorway)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(54.5775321, 38.2206224), {0., 0.},
      FromLatLon(49.9315563, 40.5529881), 608031);
}

UNIT_TEST(Germany_Italy_Malcesine)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(48.4101446, 11.5892265), {0., 0.},
      FromLatLon(45.7662964, 10.8111554), 427135);

  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(45.7662964, 10.8111554), {0., 0.},
      FromLatLon(48.4101446, 11.5892265), 431341);

  /// @todo Again strange detour (near finish) on a long route.
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(50.8499365, 12.4662169), {0., 0.},
      FromLatLon(45.7662964, 10.8111554), 776000);
}

// https://github.com/organicmaps/organicmaps/issues/3363
UNIT_TEST(Belarus_UseP27_PastawyBraslaw)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Car),
      FromLatLon(55.1187744, 26.8460319), {0., 0.},
      FromLatLon(55.6190911, 27.0938092), 86239.8);
}

// https://github.com/organicmaps/organicmaps/issues/3257
UNIT_TEST(Turkey_AvoidMountainsSecondary)
{
  TRouteResult const res = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                              FromLatLon(41.0027, 27.6752), {0., 0.},
                              FromLatLon(40.6119, 27.1136));

  // GraphHopper and OSRM make a short route via the mountain secondary.
  // Valhalla makes a long route. I think it is correct.
  /// @todo Should "split" ways for better inCity/outCity classification. Now long ways are detected as outCity (wrong).
  TestRouteLength(*res.first, 100399);
  TestRouteTime(*res.first, 5319.01);
}

// https://github.com/organicmaps/organicmaps/issues/4110
UNIT_TEST(Slovenia_Croatia_CrossBorderPenalty)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
      FromLatLon(46.038579, 14.469414), {0., 0.},
      FromLatLon(45.22718, 13.596334), 156285);
}

UNIT_TEST(USA_Birmingham_AL_KeyWest_FL_NoMotorway)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
      FromLatLon(28.9666499, -82.127271), {0., 0.},
      FromLatLon(25.8633542, -80.3878891), 457734);

  /// @note These tests works good on release server, my desktop release skips MWM Florida_Orlando ...
  /// 15 vs 8 cross-mwm candidates.

  auto const start = FromLatLon(33.5209837, -86.807945);
  auto const finish = FromLatLon(24.5534713, -81.7932587);
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
      start, {0., 0.}, finish, 1'471'410);

  RoutingOptionSetter optionsGuard(RoutingOptions::Motorway);
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
      start, {0., 0.}, finish, 1'495'860);
}

UNIT_TEST(Turkey_Salarialaca_Sanliurfa)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                  FromLatLon(38.8244409, 34.0979749), {0., 0.},
                                  FromLatLon(37.159585, 38.7919353));

  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;
  TestRouteLength(route, 656891);
  // Should be less than 6 hours (6 * 3600), between Valhalla and GraphHopper.
  TestRouteTime(route, 20453.5);
}

// https://github.com/organicmaps/organicmaps/issues/4924
// https://github.com/organicmaps/organicmaps/issues/4996
UNIT_TEST(UK_MiniRoundabout)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                FromLatLon(50.4155631, -4.17201038), {0., 0.},
                FromLatLon(50.4161337, -4.17226314), 114.957);

  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                FromLatLon(50.4173675, -4.14913092), {0., 0.},
                FromLatLon(50.4170013, -4.1471226), 153.223);

  /// @todo Fancy case, changing start/end point a little and the route is OK. Also check m_exitNum.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                FromLatLon(51.5686491, -0.00590868183), {0., 0.},
                FromLatLon(51.5684408, -0.00596725822), 40);
}

// https://github.com/organicmaps/organicmaps/issues/5069
UNIT_TEST(Germany_Netherlands_AvoidLoops)
{
  // https://www.openstreetmap.org/directions?engine=fossgis_osrm_car&route=51.682%2C10.220%3B51.919%2C5.845

  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                                  FromLatLon(51.6823791, 10.2197113), {0., 0.},
                                                  FromLatLon(51.9187916, 5.8452563));

  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;
  TestRouteLength(route, 405058);
  TestRouteTime(route, 13965.2);
}

UNIT_TEST(Germany_Cologne_Croatia_Zagreb)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(50.924, 6.943), {0., 0.},
                                   FromLatLon(45.806, 15.963), 1074730);
}

UNIT_TEST(Finland_Avoid_CompactedUnclassified)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(61.4677, 24.0025), {0., 0.},
                                   FromLatLon(61.4577, 24.035), 3128.31);
}

// https://github.com/orgs/organicmaps/discussions/5158#discussioncomment-5938807
UNIT_TEST(Greece_Crete_Use_EO94)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(35.5170594, 24.0938699), {0., 0.},
                                   FromLatLon(35.5446109, 24.1312439), 6333.82);
}

UNIT_TEST(Bulgaria_Rosenovo_Dobrich)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(43.6650649, 27.7826578), {0., 0.},
                                   FromLatLon(43.5690961, 27.8307318), 16556.3);
}

UNIT_TEST(Russia_UseGravelPrimary_Not_DefaultTertiary)
{
  /// @todo Actually, tertiary should be tagged as surface=unpaved.
  /// There is an idea to detect and update route if we have leave-enter for the same ref (named) >= primary road:
  /// {80K-004, some tertiary, 80K-004} in a reasonable distance. This is a signal that "some tertiary"
  /// in a middle is an error.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(55.8466967, 57.303653), {0., 0.},
                                   FromLatLon(55.8260004, 57.0367732), 19910);
}

// https://github.com/organicmaps/organicmaps/issues/5695
UNIT_TEST(Russia_Yekaterinburg_NChelny)
{
  // Make sense without Chelyabinsk and Izhevsk. Thus we can check really fancy cases.
  // Otherwise, good routes will be through Perm-Izhevsk or Chelyabinsk-Ufa
  auto components = CreateAllMapsComponents(VehicleType::Car, {"Russia_Chelyabinsk Oblast", "Russia_Udmurt Republic"});

  auto const start = FromLatLon(56.8382242, 60.6308866);
  auto const finish = FromLatLon(55.7341111, 52.4156012);

  {
    RoutingOptionSetter optionsGuard(RoutingOptions::Dirty | RoutingOptions::Ferry);
    // forward
    CalculateRouteAndTestRouteLength(*components,
                                     start, {0., 0.}, finish, 767702);
    // backward
    CalculateRouteAndTestRouteLength(*components,
                                     finish, {0., 0.}, start, 766226);
  }

  // OSRM, GraphHopper uses gravel, Valhalla makes a route like above.

  /// @todo Should use tertiary + gravel + villages (46km) here and below instead of primary (86km)?
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(55.9315, 58.202), {0., 0.},
                                   FromLatLon(55.7555, 57.8348), 45788);
  // forward
  CalculateRouteAndTestRouteLength(*components,
                                   start, {0., 0.}, finish, 757109);
  // backward
  CalculateRouteAndTestRouteLength(*components,
                                   finish, {0., 0.}, start, 755851);
}

// https://github.com/organicmaps/organicmaps/issues/5695
UNIT_TEST(Russia_CrossMwm_Ferry)
{
  TRouteResult const routeResult = CalculateRoute(GetVehicleComponents(VehicleType::Car),
                                                  FromLatLon(55.7840398, 54.0815156), {0., 0.},
                                                  FromLatLon(55.7726245, 54.0752932));

  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  Route const & route = *routeResult.first;
  TestRouteLength(route, 1453);
  // 2 hours duration (https://www.openstreetmap.org/way/426120647) + 20 minutes ferry landing penalty.
  /// @todo Not working now, @see SingleVehicleWorldGraph::CalculateETA.
  TEST_GREATER(route.GetTotalTimeSec(), 7200 + 20 * 60, ());
}

// https://github.com/organicmaps/organicmaps/issues/6035
UNIT_TEST(Netherlands_CrossMwm_Ferry)
{
  /// @todo Should work after reducing ferry landing penalty, but nope ..
  /// Can't realize what is going on here, maybe penalty is aggregated 2 times?
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(52.3855418, 6.12969591), {0., 0.},
                                   FromLatLon(52.3924362, 6.12166998), 1322);
}

// https://github.com/organicmaps/organicmaps/issues/6278
UNIT_TEST(Turkey_PreferSecondary_NotResidential)
{
  /// @todo Now the app wrongly takes tertiary (no limits) vs primary/secondary (with maxspeed = 30).
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(41.0529, 28.9201), {0., 0.},
                                   FromLatLon(41.0731, 28.9407), 4783.85);
}

UNIT_TEST(Ireland_NorthernIreland_NoBorderPenalty)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(53.9909441, -7.36035861), {0., 0.},
                                   FromLatLon(54.9517424, -7.73625795), 138655);
}

UNIT_TEST(Israel_Jerusalem_Palestine_NoBorderPenalty)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(31.4694833, 35.394899), {0., 0.},
                                   FromLatLon(31.7776832, 35.2236876), 76133);
}

// https://github.com/organicmaps/organicmaps/issues/6510
UNIT_TEST(EqualMaxSpeeds_PreferPrimary_NotResidential)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(46.5239, 5.6187), {0., 0.},
                                   FromLatLon(46.5240, 5.6096), 1123);
}

// https://github.com/organicmaps/organicmaps/issues/3033#issuecomment-1798343531
UNIT_TEST(Spain_NoMaxSpeeds_KeepTrunk_NotTrunkLink)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(43.3773971, -3.43177355), {0., 0.},
                                   FromLatLon(43.3685773, -3.42580007), 1116.79);
}

// https://github.com/organicmaps/organicmaps/issues/8823
UNIT_TEST(LATAM_UsePrimary_NotTrunkDetour)
{
  // 10247 or less should be here.
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(4.737768, -74.077599), {0., 0.},
                                   FromLatLon(4.684999, -74.046393), 10247.3);

  /// @todo Still have the strange detour at the end. Due to the 20/30 km/h assignment for the primary_link.
  /// Looks like it is bad to assign maxspeed for _all_ connected links if it is defined for the middle one.
}

// https://github.com/organicmaps/organicmaps/issues/8729
// https://github.com/organicmaps/organicmaps/issues/8541
UNIT_TEST(USA_UseDirt_WithMaxspeed)
{
  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(46.5361985, -111.943183), {0., 0.},
                                   FromLatLon(46.4925409, -112.105446), 20906.5);

  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(46.7336967, -111.926), {0., 0.},
                                   FromLatLon(46.7467037, -111.917147), 3527.79);

  CalculateRouteAndTestRouteLength(GetVehicleComponents(VehicleType::Car),
                                   FromLatLon(42.3889581, 19.7812567), {0., 0.},
                                   FromLatLon(42.3878106, 19.7831402), 247.139);
}

} // namespace route_test
