#include "platform/constants.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/platform.hpp"
#include "platform/platform_unix_impl.hpp"
#include "platform/settings.hpp"

#include "coding/zip_reader.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"
#include "base/thread.hpp"

#include <memory>
#include <regex>
#include <string>

#include <unistd.h>     // for sysconf
#include <sys/stat.h>

using namespace std;

Platform::Platform()
{
  /// @see initialization routine in android/app/src/main/cpp/app/organicmaps/platform/AndroidPlatform.hpp
  pl::SetMaxOpenFileLimit();
}

#ifdef DEBUG
namespace {
class DbgLogger
{
public:
  explicit DbgLogger(string const & file) : m_file(file) {}

  ~DbgLogger()
  {
    LOG(LDEBUG, ("Source for file", m_file, "is", m_src));
  }

  void SetSource(char src) { m_src = src; }

private:
  string const & m_file;
  char m_src;
};
}  // namespace
#endif


unique_ptr<ModelReader> Platform::GetReader(string const & file, string searchScope) const
{
  string ext = base::GetFileExtension(file);
  strings::AsciiToLower(ext);
  ASSERT(!ext.empty(), ());

  uint32_t const logPageSize = (ext == DATA_FILE_EXTENSION) ? READER_CHUNK_LOG_SIZE : 10;
  uint32_t const logPageCount = (ext == DATA_FILE_EXTENSION) ? READER_CHUNK_LOG_COUNT : 4;

  if (searchScope.empty())
  {
    if (file[0] == '/')
      searchScope = "f";
    else
    {
      ASSERT(ext != ".kml" && ext != ".kmb" && ext != ".kmz", ("BookmarkManager is responsible for that"));

      if (ext == DATA_FILE_EXTENSION)
      {
        if (file.starts_with(WORLD_COASTS_FILE_NAME) || file.starts_with(WORLD_FILE_NAME))
          searchScope = "wsr";
        else
          searchScope = "w";
      }
      else if (file == SETTINGS_FILE_NAME)
        searchScope = "s";
      else
        searchScope = "rw";
    }
  }

#ifdef DEBUG
  DbgLogger logger(file);
#endif

  for (char const s : searchScope)
  {
#ifdef DEBUG
    logger.SetSource(s);
#endif

    switch (s)
    {
    case 'w':
    {
      string const path = base::JoinPath(m_writableDir, file);
      if (IsFileExistsByFullPath(path))
        return make_unique<FileReader>(path, logPageSize, logPageCount);
      break;
    }

    case 's':
    {
      string const path = base::JoinPath(m_settingsDir, file);
      if (IsFileExistsByFullPath(path))
        return make_unique<FileReader>(path, logPageSize, logPageCount);
      break;
    }

    case 'f':
      if (IsFileExistsByFullPath(file))
        return make_unique<FileReader>(file, logPageSize, logPageCount);
      break;

    case 'r':
      ASSERT_EQUAL(file.find("assets/"), string::npos, ());
      try
      {
        return make_unique<ZipFileReader>(m_resourcesDir, "assets/" + file, logPageSize, logPageCount);
      }
      catch (Reader::OpenException const & e)
      {
        LOG(LWARNING, ("Can't get reader:", e.what()));
      }
      break;

    default:
      CHECK(false, ("Unsupported source:", s));
      break;
    }
  }

  LOG(LWARNING, ("Can't get reader for:", file, "in scope", searchScope));
  MYTHROW(FileAbsentException, ("File not found", file));
  return nullptr;
}

void Platform::GetFilesByRegExp(string const & directory, string const & regexp, FilesList & res)
{
  if (ZipFileReader::IsZip(directory))
  {
    // Get files list inside zip file
    typedef ZipFileReader::FileList FilesT;
    FilesT fList;
    ZipFileReader::FilesList(directory, fList);

    regex exp(regexp);

    for (FilesT::iterator it = fList.begin(); it != fList.end(); ++it)
    {
      string & name = it->first;
      if (regex_search(name.begin(), name.end(), exp))
      {
        // Remove assets/ prefix - clean files are needed for fonts white/blacklisting logic
        size_t const ASSETS_LENGTH = 7;
        if (name.find("assets/") == 0)
          name.erase(0, ASSETS_LENGTH);

        res.push_back(name);
      }
    }
  }
  else
    pl::EnumerateFilesByRegExp(directory, regexp, res);
}

int Platform::VideoMemoryLimit() const { return 10 * 1024 * 1024; }

int Platform::PreCachingDepth() const { return 3; }

bool Platform::GetFileSizeByName(string const & fileName, uint64_t & size) const
{
  try
  {
    size = ReaderPtr<Reader>(GetReader(fileName)).Size();
    return true;
  }
  catch (RootException const & ex)
  {
    LOG(LWARNING, ("Can't get file size for:", fileName));
    return false;
  }
}

// static
Platform::EError Platform::MkDir(string const & dirName)
{
  if (0 != mkdir(dirName.c_str(), 0755))
    return ErrnoToError();
  return Platform::ERR_OK;
}

void Platform::SetupMeasurementSystem() const
{
  auto units = measurement_utils::Units::Metric;
  if (settings::Get(settings::kMeasurementUnits, units))
    return;
  // @TODO Add correct implementation
  units = measurement_utils::Units::Metric;
  settings::Set(settings::kMeasurementUnits, units);
}

void Platform::GetSystemFontNames(FilesList & res) const
{
  bool wasRoboto = false;

  string const path = "/system/fonts/";
  pl::EnumerateFiles(path, [&](char const * entry)
  {
    string name(entry);
    if (name != "Roboto-Medium.ttf" && name != "Roboto-Regular.ttf")
    {
      if (!name.starts_with("NotoNaskh") && !name.starts_with("NotoSans"))
        return;

      if (name.find("-Regular") == string::npos)
        return;
    }
    else
      wasRoboto = true;

    res.push_back(path + name);
  });

  if (!wasRoboto)
  {
    string droidSans = path + "DroidSans.ttf";
    if (IsFileExistsByFullPath(droidSans))
      res.push_back(std::move(droidSans));
  }
}

// static
time_t Platform::GetFileCreationTime(std::string const & path)
{
  struct stat st;
  if (0 == stat(path.c_str(), &st))
    return st.st_atim.tv_sec;
  return 0;
}

// static
time_t Platform::GetFileModificationTime(std::string const & path)
{
  struct stat st;
  if (0 == stat(path.c_str(), &st))
    return st.st_mtim.tv_sec;
  return 0;
}
