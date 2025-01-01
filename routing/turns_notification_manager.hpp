#pragma once

#include "routing/turns_sound_settings.hpp"
#include "routing/turns_tts_text.hpp"

#include "platform/measurement_utils.hpp"
#include "platform/settings.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace routing
{
namespace turns
{
namespace sound
{
/// \brief The PronouncedNotification enum represents which sound notifications
/// for turns were heard.
enum class PronouncedNotification
{
  Nothing,
  First,
  Second, /** The second notification just before the turn was pronounced. */
};

std::string DebugPrint(PronouncedNotification notificationProgress);

/// \brief The TurnsSound class is responsible for all route turn sound notifications functionality.
/// To be able to generate turn sound notification the class needs to have correct Settings
/// and relevant speed.
class NotificationManager
{
public:
  NotificationManager();

  static NotificationManager CreateNotificationManagerForTesting(
      uint32_t startBeforeSeconds, uint32_t minStartBeforeMeters, uint32_t maxStartBeforeMeters,
      uint32_t minDistToSayNotificationMeters, measurement_utils::Units lengthUnits,
      std::string const & engShortJson, uint32_t notificationTimeSecond, double speedMeterPerSecond);

  bool IsEnabled() const { return m_enabled; }
  void Enable(bool enable);
  std::string GetLocale() const { return m_getTtsText.GetLocale(); }
  void SetLengthUnits(measurement_utils::Units units);
  void SetSpeedMetersPerSecond(double speed);
  void SetLocale(std::string const & locale) { m_getTtsText.SetLocale(locale); }
  measurement_utils::Units GetLengthUnits() const { return m_settings.GetLengthUnits(); }
  void SetLocaleWithJsonForTesting(std::string const & json, std::string const & locale);

  /// \brief Generate text of route rebuild notification.
  std::string GenerateRecalculatingText() const;

  /// \brief Generate text of speed camera notification.
  std::string GenerateSpeedCameraText() const;

  /// \brief GenerateTurnNotifications updates information about the next turn notification.
  /// It also fills turnNotifications when it's necessary.
  /// If this TurnsSound wants to play a sound message once it should push one item to
  /// the vector turnNotifications once when GenerateTurnNotifications is called.
  /// \param turns contains information about the next turns starting from the closest one.
  /// \param distanceToTurnMeters is distance to the next turn in meters.
  /// \param turnNotifications is a parameter to fill it if it's necessary.
  /// \param nextStreetInfo is the RoadNameInfo for the next street to turn on (optional)
  /// \note The client implies turnNotifications does not contain empty strings.
  void GenerateTurnNotifications(std::vector<TurnItemDist> const & turns, std::vector<std::string> & turnNotifications,
                                 RouteSegment::RoadNameInfo const & nextStreetInfo);

  /// \brief GenerateTurnNotifications updates information about the next turn notification.
  /// It also fills turnNotifications when it's necessary.
  /// If this TurnsSound wants to play a sound message once it should push one item to
  /// the vector turnNotifications once when GenerateTurnNotifications is called.
  /// \param turns contains information about the next turns starting from the closest one.
  /// \param distanceToTurnMeters is distance to the next turn in meters.
  /// \param turnNotifications is a parameter to fill it if it's necessary.
  /// \note The client implies turnNotifications does not contain empty strings.
  void GenerateTurnNotifications(std::vector<TurnItemDist> const & turns,
                                 std::vector<std::string> & turnNotifications);

  /// Reset states which reflects current route position.
  /// The method shall be called after creating a new route or after rerouting.
  void Reset();

  /// \brief The method was implemented to display the second turn notification
  /// in an appropriate moment.
  /// @return the second closest turn.
  /// The return value is different form TurnDirection::NoTurn
  /// if an end user is close enough to the first (current) turn and
  /// the distance between the closest and the second closest turn is not large.
  /// (That means a notification about turn after the closest one was pronounced.)
  /// For example, if while closing to the closest turn was pronounced
  /// "Turn right. Then turn left." 500 meters before the closest turn, after that moment
  /// GetSecondTurnNotification returns TurnDirection::TurnLeft if distance to first turn < 500
  /// meters.
  /// After the closest composed turn was passed GetSecondTurnNotification returns
  /// TurnDirection::NoTurn.
  /// \note If the returning value is TurnDirection::NoTurn no turn shall be displayed.
  /// \note If GetSecondTurnNotification returns a value different form TurnDirection::NoTurn
  /// for a turn once it continues returning the same value until the turn is changed.
  /// \note This method works independent from m_enabled value.
  /// So it works when the class enable and disable.
  CarDirection GetSecondTurnNotification() const { return m_secondTurnNotification; }

private:
  std::string GenerateTurnText(uint32_t distanceUnits, uint8_t exitNum, bool useThenInsteadOfDistance,
                               TurnItem const & turn, RouteSegment::RoadNameInfo const & nextStreetInfo) const;

  /// Generates turn sound notification for the nearest to the current position turn.
  std::string GenerateFirstTurnSound(TurnItem const & turn, double distanceToTurnMeters,
                                     RouteSegment::RoadNameInfo const & nextStreetInfo);

  /// Changes the state of the class to emulate that first turn notification is pronounced
  /// without pronunciation.
  void FastForwardFirstTurnNotification();

  /// \param turns contains information about the next turns staring from closest turn.
  /// That means turns[0] is the closest turn (if available).
  /// @return the second closest turn or TurnDirection::NoTurn.
  /// \note If GenerateSecondTurnNotification returns a value different form TurnDirection::NoTurn
  /// for a turn once it will return the same value until the turn is changed.
  /// \note This method works independent from m_enabled value.
  /// So it works when the class enable and disable.
  CarDirection GenerateSecondTurnNotification(std::vector<TurnItemDist> const & turns);

  /// m_enabled == true when tts is turned on.
  /// Important! Clients (iOS/Android) implies that m_enabled is false by default.
  bool m_enabled;

  /// In m_speedMetersPerSecond is intended for some speed which will be used for
  /// conversion a distance in seconds to distance in meters. It could be a current
  /// an end user speed or an average speed for last several seconds.
  /// @TODO(bykoianko) It's better to use an average speed
  /// for last several seconds instead of the current speed here.
  double m_speedMetersPerSecond;

  Settings m_settings;

  /// m_nextTurnNotificationProgress keeps a status which is being changing while
  /// an end user is coming to the closest (the next) turn along the route.
  /// When an end user is far from the next turn
  /// m_nextTurnNotificationProgress == Nothing.
  /// After the first turn notification has been pronounced
  /// m_nextTurnNotificationProgress == First.
  /// After the second notification has been pronounced
  /// m_nextTurnNotificationProgress == Second.
  PronouncedNotification m_nextTurnNotificationProgress;

  /// The flag is set to true if notification about the second turn was pronounced.
  /// It could happen in expressions like "Turn right. Then turn left."
  /// This flag is used to pass the information if second turn notification was pronounced
  /// between two calls of GenerateTurnNotifications() method.
  bool m_turnNotificationWithThen;

  uint32_t m_nextTurnIndex;

  /// getTtsText is a convector form turn notification information and locale to
  /// notification string.
  GetTtsText m_getTtsText;

  /// if m_secondTurnNotification == true it's time to display the second turn notification
  /// visual informer, and false otherwise.
  /// m_secondTurnNotification is a direction of the turn after the closest one
  /// if an end user shall be informed about it. If not, m_secondTurnNotification ==
  /// TurnDirection::NoTurn
  CarDirection m_secondTurnNotification = CarDirection::None;

  /// m_secondTurnNotificationIndex is an index of the closest turn on the route polyline
  /// where m_secondTurnNotification was set to true last time for a turn.
  /// If the closest turn is changed m_secondTurnNotification is set to 0.
  /// \note 0 is a valid index. But in this context it could be considered as invalid
  /// because if firstTurnIndex == 0 that means we're at very beginning of the route
  /// and we're about to making a turn. In that case it's no use to inform a user about
  /// the turn after the next one.
  uint32_t m_secondTurnNotificationIndex;
};
}  // namespace sound
}  // namespace turns
}  // namespace routing
