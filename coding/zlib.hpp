#pragma once

#include "base/assert.hpp"
#include "base/macros.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <span>

#include "zlib.h"

namespace coding
{
// ZLib wrapper for compression and decompression.
class ZLib
{
public:
  class Inflate
  {
  public:
    enum class Format
    {
      ZLib,
      GZip,
      Both
    };

    explicit Inflate(Format format) noexcept : m_format(format) {}

    template <typename OutIt>
    std::optional<void> operator()(std::span<const std::byte> data, OutIt out) const
    {
      ASSERT(!data.empty(), ());
      InflateProcessor processor(m_format, data);
      return Process(processor, out);
    }

    template <typename OutIt>
    std::optional<void> operator()(std::string const & s, OutIt out) const
    {
      return (*this)(std::as_bytes(std::span{s}), out);
    }

  private:
    Format const m_format;
  };

  class Deflate
  {
  public:
    enum class Format
    {
      ZLib,
      GZip
    };

    enum class Level
    {
      NoCompression = Z_NO_COMPRESSION,
      BestSpeed = Z_BEST_SPEED,
      BestCompression = Z_BEST_COMPRESSION,
      DefaultCompression = Z_DEFAULT_COMPRESSION
    };

    Deflate(Format format, Level level) noexcept : m_format(format), m_level(level) {}

    template <typename OutIt>
    std::optional<void> operator()(std::span<const std::byte> data, OutIt out) const
    {
      ASSERT(!data.empty(), ());
      DeflateProcessor processor(m_format, m_level, data);
      return Process(processor, out);
    }

    template <typename OutIt>
    std::optional<void> operator()(std::string const & s, OutIt out) const
    {
      return (*this)(std::as_bytes(std::span{s}), out);
    }

  private:
    Format const m_format;
    Level const m_level;
  };

private:
  class Processor
  {
  public:
    static size_t constexpr kBufferSize = 8192;

    Processor(std::span<const std::byte> data) noexcept;
    virtual ~Processor() noexcept = default;

    inline bool IsInit() const noexcept { return m_init; }
    bool ConsumedAll() const;
    bool BufferIsFull() const;

    template <typename OutIt>
    void MoveOut(OutIt out)
    {
      ASSERT(IsInit(), ());
      std::copy(m_buffer, m_buffer + kBufferSize - m_stream.avail_out, out);
      m_stream.next_out = m_buffer;
      m_stream.avail_out = kBufferSize;
    }

  protected:
    z_stream m_stream{};
    bool m_init = false;
    unsigned char m_buffer[kBufferSize] = {};

    void InitStream();

    DISALLOW_COPY_AND_MOVE(Processor);
  };

  class DeflateProcessor final : public Processor
  {
  public:
    DeflateProcessor(Deflate::Format format, Deflate::Level level, std::span<const std::byte> data) noexcept;
    virtual ~DeflateProcessor() noexcept override;

    int Process(int flush);

    DISALLOW_COPY_AND_MOVE(DeflateProcessor);
  };

  class InflateProcessor final : public Processor
  {
  public:
    InflateProcessor(Inflate::Format format, std::span<const std::byte> data) noexcept;
    virtual ~InflateProcessor() noexcept override;

    int Process(int flush);

    DISALLOW_COPY_AND_MOVE(InflateProcessor);
  };

  template <typename Processor, typename OutIt>
  static std::optional<void> Process(Processor & processor, OutIt out)
  {
    if (!processor.IsInit())
      return std::nullopt;

    int ret = Z_OK;
    while (true)
    {
      int const flush = (processor.ConsumedAll() || ret == Z_STREAM_END) ? Z_FINISH : Z_NO_FLUSH;

      while (true)
      {
        ret = processor.Process(flush);
        if (ret != Z_OK && ret != Z_STREAM_END)
          return std::nullopt;

        if (!processor.BufferIsFull())
          break;

        processor.MoveOut(out);
      }

      if (flush == Z_FINISH && ret == Z_STREAM_END)
        break;
    }

    processor.MoveOut(out);
    return processor.ConsumedAll() ? std::optional<void>{} : std::nullopt;
  }
};

}  // namespace coding
