#include "map/gps_tracker.hpp"
#include "map/framework.hpp"

#include "platform/platform.hpp"

#include "base/file_name_utils.hpp"

#include <string>

#include "defines.hpp"

using namespace std::chrono;

namespace
{

char const kEnabledKey[] = "GpsTrackingEnabled";

inline std::string GetFilePath()
{
  return base::JoinPath(GetPlatform().WritableDir(), GPS_TRACK_FILENAME);
}

inline bool GetSettingsIsEnabled()
{
  bool enabled;
  if (!settings::Get(kEnabledKey, enabled))
    enabled = false;
  return enabled;
}

inline void SetSettingsIsEnabled(bool enabled)
{
  settings::Set(kEnabledKey, enabled);
}

} // namespace

GpsTracker & GpsTracker::Instance()
{
  static GpsTracker instance;
  return instance;
}

GpsTracker::GpsTracker()
  : m_enabled(GetSettingsIsEnabled())
  , m_track(GetFilePath(), std::make_unique<GpsTrackFilter>())
{
}

void GpsTracker::SetEnabled(bool enabled)
{  
  if (enabled == m_enabled)
    return;

  SetSettingsIsEnabled(enabled);
  m_enabled = enabled;

  if (enabled)
    m_track.Clear();
}

bool GpsTracker::IsEnabled() const
{
  return m_enabled;
}

bool GpsTracker::IsEmpty() const
{
  return m_track.IsEmpty();
}

size_t GpsTracker::GetTrackSize() const
{
  return m_track.GetSize();
}

void GpsTracker::Connect(TGpsTrackDiffCallback const & fn)
{
  m_track.SetCallback(fn);
}

void GpsTracker::Disconnect()
{
  m_track.SetCallback(nullptr);
}

void GpsTracker::OnLocationUpdated(location::GpsInfo const & info)
{
  if (!m_enabled)
    return;
  m_track.AddPoint(info);
}

void GpsTracker::ForEachTrackPoint(GpsTrackCallback const & callback) const
{
  CHECK(callback != nullptr, ("Callback should be provided"));
  m_track.ForEachPoint(callback);
}
