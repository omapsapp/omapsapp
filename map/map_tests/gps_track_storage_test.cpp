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
  info.m_speedMpS = speed;
  info.m_latitude = ll.m_lat;
  info.m_longitude = ll.m_lon;
  info.m_source = location::EAndroidNative;
  return info;
}

inline string GetGpsTrackFilePath()
{
  return base::JoinPath(GetPlatform().WritableDir(), "gpstrack_test.bin");
}

UNIT_TEST(GpsTrackStorage_WriteReadWithoutTrunc)
{
  time_t const t = system_clock::to_time_t(system_clock::now());
  double const timestamp = t;
  LOG(LINFO, ("Timestamp", ctime(&t), timestamp));

  string const filePath = GetGpsTrackFilePath();
  SCOPE_GUARD(gpsTestFileDeleter, bind(FileWriter::DeleteFileX, filePath));
  FileWriter::DeleteFileX(filePath);

  size_t const fileMaxItemCount = 100000;

  vector<location::GpsInfo> points;
  points.reserve(fileMaxItemCount);
  for (size_t i = 0; i < fileMaxItemCount; ++i)
    points.emplace_back(Make(timestamp + i, ms::LatLon(-90 + i, -180 + i), 60 + i));

  // Open storage, write data and check written data
  {
    GpsTrackStorage stg(filePath, fileMaxItemCount);

    stg.Append(points);

    size_t i = 0;
    stg.ForEach([&](location::GpsTrackInfo const & point)->bool
    {
      TEST_EQUAL(point.m_latitude, points[i].m_latitude, ());
      TEST_EQUAL(point.m_longitude, points[i].m_longitude, ());
      TEST_EQUAL(point.m_timestamp, points[i].m_timestamp, ());
      TEST_EQUAL(point.m_speed, points[i].m_speedMpS, ());
      ++i;
      return true;
    });
    TEST_EQUAL(i, fileMaxItemCount, ());
  }

  // Open storage and check previously written data
  {
    GpsTrackStorage stg(filePath, fileMaxItemCount);

    size_t i = 0;
    stg.ForEach([&](location::GpsTrackInfo const & point)->bool
    {
      TEST_EQUAL(point.m_latitude, points[i].m_latitude, ());
      TEST_EQUAL(point.m_longitude, points[i].m_longitude, ());
      TEST_EQUAL(point.m_timestamp, points[i].m_timestamp, ());
      TEST_EQUAL(point.m_speed, points[i].m_speedMpS, ());
      ++i;
      return true;
    });
    TEST_EQUAL(i, fileMaxItemCount, ());

    // Clear data
    stg.Clear();
  }

  // Open storage and check there is no data
  {
    GpsTrackStorage stg(filePath, fileMaxItemCount);

    size_t i = 0;
    stg.ForEach([&](location::GpsTrackInfo const & /* point */)->bool{ ++i; return true; });
    TEST_EQUAL(i, 0, ());
  }
}

UNIT_TEST(GpsTrackStorage_WriteReadWithTrunc)
{
  time_t const t = system_clock::to_time_t(system_clock::now());
  double const timestamp = t;
  LOG(LINFO, ("Timestamp", ctime(&t), timestamp));

  string const filePath = GetGpsTrackFilePath();
  SCOPE_GUARD(gpsTestFileDeleter, bind(FileWriter::DeleteFileX, filePath));
  FileWriter::DeleteFileX(filePath);

  size_t const fileMaxItemCount = 100000;

  vector<location::GpsInfo> points1, points2;
  points1.reserve(fileMaxItemCount);
  points2.reserve(fileMaxItemCount);
  for (size_t i = 0; i < fileMaxItemCount; ++i)
  {
    points1.emplace_back(Make(timestamp + i, ms::LatLon(-90 + i, -180 + i), 60 + i));
    points2.emplace_back(Make(timestamp + i + fileMaxItemCount, ms::LatLon(-45 + i, -30 + i), 15 + i));
  }

  vector<location::GpsInfo> points3;
  points3.reserve(fileMaxItemCount/2);
  for (size_t i = 0; i < fileMaxItemCount/2; ++i)
    points3.emplace_back(Make(timestamp + i, ms::LatLon(-30 + i, -60 + i), 90 + i));

  // Open storage and write blob 1
  {
    GpsTrackStorage stg(filePath, fileMaxItemCount);
    stg.Append(points1);
  }

  // Open storage and write blob 2
  {
    GpsTrackStorage stg(filePath, fileMaxItemCount);
    stg.Append(points2);
  }

  // Open storage and write blob 3
  {
    GpsTrackStorage stg(filePath, fileMaxItemCount);
    stg.Append(points3);
  }

  // Check storage must contain second half of points2 and points3
  {
    GpsTrackStorage stg(filePath, fileMaxItemCount);

    size_t i = 0;
    stg.ForEach([&](location::GpsInfo const & point)->bool
    {
      if (i < fileMaxItemCount/2)
      {
        TEST_EQUAL(point.m_latitude, points2[fileMaxItemCount/2 + i].m_latitude, ());
        TEST_EQUAL(point.m_longitude, points2[fileMaxItemCount/2 + i].m_longitude, ());
        TEST_EQUAL(point.m_timestamp, points2[fileMaxItemCount/2 + i].m_timestamp, ());
        TEST_EQUAL(point.m_speedMpS, points2[fileMaxItemCount/2 + i].m_speedMpS, ());
      }
      else
      {
        TEST_EQUAL(point.m_latitude, points3[i - fileMaxItemCount/2].m_latitude, ());
        TEST_EQUAL(point.m_longitude, points3[i - fileMaxItemCount/2].m_longitude, ());
        TEST_EQUAL(point.m_timestamp, points3[i - fileMaxItemCount/2].m_timestamp, ());
        TEST_EQUAL(point.m_speedMpS, points3[i - fileMaxItemCount/2].m_speedMpS, ());
      }
      ++i;
      return true;
    });
    TEST_EQUAL(i, fileMaxItemCount, ());

    // Clear data
    stg.Clear();
  }

  // Check no data in storage
  {
    GpsTrackStorage stg(filePath, fileMaxItemCount);

    size_t i = 0;
    stg.ForEach([&](location::GpsInfo const & /* point */)->bool{ ++i; return true; });
    TEST_EQUAL(i, 0, ());
  }
}
} // namespace gps_track_storage_test
