#include "map/gps_track_storage.hpp"

#include "coding/endianness.hpp"
#include "coding/internal/file_data.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <algorithm>
#include <cstring>

using namespace std;

namespace
{

// Current file format version
uint32_t constexpr kCurrentVersion = 1;

// Header size in bytes, header consists of uint32_t 'version' only
uint32_t constexpr kHeaderSize = sizeof(uint32_t);

// Number of items for batch processing
size_t constexpr kItemBlockSize = 1000;

// TODO
// Now GpsInfo written as plain values, but values can be compressed.

// Size of point in bytes to write in file of read from file
size_t constexpr kPointSize = 8 * sizeof(double) + sizeof(uint8_t);

// Writes value in memory in LittleEndian
template <typename T>
void MemWrite(void * ptr, T value)
{
  value = SwapIfBigEndianMacroBased(value);
  memcpy(ptr, &value, sizeof(T));
}

// Read value from memory, which is LittleEndian in memory
template <typename T>
T MemRead(void const * ptr)
{
  T value;
  memcpy(&value, ptr, sizeof(T));
  return SwapIfBigEndianMacroBased(value);
}

void Pack(char * p, location::GpsInfo const & info)
{
  MemWrite<double>(p + 0 * sizeof(double), info.m_timestamp);
  MemWrite<double>(p + 1 * sizeof(double), info.m_latitude);
  MemWrite<double>(p + 2 * sizeof(double), info.m_longitude);
  MemWrite<double>(p + 3 * sizeof(double), info.m_altitude);
  MemWrite<double>(p + 4 * sizeof(double), info.m_speed);
  MemWrite<double>(p + 5 * sizeof(double), info.m_bearing);
  MemWrite<double>(p + 6 * sizeof(double), info.m_horizontalAccuracy);
  MemWrite<double>(p + 7 * sizeof(double), info.m_verticalAccuracy);
  ASSERT_LESS_OR_EQUAL(static_cast<int>(info.m_source), 255, ());
  uint8_t const source = static_cast<uint8_t>(info.m_source);
  MemWrite<uint8_t>(p + 8 * sizeof(double), source);
}

void Unpack(char const * p, location::GpsInfo & info)
{
  info.m_timestamp = MemRead<double>(p + 0 * sizeof(double));
  info.m_latitude = MemRead<double>(p + 1 * sizeof(double));
  info.m_longitude = MemRead<double>(p + 2 * sizeof(double));
  info.m_altitude = MemRead<double>(p + 3 * sizeof(double));
  info.m_speed = MemRead<double>(p + 4 * sizeof(double));
  info.m_bearing = MemRead<double>(p + 5 * sizeof(double));
  info.m_horizontalAccuracy = MemRead<double>(p + 6 * sizeof(double));
  info.m_verticalAccuracy = MemRead<double>(p + 7 * sizeof(double));
  uint8_t const source = MemRead<uint8_t>(p + 8 * sizeof(double));
  info.m_source = static_cast<location::TLocationSource>(source);
}

inline size_t GetItemOffset(size_t itemIndex)
{
  return kHeaderSize + itemIndex * kPointSize;
}

inline size_t GetItemCount(size_t fileSize)
{
  ASSERT_GREATER_OR_EQUAL(fileSize, kHeaderSize, ());
  return (fileSize - kHeaderSize) / kPointSize;
}

inline bool WriteVersion(fstream & f, uint32_t version)
{
  static_assert(kHeaderSize == sizeof(version));
  version = SwapIfBigEndianMacroBased(version);
  f.write(reinterpret_cast<char const *>(&version), kHeaderSize);
  return f.good() && f.flush().good();
}

inline bool ReadVersion(fstream & f, uint32_t & version)
{
  static_assert(kHeaderSize == sizeof(version));
  f.read(reinterpret_cast<char *>(&version), kHeaderSize);
  version = SwapIfBigEndianMacroBased(version);
  return f.good();
}

} // namespace

