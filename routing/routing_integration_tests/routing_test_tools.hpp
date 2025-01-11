#pragma once

#include "routing/index_router.hpp"
#include "routing/routing_callbacks.hpp"

#include "traffic/traffic_cache.hpp"

#include "storage/country_info_getter.hpp"

#include "map/features_fetcher.hpp"

#include "platform/country_file.hpp"
#include "platform/local_country_file.hpp"

#include <memory>
#include <set>
#include <string>
#include <vector>

/*
 * These tests are developed to simplify routing integration tests writing.
 * You can use the interface bellow however you want but there are some hints.
 * 1. Most likely you want to use GetCarComponents() or GetPedestrianComponents() without parameter
 *    to get a reference to IRouterComponents.
 *    It loads all the maps from directories Platform::WritableDir()
 *    and Platform::ResourcesDir() only once and then reuse it.
 *    Use GetCarComponents() or GetPedestrianComponents() with vector of maps parameter
 *    only if you want to test something on a special map set.
 * 2. Loading maps and calculating routes is a time consuming process.
 *    Do this only if you really need it.
 * 3. If you want to check that a turn is absent - use TestTurnCount.
 * 4. The easiest way to gather all the information for writing an integration test is
 *    - to put a break point in CalculateRoute() method;
 *    - to make a route with the desktop application;
 *    - to get all necessary parameters and result of the route calculation;
 *    - to place them into the test you're writing.
 * 5. The recommended way for naming tests for a route from one place to another one is
 *    <Country><City><Street1><House1><Street2><House2><Test time. TurnTest or RouteTest for the
 * time being>
 * 6. Add a comment to describe what this particular test is testing (e.g. "respect the bicycle=no tag").
 *    And add a ref to a Github issue if applicable.
 * 7. It's a good idea to use short routes for testing turns. The thing is geometry of long routes
 *    could be changed from one dataset to another. The shorter the route the less is the chance it's changed.
 */

typedef std::pair<std::shared_ptr<routing::Route>, routing::RouterResultCode> TRouteResult;

namespace integration
{
using namespace routing;
using namespace turns;
using platform::LocalCountryFile;

std::shared_ptr<FeaturesFetcher> CreateFeaturesFetcher(
    std::vector<LocalCountryFile> const & localFiles);

std::unique_ptr<storage::CountryInfoGetter> CreateCountryInfoGetter();

std::unique_ptr<IndexRouter> CreateVehicleRouter(DataSource & dataSource,
                                                 storage::CountryInfoGetter const & infoGetter,
                                                 traffic::TrafficCache const & trafficCache,
                                                 std::vector<LocalCountryFile> const & localFiles,
                                                 VehicleType vehicleType);

class IRouterComponents
{
public:
  IRouterComponents(std::vector<LocalCountryFile> const & localFiles)
    : m_featuresFetcher(CreateFeaturesFetcher(localFiles)), m_infoGetter(CreateCountryInfoGetter())
  {
  }

  virtual ~IRouterComponents() = default;

  virtual IRouter & GetRouter() const = 0;

  storage::CountryInfoGetter const & GetCountryInfoGetter() const noexcept { return *m_infoGetter; }

protected:
  std::shared_ptr<FeaturesFetcher> m_featuresFetcher;
  std::unique_ptr<storage::CountryInfoGetter> m_infoGetter;
};

class VehicleRouterComponents : public IRouterComponents
{
public:
  VehicleRouterComponents(std::vector<LocalCountryFile> const & localFiles, VehicleType vehicleType)
    : IRouterComponents(localFiles)
    , m_indexRouter(CreateVehicleRouter(m_featuresFetcher->GetDataSource(), *m_infoGetter,
                                        m_trafficCache, localFiles, vehicleType))
  {
  }

