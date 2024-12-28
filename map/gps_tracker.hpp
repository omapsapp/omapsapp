#pragma once

#include "map/gps_track.hpp"

#include <atomic>
#include <functional>
#include <utility>
#include <vector>

class GpsTracker
{
public:
  static GpsTracker & Instance();

  bool IsEnabled() const;
  void SetEnabled(bool enabled);

  bool IsEmpty() const;
  size_t GetTrackSize() const;
  GpsTrackInfo GetTrackInfo() const { return m_track.GetTrackInfo(); }

  using TGpsTrackDiffCallback =
      std::function<void(std::vector<std::pair<size_t, location::GpsInfo>> && toAdd,
                         std::pair<size_t, size_t> const & toRemove,
                         GpsTrackInfo const & trackInfo)>;

  void Connect(TGpsTrackDiffCallback const & fn);
  void Disconnect();

  void OnLocationUpdated(location::GpsInfo const & info);

  using GpsTrackCallback = std::function<bool(location::GpsInfo const &, size_t)>;
  void ForEachTrackPoint(GpsTrackCallback const & callback) const;

private:
  GpsTracker();

  std::atomic<bool> m_enabled;
  GpsTrack m_track;
};
