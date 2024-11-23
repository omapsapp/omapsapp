#include "platform/platform.hpp"

#include "coding/internal/file_data.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/random.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <thread>

#include "private.h"

#include <cerrno>

namespace
{
std::string RandomString(size_t length)
{
  /// @todo Used for temp file name, so lower-upper case is strange here, no?
  static std::string_view constexpr kCharset =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

  base::UniformRandom<size_t> rand(0, kCharset.size() - 1);
  std::string str(length, 0);
  std::generate_n(str.begin(), length, [&rand]() { return kCharset[rand()]; });
  return str;
}

bool IsSpecialDirName(std::string const & dirName)
{
  return dirName == "." || dirName == "..";
}

bool GetFileTypeChecked(std::string const & path, Platform::EFileType & type)
{
  Platform::EError const ret = Platform::GetFileType(path, type);
  if (ret != Platform::ERR_OK)
  {
    LOG(LERROR, ("Can't determine file type for", path, ":", ret));
    return false;
  }
  return true;
}
} // namespace

// static
Platform::EError Platform::ErrnoToError()
{
  switch (errno)
  {
  case ENOENT:
    return ERR_FILE_DOES_NOT_EXIST;
  case EACCES:
    return ERR_ACCESS_FAILED;
  case ENOTEMPTY:
    return ERR_DIRECTORY_NOT_EMPTY;
  case EEXIST:
    return ERR_FILE_ALREADY_EXISTS;
  case ENAMETOOLONG:
    return ERR_NAME_TOO_LONG;
  case ENOTDIR:
    return ERR_NOT_A_DIRECTORY;
  case ELOOP:
    return ERR_SYMLINK_LOOP;
  case EIO:
    return ERR_IO_ERROR;
  default:
    return ERR_UNKNOWN;
  }
}

// static
bool Platform::RmDirRecursively(std::string const & dirName)
{
  if (dirName.empty() || IsSpecialDirName(dirName))
    return false;

  bool res = true;

  FilesList allFiles;
  GetFilesByRegExp(dirName, ".*", allFiles);
  for (std::string const & file : allFiles)
  {
    std::string const path = base::JoinPath(dirName, file);

    EFileType type;
    if (GetFileType(path, type) != ERR_OK)
      continue;

    if (type == EFileType::Directory)
    {
      if (!IsSpecialDirName(file) && !RmDirRecursively(path))
        res = false;
    }
    else
    {
      if (!base::DeleteFileX(path))
        res = false;
    }
  }

  if (RmDir(dirName) != ERR_OK)
    res = false;

  return res;
}

void Platform::SetSettingsDir(std::string const & path)
{
  m_settingsDir = base::AddSlashIfNeeded(path);
}

std::string Platform::SettingsPathForFile(std::string const & file) const
{
  return base::JoinPath(SettingsDir(), file);
}

std::string Platform::WritablePathForFile(std::string const & file) const
{
  return base::JoinPath(WritableDir(), file);
}

std::string Platform::ReadPathForFile(std::string const & file, std::string searchScope) const
{
  if (searchScope.empty())
    searchScope = "wrf";

  std::string fullPath;
  for (size_t i = 0; i < searchScope.size(); ++i)
  {
    switch (searchScope[i])
    {
    case 'w':
      ASSERT(!m_writableDir.empty(), ());
      fullPath = base::JoinPath(m_writableDir, file);
      break;
    case 'r':
      ASSERT(!m_resourcesDir.empty(), ());
      fullPath = base::JoinPath(m_resourcesDir, file);
      break;
    case 's':
      ASSERT(!m_settingsDir.empty(), ());
      fullPath = base::JoinPath(m_settingsDir, file);
      break;
    case 'f':
      fullPath = file;
      break;
    default :
      CHECK(false, ("Unsupported searchScope:", searchScope));
      break;
    }
    if (IsFileExistsByFullPath(fullPath))
      return fullPath;
  }

  MYTHROW(FileAbsentException, ("File", file, "doesn't exist in the scope", searchScope,
      "\nw: ", m_writableDir, "\nr: ", m_resourcesDir, "\ns: ", m_settingsDir));
}

std::string Platform::MetaServerUrl() const
{
  return METASERVER_URL;
}

std::string Platform::DefaultUrlsJSON() const
{
  return DEFAULT_URLS_JSON;
}

