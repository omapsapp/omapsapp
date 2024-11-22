#include "map/extrapolation/extrapolator.hpp"

#include "geometry/distance_on_sphere.hpp"

#include "platform/platform.hpp"

#include "base/logging.hpp"
#include "base/math.hpp"

#include <chrono>
#include <memory>

namespace
{
// If the difference of values between two instances of GpsInfo is greater
// than the appropriate constant below the extrapolator will be switched off for
// these two instances of GpsInfo.
double constexpr kMaxExtrapolationSpeedMPS = 75.0;
double constexpr kMaxExtrapolationDistMeters = 100.0;
double constexpr kMaxExtrapolationTimeSeconds = 2.1;

class LinearExtrapolator
{
public:
  LinearExtrapolator(uint64_t timeBetweenMs, uint64_t timeAfterMs)
    : m_timeBetweenMs(timeBetweenMs), m_timeAfterMs(timeAfterMs)
  {
    CHECK_NOT_EQUAL(m_timeBetweenMs, 0, ());
  }

  double Extrapolate(double x1, double x2) const
  {
    return x2 + ((x2 - x1) / m_timeBetweenMs) * m_timeAfterMs;
  }

private:
  uint64_t m_timeBetweenMs;
  uint64_t m_timeAfterMs;
};
}  // namespace

