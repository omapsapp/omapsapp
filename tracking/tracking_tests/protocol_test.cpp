#include "testing/testing.hpp"

#include "tracking/protocol.hpp"

#include <string>

using namespace std;
using namespace tracking;

UNIT_TEST(Protocol_CreateAuthPacket)
{
  auto packet = Protocol::CreateAuthPacket("ABC");
  TEST_EQUAL(packet.size(), 7, ());
  TEST_EQUAL(Protocol::PacketType(packet[0]), Protocol::PacketType::CurrentAuth, ());
  TEST_EQUAL(packet[1], 0x00, ());
  TEST_EQUAL(packet[2], 0x00, ());
  TEST_EQUAL(packet[3], 0x03, ());
  TEST_EQUAL(packet[4], 'A', ());
  TEST_EQUAL(packet[5], 'B', ());
  TEST_EQUAL(packet[6], 'C', ());
}

UNIT_TEST(Protocol_DecodeHeader)
{
  string id_str("ABC");
  auto packet = Protocol::CreateAuthPacket(id_str);
  TEST_EQUAL(packet.size(), 7, ());
  TEST_EQUAL(Protocol::PacketType(packet[0]), Protocol::PacketType::CurrentAuth, ());

  {
    auto header = Protocol::DecodeHeader(packet);
    TEST_EQUAL(header.first, Protocol::PacketType::CurrentAuth, ());
    TEST_EQUAL(header.second, id_str.size(), ());
  }

  {
    auto header = Protocol::DecodeHeader({});
    TEST_EQUAL(header.first, Protocol::PacketType::Error, ());
    TEST_EQUAL(header.second, 0, ());
  }

  {
    auto header = Protocol::DecodeHeader({7, 9});
    TEST_EQUAL(header.first, Protocol::PacketType::Error, ());
    TEST_EQUAL(header.second, 2, ());
  }
}

UNIT_TEST(Protocol_CreateDataPacket)
{
  using Container = Protocol::DataElementsCirc;
  Container buffer(5);
  buffer.push_back(Container::value_type(1, ms::LatLon(10, 10), 1));
  buffer.push_back(Container::value_type(2, ms::LatLon(15, 15), 2));

  auto packetV0 = Protocol::CreateDataPacket(buffer, Protocol::PacketType::DataV0);
  TEST_EQUAL(packetV0.size(), 26, ());
  TEST_EQUAL(Protocol::PacketType(packetV0[0]), Protocol::PacketType::DataV0, ());
  TEST_EQUAL(packetV0[1], 0x00, ());
  TEST_EQUAL(packetV0[2], 0x00, ());
  TEST_EQUAL(packetV0[3], 22, ());

  TEST_EQUAL(packetV0[4], 1, ());
  TEST_EQUAL(packetV0[5], 227, ());
  TEST_EQUAL(packetV0[6], 241, ());

  auto packetV1 = Protocol::CreateDataPacket(buffer, Protocol::PacketType::DataV1);
  TEST_EQUAL(packetV1.size(), 28, ());
  TEST_EQUAL(Protocol::PacketType(packetV1[0]), Protocol::PacketType::DataV1, ());
  TEST_EQUAL(packetV1[1], 0x00, ());
  TEST_EQUAL(packetV1[2], 0x00, ());
  TEST_EQUAL(packetV1[3], 24, ());

  TEST_EQUAL(packetV1[4], 1, ());
  TEST_EQUAL(packetV1[5], 227, ());
  TEST_EQUAL(packetV1[6], 241, ());
}

UNIT_TEST(Protocol_DecodeAuthPacket)
{
  auto packet = Protocol::CreateAuthPacket("ABC");
  TEST_EQUAL(packet.size(), 7, ());
  TEST_EQUAL(Protocol::PacketType(packet[0]), Protocol::PacketType::CurrentAuth, ());

  auto payload = vector<uint8_t>(begin(packet) + sizeof(uint32_t /* header */), end(packet));
  auto result = Protocol::DecodeAuthPacket(Protocol::PacketType::CurrentAuth, payload);
  TEST_EQUAL(result, "ABC", ());
}

template <typename Container>
void DecodeDataPacketVersionTest(Container const & points, Protocol::PacketType version)
{
  double const kEps = 1e-5;

  auto packet = Protocol::CreateDataPacket(points, version);
  TEST_GREATER(packet.size(), 0, ());
  TEST_EQUAL(Protocol::PacketType(packet[0]), version, ());

  auto payload = vector<uint8_t>(begin(packet) + sizeof(uint32_t /* header */), end(packet));
  Container result = Protocol::DecodeDataPacket(version, payload);

  TEST_EQUAL(points.size(), result.size(), ());
  for (size_t i = 0; i < points.size(); ++i)
  {
    TEST_EQUAL(points[i].m_timestamp, result[i].m_timestamp,
               (points[i].m_timestamp, result[i].m_timestamp));
    TEST(AlmostEqualAbsOrRel(points[i].m_latLon.m_lat, result[i].m_latLon.m_lat, kEps),
         (points[i].m_latLon.m_lat, result[i].m_latLon.m_lat));
    TEST(AlmostEqualAbsOrRel(points[i].m_latLon.m_lon, result[i].m_latLon.m_lon, kEps),
         (points[i].m_latLon.m_lon, result[i].m_latLon.m_lon));
  }
}

UNIT_TEST(Protocol_DecodeDataPacket)
{
  using Container = Protocol::DataElementsVec;

  Container points;
  points.push_back(Container::value_type(1, ms::LatLon(10, 10), 1));
  points.push_back(Container::value_type(2, ms::LatLon(15, 15), 2));

  DecodeDataPacketVersionTest(points, Protocol::PacketType::DataV0);
  DecodeDataPacketVersionTest(points, Protocol::PacketType::DataV1);
}

UNIT_TEST(Protocol_DecodeWrongDataPacket)
{
  vector<vector<uint8_t>> payloads = {
      vector<uint8_t>{},
      vector<uint8_t>{0x25},
      vector<uint8_t>{0x0},
      vector<uint8_t>{0x0, 0x0, 0x23, 0xFF},
      vector<uint8_t>{0xFF, 0x1, 0x23, 0xFF, 0x1, 0x0, 0x27, 0x63, 0x32, 0x9, 0xFF},
  };
  for (auto const packetType : {Protocol::PacketType::DataV0, Protocol::PacketType::DataV1})
  {
    for (auto const & payload : payloads)
    {
      auto result = Protocol::DecodeDataPacket(packetType, payload);
      TEST(result.empty(), (packetType, payload));
    }
  }
}
