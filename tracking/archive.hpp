#pragma once

#include "traffic/speed_groups.hpp"

#include "platform/location.hpp"

#include "coding/point_coding.hpp"
#include "coding/reader.hpp"
#include "coding/varint.hpp"
#include "coding/writer.hpp"
#include "coding/zlib.hpp"

#include "base/checked_cast.hpp"
#include "base/logging.hpp"

#include <cstdint>
#include <exception>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-copy"
#endif
#include <boost/circular_buffer.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif


namespace tracking
{
namespace helpers
{
struct Restrictions
{
  static double const kMinDeltaLat;
  static double const kMaxDeltaLat;
  static double const kMinDeltaLon;
  static double const kMaxDeltaLon;
};

struct Limits
{
  double m_minLat;
  double m_maxLat;
  double m_minLon;
  double m_maxLon;
};

Limits GetLimits(bool isDelta);
} // namespace helpers

struct Packet
{
  Packet();
  explicit Packet(location::GpsInfo const & info);
  Packet(double lat, double lon, uint32_t timestamp);

  double m_lat;
  double m_lon;
  uint32_t m_timestamp;
};

struct PacketCar : Packet
{
  PacketCar();
  PacketCar(location::GpsInfo const & info, traffic::SpeedGroup const & speedGroup);
  PacketCar(double lat, double lon, uint32_t timestamp, traffic::SpeedGroup speed);

  traffic::SpeedGroup m_speedGroup;
};

template <typename Pack>
class BasicArchive
{
public:
  BasicArchive(size_t maxSize, double minDelaySeconds);

  template <typename Writer>
  bool Write(Writer & dst);

  template <typename Reader>
  bool Read(Reader & src);

  template <typename... PackParameters>
  bool Add(PackParameters &&... params);

  size_t Size() const;
  bool ReadyToDump() const;
  std::vector<Pack> Extract() const;

private:
  boost::circular_buffer<Pack> m_buffer;
  double const m_minDelaySeconds;
};

// Abstract packet traits.
template <typename Pack>
struct TraitsPacket
{
};

template <>
struct TraitsPacket<Packet>
{
  static size_t constexpr kCoordBits = 32;

  template <typename Writer>
  static void Write(Writer & writer, Packet const & packet, bool isDelta)
  {
    auto const lim = helpers::GetLimits(isDelta);

    WriteVarUint(writer, DoubleToUint32(packet.m_lat, lim.m_minLat, lim.m_maxLat, kCoordBits));
    WriteVarUint(writer, DoubleToUint32(packet.m_lon, lim.m_minLon, lim.m_maxLon, kCoordBits));
    WriteVarUint(writer, packet.m_timestamp);
  }

  template <typename Reader>
  static Packet Read(Reader & src, bool isDelta)
  {
    auto const lim = helpers::GetLimits(isDelta);

    double const lat =
        Uint32ToDouble(ReadVarUint<uint32_t>(src), lim.m_minLat, lim.m_maxLat, kCoordBits);
    double const lon =
        Uint32ToDouble(ReadVarUint<uint32_t>(src), lim.m_minLon, lim.m_maxLon, kCoordBits);
    uint32_t const timestamp = ReadVarUint<uint32_t>(src);
    return Packet(lat, lon, timestamp);
  }

  static Packet GetDelta(Packet const & current, Packet const & previous)
  {
    uint32_t const deltaTimestamp = current.m_timestamp - previous.m_timestamp;
    double const deltaLat = current.m_lat - previous.m_lat;
    double const deltaLon = current.m_lon - previous.m_lon;
    return Packet(deltaLat, deltaLon, deltaTimestamp);
  }

  static Packet Combine(Packet const & previous, Packet const & delta)
  {
    double const lat = previous.m_lat + delta.m_lat;
    double const lon = previous.m_lon + delta.m_lon;
    uint32_t const timestamp = previous.m_timestamp + delta.m_timestamp;
    return Packet(lat, lon, timestamp);
  }

  static traffic::SpeedGroup GetSpeedGroup(Packet const & /* packet */)
  {
    return traffic::SpeedGroup::Unknown;
  }
};

template <>
struct TraitsPacket<PacketCar>
{
  template <typename Writer>
  static void Write(Writer & writer, PacketCar const & packet, bool isDelta)
  {
    TraitsPacket<Packet>::Write(writer, packet, isDelta);
    static_assert(
        std::is_same<uint8_t, std::underlying_type_t<decltype(packet.m_speedGroup)>>::value, "");
    WriteVarUint(writer, static_cast<uint8_t>(packet.m_speedGroup));
  }

  template <typename Reader>
  static PacketCar Read(Reader & src, bool isDelta)
  {
    Packet const base = TraitsPacket<Packet>::Read(src, isDelta);
    auto const speedGroup = static_cast<traffic::SpeedGroup>(ReadPrimitiveFromSource<uint8_t>(src));
    return PacketCar(base.m_lat, base.m_lon, base.m_timestamp, speedGroup);
  }

