#pragma once

#include "routing/route.hpp"
#include "routing/routing_callbacks.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/speed_camera.hpp"
#include "routing/turns_notification_manager.hpp"

#include "platform/location.hpp"

#include "base/assert.hpp"
#include "base/thread_checker.hpp"

#include <cstdint>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace routing
{
// Do not change the order, it is used by platforms.
enum class SpeedCameraManagerMode
{
  Auto,
  Always,
  Never,

  MaxValue
};

// This class represents manager for speed cameras. On each changing of position
// called the |OnLocationPositionChanged()| method, that finds cameras on the following
// route and cache them.
// We use next system of notifications:
// 3 Modes:
//   1) Auto - default
//   2) Always
//   3) Never
// In |Auto| mode we warn about cameras only if user has a risk of exceeding speed limit.
// In |Always| mode we warn about cameras no matter user exceed speed limit or not.
// In |Never| we just cache cameras for highlighting in UI (in |Auto|, |Always| make highlighting too).
//
// Also we use different notifications for different zones, see |enum class Interval| for more details.
class SpeedCameraManager
{
public:
  explicit SpeedCameraManager(turns::sound::NotificationManager & notificationManager);

  enum class Interval
  {
    // Influence zone of camera, set by |kInfluenceZoneMeters|.
    ImpactZone,
    // The zone, starting in the |kBeepSignalTime| seconds before ImpactZone and ending at the
    // beginning of ImpactZone.
    BeepSignalZone,
    // The zone where we could use voice notifications. It doesn't have the beginning
    // and end at the beginning of BeepSignalZone.
    VoiceNotificationZone
  };

  static Interval GetIntervalByDistToCam(double distanceToCameraMeters, double speedMpS);

  void SetRoute(std::weak_ptr<Route> route) { m_route = std::move(route); }
  void SetSpeedCamShowCallback(SpeedCameraShowCallback && callback)
  {
    m_speedCamShowCallback = std::move(callback);
  }

  void SetSpeedCamClearCallback(SpeedCameraClearCallback && callback)
  {
    m_speedCamClearCallback = std::move(callback);
  }

  bool Enable() const { return m_mode != SpeedCameraManagerMode::Never; }
  void OnLocationPositionChanged(location::GpsInfo const & info);

  // See comments in |enum class Interval|
  /// \brief |GenerateNotifications| about |Interval::VoiceNotificationZone|.
  void GenerateNotifications(std::vector<std::string> & notifications);
  /// \brief |ShouldPlayBeepSignal| about |Interval::BeepSignalZone|.
  bool ShouldPlayBeepSignal();

  void ResetNotifications();
  void Reset();

  void SetMode(SpeedCameraManagerMode mode);
  SpeedCameraManagerMode GetMode() const { return m_mode; }
  SpeedCameraOnRoute const & GetClosestCamForTests() const { return m_closestCamera; }
  bool IsSpeedLimitExceeded() const;

private:
  // According to https://en.wikibooks.org/wiki/Physics_Study_Guide/Frictional_coefficients
  // average friction of rubber on asphalt is 0.68
  //
  // According to: https://en.wikipedia.org/wiki/Braking_distance
  // a =- \mu * g, where \mu - average friction of rubber on asphalt.
  static double constexpr kAverageFrictionOfRubberOnAspalt = 0.68;
  static double constexpr kGravitationalAcceleration = 9.80655;

  // Used for calculating distance needed to slow down before a speed camera.
  static double constexpr kAverageAccelerationOfBraking =
    -kAverageFrictionOfRubberOnAspalt * kGravitationalAcceleration; // Meters per second squared.

  static_assert(kAverageAccelerationOfBraking != 0, "");
  static_assert(kAverageAccelerationOfBraking < 0, "AverageAccelerationOfBraking must be negative");

  // We highlight camera in UI if the closest camera is placed in |kShowCameraDistanceM|
  // from us.
  static double constexpr kShowCameraDistanceM = 1000.0;

  // Additional time for user about make a decision about slow down.
  // From |https://en.wikipedia.org/wiki/Braking_distance| it's equal 2 seconds on average.
  // But in order to make VoiceNotification earlier, we make it more than 2 seconds.
  static double constexpr kTimeForDecision = 6.0;

  // Distance that we use for look ahead to search camera on the route.
  static double constexpr kLookAheadDistanceMeters = 2000.0;

  static double constexpr kDistanceEpsilonMeters = 10.0;
  static double constexpr kDistToReduceSpeedBeforeUnknownCameraM = 150.0;
  static double constexpr kInfluenceZoneMeters = 450.0;  // Influence zone of speed camera.
  // With this constant we calculate the distance for beep signal.
  // If beep signal happend, it must be before |kBeepSignalTime| seconds
  // of entering to the camera influence zone - |kInfluenceZoneMeters|.
  static double constexpr kBeepSignalTime = 2.0;

  // Number of notifications for different types.
  static uint32_t constexpr kVoiceNotificationNumber = 1;
  static uint32_t constexpr kBeepSignalNumber = 1;

  void FindCamerasOnRouteAndCache(double passedDistanceMeters);

  void PassClosestCameraToUI();

  bool SetNotificationFlags(double passedDistanceMeters, double speedMpS, SpeedCameraOnRoute const & camera);
  bool IsSpeedHigh(double distanceToCameraMeters, double speedMpS, SpeedCameraOnRoute const & camera) const;

  /// \brief Returns true if we should change |m_closestCamera| to |nextCamera|. False otherwise.
  bool NeedToUpdateClosestCamera(double passedDistanceMeters, double speedMpS, SpeedCameraOnRoute const & nextCamera);
  bool IsHighlightedCameraExpired(double distToCameraMeters) const;
  bool IsCameraCloseEnough(double distToCameraMeters) const;

  /// \brief Send stat to aloha.
  void SendNotificationStat(double passedDistanceMeters, double speedMpS, SpeedCameraOnRoute const & camera);
  void SendEnterZoneStat(double distToCameraMeters, double speedMpS, SpeedCameraOnRoute const & camera);

  bool BeepSignalAvailable() const { return m_makeBeepSignal && m_beepSignalCounter < kBeepSignalNumber; }
  bool VoiceSignalAvailable() const { return m_makeVoiceSignal && m_voiceSignalCounter < kVoiceNotificationNumber; }

private:
  SpeedCameraOnRoute m_closestCamera;
  uint32_t m_beepSignalCounter;
  uint32_t m_voiceSignalCounter;

  // Equals true, after the first time user appear in |Interval::ImpactZone|.
  // And false after |m_closestCamera| become invalid.
  bool m_hasEnteredTheZone;

  // Flag of doing sound notification about camera on a way.
  bool m_makeBeepSignal;
  bool m_makeVoiceSignal;

  // Flag if we exceed speed limit now.
  bool m_speedLimitExceeded;

  // Queue of speedCams, that we have found, but they are too far, to make warning about them.
  std::queue<SpeedCameraOnRoute> m_cachedSpeedCameras;

  size_t m_firstNotCheckedSpeedCameraIndex;
  std::weak_ptr<Route> m_route;
  turns::sound::NotificationManager & m_notificationManager;

  SpeedCameraShowCallback m_speedCamShowCallback = [](m2::PointD const & /* point */,
                                                      double /* cameraSpeedKmPH */) {};
  SpeedCameraClearCallback m_speedCamClearCallback = []() {};

  SpeedCameraManagerMode m_mode = SpeedCameraManagerMode::Auto;

  DECLARE_THREAD_CHECKER(m_threadChecker);
};

std::string SpeedCameraManagerModeForStat(SpeedCameraManagerMode mode);

std::string DebugPrint(SpeedCameraManager::Interval interval);
std::string DebugPrint(SpeedCameraManagerMode mode);
}  // namespace routing
