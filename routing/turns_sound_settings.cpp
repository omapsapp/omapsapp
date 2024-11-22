#include "routing/turns_sound_settings.hpp"

#include "platform/measurement_utils.hpp"

#include <algorithm>

namespace routing
{
namespace turns
{
namespace sound
{
using namespace std;
using namespace measurement_utils;

void Settings::SetState(uint32_t notificationTimeSeconds, uint32_t minNotificationDistanceUnits,
                        uint32_t maxNotificationDistanceUnits,
                        vector<uint32_t> const & soundedDistancesUnits,
                        measurement_utils::Units lengthUnits)
{
  m_timeSeconds = notificationTimeSeconds;
  m_minDistanceUnits = minNotificationDistanceUnits;
  m_maxDistanceUnits = maxNotificationDistanceUnits;
  m_soundedDistancesUnits = soundedDistancesUnits;
  m_lengthUnits = lengthUnits;
}

bool Settings::IsValid() const
{
  return m_minDistanceUnits <= m_maxDistanceUnits && !m_soundedDistancesUnits.empty() &&
         is_sorted(m_soundedDistancesUnits.cbegin(), m_soundedDistancesUnits.cend());
}

uint32_t Settings::ComputeTurnDistanceM(double speedMetersPerSecond) const
{
  ASSERT(IsValid(), ());

  double const turnNotificationDistanceM = m_timeSeconds * speedMetersPerSecond;
  return static_cast<uint32_t>(math::Clamp(turnNotificationDistanceM,
                                           ConvertUnitsToMeters(m_minDistanceUnits),
                                           ConvertUnitsToMeters(m_maxDistanceUnits)));
}

bool Settings::TooCloseForFisrtNotification(double distToTurnMeters) const
{
  return m_minDistToSayNotificationMeters >= distToTurnMeters;
}

uint32_t Settings::RoundByPresetSoundedDistancesUnits(uint32_t turnNotificationUnits) const
{
  ASSERT(IsValid(), ());

  auto it = upper_bound(m_soundedDistancesUnits.cbegin(), m_soundedDistancesUnits.cend(),
                        turnNotificationUnits, less_equal<uint32_t>());
  // Rounding up the result.
  if (it != m_soundedDistancesUnits.cend())
    return *it;

  ASSERT(false, ("m_soundedDistancesUnits shall contain bigger values."));
  return m_soundedDistancesUnits.empty() ? 0 : m_soundedDistancesUnits.back();
}

double Settings::ConvertMetersPerSecondToUnitsPerSecond(double speedInMetersPerSecond) const
{
  switch (m_lengthUnits)
  {
  case Units::Metric: return speedInMetersPerSecond;
  case Units::Imperial: return MetersToFeet(speedInMetersPerSecond);
  }

  ASSERT(false, ("m_lengthUnits is equal to unknown value."));
  return 0.;
}

double Settings::ConvertUnitsToMeters(double distanceInUnits) const
{
  switch (m_lengthUnits)
  {
  case Units::Metric: return distanceInUnits;
  case Units::Imperial: return FeetToMeters(distanceInUnits);
  }

  ASSERT(false, ());
  return 0.;
}

double Settings::ConvertMetersToUnits(double distanceInMeters) const
{
  switch (m_lengthUnits)
  {
  case Units::Metric: return distanceInMeters;
  case Units::Imperial: return MetersToFeet(distanceInMeters);
  }

  ASSERT(false, ());
  return 0.;
}

uint32_t Settings::ComputeDistToPronounceDistM(double speedMetersPerSecond, bool pedestrian) const
{
  ASSERT_LESS_OR_EQUAL(0, speedMetersPerSecond, ());

  auto const startBeforeSeconds = pedestrian ? m_startBeforeSecondsPedestrian : m_startBeforeSecondsVehicle;
  auto const minStartBeforeMeters = pedestrian ? m_minStartBeforeMetersPedestrian : m_minStartBeforeMetersVehicle;
  auto const maxStartBeforeMeters = pedestrian ? m_maxStartBeforeMetersPedestrian : m_maxStartBeforeMetersVehicle;

  uint32_t const startBeforeMeters =
      static_cast<uint32_t>(speedMetersPerSecond * startBeforeSeconds);

  return math::Clamp(startBeforeMeters, minStartBeforeMeters, maxStartBeforeMeters);
}

string DebugPrint(Notification const & notification)
{
  stringstream out;
  out << "Notification [ m_distanceUnits == " << notification.m_distanceUnits
      << ", m_exitNum == " << notification.m_exitNum
      << ", m_useThenInsteadOfDistance == " << notification.m_useThenInsteadOfDistance
      << ", m_turnDir == " << DebugPrint(notification.m_turnDir)
      << ", m_turnDirPedestrian == " << DebugPrint(notification.m_turnDirPedestrian)
      << ", m_lengthUnits == " << DebugPrint(notification.m_lengthUnits) << " ]" << endl;
  return out.str();
}

VecPairDist const & GetAllSoundedDistMeters()
{
  // The vector below has to be sorted. It is checked in unit test GetAllSoundedDistMetersTest
  static VecPairDist const inst = {{50, "in_50_meters"},
                                   {100, "in_100_meters"},
                                   {200, "in_200_meters"},
                                   {250, "in_250_meters"},
                                   {300, "in_300_meters"},
                                   {400, "in_400_meters"},
                                   {500, "in_500_meters"},
                                   {600, "in_600_meters"},
                                   {700, "in_700_meters"},
                                   {750, "in_750_meters"},    // Hm :)
                                   {800, "in_800_meters"},
                                   {900, "in_900_meters"},
                                   {1000, "in_1_kilometer"},
                                   {1500, "in_1_5_kilometers"},
                                   {2000, "in_2_kilometers"},
                                   {2500, "in_2_5_kilometers"},
                                   {3000, "in_3_kilometers"}};
  return inst;
}

VecPairDist const & GetAllSoundedDistFeet()
{
  // The vector below has to be sorted. It is checked in unit test GetAllSoundedDistFeet
  static VecPairDist const inst = {{50, "in_50_feet"},
                                   {100, "in_100_feet"},
                                   {200, "in_200_feet"},
                                   {300, "in_300_feet"},
                                   {400, "in_400_feet"},
                                   {500, "in_500_feet"},
                                   {600, "in_600_feet"},
                                   {700, "in_700_feet"},
                                   {800, "in_800_feet"},
                                   {900, "in_900_feet"},
                                   {1000, "in_1000_feet"},
                                   {1500, "in_1500_feet"},
                                   {2000, "in_2000_feet"},
                                   {2500, "in_2500_feet"},
                                   {3000, "in_3000_feet"},
                                   {3500, "in_3500_feet"},
                                   {4000, "in_4000_feet"},
                                   {4500, "in_4500_feet"},
                                   {5000, "in_5000_feet"},
                                   {5280, "in_1_mile"},
                                   {7920, "in_1_5_miles"},
                                   {10560, "in_2_miles"}};
  return inst;
}

vector<uint32_t> const & GetSoundedDistMeters()
{
  // The vector has to be sorted. Besides that any of its elements has to be contained in
  // the vector which GetAllSoundedDistMeters() returns.
  // It is checked in the unit test GetSoundedDistMeters.
  static vector<uint32_t> const inst = {200, 300, 400, 500, 600, 700, 800, 900, 1000, 1500, 2000};
  return inst;
}

vector<uint32_t> const & GetSoundedDistFeet()
{
  // The vector has to be sorted. Besides that any of its elements has to be contained in
  // the vector which GetAllSoundedDistFeet() returns.
  // It is checked in the unit test GetSoundedDistFeet.
  static vector<uint32_t> const inst = {500,  600,  700,  800,  900, 1000,
                                        1500, 2000, 3000, 4000, 5000};
  return inst;
}
}  // namespace sound
}  // namespace turns
}  // namespace routing