  static PacketCar GetDelta(PacketCar const & current, PacketCar const & previous)
  {
    Packet const delta = TraitsPacket<Packet>::GetDelta(current, previous);
    return PacketCar(delta.m_lat, delta.m_lon, delta.m_timestamp, current.m_speedGroup);
  }

  static PacketCar Combine(PacketCar const & previous, PacketCar const & delta)
  {
    Packet const base = TraitsPacket<Packet>::Combine(previous, delta);
    return PacketCar(base.m_lat, base.m_lon, base.m_timestamp, delta.m_speedGroup);
  }

  static traffic::SpeedGroup GetSpeedGroup(PacketCar const & packet)
  {
    return packet.m_speedGroup;
  }
};

template <typename Reader>
std::vector<uint8_t> InflateToBuffer(Reader & src)
{
  std::vector<uint8_t> deflatedBuf(base::checked_cast<size_t>(src.Size()));
  src.Read(deflatedBuf.data(), deflatedBuf.size());

  coding::ZLib::Inflate inflate(coding::ZLib::Inflate::Format::ZLib);
  std::vector<uint8_t> buffer;
  inflate(deflatedBuf.data(), deflatedBuf.size(), std::back_inserter(buffer));
  return buffer;
}

template <typename Writer>
void DeflateToDst(Writer & dst, std::vector<uint8_t> const & buf)
{
  coding::ZLib::Deflate deflate(coding::ZLib::Deflate::Format::ZLib,
                                coding::ZLib::Deflate::Level::BestCompression);
  std::vector<uint8_t> deflatedBuf;
  deflate(buf.data(), buf.size(), std::back_inserter(deflatedBuf));
  dst.Write(deflatedBuf.data(), deflatedBuf.size());
}

template <typename Pack>
BasicArchive<Pack>::BasicArchive(size_t maxSize, double minDelaySeconds)
  : m_buffer(maxSize), m_minDelaySeconds(minDelaySeconds)
{
}

template <typename Pack>
template <typename... PackParameters>
bool BasicArchive<Pack>::Add(PackParameters &&... params)
{
  Pack newPacket = Pack(std::forward<PackParameters>(params)...);
  if (!m_buffer.empty() && newPacket.m_timestamp < m_buffer.back().m_timestamp + m_minDelaySeconds)
    return false;

  m_buffer.push_back(std::move(newPacket));
  return true;
}

template <typename Pack>
size_t BasicArchive<Pack>::Size() const
{
  return m_buffer.size();
}

template <typename Pack>
bool BasicArchive<Pack>::ReadyToDump() const
{
  return m_buffer.full();
}

template <typename Pack>
std::vector<Pack> BasicArchive<Pack>::Extract() const
{
  std::vector<Pack> res;
  res.reserve(m_buffer.size());
  res.insert(res.end(), m_buffer.begin(), m_buffer.end());
  return res;
}

template <typename Pack>
template <typename Writer>
bool BasicArchive<Pack>::Write(Writer & dst)
{
  if (m_buffer.empty())
    return false;

  try
  {
    std::vector<uint8_t> buf;
    MemWriter<std::vector<uint8_t>> writer(buf);
    // For aggregating tracks we use two-step approach: at first we delta-code the coordinates and
    // timestamps; then we use coding::ZLib::Inflate to compress data.
    TraitsPacket<Pack>::Write(writer, m_buffer[0], false /* isDelta */);

    for (size_t i = 1; i < m_buffer.size(); ++i)
    {
      Pack const delta = TraitsPacket<Pack>::GetDelta(m_buffer[i], m_buffer[i - 1]);
      TraitsPacket<Pack>::Write(writer, delta, true /* isDelta */);
    }

    DeflateToDst(dst, buf);

    LOG(LDEBUG, ("Dumped to disk", m_buffer.size(), "items"));

    m_buffer.clear();
    return true;
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("Error writing to file", e.what()));
  }
  return false;
}

template <typename Pack>
template <typename Reader>
bool BasicArchive<Pack>::Read(Reader & src)
{
  try
  {
    auto const buffer = InflateToBuffer(src);
    ReaderSource<MemReaderWithExceptions> reader(MemReaderWithExceptions(buffer.data(), buffer.size()));
    if (reader.Size() == 0)
      return false;

    m_buffer.clear();

    // Read first point.
    m_buffer.push_back(TraitsPacket<Pack>::Read(reader, false /* isDelta */));

    // Read with delta.
    while (reader.Size() > 0)
    {
      Pack const delta = TraitsPacket<Pack>::Read(reader, true  /* isDelta */);
      m_buffer.push_back(TraitsPacket<Pack>::Combine(m_buffer.back(), delta));
    }
    return true;
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("Error reading file", e.what()));
  }
  return false;
}
}  // namespace tracking
