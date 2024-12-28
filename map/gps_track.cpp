#include "map/gps_track.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <algorithm>

using namespace std;
using namespace std::chrono;

namespace gps_track
{

inline pair<size_t, size_t> UnionRanges(pair<size_t, size_t> const & a, pair<size_t, size_t> const & b)
{
  if (a.first == GpsTrack::kInvalidId)
  {
    ASSERT_EQUAL(a.second, GpsTrack::kInvalidId, ());
    return b;
  }
  if (b.first == GpsTrack::kInvalidId)
  {
    ASSERT_EQUAL(b.second, GpsTrack::kInvalidId, ());
    return a;
  }
  ASSERT_LESS_OR_EQUAL(a.first, a.second, ());
  ASSERT_LESS_OR_EQUAL(b.first, b.second, ());
  return make_pair(min(a.first, b.first), max(a.second, b.second));
}

size_t constexpr kItemBlockSize = 1000;

} // namespace gps_track

size_t const GpsTrack::kInvalidId = GpsTrackCollection::kInvalidId;

GpsTrack::GpsTrack(string const & filePath, unique_ptr<IGpsTrackFilter> && filter)
  : m_filePath(filePath)
  , m_needClear(false)
  , m_needSendSnapshop(false)
  , m_filter(std::move(filter))
  , m_threadExit(false)
  , m_threadWakeup(false)
{
  if (!m_filter)
    m_filter = make_unique<GpsTrackNullFilter>();

  ASSERT(!m_filePath.empty(), ());
}

GpsTrack::~GpsTrack()
{
  if (m_thread.joinable())
  {
    {
      lock_guard<mutex> lg(m_threadGuard);
      m_threadExit = true;
      m_cv.notify_one();
    }
    m_thread.join();
  }
}

void GpsTrack::AddPoint(location::GpsInfo const & point)
{
  {
    std::lock_guard lg(m_dataGuard);
    m_points.emplace_back(point);
  }
  ScheduleTask();
}

void GpsTrack::AddPoints(vector<location::GpsInfo> const & points)
{
  {
    std::lock_guard lg(m_dataGuard);
    m_points.insert(m_points.end(), points.begin(), points.end());
  }
  ScheduleTask();
}

GpsTrackInfo GpsTrack::GetTrackInfo() const
{
  return m_collection ? m_collection->GetTrackInfo() : GpsTrackInfo();
}

void GpsTrack::Clear()
{
  {
    std::lock_guard lg(m_dataGuard);
    m_points.clear();
    m_needClear = true;
  }
  ScheduleTask();
}

size_t GpsTrack::GetSize() const
{
  CHECK(m_collection != nullptr, ());
  return m_collection->GetSize();
}

bool GpsTrack::IsEmpty() const
{
  if (!m_collection)
    return true;
  return m_collection->IsEmpty();
}

void GpsTrack::SetCallback(TGpsTrackDiffCallback callback)
{
  {
    lock_guard<mutex> lg(m_callbackGuard);
    m_callback = callback;
    m_needSendSnapshop = true;
  }
  ScheduleTask();
}

void GpsTrack::ScheduleTask()
{
  lock_guard<mutex> lg(m_threadGuard);

  if (m_thread.get_id() == std::thread::id())
  {
    m_thread = threads::SimpleThread([this]()
    {
      unique_lock<mutex> ul(m_threadGuard);
      while (true)
      {
        m_cv.wait(ul, [this]()->bool{ return m_threadExit || m_threadWakeup; });
        if (m_threadExit)
          break;
        m_threadWakeup = false;
        ProcessPoints();
      }

      m_storage.reset();
    });
  }

  m_threadWakeup = true;
  m_cv.notify_one();
}

void GpsTrack::InitStorageIfNeed()
{
  if (m_storage)
    return;

  try
  {
    m_storage = make_unique<GpsTrackStorage>(m_filePath);
  }
  catch (RootException const & e)
  {
    LOG(LWARNING, ("Track storage creation error:", e.Msg()));
  }
}

