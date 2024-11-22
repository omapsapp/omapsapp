#include "testing/testing.hpp"

#include "routing/routing_tests/tools.hpp"

#include "routing/route.hpp"
#include "routing/router.hpp"
#include "routing/routing_callbacks.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/routing_session.hpp"

#include "geometry/point2d.hpp"
#include "geometry/point_with_altitude.hpp"

#include "base/logging.hpp"

#include <chrono>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace routing_session_test
{
using namespace routing;
using namespace std;

using chrono::seconds;
using chrono::steady_clock;

vector<m2::PointD> kTestRoute = {{0., 1.}, {0., 1.}, {0., 3.}, {0., 4.}};
vector<Segment> const kTestSegments = {{0, 0, 0, true}, {0, 0, 1, true}, {0, 0, 2, true}};
vector<turns::TurnItem> const kTestTurnsReachOnly =
                                 {turns::TurnItem(1, turns::CarDirection::None),
                                  turns::TurnItem(2, turns::CarDirection::None),
                                  turns::TurnItem(3, turns::CarDirection::ReachedYourDestination)};
vector<turns::TurnItem> const kTestTurns =
                                 {turns::TurnItem(1, turns::CarDirection::None),
                                  turns::TurnItem(2, turns::CarDirection::TurnLeft),
                                  turns::TurnItem(3, turns::CarDirection::ReachedYourDestination)};
vector<double> const kTestTimes = {5.0, 10.0, 15.0};
auto const kRouteBuildingMaxDuration = seconds(30);

void FillSubroutesInfo(Route & route, vector<turns::TurnItem> const & turns = kTestTurnsReachOnly);

// Simple router. It returns route given to him on creation.
class DummyRouter : public IRouter
{
private:
  Route m_route;
  RouterResultCode m_code;
  size_t & m_buildCount;

public:
  DummyRouter(Route & route, RouterResultCode code, size_t & buildCounter)
    : m_route(route), m_code(code), m_buildCount(buildCounter)
  {
  }

  string GetName() const override { return "dummy"; }
  void ClearState() override {}
  void SetGuides(GuidesTracks && /* guides */) override {}

  RouterResultCode CalculateRoute(Checkpoints const & /* checkpoints */,
                                  m2::PointD const & /* startDirection */, bool /* adjust */,
                                  RouterDelegate const & /* delegate */, Route & route) override
  {
    ++m_buildCount;
    route = m_route;
    return m_code;
  }

  bool FindClosestProjectionToRoad(m2::PointD const & point, m2::PointD const & direction,
                                   double radius, EdgeProj & proj) override
  {
    return false;
  }
};

// Router which every next call of CalculateRoute() method return different return codes.
class ReturnCodesRouter : public IRouter
{
public:
  ReturnCodesRouter(initializer_list<RouterResultCode> const & returnCodes,
                    vector<m2::PointD> const & route)
    : m_returnCodes(returnCodes), m_route(route)
  {
  }

  // IRouter overrides:
  string GetName() const override { return "return codes router"; }
  void ClearState() override {}
  void SetGuides(GuidesTracks && /* guides */) override {}

  RouterResultCode CalculateRoute(Checkpoints const & /* checkpoints */,
                                  m2::PointD const & /* startDirection */, bool /* adjust */,
                                  RouterDelegate const & /* delegate */, Route & route) override
  {
    TEST_LESS(m_returnCodesIdx, m_returnCodes.size(), ());
    route = Route(GetName(), m_route.begin(), m_route.end(), 0 /* route id */);
    FillSubroutesInfo(route);
    return m_returnCodes[m_returnCodesIdx++];
  }

  bool FindClosestProjectionToRoad(m2::PointD const & point, m2::PointD const & direction,
                                   double radius, EdgeProj & proj) override
  {
    return false;
  }

private:
  vector<RouterResultCode> m_returnCodes;
  vector<m2::PointD> m_route;
  size_t m_returnCodesIdx = 0;
};

class TimedSignal
{
public:
  TimedSignal() : m_flag(false) {}
  void Signal()
  {
    lock_guard<mutex> guard(m_waitingMutex);
    m_flag = true;
    m_cv.notify_one();
  }

  bool WaitUntil(steady_clock::time_point const & time)
  {
    unique_lock<mutex> lock(m_waitingMutex);
    m_cv.wait_until(lock, time, [this, &time]
                    {
                      return m_flag || steady_clock::now() > time;
                    });
    return m_flag;
  }

private:
  mutex m_waitingMutex;
  condition_variable m_cv;
  bool m_flag;
};

/// \brief This class is developed to test callback on RoutingSession::m_state changing.
/// An new instance of the class should be constructed for every new test.
class SessionStateTest
{
public:
  SessionStateTest(initializer_list<SessionState> expectedStates, RoutingSession & routingSession)
    : m_expectedStates(expectedStates), m_session(routingSession)
  {
    for (size_t i = 1; i < expectedStates.size(); ++i)
    {
      // Note. Change session state callback is called only if the state is change.
      if (m_expectedStates[i - 1] != m_expectedStates[i])
        ++m_expectedNumberOfStateChanging;
    }

    TimedSignal timedSignal;
    GetPlatform().RunTask(Platform::Thread::Gui, [this, &timedSignal]() {
      m_session.SetChangeSessionStateCallback([this](SessionState previous, SessionState current) {
        TestChangeSessionStateCallbackCall(previous, current);
      });
      timedSignal.Signal();
    });
    TEST(timedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Callback is not set."));
  }

  ~SessionStateTest()
  {
    TEST_EQUAL(m_numberOfTestCalls, m_expectedNumberOfStateChanging,
               ("Wrong number of calls of SetState() callback.", m_expectedStates));
    TimedSignal timedSignal;
    GetPlatform().RunTask(Platform::Thread::Gui, [this, &timedSignal]() {
      m_session.SetChangeSessionStateCallback(nullptr);
      timedSignal.Signal();
    });
    TEST(timedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Callback is not set."));
  }

private:
  void TestChangeSessionStateCallbackCall(SessionState previous, SessionState current)
  {
    TEST_LESS(m_numberOfTestCalls + 1, m_expectedStates.size(),
              ("Too many calls of the method. previous:", previous, ", current:", current));
    TEST_EQUAL(previous, m_expectedStates[m_numberOfTestCalls], (previous, current));
    TEST_EQUAL(current, m_expectedStates[m_numberOfTestCalls + 1], (previous, current));

    ++m_numberOfTestCalls;
  }

  size_t m_numberOfTestCalls = 0;
  vector<SessionState> const m_expectedStates;
  size_t m_expectedNumberOfStateChanging = 0;
  RoutingSession & m_session;
};

void FillSubroutesInfo(Route & route, vector<turns::TurnItem> const & turns /* = kTestTurnsReachOnly */)
{
  vector<geometry::PointWithAltitude> junctions;
  for (auto const & point : kTestRoute)
    junctions.emplace_back(point, geometry::kDefaultAltitudeMeters);

  vector<RouteSegment> segmentInfo;
  RouteSegmentsFrom(kTestSegments, kTestRoute, turns, {}, segmentInfo);
  FillSegmentInfo(kTestTimes, segmentInfo);
  route.SetRouteSegments(std::move(segmentInfo));
  route.SetSubroteAttrs(vector<Route::SubrouteAttrs>(
      {Route::SubrouteAttrs(junctions.front(), junctions.back(), 0, kTestSegments.size())}));
}

void TestMovingByUpdatingLat(SessionStateTest const & sessionState, vector<double> const & lats,
                             location::GpsInfo const & info, RoutingSession & session)
{
  location::GpsInfo uptInfo(info);
  TimedSignal signal;
  GetPlatform().RunTask(Platform::Thread::Gui, [&session, &signal, &lats, &uptInfo]() {
    for (auto const lat : lats)
    {
      uptInfo.m_latitude = lat;
      session.OnLocationPositionChanged(uptInfo);
    }

    signal.Signal();
  });
  TEST(signal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration),
       ("Along route moving timeout."));
}

void TestLeavingRoute(RoutingSession & session, location::GpsInfo const & info)
{
  vector<double> const latitudes = {0.0,    -0.001, -0.002, -0.003, -0.004, -0.005,
                                    -0.006, -0.007, -0.008, -0.009, -0.01,  -0.011};
  SessionStateTest sessionStateTest({SessionState::OnRoute, SessionState::RouteNeedRebuild},
                                    session);
  TestMovingByUpdatingLat(sessionStateTest, latitudes, info, session);
}

UNIT_CLASS_TEST(AsyncGuiThreadTestWithRoutingSession, TestRouteBuilding)
{
  // Multithreading synchronization note. |counter| and |session| are constructed on the main thread,
  // then used on gui thread and then if timeout in timedSignal.WaitUntil() is not reached,
  // |counter| is used again.
  TimedSignal timedSignal;
  size_t counter = 0;

  GetPlatform().RunTask(Platform::Thread::Gui, [&timedSignal, &counter, this]() {
    InitRoutingSession();
    Route masterRoute("dummy", kTestRoute.begin(), kTestRoute.end(), 0 /* route id */);

    unique_ptr<DummyRouter> router =
        make_unique<DummyRouter>(masterRoute, RouterResultCode::NoError, counter);
    m_session->SetRouter(std::move(router), nullptr);
    m_session->SetRoutingCallbacks(
        [&timedSignal](Route const &, RouterResultCode) {
          LOG(LINFO, ("Ready"));
          timedSignal.Signal();
        },
        nullptr /* rebuildReadyCallback */, nullptr /* needMoreMapsCallback */,
        nullptr /* removeRouteCallback */);
    m_session->BuildRoute(Checkpoints(kTestRoute.front(), kTestRoute.back()),
                          RouterDelegate::kNoTimeout);
  });

  // Manual check of the routeBuilding mutex to avoid spurious results.
  TEST(timedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Route was not built."));
  TEST_EQUAL(counter, 1, ());
}

// Test on route rebuilding when current position moving from the route. Each next position
// is farther from the route then previous one.
UNIT_CLASS_TEST(AsyncGuiThreadTestWithRoutingSession, TestRouteRebuildingMovingAway)
{
  location::GpsInfo info;
  size_t counter = 0;

  TimedSignal alongTimedSignal;
  GetPlatform().RunTask(Platform::Thread::Gui, [&alongTimedSignal, this, &counter]() {
    InitRoutingSession();
    Route masterRoute("dummy", kTestRoute.begin(), kTestRoute.end(), 0 /* route id */);
    FillSubroutesInfo(masterRoute);

    unique_ptr<DummyRouter> router =
        make_unique<DummyRouter>(masterRoute, RouterResultCode::NoError, counter);
    m_session->SetRouter(std::move(router), nullptr);

    // Go along the route.
    m_session->SetRoutingCallbacks(
        [&alongTimedSignal](Route const &, RouterResultCode) { alongTimedSignal.Signal(); },
        nullptr /* rebuildReadyCallback */, nullptr /* needMoreMapsCallback */,
        nullptr /* removeRouteCallback */);
    m_session->BuildRoute(Checkpoints(kTestRoute.front(), kTestRoute.back()),
                          RouterDelegate::RouterDelegate::kNoTimeout);
  });
  TEST(alongTimedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Route was not built."));
  TEST_EQUAL(counter, 1, ());

  {
    SessionStateTest sessionStateTest(
        {SessionState::RouteNotStarted, SessionState::OnRoute, SessionState::RouteBuilding,
         SessionState::RouteNotStarted},
        *m_session);
    TimedSignal oppositeTimedSignal;
    GetPlatform().RunTask(Platform::Thread::Gui, [&oppositeTimedSignal, &info, this]() {
      info.m_horizontalAccuracy = 0.01;
      info.m_verticalAccuracy = 0.01;
      info.m_longitude = 0.;
      info.m_latitude = 1.;
      SessionState code = SessionState::NoValidRoute;

      {
        while (info.m_latitude < kTestRoute.back().y)
        {
          code = m_session->OnLocationPositionChanged(info);
          TEST_EQUAL(code, SessionState::OnRoute, ());
          info.m_latitude += 0.01;
        }
      }

      // Rebuild route and go in opposite direction. So initiate a route rebuilding flag.
      m_session->SetRoutingCallbacks(
          [&oppositeTimedSignal](Route const &, RouterResultCode) { oppositeTimedSignal.Signal(); },
          nullptr /* rebuildReadyCallback */, nullptr /* needMoreMapsCallback */,
          nullptr /* removeRouteCallback */);
      {
        m_session->BuildRoute(Checkpoints(kTestRoute.front(), kTestRoute.back()),
                              RouterDelegate::kNoTimeout);
      }
    });
    TEST(oppositeTimedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Route was not built."));
  }

  // Going away from route to set rebuilding flag.
  TimedSignal checkTimedSignal;
  GetPlatform().RunTask(Platform::Thread::Gui, [&checkTimedSignal, &info, this]() {
    info.m_longitude = 0.;
    info.m_latitude = 1.;
    info.m_speed = measurement_utils::KmphToMps(60);
    SessionState code = SessionState::NoValidRoute;
    for (size_t i = 0; i < 10; ++i)
    {
      code = m_session->OnLocationPositionChanged(info);
      info.m_latitude -= 0.1;
    }
    TEST_EQUAL(code, SessionState::RouteNeedRebuild, ());
    checkTimedSignal.Signal();
  });
  TEST(checkTimedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration),
       ("Route was not rebuilt."));
}

// Test on route rebuilding when current position moving to the route starting far from the route.
// Each next position is closer to the route then previous one but is not covered by route matching passage.
UNIT_CLASS_TEST(AsyncGuiThreadTestWithRoutingSession, TestRouteRebuildingMovingToRoute)
{
  location::GpsInfo info;
  size_t counter = 0;

  TimedSignal alongTimedSignal;
  GetPlatform().RunTask(Platform::Thread::Gui, [&alongTimedSignal, this, &counter]() {
    InitRoutingSession();
    Route masterRoute("dummy", kTestRoute.begin(), kTestRoute.end(), 0 /* route id */);
    FillSubroutesInfo(masterRoute);

    unique_ptr<DummyRouter> router =
        make_unique<DummyRouter>(masterRoute, RouterResultCode::NoError, counter);
    m_session->SetRouter(std::move(router), nullptr);

    m_session->SetRoutingCallbacks(
        [&alongTimedSignal](Route const &, RouterResultCode) { alongTimedSignal.Signal(); },
        nullptr /* rebuildReadyCallback */, nullptr /* needMoreMapsCallback */,
        nullptr /* removeRouteCallback */);
    m_session->BuildRoute(Checkpoints(kTestRoute.front(), kTestRoute.back()),
                          RouterDelegate::kNoTimeout);
  });
  TEST(alongTimedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Route was not built."));
  TEST_EQUAL(counter, 1, ());

  // Going starting far from the route and moving to the route but rebuild flag still is set.
  {
    SessionStateTest sessionStateTest(
        {SessionState::RouteNotStarted, SessionState::RouteNeedRebuild},
        *m_session);
    TimedSignal checkTimedSignalAway;
    GetPlatform().RunTask(Platform::Thread::Gui, [&checkTimedSignalAway, &info, this]() {
      info.m_longitude = 0.0;
      info.m_latitude = 0.0;
      info.m_speed = measurement_utils::KmphToMps(60);
      SessionState code = SessionState::NoValidRoute;
      {
        for (size_t i = 0; i < 8; ++i)
        {
          code = m_session->OnLocationPositionChanged(info);
          info.m_latitude += 0.1;
        }
      }
      TEST_EQUAL(code, SessionState::RouteNeedRebuild, ());
      checkTimedSignalAway.Signal();
    });
    TEST(checkTimedSignalAway.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration),
         ("Route was not rebuilt."));
  }
}

UNIT_CLASS_TEST(AsyncGuiThreadTestWithRoutingSession, TestFollowRouteFlagPersistence)
{
  location::GpsInfo info;
  size_t counter = 0;

  TimedSignal alongTimedSignal;
  GetPlatform().RunTask(Platform::Thread::Gui, [&alongTimedSignal, this, &counter]() {
    InitRoutingSession();
    Route masterRoute("dummy", kTestRoute.begin(), kTestRoute.end(), 0 /* route id */);
    FillSubroutesInfo(masterRoute, kTestTurns);
    unique_ptr<DummyRouter> router =
        make_unique<DummyRouter>(masterRoute, RouterResultCode::NoError, counter);
    m_session->SetRouter(std::move(router), nullptr);

    // Go along the route.
    m_session->SetRoutingCallbacks(
        [&alongTimedSignal](Route const &, RouterResultCode) { alongTimedSignal.Signal(); },
        nullptr /* rebuildReadyCallback */, nullptr /* needMoreMapsCallback */,
        nullptr /* removeRouteCallback */);
    m_session->BuildRoute(Checkpoints(kTestRoute.front(), kTestRoute.back()),
                          RouterDelegate::kNoTimeout);
  });
  TEST(alongTimedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Route was not built."));
  TEST_EQUAL(m_onNewTurnCallbackCounter, 0, ());

  TimedSignal oppositeTimedSignal;
  GetPlatform().RunTask(Platform::Thread::Gui, [&oppositeTimedSignal, &info, this, &counter]() {
    TEST(!m_session->IsFollowing(), ());
    m_session->EnableFollowMode();
    TEST(m_session->IsFollowing(), ());

    info.m_horizontalAccuracy = 0.01;
    info.m_verticalAccuracy = 0.01;
    info.m_longitude = 0.;
    info.m_latitude = 1.;
    while (info.m_latitude < kTestRoute.back().y)
    {
      m_session->OnLocationPositionChanged(info);
      TEST(m_session->IsOnRoute(), ());
      TEST(m_session->IsFollowing(), ());
      info.m_latitude += 0.01;
    }
    TEST_EQUAL(counter, 1, ());

    // Rebuild route and go in opposite direction. So initiate a route rebuilding flag.
    m_session->SetRoutingCallbacks(
        [&oppositeTimedSignal](Route const &, RouterResultCode) { oppositeTimedSignal.Signal(); },
        nullptr /* rebuildReadyCallback */, nullptr /* needMoreMapsCallback */,
        nullptr /* removeRouteCallback */);
    m_session->BuildRoute(Checkpoints(kTestRoute.front(), kTestRoute.back()),
                          RouterDelegate::kNoTimeout);
  });
  TEST(oppositeTimedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Route was not built."));
  TEST_EQUAL(m_onNewTurnCallbackCounter, 1, ());

  TimedSignal rebuildTimedSignal;
  GetPlatform().RunTask(Platform::Thread::Gui, [&rebuildTimedSignal, &info, this] {
    // Manual route building resets the following flag.
    TEST(!m_session->IsFollowing(), ());
    m_session->EnableFollowMode();
    TEST(m_session->IsFollowing(), ());
    info.m_longitude = 0.;
    info.m_latitude = 1.;
    info.m_speed = measurement_utils::KmphToMps(60);
    SessionState code = SessionState::NoValidRoute;
    for (size_t i = 0; i < 10; ++i)
    {
      code = m_session->OnLocationPositionChanged(info);
      info.m_latitude -= 0.1;
    }
    TEST_EQUAL(code, SessionState::RouteNeedRebuild, ());
    TEST(m_session->IsFollowing(), ());

    m_session->RebuildRoute(
        kTestRoute.front(),
        [&rebuildTimedSignal](Route const &, RouterResultCode) { rebuildTimedSignal.Signal(); },
        nullptr /* needMoreMapsCallback */, nullptr /* removeRouteCallback */, RouterDelegate::kNoTimeout,
        SessionState::RouteBuilding, false /* adjust */);
  });
  TEST(rebuildTimedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Route was not built."));
  TEST_EQUAL(m_onNewTurnCallbackCounter, 1, ());

  TimedSignal checkTimedSignal;
  GetPlatform().RunTask(Platform::Thread::Gui, [&checkTimedSignal, this] {
    TEST(m_session->IsFollowing(), ());
    checkTimedSignal.Signal();
  });
  TEST(checkTimedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Route checking timeout."));
  TEST_EQUAL(m_onNewTurnCallbackCounter, 1, ());
}

UNIT_CLASS_TEST(AsyncGuiThreadTestWithRoutingSession, TestFollowRoutePercentTest)
{
  TimedSignal alongTimedSignal;
  GetPlatform().RunTask(Platform::Thread::Gui, [&alongTimedSignal, this]() {
    InitRoutingSession();
    Route masterRoute("dummy", kTestRoute.begin(), kTestRoute.end(), 0 /* route id */);
    FillSubroutesInfo(masterRoute);

    size_t counter = 0;
    unique_ptr<DummyRouter> router =
        make_unique<DummyRouter>(masterRoute, RouterResultCode::NoError, counter);
    m_session->SetRouter(std::move(router), nullptr);

    // Get completion percent of unexisted route.
    TEST_EQUAL(m_session->GetCompletionPercent(), 0, (m_session->GetCompletionPercent()));
    // Go along the route.
    m_session->SetRoutingCallbacks(
        [&alongTimedSignal](Route const &, RouterResultCode) { alongTimedSignal.Signal(); },
        nullptr /* rebuildReadyCallback */, nullptr /* needMoreMapsCallback */,
        nullptr /* removeRouteCallback */);
    m_session->BuildRoute(Checkpoints(kTestRoute.front(), kTestRoute.back()),
                          RouterDelegate::kNoTimeout);
  });
  TEST(alongTimedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Route was not built."));

  TimedSignal checkTimedSignal;
  GetPlatform().RunTask(Platform::Thread::Gui, [&checkTimedSignal, this] {
    // Get completion percent of unstarted route.
    TEST_EQUAL(m_session->GetCompletionPercent(), 0, (m_session->GetCompletionPercent()));

    location::GpsInfo info;
    info.m_horizontalAccuracy = 0.01;
    info.m_verticalAccuracy = 0.01;

    // Go through the route.
    info.m_longitude = 0.;
    info.m_latitude = 1.;
    m_session->OnLocationPositionChanged(info);
    TEST(AlmostEqualAbs(m_session->GetCompletionPercent(), 0., 0.5),
         (m_session->GetCompletionPercent()));

    info.m_longitude = 0.;
    info.m_latitude = 2.;
    m_session->OnLocationPositionChanged(info);
    TEST(AlmostEqualAbs(m_session->GetCompletionPercent(), 33.3, 0.5),
         (m_session->GetCompletionPercent()));

    info.m_longitude = 0.;
    info.m_latitude = 3.;
    m_session->OnLocationPositionChanged(info);
    TEST(AlmostEqualAbs(m_session->GetCompletionPercent(), 66.6, 0.5),
         (m_session->GetCompletionPercent()));

    info.m_longitude = 0.;
    info.m_latitude = 3.99;
    m_session->OnLocationPositionChanged(info);
    TEST(AlmostEqualAbs(m_session->GetCompletionPercent(), 100., 0.5),
         (m_session->GetCompletionPercent()));
    checkTimedSignal.Signal();
  });
  TEST(checkTimedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration),
       ("Route checking timeout."));
}

UNIT_CLASS_TEST(AsyncGuiThreadTestWithRoutingSession, TestRouteRebuildingError)
{
  vector<m2::PointD> const kRoute = {{0.0, 0.001}, {0.0, 0.002}, {0.0, 0.003}, {0.0, 0.004}};
  // Creation RoutingSession.
  TimedSignal createTimedSignal;
  GetPlatform().RunTask(Platform::Thread::Gui, [this, &kRoute, &createTimedSignal]() {
    InitRoutingSession();
    unique_ptr<ReturnCodesRouter> router = make_unique<ReturnCodesRouter>(initializer_list<
        RouterResultCode>{RouterResultCode::NoError, RouterResultCode::InternalError}, kRoute);
    m_session->SetRouter(std::move(router), nullptr);
    createTimedSignal.Signal();
  });
  TEST(createTimedSignal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration),
       ("RouteSession was not created."));

  // Building a route.
  {
    SessionStateTest sessionStateTest(
        {SessionState::NoValidRoute, SessionState::RouteBuilding, SessionState::RouteNotStarted},
        *m_session);
    TimedSignal buildTimedSignal;
    GetPlatform().RunTask(Platform::Thread::Gui, [this, &kRoute, &buildTimedSignal]() {
      m_session->SetRoutingCallbacks(
          [&buildTimedSignal](Route const &, RouterResultCode) { buildTimedSignal.Signal(); },
          nullptr /* rebuildReadyCallback */, nullptr /* needMoreMapsCallback */,
          nullptr /* removeRouteCallback */);

      m_session->BuildRoute(Checkpoints(kRoute.front(), kRoute.back()),
                            RouterDelegate::kNoTimeout);
    });
    TEST(buildTimedSignal
             .WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration), ("Route was not built."));
  }

  location::GpsInfo info;
  info.m_horizontalAccuracy = 5.0; // meters
  info.m_verticalAccuracy = 5.0; // meters
  info.m_longitude = 0.0;

  // Moving along route.
  {
    SessionStateTest sessionStateTest({SessionState::RouteNotStarted, SessionState::OnRoute},
                                      *m_session);
    vector<double> const latitudes = {0.001, 0.0015, 0.002};
    TestMovingByUpdatingLat(sessionStateTest, latitudes, info, *m_session);
  }

  // Test 1. Leaving the route and returning to the route when state is |SessionState::RouteNeedRebuil|.
  TestLeavingRoute(*m_session, info);

  // Continue moving along the route.
  {
    SessionStateTest sessionStateTest({SessionState::RouteNeedRebuild, SessionState::OnRoute},
                                      *m_session);
    vector<double> const latitudes = {0.002, 0.0025, 0.003};
    TestMovingByUpdatingLat(sessionStateTest, latitudes, info, *m_session);
  }

  // Test 2. Leaving the route until, can not rebuilding it, and going along an old route.
  // It happens we the route is left and it's impossible to build a new route.
  // In this case the navigation is continued based on the former route.
  TestLeavingRoute(*m_session, info);
  {
    SessionStateTest sessionStateTest(
        {SessionState::RouteNeedRebuild, SessionState::RouteRebuilding}, *m_session);
    TimedSignal signal;
    GetPlatform().RunTask(Platform::Thread::Gui, [this, &signal]() {
      m_session->SetState(SessionState::RouteRebuilding);
      signal.Signal();
    });
    TEST(signal.WaitUntil(steady_clock::now() + kRouteBuildingMaxDuration),
         ("State was not set."));
  }

  // Continue moving along the route again.
  {
    // Test on state is not changed.
    SessionStateTest sessionStateTest({SessionState::RouteRebuilding, SessionState::OnRoute},
                                      *m_session);
    vector<double> const latitudes = {0.003, 0.0035, 0.004};
    TestMovingByUpdatingLat(sessionStateTest, latitudes, info, *m_session);
  }
}
}  // namespace routing_session_test
