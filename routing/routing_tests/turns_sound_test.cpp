#include "testing/testing.hpp"

#include "routing/turns_notification_manager.hpp"
#include "routing/turns_sound_settings.hpp"

#include <vector>

namespace turns_sound_test
{
using namespace std;
using routing::turns::CarDirection;
using routing::turns::TurnItemDist;
using routing::turns::sound::NotificationManager;
using routing::turns::sound::Settings;

// An error to compare two double after conversion feet to meters.
double const kEps = 1.;

UNIT_TEST(TurnNotificationSettings_MetersTest)
{
  Settings const settings(20 /* notificationTimeSeconds */, 200 /* minNotificationDistanceUnits */,
                          700 /* maxNotificationDistanceUnits */, 5 /* m_startBeforeSeconds */,
                          25 /* m_minStartBeforeMeters */, 150 /* m_maxStartBeforeMeters */,
                          170 /* m_minDistToSayNotificationMeters */,
                          {100, 200, 300, 400, 500, 600, 700} /* soundedDistancesUnits */,
                          measurement_utils::Units::Metric /* lengthUnits */);

  TEST(settings.IsValid(), ());
  TEST(AlmostEqualAbs(
      settings.ConvertMetersPerSecondToUnitsPerSecond(20.), 20., kEps), ());
  TEST(AlmostEqualAbs(
      settings.ConvertMetersPerSecondToUnitsPerSecond(0.), 0., kEps), ());
  TEST(AlmostEqualAbs(settings.ConvertUnitsToMeters(300. /* distanceInUnits */), 300., kEps), ());
  TEST_EQUAL(settings.RoundByPresetSoundedDistancesUnits(300 /* distanceInUnits */), 300, ());
  TEST_EQUAL(settings.RoundByPresetSoundedDistancesUnits(0 /* distanceInUnits */), 100, ());

  TEST_EQUAL(settings.ComputeTurnDistanceM(0. /* speedMetersPerSecond */), 200., ());
  TEST_EQUAL(settings.ComputeTurnDistanceM(10. /* speedMetersPerSecond */), 200., ());
  TEST_EQUAL(settings.ComputeTurnDistanceM(20. /* speedMetersPerSecond */), 400., ());
  TEST_EQUAL(settings.ComputeTurnDistanceM(35. /* speedMetersPerSecond */), 700., ());
  TEST_EQUAL(settings.ComputeTurnDistanceM(200. /* speedMetersPerSecond */), 700., ());

  TEST_EQUAL(settings.ComputeDistToPronounceDistM(0. /* speedMetersPerSecond */), 25., ());
  TEST_EQUAL(settings.ComputeDistToPronounceDistM(10. /* speedMetersPerSecond */), 50., ());
  TEST_EQUAL(settings.ComputeDistToPronounceDistM(20. /* speedMetersPerSecond */), 100., ());
  TEST_EQUAL(settings.ComputeDistToPronounceDistM(35. /* speedMetersPerSecond */), 150., ());
  TEST_EQUAL(settings.ComputeDistToPronounceDistM(200. /* speedMetersPerSecond */), 150., ());
}

UNIT_TEST(TurnNotificationSettings_FeetTest)
{
  Settings const settings(20 /* notificationTimeSeconds */, 500 /* minNotificationDistanceUnits */,
                          2000 /* maxNotificationDistanceUnits */, 5 /* m_startBeforeSeconds */,
                          25 /* m_minStartBeforeMeters */, 150 /* m_maxStartBeforeMeters */,
                          170 /* m_minDistToSayNotificationMeters */,
                          {200, 400, 600, 800, 1000, 1500, 2000} /* soundedDistancesUnits */,
                          measurement_utils::Units::Imperial /* lengthUnits */);

  TEST(settings.IsValid(), ());
  TEST(AlmostEqualAbs(
      settings.ConvertMetersPerSecondToUnitsPerSecond(20.), 65., kEps), ());
  TEST(AlmostEqualAbs(
      settings.ConvertMetersPerSecondToUnitsPerSecond(0.), 0., kEps), ());
  TEST(AlmostEqualAbs(settings.ConvertUnitsToMeters(300. /* distanceInUnits */), 91., kEps), ());
  TEST_EQUAL(settings.RoundByPresetSoundedDistancesUnits(500 /* distanceInUnits */), 600, ());
  TEST_EQUAL(settings.RoundByPresetSoundedDistancesUnits(0 /* distanceInUnits */), 200, ());
}

UNIT_TEST(TurnNotificationSettings_NotValidTest)
{
  Settings settings1(20 /* notificationTimeSeconds */, 500 /* minNotificationDistanceUnits */,
                     2000 /* maxNotificationDistanceUnits */, 5 /* m_startBeforeSeconds */,
                     25 /* m_minStartBeforeMeters */, 150 /* m_maxStartBeforeMeters */,
                     170 /* m_minDistToSayNotificationMeters */,
                     {200, 400, 800, 600, 1000, 1500, 2000} /* soundedDistancesUnits */,
                     measurement_utils::Units::Imperial /* lengthUnits */);
  TEST(!settings1.IsValid(), ());

  Settings settings2(20 /* notificationTimeSeconds */, 5000 /* minNotificationDistanceUnits */,
                     2000 /* maxNotificationDistanceUnits */, 5 /* m_startBeforeSeconds */,
                     25 /* m_minStartBeforeMeters */, 150 /* m_maxStartBeforeMeters */,
                     170 /* m_minDistToSayNotificationMeters */,
                     {200, 400, 600, 800, 1000, 1500, 2000} /* soundedDistancesUnits */,
                     measurement_utils::Units::Metric /* lengthUnits */);
  TEST(!settings2.IsValid(), ());
}

UNIT_TEST(TurnsSound_MetersTest)
{
  string const engShortJson =
      "\
      {\
      \"in_600_meters\":\"In 600 meters.\",\
      \"make_a_right_turn\":\"Make a right turn.\"\
      }";
  auto notificationManager = NotificationManager::CreateNotificationManagerForTesting(
      5 /* startBeforeSeconds */, 10 /* minStartBeforeMeters */, 100 /* maxStartBeforeMeters */,
      100 /* minDistToSayNotificationMeters */, measurement_utils::Units::Metric, engShortJson,
      20 /* notificationTimeSecond */, 30.0 /* speedMeterPerSecond */);

  vector<TurnItemDist> turns = {{{5 /* idx */, CarDirection::TurnRight}, 1000.}};
  vector<string> turnNotifications;

  // Starting nearing the turnItem.
  // 1000 meters till the turn. No sound notifications is required.
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 700 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 700.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 699 meters till the turn. It's time to pronounce the first voice notification.
  // Why? The current speed is 30 meters per seconds. According to correctSettingsMeters
  // we need to play the first voice notification 20 seconds before the turn.
  // Besides that we need 5 seconds (but 100 meters maximum) for playing the notification.
  // So we start playing the first notification when the distance till the turn is less
  // then 20 seconds * 30 meters per seconds + 100 meters = 700 meters.
  turns.front().m_distMeters = 699.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  vector<string> const expectedNotification1 = {{"In 600 meters. Make a right turn."}};
  TEST_EQUAL(turnNotifications, expectedNotification1, ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 650 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 650.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  notificationManager.SetSpeedMetersPerSecond(32.);

  // 150 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 150.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 100 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 100.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 99 meters till the turn. It's time to pronounce the second voice notification.
  turns.front().m_distMeters = 99.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  vector<string> const expectedNotification2 = {{"Make a right turn."}};
  TEST_EQUAL(turnNotifications, expectedNotification2, ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 99 meters till the turn again. No sound notifications is required.
  turns.front().m_distMeters = 99.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 50 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 50.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 0 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 0.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  TEST(notificationManager.IsEnabled(), ());
}

// Test case:
// - Two turns;
// - They are close to each other;
// So the first notification of the second turn shall be skipped.
UNIT_TEST(TurnsSound_MetersTwoTurnsTest)
{
  string const engShortJson =
      "\
      {\
      \"in_600_meters\":\"In 600 meters.\",\
      \"make_a_sharp_right_turn\":\"Make a sharp right turn.\",\
      \"enter_the_roundabout\":\"Enter the roundabout.\"\
      }";
  auto notificationManager = NotificationManager::CreateNotificationManagerForTesting(
      5 /* startBeforeSeconds */, 10 /* minStartBeforeMeters */, 100 /* maxStartBeforeMeters */,
      100 /* minDistToSayNotificationMeters */, measurement_utils::Units::Metric, engShortJson,
      20 /* notificationTimeSecond */, 35.0 /* speedMeterPerSecond */);

  vector<TurnItemDist> turns = {{{5 /* idx */, CarDirection::TurnSharpRight}, 800.}};
  vector<string> turnNotifications;

  // Starting nearing the first turn.
  // 800 meters till the turn. No sound notifications is required.
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());

  // 700 meters till the turn. It's time to pronounce the first voice notification.
  // The speed is high.
  // The compensation of
  // NotificationManager::m_startBeforeSeconds/NotificationManager::m_minStartBeforeMeters/
  // NotificationManager::m_maxStartBeforeMeters is not enough.
  // The user will be closer to the turn while pronouncing despite the compensation.
  // So it should be pronounced "In 600 meters."
  turns.front().m_distMeters = 700.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  vector<string> const expectedNotification1 = {{"In 600 meters. Make a sharp right turn."}};
  TEST_EQUAL(turnNotifications, expectedNotification1, ());

  notificationManager.SetSpeedMetersPerSecond(32.);

  // 150 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 150.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());

  // 99 meters till the turn. It's time to pronounce the second voice notification.
  turns.front().m_distMeters = 99.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  vector<string> const expectedNotification2 = {{"Make a sharp right turn."}};
  TEST_EQUAL(turnNotifications, expectedNotification2, ());

  notificationManager.SetSpeedMetersPerSecond(10.);

  // 0 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 0.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());

  vector<TurnItemDist> turns2 = {{{11 /* idx */, CarDirection::EnterRoundAbout,
                                         2 /* exitNum */}, 60.}};

  // Starting nearing the second turn.
  notificationManager.GenerateTurnNotifications(turns2, turnNotifications);
  TEST(turnNotifications.empty(), ());

  // 40 meters till the second turn. It's time to pronounce the second voice notification
  // without the first one.
  turns2.front().m_distMeters = 40.;
  notificationManager.GenerateTurnNotifications(turns2, turnNotifications);
  vector<string> const expectedNotification3 = {{"Enter the roundabout."}};
  TEST_EQUAL(turnNotifications, expectedNotification3, ());

  TEST(notificationManager.IsEnabled(), ());
}

UNIT_TEST(TurnsSound_FeetTest)
{
  string const engShortJson =
      "\
      {\
      \"in_2000_feet\":\"In 2000 feet.\",\
      \"enter_the_roundabout\":\"Enter the roundabout.\"\
      }";
  auto notificationManager = NotificationManager::CreateNotificationManagerForTesting(
      5 /* startBeforeSeconds */, 10 /* minStartBeforeMeters */, 100 /* maxStartBeforeMeters */,
      100 /* minDistToSayNotificationMeters */, measurement_utils::Units::Imperial, engShortJson,
      20 /* notificationTimeSecond */, 30.0 /* speedMeterPerSecond */);

  vector<TurnItemDist> turns = {{{7 /* idx */, CarDirection::EnterRoundAbout,
                                  3 /* exitNum */}, 1000.}};
  vector<string> turnNotifications;

  // Starting nearing the turnItem.
  // 1000 meters till the turn. No sound notifications is required.
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());

  // 700 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 700.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());

  // 699 meters till the turn. It's time to pronounce the first voice notification.
  // Why? The current speed is 30 meters per seconds. According to correctSettingsMeters
  // we need to play the first voice notification 20 seconds before the turn.
  // Besides that we need 5 seconds (but 100 meters maximum) for playing the notification.
  // So we start playing the first notification when the distance till the turn is less
  // then 20 seconds * 30 meters per seconds + 100 meters = 700 meters.
  turns.front().m_distMeters = 699.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  vector<string> const expectedNotification1 = {{"In 2000 feet. Enter the roundabout."}};
  TEST_EQUAL(turnNotifications, expectedNotification1, ());

  // 650 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 650.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());

  // 150 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 150.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());

  // 100 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 100.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());

  // 99 meters till the turn. It's time to pronounce the second voice notification.
  turns.front().m_distMeters = 99.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  vector<string> const expectedNotification2 = {{"Enter the roundabout."}};
  TEST_EQUAL(turnNotifications, expectedNotification2, ());

  // 99 meters till the turn again. No sound notifications is required.
  turns.front().m_distMeters = 99.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());

