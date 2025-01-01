#include "routing/turns_notification_manager.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <vector>

namespace routing
{
namespace turns
{
namespace sound
{

namespace
{
// If the distance between two sequential turns is less than kSecondTurnThresholdDistM
// the information about the second turn will be shown or pronounced
// when the user is approaching to the first one with "Then.".
double constexpr kSecondTurnThresholdDistM = 400.0;
// If the distance between two sequential turns is less than kDistanceNotifyThresholdM
// the notification will *not* append second distance, (like "In 500 meters. Turn left. Then. Turn right.")
double constexpr kDistanceNotifyThresholdM = 50.0;

// Returns true if the closest turn is an entrance to a roundabout and the second is
// an exit form a roundabout.
// Note. There are some cases when another turn (besides an exit from roundabout)
// follows an entrance to a roundabout. It could happend in case of turns inside a roundabout.
// Returns false otherwise.
bool IsClassicEntranceToRoundabout(routing::turns::TurnItemDist const & firstTurn,
                                   routing::turns::TurnItemDist const & secondTurn)
{
  return firstTurn.m_turnItem.m_turn == routing::turns::CarDirection::EnterRoundAbout &&
         secondTurn.m_turnItem.m_turn == routing::turns::CarDirection::LeaveRoundAbout;
}
}  // namespace

NotificationManager::NotificationManager()
  : m_enabled(false)
  , m_speedMetersPerSecond(0.0)
  , m_settings(8 /* m_startBeforeSecondsVehicle */, 35 /* m_minStartBeforeMetersVehicle */,
               150 /* m_maxStartBeforeMetersVehicle */, 170, /* m_minDistToSayNotificationMeters */
               8 /* m_startBeforeSecondsPedestrian */, 15 /* m_minStartBeforeMetersPedestrian */,
               25 /* m_maxStartBeforeMetersPedestrian */)
  , m_nextTurnNotificationProgress(PronouncedNotification::Nothing)
  , m_turnNotificationWithThen(false)
  , m_nextTurnIndex(0)
  , m_secondTurnNotification(CarDirection::None)
  , m_secondTurnNotificationIndex(0)
{
}

// static
NotificationManager NotificationManager::CreateNotificationManagerForTesting(
    uint32_t startBeforeSeconds, uint32_t minStartBeforeMeters, uint32_t maxStartBeforeMeters,
    uint32_t minDistToSayNotificationMeters, measurement_utils::Units lengthUnits,
    std::string const & engShortJson, uint32_t notificationTimeSecond, double speedMeterPerSecond)
{
  NotificationManager notificationManager;
  notificationManager.m_settings = Settings(startBeforeSeconds, minStartBeforeMeters,
                                            maxStartBeforeMeters, minDistToSayNotificationMeters,
                                            startBeforeSeconds, minStartBeforeMeters,
                                            maxStartBeforeMeters);
  notificationManager.Enable(true);
  notificationManager.SetLengthUnits(lengthUnits);
  notificationManager.m_getTtsText.ForTestingSetLocaleWithJson(engShortJson, "en");
  notificationManager.m_settings.ForTestingSetNotificationTimeSecond(notificationTimeSecond);
  notificationManager.Reset();
  notificationManager.SetSpeedMetersPerSecond(speedMeterPerSecond);

  return notificationManager;
}

std::string NotificationManager::GenerateTurnText(uint32_t distanceUnits, uint8_t exitNum,
                                                  bool useThenInsteadOfDistance, TurnItem const & turn,
                                                  RouteSegment::RoadNameInfo const & nextStreetInfo) const
{
  auto const lengthUnits = m_settings.GetLengthUnits();

  if (turn.m_turn != CarDirection::None)
    return m_getTtsText.GetTurnNotification(
        {distanceUnits, exitNum, useThenInsteadOfDistance, turn.m_turn, lengthUnits, nextStreetInfo});

  return m_getTtsText.GetTurnNotification({distanceUnits, exitNum, useThenInsteadOfDistance, turn.m_pedestrianTurn, lengthUnits, nextStreetInfo});
}

std::string NotificationManager::GenerateRecalculatingText() const
{
  return m_getTtsText.GetRecalculatingNotification();
}

std::string NotificationManager::GenerateSpeedCameraText() const
{
  return m_getTtsText.GetSpeedCameraNotification();
}

void NotificationManager::GenerateTurnNotifications(std::vector<TurnItemDist> const & turns,
                                                    std::vector<std::string> & turnNotifications)
{
  GenerateTurnNotifications(turns, turnNotifications, RouteSegment::RoadNameInfo{});
}

void NotificationManager::GenerateTurnNotifications(std::vector<TurnItemDist> const & turns,
                                                    std::vector<std::string> & turnNotifications,
                                                    RouteSegment::RoadNameInfo const & nextStreetInfo)
{
  m_secondTurnNotification = GenerateSecondTurnNotification(turns);

  turnNotifications.clear();
  if (!m_enabled || turns.empty())
    return;

  TurnItemDist const & firstTurn = turns.front();

  std::string firstNotification = GenerateFirstTurnSound(firstTurn.m_turnItem, firstTurn.m_distMeters, nextStreetInfo);
  if (m_nextTurnNotificationProgress == PronouncedNotification::Nothing)
    return;
  if (firstNotification.empty())
    return;
  turnNotifications.emplace_back(std::move(firstNotification));

  // Generating notifications like "Then turn left" if necessary.
  if (turns.size() < 2)
    return;
  TurnItemDist const & secondTurn = turns[1];
  ASSERT_LESS_OR_EQUAL(firstTurn.m_distMeters, secondTurn.m_distMeters, ());

  double distBetweenTurnsMeters = secondTurn.m_distMeters - firstTurn.m_distMeters;
  ASSERT_GREATER_OR_EQUAL(distBetweenTurnsMeters, 0, ());
  if (distBetweenTurnsMeters > kSecondTurnThresholdDistM &&
      !IsClassicEntranceToRoundabout(firstTurn, secondTurn))
  {
    return;
  }

  if (distBetweenTurnsMeters < kDistanceNotifyThresholdM)
  {
    // distanceUnits is not used because of "Then" is used
    distBetweenTurnsMeters = 0;
  }

  std::string secondNotification =
      GenerateTurnText(m_settings.ConvertMetersToUnits(distBetweenTurnsMeters), secondTurn.m_turnItem.m_exitNum,
                       true /* useThenInsteadOfDistance */, secondTurn.m_turnItem, RouteSegment::RoadNameInfo{});
  if (secondNotification.empty())
    return;
  turnNotifications.emplace_back(std::move(secondNotification));

  // Turn notification with word "Then" (about the second turn) will be pronounced.
  // When this second turn become the first one the first notification about the turn
  // shall be skipped.
  m_turnNotificationWithThen = true;
}

std::string NotificationManager::GenerateFirstTurnSound(TurnItem const & turn, double distanceToTurnMeters,
                                                        RouteSegment::RoadNameInfo const & nextStreetInfo)
{
  if (m_nextTurnIndex != turn.m_index)
  {
    m_nextTurnNotificationProgress = PronouncedNotification::Nothing;
    m_nextTurnIndex = turn.m_index;
  }

  bool const pedestrian = turn.m_pedestrianTurn != PedestrianDirection::None;

  uint32_t const distanceToPronounceNotificationM =
      m_settings.ComputeDistToPronounceDistM(m_speedMetersPerSecond, pedestrian);

  if (m_nextTurnNotificationProgress == PronouncedNotification::Nothing)
  {
    if (!m_settings.TooCloseForFisrtNotification(distanceToTurnMeters))
    {
      uint32_t const startPronounceDistMeters =
          m_settings.ComputeTurnDistanceM(m_speedMetersPerSecond) +
          distanceToPronounceNotificationM;
      if (distanceToTurnMeters < startPronounceDistMeters)
      {
        if (m_turnNotificationWithThen)
        {
          FastForwardFirstTurnNotification();
        }
        else
        {
          double const distToPronounceMeters = distanceToTurnMeters - distanceToPronounceNotificationM;
          if (distToPronounceMeters < 0)
          {
            FastForwardFirstTurnNotification();
            return {};  // The current position is too close to the turn for the first notification.
          }

          // Pronouncing first turn sound notification.
          double const distToPronounceUnits =
              m_settings.ConvertMetersToUnits(distToPronounceMeters);
          uint32_t const roundedDistToPronounceUnits =
              m_settings.RoundByPresetSoundedDistancesUnits(distToPronounceUnits);
          m_nextTurnNotificationProgress = PronouncedNotification::First;

          LOG(LINFO, ("TTS meters to pronounce", distanceToPronounceNotificationM, "meters to turn", distanceToTurnMeters, "meters to start pronounce", startPronounceDistMeters, "speed m/s", m_speedMetersPerSecond));
          return GenerateTurnText(roundedDistToPronounceUnits, turn.m_exitNum, false /* useThenInsteadOfDistance */,
                                  turn, nextStreetInfo);
        }
      }
    }
    else
    {
      // The first notification has not been pronounced but the distance to the turn is too short.
      // It happens if one turn follows shortly behind another one.
      m_nextTurnNotificationProgress = PronouncedNotification::First;
      FastForwardFirstTurnNotification();
    }
    return {};
  }

  if (m_nextTurnNotificationProgress == PronouncedNotification::First &&
      distanceToTurnMeters < distanceToPronounceNotificationM)
  {
    m_nextTurnNotificationProgress = PronouncedNotification::Second;
    FastForwardFirstTurnNotification();

    LOG(LINFO,("TTS meters to pronounce", distanceToPronounceNotificationM, "meters to turn", distanceToTurnMeters, "speed m/s", m_speedMetersPerSecond));
    return GenerateTurnText(0 /* distMeters */, turn.m_exitNum, false /* useThenInsteadOfDistance */, turn,
                            nextStreetInfo);
  }
  return {};
}

void NotificationManager::Enable(bool enable)
{
  if (enable && !m_enabled)
    Reset();
  m_enabled = enable;
}

void NotificationManager::SetLengthUnits(measurement_utils::Units units)
{
  m_settings.SetLengthUnits(units);
  switch (units)
  {
  case measurement_utils::Units::Metric:
    m_settings.SetState(30 /* notificationTimeSeconds */, 200 /* minNotificationDistanceUnits */,
                        2000 /* maxNotificationDistanceUnits */,
                        GetSoundedDistMeters() /* soundedDistancesUnits */,
                        measurement_utils::Units::Metric /* lengthUnits */);
    return;
  case measurement_utils::Units::Imperial:
    m_settings.SetState(30 /* notificationTimeSeconds */, 500 /* minNotificationDistanceUnits */,
                        5000 /* maxNotificationDistanceUnits */,
                        GetSoundedDistFeet() /* soundedDistancesUnits */,
                        measurement_utils::Units::Imperial /* lengthUnits */);
    return;
  }
}

void NotificationManager::SetSpeedMetersPerSecond(double speed)
{
  // When the quality of GPS data is bad the current speed may be less then zero.
  // It's easy to reproduce at an office with Nexus 5.
  // In that case zero meters per second is used.
  m_speedMetersPerSecond = std::max(0., speed);
}

void NotificationManager::Reset()
{
  m_nextTurnNotificationProgress = PronouncedNotification::Nothing;
  m_nextTurnIndex = 0;
  m_turnNotificationWithThen = false;
  m_secondTurnNotification = CarDirection::None;
  m_secondTurnNotificationIndex = 0;
}

void NotificationManager::FastForwardFirstTurnNotification()
{
  m_turnNotificationWithThen = false;
  if (m_nextTurnNotificationProgress == PronouncedNotification::Nothing)
    m_nextTurnNotificationProgress = PronouncedNotification::First;
}

CarDirection NotificationManager::GenerateSecondTurnNotification(std::vector<TurnItemDist> const & turns)
{
  if (turns.size() < 2)
  {
    m_secondTurnNotificationIndex = 0;
    return CarDirection::None;
  }

  TurnItemDist const & firstTurn = turns[0];
  TurnItemDist const & secondTurn = turns[1];

  if (firstTurn.m_turnItem.m_index != m_secondTurnNotificationIndex)
    m_secondTurnNotificationIndex = 0;  // It's a new closest (first) turn.
  else if (m_secondTurnNotificationIndex != 0)
    return secondTurn.m_turnItem.m_turn;  // m_secondTurnNotificationIndex was set to true before.

  double const distBetweenTurnsMeters = secondTurn.m_distMeters - firstTurn.m_distMeters;
  ASSERT_LESS_OR_EQUAL(0., distBetweenTurnsMeters, ());

  if (distBetweenTurnsMeters > kSecondTurnThresholdDistM)
    return CarDirection::None;

  uint32_t const startPronounceDistMeters =
      m_settings.ComputeTurnDistanceM(m_speedMetersPerSecond) +
      m_settings.ComputeDistToPronounceDistM(m_speedMetersPerSecond);

  if (firstTurn.m_distMeters <= startPronounceDistMeters)
  {
    m_secondTurnNotificationIndex = firstTurn.m_turnItem.m_index;
    return secondTurn.m_turnItem.m_turn;  // It's time to inform about the turn after the next one.
  }

  return CarDirection::None;
}

void NotificationManager::SetLocaleWithJsonForTesting(std::string const & json, std::string const & locale)
{
  m_getTtsText.ForTestingSetLocaleWithJson(json, locale);
}

std::string DebugPrint(PronouncedNotification notificationProgress)
{
  switch (notificationProgress)
  {
  case PronouncedNotification::Nothing:
    return "Nothing";
  case PronouncedNotification::First:
    return "First";
  case PronouncedNotification::Second:
    return "Second";
  }

  ASSERT(false, ());
  std::stringstream out;
  out << "unknown PronouncedNotification (" << static_cast<int>(notificationProgress) << ")";
  return out.str();
}
}  // namespace sound
}  // namespace turns
}  // namespace routing