GpsTrackStorage::GpsTrackStorage(string const & filePath)
  : m_filePath(filePath)
  , m_itemCount(0)
{
  auto const createNewFile = [this]
  {
    m_stream.open(m_filePath, ios::in | ios::out | ios::binary | ios::trunc);

    if (!m_stream)
      MYTHROW(OpenException, ("Open file error.", m_filePath));

    if (!WriteVersion(m_stream, kCurrentVersion))
      MYTHROW(OpenException, ("Write version error.", m_filePath));

    m_itemCount = 0;
  };


  // Open existing file
  m_stream.open(m_filePath, ios::in | ios::out | ios::binary);

  if (m_stream)
  {
    uint32_t version = 0;
    if (!ReadVersion(m_stream, version))
    {
      LOG(LWARNING, ("Recreating", m_filePath, "because can't read version from it."));
      m_stream.close();
      createNewFile();
      version = kCurrentVersion;
    }

    if (version == kCurrentVersion)
    {
      // Seek to end to get file size
      m_stream.seekp(0, ios::end);
      if (!m_stream.good())
        MYTHROW(OpenException, ("Seek to the end error.", m_filePath));

      auto const fileSize = static_cast<size_t>(m_stream.tellp());

      m_itemCount = GetItemCount(fileSize);

      // Set write position after last item position
      size_t const offset = GetItemOffset(m_itemCount);
      m_stream.seekp(offset, ios::beg);
      if (!m_stream.good())
        MYTHROW(OpenException, ("Seek to the offset error:", offset, m_filePath));

      LOG(LINFO, ("Restored", m_itemCount, "points from gps track storage"));
    }
    else
    {
      m_stream.close();
      // TODO: migration for file m_filePath from version 'version' to version 'kCurrentVersion'
      // TODO: For now we support only one version, but in future migration may be needed.
    }
  }

  if (!m_stream)
    createNewFile();
}

void GpsTrackStorage::Append(vector<TItem> const & items)
{
  ASSERT(m_stream.is_open(), ());

  if (items.empty())
    return;

  // Write position must be after last item position
  ASSERT_EQUAL(m_stream.tellp(), static_cast<typename fstream::pos_type>(
                   GetItemOffset(m_itemCount)), ());

  vector<char> buff(min(kItemBlockSize, items.size()) * kPointSize);
  for (size_t i = 0; i < items.size();)
  {
    size_t const n = min(items.size() - i, kItemBlockSize);

    for (size_t j = 0; j < n; ++j)
      Pack(&buff[0] + j * kPointSize, items[i + j]);

    m_stream.write(&buff[0], n * kPointSize);
    if (!m_stream.good())
      MYTHROW(WriteException, ("File:", m_filePath));

    i += n;
  }

  m_stream.flush();
  if (!m_stream.good())
    MYTHROW(WriteException, ("File:", m_filePath));

  m_itemCount += items.size();
}

void GpsTrackStorage::Clear()
{
  ASSERT(m_stream.is_open(), ());

  m_itemCount = 0;

  m_stream.close();

  m_stream.open(m_filePath, ios::in | ios::out | ios::binary | ios::trunc);

  if (!m_stream)
    MYTHROW(WriteException, ("File:", m_filePath));

  if (!WriteVersion(m_stream, kCurrentVersion))
    MYTHROW(WriteException, ("File:", m_filePath));

  // Write position is set to the first item in the file
  ASSERT_EQUAL(m_stream.tellp(), static_cast<typename fstream::pos_type>(
                   GetItemOffset(0)), ());
}

void GpsTrackStorage::ForEach(std::function<bool(TItem const & item)> const & fn)
{
  ASSERT(m_stream.is_open(), ());

  size_t i = 0;

  // Set read position to the first item
  m_stream.seekg(GetItemOffset(i), ios::beg);
  if (!m_stream.good())
    MYTHROW(ReadException, ("File:", m_filePath));

  vector<char> buff(min(kItemBlockSize, m_itemCount) * kPointSize);
  for (; i < m_itemCount;)
  {
    size_t const n = min(m_itemCount - i, kItemBlockSize);

    m_stream.read(&buff[0], n * kPointSize);
    if (!m_stream.good())
      MYTHROW(ReadException, ("File:", m_filePath));

    for (size_t j = 0; j < n; ++j)
    {
      TItem item;
      Unpack(&buff[0] + j * kPointSize, item);
      if (!fn(item))
        return;
    }

    i += n;
  }
}
