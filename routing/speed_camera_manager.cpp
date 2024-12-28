#include "routing/speed_camera_manager.hpp"

#include "routing/speed_camera.hpp"

#include <cmath>

namespace routing
{
std::string_view constexpr kSpeedCamModeKey = "speed_cam_mode";

SpeedCameraManager::SpeedCameraManager(turns::sound::NotificationManager & notificationManager)
  : m_notificationManager(notificationManager)
{
  Reset();

  uint32_t mode = 0;
  if (settings::Get(kSpeedCamModeKey, mode))
  {
    CHECK_LESS(mode, static_cast<int>(SpeedCameraManagerMode::MaxValue), ("Invalid speedcam mode."));
    m_mode = static_cast<SpeedCameraManagerMode>(mode);
  }
}

//static
SpeedCameraManager::Interval
SpeedCameraManager::GetIntervalByDistToCam(double distanceToCameraMeters, double speedMpS)
{
  if (distanceToCameraMeters < kInfluenceZoneMeters)
    return Interval::ImpactZone;

  double const beepDist = kBeepSignalTime * speedMpS;
  if (distanceToCameraMeters < kInfluenceZoneMeters + beepDist)
    return Interval::BeepSignalZone;

  return Interval::VoiceNotificationZone;
}

void SpeedCameraManager::OnLocationPositionChanged(location::GpsInfo const & info)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());
  CHECK(!m_route.expired(), ());

  auto const passedDistanceMeters = m_route.lock()->GetCurrentDistanceFromBeginMeters();

  // Step 1. Find new cameras and cache them.
  FindCamerasOnRouteAndCache(passedDistanceMeters);

  double distFromCurrentPosAndClosestCam = -1.0;
  m_speedLimitExceeded = false;
  if (m_closestCamera.IsValid())
  {
    distFromCurrentPosAndClosestCam = m_closestCamera.m_distFromBeginMeters - passedDistanceMeters;
    if (distFromCurrentPosAndClosestCam < -kInfluenceZoneMeters)
    {
      m_closestCamera.Invalidate();
      m_speedCamClearCallback();
    }
    else if (!m_closestCamera.NoSpeed())
    {
      m_speedLimitExceeded = IsSpeedHigh(distFromCurrentPosAndClosestCam, info.m_speed, m_closestCamera);
    }
  }

  // Step 2. Check cached cameras. Do it only after pass through closest camera.
  if (!m_cachedSpeedCameras.empty() && distFromCurrentPosAndClosestCam < 0)
  {
    // Do not use reference here, because ProcessCameraWarning() can
    // invalidate |closestSpeedCam|.
    auto const closestSpeedCam = m_cachedSpeedCameras.front();

    if (NeedToUpdateClosestCamera(passedDistanceMeters, info.m_speed, closestSpeedCam))
    {
      m_closestCamera = closestSpeedCam;
      ResetNotifications();
      m_cachedSpeedCameras.pop();
      PassClosestCameraToUI();
    }
  }

  if (m_closestCamera.IsValid() &&
      SetNotificationFlags(passedDistanceMeters, info.m_speed, m_closestCamera))
  {
    // If some notifications available now.
    SendNotificationStat(passedDistanceMeters, info.m_speed, m_closestCamera);
  }
}

void SpeedCameraManager::GenerateNotifications(std::vector<std::string> & notifications)
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  if (!Enable())
    return;

  if (VoiceSignalAvailable())
  {
    notifications.emplace_back(m_notificationManager.GenerateSpeedCameraText());
    ++m_voiceSignalCounter;
  }

  m_makeVoiceSignal = false;
}

bool SpeedCameraManager::ShouldPlayBeepSignal()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  if (!Enable())
    return false;

  if (BeepSignalAvailable())
  {
    m_makeBeepSignal = false;
    ++m_beepSignalCounter;
    return true;
  }

  m_makeBeepSignal = false;
  return false;
}

void SpeedCameraManager::ResetNotifications()
{
  m_makeVoiceSignal = false;
  m_makeBeepSignal = false;
  m_speedLimitExceeded = false;
  m_beepSignalCounter = 0;
  m_voiceSignalCounter = 0;
  m_hasEnteredTheZone = false;
}

void SpeedCameraManager::Reset()
{
  CHECK_THREAD_CHECKER(m_threadChecker, ());

  ResetNotifications();

  m_speedCamClearCallback();

  m_closestCamera.Invalidate();

  m_firstNotCheckedSpeedCameraIndex = 1;
  m_cachedSpeedCameras = std::queue<SpeedCameraOnRoute>();
}

bool SpeedCameraManager::IsSpeedLimitExceeded() const
{
  if (!m_closestCamera.IsValid())
    return false;

  return m_speedLimitExceeded;
}

