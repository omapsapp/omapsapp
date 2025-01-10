#include "coding/zlib.hpp"
#include <optional>
#include <span>

namespace coding
{
namespace
{
constexpr int kGzipBits = 16;
constexpr int kBothBits = 32;

constexpr int ToInt(ZLib::Deflate::Level level)
{
  using Level = ZLib::Deflate::Level;
  switch (level)
  {
  case Level::NoCompression: return Z_NO_COMPRESSION;
  case Level::BestSpeed: return Z_BEST_SPEED;
  case Level::BestCompression: return Z_BEST_COMPRESSION;
  case Level::DefaultCompression: return Z_DEFAULT_COMPRESSION;
  }
  UNREACHABLE();
}

template <typename InitFunc>
std::optional<z_stream> InitStream(InitFunc func)
{
  z_stream stream{};
  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;

  if (func(stream) == Z_OK)
    return stream;
  return std::nullopt;
}
}  // namespace

// ZLib::Processor ---------------------------------------------------------------------------------
ZLib::Processor::Processor(std::span<const std::byte> data) noexcept : m_init(false)
{
  ASSERT(!data.empty(), ());
  m_stream.next_in = reinterpret_cast<unsigned char const*>(data.data());
  m_stream.avail_in = static_cast<unsigned int>(data.size());

  m_stream.next_out = m_buffer;
  m_stream.avail_out = kBufferSize;

  m_stream.zalloc = Z_NULL;
  m_stream.zfree = Z_NULL;
  m_stream.opaque = Z_NULL;
}

bool ZLib::Processor::ConsumedAll() const
{
  ASSERT(IsInit(), ());
  return m_stream.avail_in == 0;
}

bool ZLib::Processor::BufferIsFull() const
{
  ASSERT(IsInit(), ());
  return m_stream.avail_out == 0;
}

// ZLib::Deflate -----------------------------------------------------------------------------------
ZLib::DeflateProcessor::DeflateProcessor(Deflate::Format format, Deflate::Level level,
                                         std::span<const std::byte> data) noexcept
  : Processor(data)
{
  auto bits = MAX_WBITS;
  switch (format)
  {
  case Deflate::Format::ZLib: break;
  case Deflate::Format::GZip: bits |= kGzipBits; break;
  }

  auto maybeStream = InitStream([&](z_stream& stream) {
    return deflateInit2(&stream, ToInt(level), Z_DEFLATED, bits, 8, Z_DEFAULT_STRATEGY);
  });

  if (maybeStream)
  {
    m_stream = *maybeStream;
    m_init = true;
  }
}

ZLib::DeflateProcessor::~DeflateProcessor() noexcept
{
  if (m_init)
    deflateEnd(&m_stream);
}

int ZLib::DeflateProcessor::Process(int flush)
{
  ASSERT(IsInit(), ());
  return deflate(&m_stream, flush);
}

// ZLib::Inflate -----------------------------------------------------------------------------------
ZLib::InflateProcessor::InflateProcessor(Inflate::Format format, std::span<const std::byte> data) noexcept
  : Processor(data)
{
  auto bits = MAX_WBITS;
  switch (format)
  {
  case Inflate::Format::ZLib: break;
  case Inflate::Format::GZip: bits |= kGzipBits; break;
  case Inflate::Format::Both: bits |= kBothBits; break;
  }

  auto maybeStream = InitStream([&](z_stream& stream) {
    return inflateInit2(&stream, bits);
  });

  if (maybeStream)
  {
    m_stream = *maybeStream;
    m_init = true;
  }
}

ZLib::InflateProcessor::~InflateProcessor() noexcept
{
  if (m_init)
    inflateEnd(&m_stream);
}

int ZLib::InflateProcessor::Process(int flush)
{
  ASSERT(IsInit(), ());
  return inflate(&m_stream, flush);
}
}  // namespace coding