  IRouter & GetRouter() const override { return *m_indexRouter; }

private:
  traffic::TrafficCache m_trafficCache;
  std::unique_ptr<IndexRouter> m_indexRouter;
};

void GetAllLocalFiles(std::vector<LocalCountryFile> & localFiles);
void TestOnlineCrosses(ms::LatLon const & startPoint, ms::LatLon const & finalPoint,
                       std::vector<std::string> const & expected,
                       IRouterComponents & routerComponents);
void TestOnlineFetcher(ms::LatLon const & startPoint, ms::LatLon const & finalPoint,
                       std::vector<std::string> const & expected,
                       IRouterComponents & routerComponents);

std::shared_ptr<VehicleRouterComponents>
CreateAllMapsComponents(VehicleType vehicleType, std::set<std::string> const & skipMaps);

IRouterComponents & GetVehicleComponents(VehicleType vehicleType);

TRouteResult CalculateRoute(IRouterComponents const & routerComponents,
                            m2::PointD const & startPoint, m2::PointD const & startDirection,
                            m2::PointD const & finalPoint);

TRouteResult CalculateRoute(IRouterComponents const & routerComponents,
                            Checkpoints const & checkpoints, GuidesTracks && guides);

void TestTurnCount(Route const & route, uint32_t expectedTurnCount);
void TestTurns(Route const & route, std::vector<CarDirection> const & expectedTurns);

/// Testing route length.
/// It is used for checking if routes have expected(sample) length.
/// A created route will pass the test iff
/// expectedRouteMeters - expectedRouteMeters * relativeError  <= route->GetDistance()
/// && expectedRouteMeters + expectedRouteMeters * relativeError >= route->GetDistance()
void TestRouteLength(Route const & route, double expectedRouteMeters, double relativeError = 0.01);
void TestRouteTime(Route const & route, double expectedRouteSeconds, double relativeError = 0.01);
void TestRoutePointsNumber(Route const & route, size_t expectedPointsNumber,
                           double relativeError = 0.1);

void CalculateRouteAndTestRouteLength(IRouterComponents const & routerComponents,
                                      m2::PointD const & startPoint,
                                      m2::PointD const & startDirection,
                                      m2::PointD const & finalPoint, double expectedRouteMeters,
                                      double relativeError = 0.02);

void CalculateRouteAndTestRouteTime(IRouterComponents const & routerComponents,
                                    m2::PointD const & startPoint,
                                    m2::PointD const & startDirection,
                                    m2::PointD const & finalPoint, double expectedTimeSeconds,
                                    double relativeError = 0.07);

void CheckSubwayExistence(Route const & route);
void CheckSubwayAbsent(Route const & route);

class TestTurn
{
  friend TestTurn GetNthTurn(Route const & route, uint32_t turnNumber);

  m2::PointD const m_point;
  CarDirection const m_direction;
  uint32_t const m_roundAboutExitNum;
  bool const m_isValid;

  TestTurn()
    : m_point({0.0, 0.0})
    , m_direction(CarDirection::None)
    , m_roundAboutExitNum(0)
    , m_isValid(false)
  {
  }
  TestTurn(m2::PointD const & pnt, CarDirection direction, uint32_t roundAboutExitNum)
    : m_point(pnt), m_direction(direction), m_roundAboutExitNum(roundAboutExitNum), m_isValid(true)
  {
  }

public:
  const TestTurn & TestValid() const;
  const TestTurn & TestNotValid() const;
  const TestTurn & TestPoint(m2::PointD const & expectedPoint, double inaccuracyMeters = 3.) const;
  const TestTurn & TestDirection(CarDirection expectedDirection) const;
  const TestTurn & TestOneOfDirections(std::set<CarDirection> const & expectedDirections) const;
  const TestTurn & TestRoundAboutExitNum(uint32_t expectedRoundAboutExitNum) const;
};

/// Extracting appropriate TestTurn if any. If not TestTurn::isValid() returns false.
/// inaccuracy is set in meters.
TestTurn GetNthTurn(Route const & route, uint32_t turnNumber);

void TestCurrentStreetName(routing::Route const & route, std::string const & expectedStreetName);

void TestNextStreetName(routing::Route const & route, std::string const & expectedStreetName);

LocalCountryFile GetLocalCountryFileByCountryId(platform::CountryFile const & country);
}  // namespace integration