bool Platform::RemoveFileIfExists(std::string const & filePath)
{
  return IsFileExistsByFullPath(filePath) ? base::DeleteFileX(filePath) : true;
}

std::string Platform::TmpPathForFile() const
{
  size_t constexpr kNameLen = 32;
  return base::JoinPath(TmpDir(), RandomString(kNameLen));
}

std::string Platform::TmpPathForFile(std::string const & prefix, std::string const & suffix) const
{
  size_t constexpr kRandomLen = 8;
  return base::JoinPath(TmpDir(), prefix + RandomString(kRandomLen) + suffix);
}

void Platform::GetFontNames(FilesList & res) const
{
  ASSERT(res.empty(), ());

  /// @todo Actually, this list should present once in all our code.
  char constexpr const * arrDef[] = {
    "fonts/00_NotoNaskhArabic-Regular.ttf",
    "fonts/00_NotoSansBengali-Regular.ttf",
    "fonts/00_NotoSansHebrew-Regular.ttf",
    "fonts/00_NotoSansMalayalam-Regular.ttf",
    "fonts/00_NotoSansThai-Regular.ttf",
    "fonts/00_NotoSerifDevanagari-Regular.ttf",
    "fonts/01_dejavusans.ttf",
    "fonts/02_droidsans-fallback.ttf",
    "fonts/03_jomolhari-id-a3d.ttf",
    "fonts/04_padauk.ttf",
    "fonts/05_khmeros.ttf",
    "fonts/06_code2000.ttf",
    "fonts/07_roboto_medium.ttf",
  };
  res.insert(res.end(), arrDef, arrDef + ARRAY_SIZE(arrDef));

  GetSystemFontNames(res);

  LOG(LINFO, ("Available font files:", (res)));
}

void Platform::GetFilesByExt(std::string const & directory, std::string_view ext, FilesList & outFiles)
{
  // Transform extension mask to regexp (.mwm -> \.mwm$)
  ASSERT ( !ext.empty(), () );
  ASSERT_EQUAL ( ext[0], '.' , () );
  std::string regexp = "\\";
  GetFilesByRegExp(directory, regexp.append(ext).append("$"), outFiles);
}

// static
void Platform::GetFilesByType(std::string const & directory, unsigned typeMask,
                              TFilesWithType & outFiles)
{
  FilesList allFiles;
  GetFilesByRegExp(directory, ".*", allFiles);
  for (auto const & file : allFiles)
  {
    EFileType type;
    if (GetFileType(base::JoinPath(directory, file), type) != ERR_OK)
      continue;
    if (typeMask & type)
      outFiles.emplace_back(file, type);
  }
}

// static
bool Platform::IsDirectory(std::string const & path)
{
  EFileType fileType;
  if (GetFileType(path, fileType) != ERR_OK)
    return false;
  return fileType == EFileType::Directory;
}

// static
void Platform::GetFilesRecursively(std::string const & directory, FilesList & filesList)
{
  TFilesWithType files;

  GetFilesByType(directory, EFileType::Regular, files);
  for (auto const & p : files)
  {
    auto const & file = p.first;
    CHECK_EQUAL(p.second, EFileType::Regular, ("dir:", directory, "file:", file));
    filesList.push_back(base::JoinPath(directory, file));
  }

  TFilesWithType subdirs;
  GetFilesByType(directory, EFileType::Directory, subdirs);

  for (auto const & p : subdirs)
  {
    auto const & subdir = p.first;
    CHECK_EQUAL(p.second, EFileType::Directory, ("dir:", directory, "subdir:", subdir));
    if (subdir == "." || subdir == "..")
      continue;

    GetFilesRecursively(base::JoinPath(directory, subdir), filesList);
  }
}

void Platform::SetWritableDirForTests(std::string const & path)
{
  m_writableDir = base::AddSlashIfNeeded(path);
}

void Platform::SetResourceDir(std::string const & path)
{
  m_resourcesDir = base::AddSlashIfNeeded(path);
}