  // 50 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 50.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());

  // 0 meters till the turn. No sound notifications is required.
  turns.front().m_distMeters = 0.;
  notificationManager.GenerateTurnNotifications(turns, turnNotifications);
  TEST(turnNotifications.empty(), ());

  TEST(notificationManager.IsEnabled(), ());
}

UNIT_TEST(TurnsSound_ComposedTurnTest)
{
  string const engShortJson =
      "\
      {\
      \"in_600_meters\":\"In 600 meters.\",\
      \"in_200_meters\":\"In 200 meters.\",\
      \"make_a_right_turn\":\"Turn right.\",\
      \"enter_the_roundabout\":\"Enter the roundabout.\",\
      \"then\":\"Then.\",\
      \"you_have_reached_the_destination\":\"You have reached the destination.\"\
      }";
  auto notificationManager = NotificationManager::CreateNotificationManagerForTesting(
      5 /* startBeforeSeconds */, 10 /* minStartBeforeMeters */, 100 /* maxStartBeforeMeters */,
      100 /* minDistToSayNotificationMeters */, measurement_utils::Units::Metric, engShortJson,
      30.0 /* notificationTimeSecond */, 20.0 /* speedMeterPerSecond */);

  vector<string> turnNotifications;

  // Starting nearing the first turn.
  // 800 meters till the first turn.
  vector<TurnItemDist> const turns1 = {{{5 /* idx */, CarDirection::TurnRight}, 800. /* m_distMeters */},
                                       {{10 /* idx */, CarDirection::EnterRoundAbout}, 1000. /* m_distMeters */}};
  notificationManager.GenerateTurnNotifications(turns1, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 620 meters till the first turn.
  turnNotifications.clear();
  vector<TurnItemDist> const turns2 = {{{5 /* idx */, CarDirection::TurnRight}, 620. /* m_distMeters */},
                                       {{10 /* idx */, CarDirection::EnterRoundAbout}, 665. /* m_distMeters */}};
  vector<string> const expectedNotification2 = {{"In 600 meters. Turn right."},
                                                {"Then. Enter the roundabout."}};
  notificationManager.GenerateTurnNotifications(turns2, turnNotifications);
  TEST_EQUAL(turnNotifications, expectedNotification2, ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::EnterRoundAbout, ());

  // 300 meters till the first turn.
  turnNotifications.clear();
  vector<TurnItemDist> const turns3 = {{{5 /* idx */, CarDirection::TurnRight}, 300. /* m_distMeters */},
                                       {{10 /* idx */, CarDirection::EnterRoundAbout}, 500. /* m_distMeters */}};
  notificationManager.GenerateTurnNotifications(turns3, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::EnterRoundAbout, ());

  // 20 meters till the first turn.
  turnNotifications.clear();
  vector<TurnItemDist> const turns4 = {{{5 /* idx */, CarDirection::TurnRight}, 20. /* m_distMeters */},
                                       {{10 /* idx */, CarDirection::EnterRoundAbout}, 220. /* m_distMeters */}};
  vector<string> const expectedNotification4 = {{"Turn right."},
                                                {"Then. In 200 meters. Enter the roundabout."}};
  notificationManager.GenerateTurnNotifications(turns4, turnNotifications);
  TEST_EQUAL(turnNotifications, expectedNotification4, ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::EnterRoundAbout, ());

  // After the first turn.
  turnNotifications.clear();
  vector<TurnItemDist> const turns5 = {{{10 /* idx */, CarDirection::EnterRoundAbout}, 180. /* m_distMeters */},
                                       {{15 /* idx */, CarDirection::ReachedYourDestination}, 1180. /* m_distMeters */}};
  notificationManager.GenerateTurnNotifications(turns5, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // Just before the second turn.
  turnNotifications.clear();
  vector<TurnItemDist> const turns6 = {{{10 /* idx */, CarDirection::EnterRoundAbout}, 10. /* m_distMeters */},
                                       {{15 /* idx */, CarDirection::ReachedYourDestination}, 1010. /* m_distMeters */}};
  vector<string> const expectedNotification6 = {{"Enter the roundabout."}};
  notificationManager.GenerateTurnNotifications(turns6, turnNotifications);
  TEST_EQUAL(turnNotifications, expectedNotification6, ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());
}

UNIT_TEST(TurnsSound_RoundaboutTurnTest)
{
  string const engShortJson =
      "\
      {\
      \"enter_the_roundabout\":\"Enter the roundabout.\",\
      \"leave_the_roundabout\":\"Leave the roundabout.\",\
      \"take_the_1_exit\":\"Take the first exit.\",\
      \"take_the_2_exit\":\"Take the second exit.\",\
      \"take_the_4_exit\":\"Take the fourth exit.\",\
      \"in_600_meters\":\"In 600 meters.\",\
      \"in_1_kilometer\":\"In 1 kilometer.\",\
      \"then\":\"Then.\"\
      }";
  auto notificationManager = NotificationManager::CreateNotificationManagerForTesting(
      5 /* startBeforeSeconds */, 10 /* minStartBeforeMeters */, 100 /* maxStartBeforeMeters */,
      100 /* minDistToSayNotificationMeters */, measurement_utils::Units::Metric, engShortJson,
      30 /* notificationTimeSecond */, 20.0 /* speedMeterPerSecond */);

  vector<string> turnNotifications;

  // Starting nearing the first turn.
  // 1000 meters till the first turn.
  vector<TurnItemDist> const turns1 = {{{5 /* idx */, CarDirection::EnterRoundAbout, 2 /* m_exitNum */},
                                        1000. /* m_distMeters */},
                                       {{10 /* idx */, CarDirection::LeaveRoundAbout, 2 /* m_exitNum */},
                                        2000. /* m_distMeters */}};
  notificationManager.GenerateTurnNotifications(turns1, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 620 meters till the first turn.
  vector<TurnItemDist> const turns2 = {{{5 /* idx */, CarDirection::EnterRoundAbout, 2 /* m_exitNum */},
                                        620. /* m_distMeters */},
                                       {{10 /* idx */, CarDirection::LeaveRoundAbout, 2 /* m_exitNum */},
                                        1620. /* m_distMeters */}};
  vector<string> const expectedNotification2 = {{"In 600 meters. Enter the roundabout."},
                                                {"Then. In 1 kilometer. Take the second exit."}};
  notificationManager.GenerateTurnNotifications(turns2, turnNotifications);
  TEST_EQUAL(turnNotifications, expectedNotification2, ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 3 meters till the first turn.
  vector<TurnItemDist> const turns3 = {{{5 /* idx */, CarDirection::EnterRoundAbout, 2 /* m_exitNum */},
                                        3. /* m_distMeters */},
                                       {{10 /* idx */, CarDirection::LeaveRoundAbout, 2 /* m_exitNum */},
                                        1003. /* m_distMeters */}};
  vector<string> const expectedNotification3 = {{"Enter the roundabout."},
                                                {"Then. In 1 kilometer. Take the second exit."}};
  notificationManager.GenerateTurnNotifications(turns3, turnNotifications);
  TEST_EQUAL(turnNotifications, expectedNotification3, ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 900 meters till the second turn.
  vector<TurnItemDist> const turns4 = {{{10 /* idx */, CarDirection::LeaveRoundAbout, 2 /* m_exitNum */},
                                        900. /* m_distMeters */},
                                       {{15 /* idx */, CarDirection::EnterRoundAbout, 1 /* m_exitNum */},
                                        1900. /* m_distMeters */}};
  notificationManager.GenerateTurnNotifications(turns4, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 300 meters till the second turn.
  vector<TurnItemDist> const turns5 = {{{10 /* idx */, CarDirection::LeaveRoundAbout, 2 /* m_exitNum */},
                                        300. /* m_distMeters */},
                                       {{15 /* idx */, CarDirection::EnterRoundAbout, 1 /* m_exitNum */},
                                        1300. /* m_distMeters */}};
  notificationManager.GenerateTurnNotifications(turns5, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 3 meters till the second turn.
  vector<TurnItemDist> const turns6 = {{{10 /* idx */, CarDirection::LeaveRoundAbout, 2 /* m_exitNum */},
                                        3. /* m_distMeters */},
                                       {{15 /* idx */, CarDirection::EnterRoundAbout, 1 /* m_exitNum */},
                                        1003. /* m_distMeters */}};
  vector<string> const expectedNotification6 = {{"Leave the roundabout."}};
  notificationManager.GenerateTurnNotifications(turns6, turnNotifications);
  TEST_EQUAL(turnNotifications, expectedNotification6, ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 5 meters till the third turn.
  vector<TurnItemDist> const turns7 = {{{15 /* idx */, CarDirection::EnterRoundAbout, 1 /* m_exitNum */},
                                        5. /* m_distMeters */},
                                       {{20 /* idx */, CarDirection::LeaveRoundAbout, 1 /* m_exitNum */},
                                        1005. /* m_distMeters */}};
  vector<string> const expectedNotification7 = {{"Enter the roundabout."},
                                                {"Then. In 1 kilometer. Take the first exit."}};
  notificationManager.GenerateTurnNotifications(
      turns7, turnNotifications);  // The first notification fast forwarding.
  notificationManager.GenerateTurnNotifications(turns7, turnNotifications);
  TEST_EQUAL(turnNotifications, expectedNotification7, ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 900 meters till the 4th turn.
  notificationManager.Reset();
  vector<TurnItemDist> const turns8 = {{{25 /* idx */, CarDirection::EnterRoundAbout, 4 /* m_exitNum */},
                                        900. /* m_distMeters */},
                                       {{30 /* idx */, CarDirection::LeaveRoundAbout, 4 /* m_exitNum */},
                                        1200. /* m_distMeters */}};
  notificationManager.GenerateTurnNotifications(turns8, turnNotifications);
  TEST(turnNotifications.empty(), ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::None, ());

  // 620 meters till the 4th turn.
  vector<TurnItemDist> const turns9 = {{{25 /* idx */, CarDirection::EnterRoundAbout, 4 /* m_exitNum */},
                                        620. /* m_distMeters */},
                                       {{30 /* idx */, CarDirection::LeaveRoundAbout, 4 /* m_exitNum */},
                                        665. /* m_distMeters */}};
  vector<string> const expectedNotification9 = {{"In 600 meters. Enter the roundabout."},
                                                {"Then. Take the fourth exit."}};
  notificationManager.GenerateTurnNotifications(turns9, turnNotifications);
  TEST_EQUAL(turnNotifications, expectedNotification9, ());
  TEST_EQUAL(notificationManager.GetSecondTurnNotification(), CarDirection::LeaveRoundAbout, ());
}

UNIT_TEST(GetJsonBufferTest)
{
  string const localeNameEn = "en";
  string jsonBuffer;
  TEST(GetJsonBuffer(platform::TextSource::TtsSound, localeNameEn, jsonBuffer), ());
  TEST(!jsonBuffer.empty(), ());

  string const localeNameRu = "ru";
  jsonBuffer.clear();
  TEST(GetJsonBuffer(platform::TextSource::TtsSound, localeNameRu, jsonBuffer), ());
  TEST(!jsonBuffer.empty(), ());
}
}  // namespace turns_sound_test
