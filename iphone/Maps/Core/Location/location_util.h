#pragma once

namespace location_util {

static location::GpsInfo gpsInfoFromLocation(CLLocation * l, location::TLocationSource source)
{
  location::GpsInfo info;
  info.m_source = source;

  info.m_latitude = l.coordinate.latitude;
  info.m_longitude = l.coordinate.longitude;
  info.m_timestamp = l.timestamp.timeIntervalSince1970;

  if (l.horizontalAccuracy >= 0.0)
    info.m_horizontalAccuracy = l.horizontalAccuracy;

  if (l.verticalAccuracy >= 0.0)
  {
    info.m_verticalAccuracy = l.verticalAccuracy;
    info.m_altitude = l.altitude;
  }

  if (l.course >= 0.0)
    info.m_bearing = l.course;

  if (l.speed >= 0.0)
    info.m_speed = l.speed;
  return info;
}

static location::CompassInfo compassInfoFromHeading(CLHeading * h)
{
  location::CompassInfo info;
  if (h.trueHeading >= 0.0)
    info.m_bearing = math::DegToRad(h.trueHeading);
  else if (h.headingAccuracy >= 0.0)
    info.m_bearing = math::DegToRad(h.magneticHeading);
  return info;
}

} // namespace location_util