std::string SpeedCameraManagerModeForStat(SpeedCameraManagerMode mode)
{
  CHECK_NOT_EQUAL(mode, SpeedCameraManagerMode::MaxValue, ());

  switch(mode)
  {
  case SpeedCameraManagerMode::Always: return "1";
  case SpeedCameraManagerMode::Auto: return "0";
  case SpeedCameraManagerMode::Never: return "-1";
  case SpeedCameraManagerMode::MaxValue: return "-2";
  }

  UNREACHABLE();
}

void SpeedCameraManager::FindCamerasOnRouteAndCache(double passedDistanceMeters)
{
  CHECK(!m_route.expired(), ());

  auto const & segments = m_route.lock()->GetRouteSegments();
  size_t firstNotChecked = m_firstNotCheckedSpeedCameraIndex;
  if (firstNotChecked == segments.size())
    return;

  CHECK_LESS(firstNotChecked, segments.size(), ());

  double distToPrevSegment = segments[firstNotChecked].GetDistFromBeginningMeters();
  double distFromCurPosToLatestCheckedSegmentM = distToPrevSegment - passedDistanceMeters;

  while (firstNotChecked < segments.size() &&
         distFromCurPosToLatestCheckedSegmentM < kLookAheadDistanceMeters)
  {
    CHECK_GREATER(firstNotChecked, 0, ());

    auto const & lastSegment = segments[firstNotChecked];
    auto const & prevSegment = segments[firstNotChecked - 1];

    auto const & endPoint = lastSegment.GetJunction().GetPoint();
    auto const & startPoint = prevSegment.GetJunction().GetPoint();
    auto const direction = endPoint - startPoint;

    auto const & speedCamsVector = lastSegment.GetSpeedCams();
    double segmentLength = m_route.lock()->GetSegLenMeters(firstNotChecked);

    for (auto const & speedCam : speedCamsVector)
    {
      segmentLength *= speedCam.m_coef;
      m_cachedSpeedCameras.emplace(distToPrevSegment + segmentLength, speedCam.m_maxSpeedKmPH,
                                   startPoint + direction * speedCam.m_coef);
    }

    distToPrevSegment = lastSegment.GetDistFromBeginningMeters();
    distFromCurPosToLatestCheckedSegmentM = distToPrevSegment - passedDistanceMeters;
    ++firstNotChecked;
  }

  m_firstNotCheckedSpeedCameraIndex = firstNotChecked;
}

void SpeedCameraManager::PassClosestCameraToUI()
{
  CHECK(m_closestCamera.IsValid(), ("Attempt to show invalid speed cam"));
  // Clear previous speed cam in UI.
  m_speedCamClearCallback();

  if (Enable())
    m_speedCamShowCallback(m_closestCamera.m_position, m_closestCamera.m_maxSpeedKmH);
}

bool SpeedCameraManager::IsSpeedHigh(double distanceToCameraMeters, double speedMpS,
                                     SpeedCameraOnRoute const & camera) const
{
  if (camera.NoSpeed())
    return distanceToCameraMeters < kInfluenceZoneMeters + kDistToReduceSpeedBeforeUnknownCameraM;

  double const distToDangerousZone = std::abs(distanceToCameraMeters) - kInfluenceZoneMeters;

  if (distToDangerousZone < 0)
  {
    if (distToDangerousZone < -kInfluenceZoneMeters)
      return false;

    return speedMpS > measurement_utils::KmphToMps(camera.m_maxSpeedKmH);
  }

  if (speedMpS < measurement_utils::KmphToMps(camera.m_maxSpeedKmH))
    return false;

  double timeToSlowSpeed =
    (measurement_utils::KmphToMps(camera.m_maxSpeedKmH) - speedMpS) / kAverageAccelerationOfBraking;

  // Look to: https://en.wikipedia.org/wiki/Acceleration#Uniform_acceleration
  // S = V_0 * t + at^2 / 2, where
  //   V_0 - current speed
  //   a - kAverageAccelerationOfBraking
  double distanceNeedsToSlowDown = timeToSlowSpeed * speedMpS +
                                   (kAverageAccelerationOfBraking * timeToSlowSpeed * timeToSlowSpeed) / 2;
  distanceNeedsToSlowDown += kTimeForDecision * speedMpS;

  return distToDangerousZone < distanceNeedsToSlowDown + kDistanceEpsilonMeters;
}