namespace extrapolation
{
using namespace std;

location::GpsInfo LinearExtrapolation(location::GpsInfo const & gpsInfo1,
                                      location::GpsInfo const & gpsInfo2,
                                      uint64_t timeAfterPoint2Ms)
{
  if (gpsInfo2.m_timestamp <= gpsInfo1.m_timestamp)
  {
    ASSERT(false, ("Incorrect gps data"));
    return gpsInfo2;
  }

  auto const timeBetweenPointsMs =
      static_cast<uint64_t>((gpsInfo2.m_timestamp - gpsInfo1.m_timestamp) * 1000);
  if (timeBetweenPointsMs == 0)
    return gpsInfo2;

  location::GpsInfo result = gpsInfo2;
  LinearExtrapolator const e(timeBetweenPointsMs, timeAfterPoint2Ms);

  result.m_timestamp += static_cast<double>(timeAfterPoint2Ms) / 1000.0;
  result.m_longitude =
      math::Clamp(e.Extrapolate(gpsInfo1.m_longitude, gpsInfo2.m_longitude), -180.0, 180.0);
  result.m_latitude =
      math::Clamp(e.Extrapolate(gpsInfo1.m_latitude, gpsInfo2.m_latitude), -90.0, 90.0);
  result.m_altitude = e.Extrapolate(gpsInfo1.m_altitude, gpsInfo2.m_altitude);

  // @TODO(bykoianko) Now |result.m_bearing| == |gpsInfo2.m_bearing|.
  // In case of |gpsInfo1.HasBearing() && gpsInfo2.HasBearing() == true|
  // consider finding an average value between |gpsInfo1.m_bearing| and |gpsInfo2.m_bearing|
  // taking into account that they are periodic. It's important to implement it
  // because current implementation leads to changing course by steps. It doesn't
  // look nice when the road changes its direction.

  if (gpsInfo1.HasSpeed() && gpsInfo2.HasSpeed())
    result.m_speed = e.Extrapolate(gpsInfo1.m_speed, gpsInfo2.m_speed);

  return result;
}

bool AreCoordsGoodForExtrapolation(location::GpsInfo const & info1, location::GpsInfo const & info2)
{
  if (!info1.IsValid() || !info2.IsValid())
    return false;

  double const distM =
      ms::DistanceOnEarth(info1.m_latitude, info1.m_longitude,
                          info2.m_latitude, info2.m_longitude);

  double const timeS = info2.m_timestamp - info1.m_timestamp;
  if (timeS <= 0.0)
    return false;

  // |maxDistAfterExtrapolationM| is maximum possible distance from |info2| to
  // the furthest extrapolated point.
  double const maxDistAfterExtrapolationM =
      distM * (Extrapolator::kMaxExtrapolationTimeMs / 1000.0) / timeS;
  // |maxDistForAllExtrapolationsM| is maximum possible distance from |info2| to
  // all extrapolated points in any cases.
  double const maxDistForAllExtrapolationsM =
      kMaxExtrapolationSpeedMPS * kMaxExtrapolationTimeSeconds;
  double const distLastGpsInfoToMeridian180 = ms::DistanceOnEarth(
      info2.m_latitude, info2.m_longitude, info2.m_latitude, 180.0 /* lon2Deg */);
  // Switching off extrapolation if |info2| are so close to meridian 180 that extrapolated
  // points may cross meridian 180 or if |info1| and |info2| are located on
  // different sides of meridian 180.
  if (distLastGpsInfoToMeridian180 < maxDistAfterExtrapolationM ||
      (distLastGpsInfoToMeridian180 < maxDistForAllExtrapolationsM &&
       info2.m_longitude * info1.m_longitude < 0.0) ||
      ms::DistanceOnEarth(info2.m_latitude, info2.m_longitude, 90.0 /* lat2Deg */,
                          info2.m_longitude) < maxDistAfterExtrapolationM ||
      ms::DistanceOnEarth(info2.m_latitude, info2.m_longitude, -90.0 /* lat2Deg */,
                          info2.m_longitude) < maxDistAfterExtrapolationM)
  {
    return false;
  }

  // Note. |timeS| may be less than zero. (info1.m_timestampS >=
  // info2.m_timestampS) It may happen in rare cases because GpsInfo::m_timestampS is not
  // monotonic generally. Please see comment in declaration of class GpsInfo for details.

  // @TODO(bykoianko) Switching off extrapolation based on acceleration should be implemented.
  // Switching off extrapolation based on speed, distance and time.
  return distM / timeS <= kMaxExtrapolationSpeedMPS &&
         distM <= kMaxExtrapolationDistMeters && timeS <= kMaxExtrapolationTimeSeconds;
}

// Extrapolator ------------------------------------------------------------------------------------
Extrapolator::Extrapolator(ExtrapolatedLocationUpdateFn const & update)
  : m_isEnabled(false), m_extrapolatedLocationUpdate(update)
{
  RunTaskOnBackgroundThread(false /* delayed */);
}

void Extrapolator::OnLocationUpdate(location::GpsInfo const & gpsInfo)
{
  {
    lock_guard<mutex> guard(m_mutex);
    m_beforeLastGpsInfo = m_lastGpsInfo;
    m_lastGpsInfo = gpsInfo;
    m_consecutiveRuns = 0;
    // Canceling all background tasks which are put to the queue before the task run in this method.
    ++m_locationUpdateCounter;
    m_locationUpdateMinValid = m_locationUpdateCounter;
  }
  RunTaskOnBackgroundThread(false /* delayed */);
}

void Extrapolator::Enable(bool enabled)
{
  lock_guard<mutex> guard(m_mutex);
  m_isEnabled = enabled;
}

void Extrapolator::ExtrapolatedLocationUpdate(uint64_t locationUpdateCounter)
{
  location::GpsInfo gpsInfo;
  {
    lock_guard<mutex> guard(m_mutex);
    // Canceling all calls of the method which were activated before |m_locationUpdateMinValid|.
    if (locationUpdateCounter < m_locationUpdateMinValid)
      return;

    uint64_t const extrapolationTimeMs = kExtrapolationPeriodMs * m_consecutiveRuns;
    if (extrapolationTimeMs < kMaxExtrapolationTimeMs && m_lastGpsInfo.IsValid())
    {
      if (DoesExtrapolationWork())
        gpsInfo = LinearExtrapolation(m_beforeLastGpsInfo, m_lastGpsInfo, extrapolationTimeMs);
      else
        gpsInfo = m_lastGpsInfo;
    }
  }

  if (gpsInfo.IsValid())
  {
    GetPlatform().RunTask(Platform::Thread::Gui,
                          [this, gpsInfo]() { m_extrapolatedLocationUpdate(gpsInfo); });
  }

  {
    lock_guard<mutex> guard(m_mutex);
    if (m_consecutiveRuns != kExtrapolationCounterUndefined)
      ++m_consecutiveRuns;
  }

  // Calling ExtrapolatedLocationUpdate() in |kExtrapolationPeriodMs| milliseconds.
  RunTaskOnBackgroundThread(true /* delayed */);
}

void Extrapolator::RunTaskOnBackgroundThread(bool delayed)
{
  uint64_t locationUpdateCounter = 0;
  {
    lock_guard<mutex> guard(m_mutex);
    locationUpdateCounter = m_locationUpdateCounter;
  }

  if (delayed)
  {
    auto constexpr period = std::chrono::milliseconds(kExtrapolationPeriodMs);
    GetPlatform().RunDelayedTask(Platform::Thread::Background, period, [this, locationUpdateCounter]
    {
      ExtrapolatedLocationUpdate(locationUpdateCounter);
    });
  }
  else
  {
    GetPlatform().RunTask(Platform::Thread::Background, [this, locationUpdateCounter]
    {
      ExtrapolatedLocationUpdate(locationUpdateCounter);
    });
  }
}

bool Extrapolator::DoesExtrapolationWork() const
{
  if (!m_isEnabled || m_consecutiveRuns == kExtrapolationCounterUndefined)
    return false;

  return AreCoordsGoodForExtrapolation(m_beforeLastGpsInfo, m_lastGpsInfo);
}
}  // namespace extrapolation
