#pragma once

#include "map/gps_track_collection.hpp"
#include "map/gps_track_filter.hpp"
#include "map/gps_track_storage.hpp"

#include "base/thread.hpp"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

class GpsTrack final
{
public:
  static size_t const kInvalidId; // = numeric_limits<size_t>::max();

  /// @param filePath - path to the file on disk to persist track
  /// @param filter - filter object used for filtering points, GpsTrackNullFilter is created by default
  GpsTrack(std::string const & filePath, std::unique_ptr<IGpsTrackFilter> && filter = std::unique_ptr<IGpsTrackFilter>());
  ~GpsTrack();

  /// Adds point or collection of points to gps tracking
  /// @note Callback is called with 'toAdd' and 'toRemove' points, if some points were added or removed.
  /// @note Only points with good timestamp will be added, other will be skipped.
  void AddPoint(location::GpsInfo const & point);
  void AddPoints(std::vector<location::GpsInfo> const & points);

  /// Returns track statistics
  GpsTrackInfo GetTrackInfo() const;

  /// Clears any previous tracking info
  /// @note Callback is called with 'toRemove' points, if some points were removed.
  void Clear();

  bool IsEmpty() const;
  size_t GetSize() const;

  /// Notification callback about a change of the gps track.
  /// @param toAdd - collection of points and ids to add.
  /// @param toRemove - range of point indices to remove, or pair(kInvalidId,kInvalidId) if nothing to remove
  /// @note Calling of a GpsTrack.SetCallback function from the callback causes deadlock.
  using TGpsTrackDiffCallback =
      std::function<void(std::vector<std::pair<size_t, location::GpsInfo>> && toAdd,
                         std::pair<size_t, size_t> const & toRemove,
                         GpsTrackInfo const & trackInfo)>;

  /// Sets callback on change of gps track.
  /// @param callback - callback callable object
  /// @note Only one callback is supported at time.
  /// @note When sink is attached, it receives all points in 'toAdd' at first time,
  /// next time callbacks it receives only modifications. It simplifies getter/callback model.
  void SetCallback(TGpsTrackDiffCallback callback);

  template <typename F>
  void ForEachPoint(F && f) const
  {
    m_collection->ForEach(std::move(f));
  }

private:
  DISALLOW_COPY_AND_MOVE(GpsTrack);

  void ScheduleTask();
  void ProcessPoints(); // called on the worker thread
  bool HasCallback();
  void InitStorageIfNeed();
  void InitCollection();
  void UpdateStorage(bool needClear, std::vector<location::GpsInfo> const & points);
  void UpdateCollection(bool needClear,
                        std::vector<location::GpsInfo> const & points,
                        std::pair<size_t, size_t> & addedIds,
                        std::pair<size_t, size_t> & evictedIds);
  void NotifyCallback(std::pair<size_t, size_t> const & addedIds,
                      std::pair<size_t, size_t> const & evictedIds);

  std::string const m_filePath;

  mutable std::mutex m_dataGuard;           // protects data for stealing
  std::vector<location::GpsInfo> m_points;  // accumulated points for adding
  bool m_needClear; // need clear file

  std::mutex m_callbackGuard;
  // Callback is protected by m_callbackGuard. It ensures that SetCallback and call callback
  // will not be interleaved and after SetCallback(null) callback is never called. The negative side
  // is that GpsTrack.SetCallback must be never called from the callback.
  TGpsTrackDiffCallback m_callback;
  bool m_needSendSnapshop; // need send initial snapshot

  std::unique_ptr<GpsTrackStorage> m_storage;        // used in the worker thread
  std::unique_ptr<GpsTrackCollection> m_collection;  // used in the worker thread
  std::unique_ptr<IGpsTrackFilter> m_filter;         // used in the worker thread

  std::mutex m_threadGuard;
  threads::SimpleThread m_thread;
  bool m_threadExit; // need exit thread
  bool m_threadWakeup; // need wakeup thread
  std::condition_variable m_cv;
};