bool SpeedCameraManager::SetNotificationFlags(double passedDistanceMeters, double speedMpS,
                                              SpeedCameraOnRoute const & camera)
{
  if (!Enable())
    return false;

  auto const distToCameraMeters = camera.m_distFromBeginMeters - passedDistanceMeters;
  // We should reset signal type flags before setting one of them.
  m_makeBeepSignal = false;
  m_makeVoiceSignal = false;

  Interval interval = SpeedCameraManager::GetIntervalByDistToCam(distToCameraMeters, speedMpS);
  switch (interval)
  {
  case Interval::ImpactZone:
  {
    SendEnterZoneStat(distToCameraMeters, speedMpS, camera);

    if (IsSpeedHigh(distToCameraMeters, speedMpS, camera))
    {
      m_makeBeepSignal = true;
      return true;
    }

    // If we did voice notification, and didn't beep signal in |BeepSignalZone|, let's do it now.
    // Only for Auto mode.
    if (m_voiceSignalCounter > 0 && m_beepSignalCounter == 0 &&
        m_mode == SpeedCameraManagerMode::Auto)
    {
      m_makeBeepSignal = true;
      return true;
    }

    if (m_mode == SpeedCameraManagerMode::Always)
    {
      m_makeVoiceSignal = true;
      return true;
    }

    return false;
  }
  case Interval::BeepSignalZone:
  {
    // If we exceeding speed limit, in |BeepSignalZone|, we should make "beep" signal.
    if (IsSpeedHigh(distToCameraMeters, speedMpS, camera))
    {
      m_makeBeepSignal = true;
      return true;
    }

    // If we did voice notification, we should do "beep" signal before |ImpactZone|.
    if (m_voiceSignalCounter > 0)
    {
      m_makeBeepSignal = true;
      return true;
    }

    return false;
  }
  case Interval::VoiceNotificationZone:
  {
    // If we exceeding speed limit, in |VoiceNotificationZone|, we should make "voice"
    // notification.
    if (IsSpeedHigh(distToCameraMeters, speedMpS, camera))
    {
      m_makeVoiceSignal = true;
      return true;
    }

    return false;
  }
  }

  UNREACHABLE();
}

bool SpeedCameraManager::NeedToUpdateClosestCamera(double passedDistanceMeters, double speedMpS,
                                                   SpeedCameraOnRoute const & nextCamera)
{
  auto const distToNewCameraMeters = nextCamera.m_distFromBeginMeters - passedDistanceMeters;
  if (m_closestCamera.IsValid())
  {
    auto const distToOldCameraMeters = m_closestCamera.m_distFromBeginMeters - passedDistanceMeters;

    // If we passed the previous nextCamera and the next is close enough to work with it.
    return distToOldCameraMeters < 0 && IsCameraCloseEnough(distToNewCameraMeters);
  }

  return IsCameraCloseEnough(distToNewCameraMeters);
}

bool SpeedCameraManager::IsHighlightedCameraExpired(double distToCameraMeters) const
{
  return distToCameraMeters < -kInfluenceZoneMeters;
}

bool SpeedCameraManager::IsCameraCloseEnough(double distToCameraMeters) const
{
  return -kInfluenceZoneMeters < distToCameraMeters && distToCameraMeters < kShowCameraDistanceM;
}

void SpeedCameraManager::SendNotificationStat(double passedDistanceMeters, double speedMpS,
                                              SpeedCameraOnRoute const & camera)
{
  // Send stat about notification only when we are going to pronounce it.
  if (!BeepSignalAvailable() && !VoiceSignalAvailable())
    return;

  auto const distToCameraMeters = camera.m_distFromBeginMeters - passedDistanceMeters;

  CHECK(
      m_makeBeepSignal != m_makeVoiceSignal,
      ("In each moment of time only one flag should be up.", m_makeVoiceSignal, distToCameraMeters,
       measurement_utils::MpsToKmph(speedMpS), m_beepSignalCounter, m_voiceSignalCounter,
       m_hasEnteredTheZone, m_speedLimitExceeded, m_firstNotCheckedSpeedCameraIndex,
       mercator::ToLatLon(camera.m_position)));
}

void SpeedCameraManager::SendEnterZoneStat(double distToCameraMeters, double speedMpS,
                                           SpeedCameraOnRoute const & camera)
{
  using strings::to_string;

  if (m_hasEnteredTheZone)
    return;
  m_hasEnteredTheZone = true;
}

void SpeedCameraManager::SetMode(SpeedCameraManagerMode mode)
{
  m_mode = mode;
  settings::Set(kSpeedCamModeKey, static_cast<int>(mode));
}

std::string DebugPrint(SpeedCameraManager::Interval interval)
{
  switch (interval)
  {
  case SpeedCameraManager::Interval::BeepSignalZone: return "BeepSignalZone";
  case SpeedCameraManager::Interval::VoiceNotificationZone: return "VoiceNotificationZone";
  case SpeedCameraManager::Interval::ImpactZone: return "ImpactZone";
  }

  UNREACHABLE();
}

std::string DebugPrint(SpeedCameraManagerMode mode)
{
  switch (mode)
  {
  case SpeedCameraManagerMode::Auto:
    return "auto";
  case SpeedCameraManagerMode::Always:
    return "always";
  case SpeedCameraManagerMode::Never:
    return "never";
  case SpeedCameraManagerMode::MaxValue:
    return "max_value";
  }

  UNREACHABLE();
}
}  // namespace routing