// static
bool Platform::MkDirChecked(std::string const & dirName)
{
  switch (EError const ret = MkDir(dirName))
  {
  case ERR_OK: return true;
  case ERR_FILE_ALREADY_EXISTS:
  {
    EFileType type;
    if (!GetFileTypeChecked(dirName, type))
      return false;
    if (type != Directory)
    {
      LOG(LERROR, (dirName, "exists, but not a dirName:", type));
      return false;
    }
    return true;
  }
  default: LOG(LERROR, (dirName, "can't be created:", ret)); return false;
  }
}

// static
bool Platform::MkDirRecursively(std::string const & dirName)
{
  CHECK(!dirName.empty(), ());

  std::string::value_type const sep[] = { base::GetNativeSeparator(), 0};
  std::string path = dirName.starts_with(sep[0]) ? sep : ".";
  for (auto const & t : strings::Tokenize(dirName, sep))
  {
    path = base::JoinPath(path, std::string{t});
    if (!IsFileExistsByFullPath(path))
    {
      switch (MkDir(path))
      {
      case ERR_OK: break;
      case ERR_FILE_ALREADY_EXISTS:
      {
        if (!IsDirectory(path))
          return false;
        break;
      }
      default: return false;
      }
    }
  }

  return true;
}

unsigned Platform::CpuCores()
{
  unsigned const cores = std::thread::hardware_concurrency();
  return cores > 0 ? cores : 1;
}

void Platform::ShutdownThreads()
{
  ASSERT(m_networkThread && m_fileThread && m_backgroundThread, ());
  ASSERT(!m_networkThread->IsShutDown(), ());
  ASSERT(!m_fileThread->IsShutDown(), ());
  ASSERT(!m_backgroundThread->IsShutDown(), ());

  m_batteryTracker.UnsubscribeAll();

  m_networkThread->ShutdownAndJoin();
  m_fileThread->ShutdownAndJoin();
  m_backgroundThread->ShutdownAndJoin();
}

void Platform::RunThreads()
{
  ASSERT(!m_networkThread || m_networkThread->IsShutDown(), ());
  ASSERT(!m_fileThread || m_fileThread->IsShutDown(), ());
  ASSERT(!m_backgroundThread || m_backgroundThread->IsShutDown(), ());

  m_networkThread = std::make_unique<base::DelayedThreadPool>();
  m_fileThread = std::make_unique<base::DelayedThreadPool>();
  m_backgroundThread = std::make_unique<base::DelayedThreadPool>();
}

void Platform::SetGuiThread(std::unique_ptr<base::TaskLoop> guiThread)
{
  m_guiThread = std::move(guiThread);
}

void Platform::CancelTask(Thread thread, base::TaskLoop::TaskId id)
{
  ASSERT(m_networkThread && m_fileThread && m_backgroundThread, ());
  switch (thread)
  {
  case Thread::File: m_fileThread->Cancel(id); return;
  case Thread::Network: m_networkThread->Cancel(id); return;
  case Thread::Gui: CHECK(false, ("Task cancelling for gui thread is not supported yet")); return;
  case Thread::Background: m_backgroundThread->Cancel(id); return;
  }
}

std::string DebugPrint(Platform::EError err)
{
  switch (err)
  {
  case Platform::ERR_OK: return "Ok";
  case Platform::ERR_FILE_DOES_NOT_EXIST: return "File does not exist.";
  case Platform::ERR_ACCESS_FAILED: return "Access failed.";
  case Platform::ERR_DIRECTORY_NOT_EMPTY: return "Directory not empty.";
  case Platform::ERR_FILE_ALREADY_EXISTS: return "File already exists.";
  case Platform::ERR_NAME_TOO_LONG:
    return "The length of a component of path exceeds {NAME_MAX} characters.";
  case Platform::ERR_NOT_A_DIRECTORY:
    return "A component of the path prefix of Path is not a directory.";
  case Platform::ERR_SYMLINK_LOOP:
    return "Too many symbolic links were encountered in translating path.";
  case Platform::ERR_IO_ERROR: return "An I/O error occurred.";
  case Platform::ERR_UNKNOWN: return "Unknown";
  }
  UNREACHABLE();
}

std::string DebugPrint(Platform::ChargingStatus status)
{
  switch (status)
  {
  case Platform::ChargingStatus::Unknown: return "Unknown";
  case Platform::ChargingStatus::Plugged: return "Plugged";
  case Platform::ChargingStatus::Unplugged: return "Unplugged";
  }
  UNREACHABLE();
}
