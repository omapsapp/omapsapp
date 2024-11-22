#include "testing/testing.hpp"

#include "tracking/reporter.hpp"
#include "tracking/protocol.hpp"

#include "coding/traffic.hpp"

#include "platform/location.hpp"
#include "platform/platform_tests_support/test_socket.hpp"
#include "platform/socket.hpp"

#include "base/math.hpp"
#include "base/thread.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

using namespace std;
using namespace std::chrono;
using namespace tracking;
using namespace platform::tests_support;

namespace
{
void TransferLocation(Reporter & reporter, TestSocket & testSocket, double timestamp,
                      double latidute, double longtitude)
{
  location::GpsInfo gpsInfo;
  gpsInfo.m_timestamp = timestamp;
  gpsInfo.m_latitude = latidute;
  gpsInfo.m_longitude = longtitude;
  gpsInfo.m_horizontalAccuracy = 1.0;
  reporter.AddLocation(gpsInfo, traffic::SpeedGroup::Unknown);

  using Packet = tracking::Protocol::PacketType;
  vector<uint8_t> buffer;
  size_t readSize = 0;
  size_t attempts = 3;
  do
  {
    readSize = testSocket.ReadServer(buffer);
    if (attempts-- && readSize == 0)
      continue;
    switch (Packet(buffer[0]))
    {
    case Packet::CurrentAuth:
    {
      buffer.clear();
      testSocket.WriteServer(tracking::Protocol::kOk);
      break;
    }
    case Packet::Error:
    case Packet::DataV0:
    case Packet::DataV1:
    {
      readSize = 0;
      break;
    }
    }
  } while (readSize);

  TEST(!buffer.empty(), ());
  vector<coding::TrafficGPSEncoder::DataPoint> points;
  MemReader memReader(buffer.data(), buffer.size());
  ReaderSource<MemReader> src(memReader);
  src.Skip(sizeof(uint32_t /* header */));
  coding::TrafficGPSEncoder::DeserializeDataPoints(coding::TrafficGPSEncoder::kLatestVersion, src,
                                                   points);

  TEST_EQUAL(points.size(), 1, ());
  auto const & point = points[0];
  TEST_EQUAL(point.m_timestamp, timestamp, ());
  TEST(AlmostEqualAbs(point.m_latLon.m_lat, latidute, 0.001), ());
  TEST(AlmostEqualAbs(point.m_latLon.m_lon, longtitude, 0.001), ());
}
}

UNIT_TEST(Reporter_Smoke)
{
  {
    unique_ptr<platform::Socket> socket;
    Reporter reporter(std::move(socket), "localhost", 0, milliseconds(10) /* pushDelay */);
  }
  {
    auto socket = make_unique<TestSocket>();
    Reporter reporter(std::move(socket), "localhost", 0, milliseconds(10) /* pushDelay */);
  }
  {
    auto socket = platform::CreateSocket();
    Reporter reporter(std::move(socket), "localhost", 0, milliseconds(10) /* pushDelay */);
  }
}

UNIT_TEST(Reporter_TransferLocations)
{
  auto socket = make_unique<TestSocket>();
  TestSocket & testSocket = *socket.get();

  Reporter reporter(std::move(socket), "localhost", 0, milliseconds(10) /* pushDelay */);
  TransferLocation(reporter, testSocket, 1.0, 2.0, 3.0);
  TransferLocation(reporter, testSocket, 4.0, 5.0, 6.0);
  TransferLocation(reporter, testSocket, 7.0, 8.0, 9.0);
}
