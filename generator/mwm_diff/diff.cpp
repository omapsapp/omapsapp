#include "generator/mwm_diff/diff.hpp"

#include "coding/buffered_file_writer.hpp"
#include "coding/file_reader.hpp"
#include "coding/file_writer.hpp"
#include "coding/reader.hpp"
#include "coding/write_to_sink.hpp"
#include "coding/writer.hpp"
#include "coding/zlib.hpp"

#include "base/assert.hpp"
#include "base/cancellable.hpp"
#include "base/checked_cast.hpp"
#include "base/logging.hpp"

#include <cstdint>
#include <iterator>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace
{
enum Version
{
  // Format Version 0: XOR-based diff + gzip.
  VERSION_V0 = 0,
  VERSION_LATEST = VERSION_V0
};

bool MakeDiffVersion0(FileReader & oldReader, FileReader & newReader, FileWriter & diffFileWriter, base::Cancellable const & cancellable)
{
  try
  {
    std::vector<uint8_t> oldData(oldReader.Size());
    oldReader.Read(0, oldData.data(), oldData.size());

    std::vector<uint8_t> newData(newReader.Size());
    newReader.Read(0, newData.data(), newData.size());

    std::vector<uint8_t> diffBuf(std::max(oldData.size(), newData.size()));
    for (size_t i = 0; i < diffBuf.size(); ++i)
    {
      if (cancellable.IsCancelled())
      {
        LOG(LINFO, ("MakeDiffVersion0 cancelled at index", i));
        return false;
      }
      diffBuf[i] = (i < oldData.size() ? oldData[i] : 0) ^ (i < newData.size() ? newData[i] : 0);
    }

    using Deflate = coding::ZLib::Deflate;
    Deflate deflate(Deflate::Format::ZLib, Deflate::Level::BestCompression);

    std::vector<uint8_t> deflatedDiffBuf;
    deflate(diffBuf.data(), diffBuf.size(), back_inserter(deflatedDiffBuf));

    // A basic header that holds only version.
    WriteToSink(diffFileWriter, static_cast<uint32_t>(VERSION_V0));
    diffFileWriter.Write(deflatedDiffBuf.data(), deflatedDiffBuf.size());

    return true;
  }
  catch (std::exception const & e)
  {
    LOG(LERROR, ("Error during MakeDiffVersion0:", e.what()));
    return false;
  }
}

generator::mwm_diff::DiffApplicationResult ApplyDiffVersion0(
    FileReader & oldReader, FileWriter & newWriter, ReaderSource<FileReader> & diffFileSource,
    base::Cancellable const & cancellable)
{
  using generator::mwm_diff::DiffApplicationResult;

  try
  {
    std::vector<uint8_t> deflatedDiff(base::checked_cast<size_t>(diffFileSource.Size()));
    diffFileSource.Read(deflatedDiff.data(), deflatedDiff.size());

    if (cancellable.IsCancelled())
    {
      LOG(LINFO, ("ApplyDiffVersion0 cancelled before inflation"));
      return DiffApplicationResult::Cancelled;
    }

    using Inflate = coding::ZLib::Inflate;
    Inflate inflate(Inflate::Format::ZLib);
    std::vector<uint8_t> diffBuf;
    if (!inflate(deflatedDiff.data(), deflatedDiff.size(), back_inserter(diffBuf)))
    {
      LOG(LERROR, ("Inflation failed"));
      return DiffApplicationResult::Failed;
    }

    if (cancellable.IsCancelled())
    {
      LOG(LINFO, ("ApplyDiffVersion0 cancelled during inflation"));
      return DiffApplicationResult::Cancelled;
    }

    std::vector<uint8_t> oldData(oldReader.Size());
    oldReader.Read(0, oldData.data(), oldData.size());

    std::vector<uint8_t> newData(diffBuf.size());
    for (size_t i = 0; i < newData.size(); ++i)
    {
      if (cancellable.IsCancelled())
      {
        LOG(LINFO, ("ApplyDiffVersion0 cancelled at index", i));
        return DiffApplicationResult::Cancelled;
      }
      newData[i] = (i < oldData.size() ? oldData[i] : 0) ^ diffBuf[i];
    }

    newWriter.Write(newData.data(), newData.size());

    return DiffApplicationResult::Ok;
  }
  catch (std::exception const & e)
  {
    LOG(LERROR, ("Error during ApplyDiffVersion0:", e.what()));
    return DiffApplicationResult::Failed;
  }
}
}  // namespace

namespace generator
{
namespace mwm_diff
{
bool MakeDiff(std::string const & oldMwmPath, std::string const & newMwmPath,
              std::string const & diffPath, base::Cancellable const & cancellable)
{
  try
  {
    FileReader oldReader(oldMwmPath);
    FileReader newReader(newMwmPath);
    FileWriter diffFileWriter(diffPath);

    switch (VERSION_LATEST)
    {
    case VERSION_V0: return MakeDiffVersion0(oldReader, newReader, diffFileWriter, cancellable);
    default:
      LOG(LERROR,
          ("Making mwm diffs with diff format version", VERSION_LATEST, "is not implemented"));
    }
  }
  catch (Reader::Exception const & e)
  {
    LOG(LERROR, ("Could not open file when creating a patch:", e.Msg()));
    return false;
  }
  catch (Writer::Exception const & e)
  {
    LOG(LERROR, ("Could not open file when creating a patch:", e.Msg()));
    return false;
  }

  return false;
}

DiffApplicationResult ApplyDiff(std::string const & oldMwmPath, std::string const & newMwmPath,
                                std::string const & diffPath, base::Cancellable const & cancellable)
{
  try
  {
    FileReader oldReader(oldMwmPath);
    BufferedFileWriter newWriter(newMwmPath);
    FileReader diffFileReader(diffPath);

    ReaderSource<FileReader> diffFileSource(diffFileReader);
    auto const version = ReadPrimitiveFromSource<uint32_t>(diffFileSource);

    switch (version)
    {
    case VERSION_V0:
      return ApplyDiffVersion0(oldReader, newWriter, diffFileSource, cancellable);
    default:
      LOG(LERROR, ("Unknown version format of mwm diff:", version));
      return DiffApplicationResult::Failed;
    }
  }
  catch (Reader::Exception const & e)
  {
    LOG(LERROR, ("Could not open file for reading when applying a patch:", e.Msg()));
  }
  catch (Writer::Exception const & e)
  {
    LOG(LERROR, ("Could not open file for writing when applying a patch:", e.Msg()));
  }

  return cancellable.IsCancelled() ? DiffApplicationResult::Cancelled
                                   : DiffApplicationResult::Failed;
}

std::string DebugPrint(DiffApplicationResult const & result)
{
  switch (result)
  {
  case DiffApplicationResult::Ok: return "Ok";
  case DiffApplicationResult::Failed: return "Failed";
  case DiffApplicationResult::Cancelled: return "Cancelled";
  }
  UNREACHABLE();
}
}  // namespace mwm_diff
}  // namespace generator
