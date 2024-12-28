#include "testing/testing.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "geometry/mercator.hpp"

namespace transit_route_test
{
using namespace routing;

UNIT_TEST(Transit_Moscow_CenterToKotelniki_CrossMwm)
{
  TRouteResult routeResult =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Transit),
                                  mercator::FromLatLon(55.75018, 37.60971), {0.0, 0.0},
                                  mercator::FromLatLon(55.67245, 37.86130));
  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  integration::TestRouteLength(*routeResult.first, 22968.6);

  CHECK(routeResult.first, ());
  integration::CheckSubwayExistence(*routeResult.first);
}

UNIT_TEST(Transit_Moscow_DubrovkaToTrtykovskya)
{
  TRouteResult routeResult =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Transit),
                                  mercator::FromLatLon(55.71813, 37.67756), {0.0, 0.0},
                                  mercator::FromLatLon(55.74089, 37.62831));
  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  integration::TestRouteLength(*routeResult.first, 7622.19);

  CHECK(routeResult.first, ());
  integration::CheckSubwayExistence(*routeResult.first);
}

UNIT_TEST(Transit_Moscow_NoSubwayTest)
{
  TRouteResult routeResult =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Transit),
                                  mercator::FromLatLon(55.73893, 37.62438), {0.0, 0.0},
                                  mercator::FromLatLon(55.73470, 37.62617));
  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  integration::TestRouteLength(*routeResult.first, 604.86);

  CHECK(routeResult.first, ());
  integration::CheckSubwayAbsent(*routeResult.first);
}

UNIT_TEST(Transit_Piter_FrunzenskyaToPlochadVosstaniya)
{
  TRouteResult routeResult =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Transit),
                                  mercator::FromLatLon(59.90511, 30.31425), {0.0, 0.0},
                                  mercator::FromLatLon(59.93096, 30.35872));
  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  /// @todo Check https://github.com/organicmaps/organicmaps/issues/1669 for details.
  integration::TestRouteLength(*routeResult.first, 5837.21);

  TEST(routeResult.first, ());
  integration::CheckSubwayExistence(*routeResult.first);
}

UNIT_TEST(Transit_Piter_TooLongPedestrian)
{
  TRouteResult const routeResult =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Transit),
                                  mercator::FromLatLon(59.90511, 30.31425), {0.0, 0.0},
                                  mercator::FromLatLon(59.78014, 30.50036));

  /// @todo Returns valid route now with long pedestrian part in the end, I don't see problems here.
  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  auto const & route = *routeResult.first;

  integration::CheckSubwayExistence(route);
  integration::TestRouteLength(route, 23521.9);
  TEST_LESS(route.GetTotalTimeSec(), 3600 * 3, ());
}

UNIT_TEST(Transit_Vatikan_NotEnoughGraphDataAtThenEnd)
{
  TRouteResult const routeResult =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Transit),
                                  mercator::FromLatLon(41.89543, 12.41481), {0.0, 0.0},
                                  mercator::FromLatLon(41.89203, 12.46263));

  /// @todo Returns valid route now with long pedestrian part in the end, I don't see problems here.
  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  TEST(routeResult.first, ());
  auto const & route = *routeResult.first;

  integration::CheckSubwayExistence(route);
  integration::TestRouteLength(route, 7703.56);
  TEST_LESS(route.GetTotalTimeSec(), 4000, ());
}

UNIT_TEST(Transit_Vatikan_CorneliaToOttaviano)
{
  TRouteResult routeResult =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Transit),
                                  mercator::FromLatLon(41.90052, 12.42642), {0.0, 0.0},
                                  mercator::FromLatLon(41.90414, 12.45640));

  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  // I don't see any bad routing sections here. Make actual value.
  integration::TestRouteLength(*routeResult.first, 4316.61);

  CHECK(routeResult.first, ());
  integration::CheckSubwayExistence(*routeResult.first);
}

UNIT_TEST(Transit_London_PoplarToOval)
{
  TRouteResult routeResult =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Transit),
                                  mercator::FromLatLon(51.50818, -0.01634), {0.0, 0.0},
                                  mercator::FromLatLon(51.48041, -0.10843));

  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  integration::TestRouteLength(*routeResult.first, 9421.72);

  CHECK(routeResult.first, ());
  integration::CheckSubwayExistence(*routeResult.first);
}

UNIT_TEST(Transit_London_DeptfordBridgeToCyprus)
{
  TRouteResult routeResult =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Transit),
                                  mercator::FromLatLon(51.47149, -0.030558), {0.0, 0.0},
                                  mercator::FromLatLon(51.51242, 0.07101));

  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  // I don't see any bad routing sections here. Make actual value.
  integration::TestRouteLength(*routeResult.first, 12882.2);

  CHECK(routeResult.first, ());
  integration::CheckSubwayExistence(*routeResult.first);
}

UNIT_TEST(Transit_Washington_FoggyToShaw)
{
  TRouteResult routeResult =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Transit),
                                  mercator::FromLatLon(38.89582, -77.04934), {0.0, 0.0},
                                  mercator::FromLatLon(38.91516, -77.01513));

  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  // I don't see any bad routing sections here. Make actual value.
  integration::TestRouteLength(*routeResult.first, 5685.82);

  CHECK(routeResult.first, ());
  integration::CheckSubwayExistence(*routeResult.first);
}

UNIT_TEST(Transit_NewYork_GrassmereToPleasantPlains)
{
  TRouteResult routeResult =
      integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Transit),
                                  mercator::FromLatLon(40.60536, -74.07736), {0.0, 0.0},
                                  mercator::FromLatLon(40.53015, -74.21559));

  TEST_EQUAL(routeResult.second, RouterResultCode::NoError, ());

  // I don't see any bad routing sections here. Make actual value.
  integration::TestRouteLength(*routeResult.first, 17433.7);

  CHECK(routeResult.first, ());
  integration::CheckSubwayExistence(*routeResult.first);
}
} // namespace transit_route_test
