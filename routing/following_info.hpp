#pragma once

#include "geometry/latlon.hpp"

#include "platform/distance.hpp"

#include "routing/turns.hpp"
#include "routing/turns_sound_settings.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace routing
{
class FollowingInfo
{
public:
  FollowingInfo()
    : m_turn(turns::CarDirection::None)
    , m_nextTurn(turns::CarDirection::None)
    , m_exitNum(0)
    , m_time(0)
    , m_completionPercent(0)
    , m_pedestrianTurn(turns::PedestrianDirection::None)
  {
  }

  bool IsValid() const { return m_distToTarget.IsValid(); }

  /// @name Formatted covered distance.
  platform::Distance m_distToTarget;

  /// @name Formatted distance to the next turn.
  //@{
  platform::Distance m_distToTurn;
  turns::CarDirection m_turn;
  /// Turn after m_turn. Returns NoTurn if there is no turns after.
  turns::CarDirection m_nextTurn;
  uint32_t m_exitNum;
  //@}
  int m_time;
  // m_lanes contains lane information on the edge before the turn.
  turns::lanes::LanesInfo m_lanes;
  // m_turnNotifications contains information about the next turn notifications.
  // If there is nothing to pronounce m_turnNotifications is empty.
  // If there is something to pronounce the size of m_turnNotifications may be one or even more
  // depends on the number of notifications to prononce.
  std::vector<std::string> m_turnNotifications;
  // Current street name.
  std::string m_sourceName;
  // The next street name.
  std::string m_targetName;
  // Street name to display. May be empty.
  std::string m_displayedStreetName;

  // Percentage of the route completion.
  double m_completionPercent;

  /// @name Pedestrian direction information
  //@{
  turns::PedestrianDirection m_pedestrianTurn;
  //@}

  // Current speed limit in meters per second.
  // If no info about speed limit then m_speedLimitMps < 0.
  double m_speedLimitMps = -1.0;
};
}  // namespace routing
