#include "testing/testing.hpp"

#include "map/gps_track_storage.hpp"

#include "platform/platform.hpp"

#include "coding/file_writer.hpp"

#include "geometry/latlon.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace gps_track_storage_test
{
using namespace std;
using namespace std::chrono;

location::GpsInfo Make(double timestamp, ms::LatLon const & ll, double speed)
{
  location::GpsInfo info;
  info.m_timestamp = timestamp;
  info.m_speed = speed;
  info.m_latitude = ll.m_lat;
  info.m_longitude = ll.m_lon;
  info.m_source = location::EAndroidNative;
  return info;
}

inline string GetGpsTrackFilePath()
{
  return base::JoinPath(GetPlatform().WritableDir(), "gpstrack_test.bin");
}

UNIT_TEST(GpsTrackStorage_WriteRead)
{
  time_t const t = system_clock::to_time_t(system_clock::now());
  double const timestamp = t;
  LOG(LINFO, ("Timestamp", ctime(&t), timestamp));

  string const filePath = GetGpsTrackFilePath();
  SCOPE_GUARD(gpsTestFileDeleter, bind(FileWriter::DeleteFileX, filePath));
  FileWriter::DeleteFileX(filePath);

  size_t const itemCount = 100000;

  vector<location::GpsInfo> points;
  points.reserve(itemCount);
  for (size_t i = 0; i < itemCount; ++i)
    points.emplace_back(Make(timestamp + i, ms::LatLon(-90 + i, -180 + i), 60 + i));

  // Open storage, write data and check written data
  {
    GpsTrackStorage stg(filePath);

    stg.Append(points);

    size_t i = 0;
    stg.ForEach([&](location::GpsInfo const & point)->bool
    {
      TEST_EQUAL(point.m_latitude, points[i].m_latitude, ());
      TEST_EQUAL(point.m_longitude, points[i].m_longitude, ());
      TEST_EQUAL(point.m_timestamp, points[i].m_timestamp, ());
      TEST_EQUAL(point.m_speed, points[i].m_speed, ());
      ++i;
      return true;
    });
    TEST_EQUAL(i, itemCount, ());
  }

  // Open storage and check previously written data
  {
    GpsTrackStorage stg(filePath);

    size_t i = 0;
    stg.ForEach([&](location::GpsInfo const & point)->bool
    {
      TEST_EQUAL(point.m_latitude, points[i].m_latitude, ());
      TEST_EQUAL(point.m_longitude, points[i].m_longitude, ());
      TEST_EQUAL(point.m_timestamp, points[i].m_timestamp, ());
      TEST_EQUAL(point.m_speed, points[i].m_speed, ());
      ++i;
      return true;
    });
    TEST_EQUAL(i, itemCount, ());

    // Clear data
    stg.Clear();
  }

  // Open storage and check there is no data
  {
    GpsTrackStorage stg(filePath);

    size_t i = 0;
    stg.ForEach([&](location::GpsInfo const & point)->bool{ ++i; return true; });
    TEST_EQUAL(i, 0, ());
  }
}
} // namespace gps_track_storage_test