void GpsTrack::InitCollection()
{
  ASSERT(m_collection == nullptr, ());

  m_collection = make_unique<GpsTrackCollection>();

  InitStorageIfNeed();
  if (!m_storage)
    return;

  try
  {
    // All origin points have been written in the storage,
    // and filtered points are inserted in the runtime collection.

    vector<location::GpsInfo> originPoints;
    originPoints.reserve(gps_track::kItemBlockSize);

    m_storage->ForEach([this, &originPoints](location::GpsInfo const & originPoint)->bool
    {
      originPoints.emplace_back(originPoint);
      if (originPoints.size() == originPoints.capacity())
      {
        vector<location::GpsInfo> points;
        m_filter->Process(originPoints, points);

        m_collection->Add(points);

        originPoints.clear();
      }
      return true;
    });

    if (!originPoints.empty())
    {
      vector<location::GpsInfo> points;
      m_filter->Process(originPoints, points);

      m_collection->Add(points);
    }
  }
  catch (RootException const & e)
  {
    LOG(LWARNING, ("Track storage exception:", e.Msg()));
    m_collection->Clear();
    m_storage.reset();
  }
}

void GpsTrack::ProcessPoints()
{
  vector<location::GpsInfo> originPoints;
  bool needClear;
  // Steal data for processing
  {
    std::lock_guard lg(m_dataGuard);
    originPoints.swap(m_points);
    needClear = m_needClear;
    m_needClear = false;
  }

  // Create collection only if callback appears
  if (!m_collection && HasCallback())
    InitCollection();

  // All origin points are written in the storage,
  // and filtered points are inserted in the runtime collection.

  UpdateStorage(needClear, originPoints);

  if (!m_collection)
    return;

  vector<location::GpsInfo> points;
  m_filter->Process(originPoints, points);

  pair<size_t, size_t> addedIds;
  pair<size_t, size_t> evictedIds;
  UpdateCollection(needClear, points, addedIds, evictedIds);

  NotifyCallback(addedIds, evictedIds);
}

bool GpsTrack::HasCallback()
{
  lock_guard<mutex> lg(m_callbackGuard);
  return m_callback != nullptr;
}

void GpsTrack::UpdateStorage(bool needClear, vector<location::GpsInfo> const & points)
{
  InitStorageIfNeed();
  if (!m_storage)
    return;

  try
  {
    if (needClear)
      m_storage->Clear();

    m_storage->Append(points);
  }
  catch (RootException const & e)
  {
    LOG(LWARNING, ("Track storage exception:", e.Msg()));
    m_storage.reset();
  }
}

void GpsTrack::UpdateCollection(bool needClear, vector<location::GpsInfo> const & points,
                                pair<size_t, size_t> & addedIds, pair<size_t, size_t> & evictedIds)
{
  // Apply Clear and Add points
  // Clear points from collection, if need.
  evictedIds = needClear ? m_collection->Clear(false /* resetIds */) : make_pair(kInvalidId, kInvalidId);;

  // Add points to the collection, if need
  if (!points.empty())
    addedIds = m_collection->Add(points);
  else
    addedIds = make_pair(kInvalidId, kInvalidId);
}

void GpsTrack::NotifyCallback(pair<size_t, size_t> const & addedIds, pair<size_t, size_t> const & evictedIds)
{
  lock_guard<mutex> lg(m_callbackGuard);

  if (!m_callback)
    return;

  if (m_needSendSnapshop)
  {
    m_needSendSnapshop = false;

    vector<pair<size_t, location::GpsInfo>> toAdd;
    toAdd.reserve(m_collection->GetSize());
    m_collection->ForEach([&toAdd](location::GpsInfo const & point, size_t id)->bool
    {
      toAdd.emplace_back(id, point);
      return true;
    });

    if (toAdd.empty())
      return; // nothing to send

    m_callback(std::move(toAdd), make_pair(kInvalidId, kInvalidId), m_collection->GetTrackInfo());
  }
  else
  {
    vector<pair<size_t, location::GpsInfo>> toAdd;
    if (addedIds.first != kInvalidId)
    {
      size_t const addedCount = addedIds.second - addedIds.first + 1;
      ASSERT_GREATER_OR_EQUAL(m_collection->GetSize(), addedCount, ());
      toAdd.reserve(addedCount);
      m_collection->ForEach([&toAdd](location::GpsInfo const & point, size_t id)->bool
      {
        toAdd.emplace_back(id, point);
        return true;
      }, m_collection->GetSize() - addedCount);
      ASSERT_EQUAL(toAdd.size(), addedCount, ());
    }

    if (toAdd.empty() && evictedIds.first == kInvalidId)
      return; // nothing to send

    m_callback(std::move(toAdd), evictedIds, m_collection->GetTrackInfo());
  }
}
