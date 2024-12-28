#include "map/gps_track_collection.hpp"

#include "base/assert.hpp"

#include "geometry/distance_on_sphere.hpp"

#include <algorithm>

namespace
{

// Simple rollbacker which restores deque state
template <typename T>
class Rollbacker
{
public:
  Rollbacker(std::deque<T> & cont)
    : m_cont(&cont)
    , m_size(cont.size())
  {}
  ~Rollbacker()
  {
    if (m_cont && m_cont->size() > m_size)
      m_cont->erase(m_cont->begin() + m_size, m_cont->end());
  }
  void Reset() { m_cont = nullptr; }
private:
  std::deque<T> * m_cont;
  size_t const m_size;
};

} // namespace

size_t const GpsTrackCollection::kInvalidId = std::numeric_limits<size_t>::max();

GpsTrackCollection::GpsTrackCollection()
  : m_lastId(0)
  , m_trackInfo(GpsTrackInfo())
{}

std::pair<size_t, size_t> GpsTrackCollection::Add(std::vector<TItem> const & items)
{
  size_t startId = m_lastId;
  size_t added = 0;

  // Rollbacker ensure strong guarantee if exception happens while adding items
  Rollbacker<TItem> rollbacker(m_items);

  for (auto const & item : items)
  {
    if (!m_items.empty() && m_items.back().m_timestamp > item.m_timestamp)
      continue;

    if (m_items.empty())
    {
      m_trackInfo.m_maxElevation = item.m_altitude;
      m_trackInfo.m_minElevation = item.m_altitude;
    }
    else
    {
      auto const & lastItem = m_items.back();
      m_trackInfo.m_length += ms::DistanceOnEarth(lastItem.GetLatLon(), item.GetLatLon());
      m_trackInfo.m_duration = item.m_timestamp - m_items.front().m_timestamp;

      auto const deltaAltitude = item.m_altitude - lastItem.m_altitude;
      if (item.m_altitude > lastItem.m_altitude)
        m_trackInfo.m_ascent += deltaAltitude;
      if (item.m_altitude < lastItem.m_altitude)
        m_trackInfo.m_descent -= deltaAltitude;

      m_trackInfo.m_maxElevation = std::max(static_cast<double>(m_trackInfo.m_maxElevation), item.m_altitude);
      m_trackInfo.m_minElevation = std::min(static_cast<double>(m_trackInfo.m_minElevation), item.m_altitude);
    }

    m_items.emplace_back(item);
    ++added;
  }

  rollbacker.Reset();

  if (0 == added)
  {
    // Invalid timestamp order
    return std::make_pair(kInvalidId, kInvalidId); // Nothing was added
  }

  m_lastId += added;

  return std::make_pair(startId, startId + added - 1);
}

std::pair<size_t, size_t> GpsTrackCollection::Clear(bool resetIds)
{
  if (m_items.empty())
  {
    if (resetIds)
      m_lastId = 0;

    return std::make_pair(kInvalidId, kInvalidId);
  }

  ASSERT_GREATER_OR_EQUAL(m_lastId, m_items.size(), ());

  // Range of evicted items
  auto const res = std::make_pair(m_lastId - m_items.size(), m_lastId - 1);

  m_items.clear();
  m_items.shrink_to_fit();
  m_trackInfo = {};

  if (resetIds)
    m_lastId = 0;

  return res;
}

size_t GpsTrackCollection::GetSize() const
{
  return m_items.size();
}

bool GpsTrackCollection::IsEmpty() const
{
  return m_items.empty();
}
