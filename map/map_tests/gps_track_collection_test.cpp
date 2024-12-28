#include "testing/testing.hpp"

#include "map/gps_track_collection.hpp"

#include "geometry/latlon.hpp"

#include "base/logging.hpp"

#include <chrono>
#include <ctime>
#include <map>
#include <utility>

namespace gps_track_collection_test
{
using namespace std::chrono;

location::GpsInfo MakeGpsTrackInfo(double timestamp, ms::LatLon const & ll, double speed)
{
  location::GpsInfo info;
  info.m_timestamp = timestamp;
  info.m_speed = speed;
  info.m_latitude = ll.m_lat;
  info.m_longitude = ll.m_lon;
  return info;
}

UNIT_TEST(GpsTrackCollection_Simple)
{
  time_t const t = system_clock::to_time_t(system_clock::now());
  double const timestamp = t;
  LOG(LINFO, ("Timestamp", ctime(&t), timestamp));

  GpsTrackCollection collection;

  std::map<size_t, location::GpsInfo> data;

  for (size_t i = 0; i < 50; ++i)
  {
    auto info = MakeGpsTrackInfo(timestamp + i, ms::LatLon(-90 + i, -180 + i), i);
    std::pair<size_t, size_t> addedIds = collection.Add({info});
    TEST_EQUAL(addedIds.second, i, ());
    data[addedIds.second] = info;
  }

  TEST_EQUAL(50, collection.GetSize(), ());

  collection.ForEach([&data](location::GpsInfo const & info, size_t id)->bool
  {
    TEST(data.end() != data.find(id), ());
    location::GpsInfo const & originInfo = data[id];
    TEST_EQUAL(info.m_latitude, originInfo.m_latitude, ());
    TEST_EQUAL(info.m_longitude, originInfo.m_longitude, ());
    TEST_EQUAL(info.m_speed, originInfo.m_speed, ());
    TEST_EQUAL(info.m_timestamp, originInfo.m_timestamp, ());
    return true;
  });

  auto res = collection.Clear();
  TEST_EQUAL(res.first, 0, ());
  TEST_EQUAL(res.second, 49, ());

  TEST_EQUAL(0, collection.GetSize(), ());
}
}  // namespace gps_track_collection_test
