#include "private.h"
#include "platform/platform.hpp"
#include "platform/platform_unix_impl.hpp"

#include "platform/socket.hpp"

#include "coding/file_reader.hpp"

#include "base/exception.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/macros.hpp"
#include "base/scope_guard.hpp"

#include <algorithm>
#include <functional>  // bind
#include <initializer_list>
#include <optional>
#include <string>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>  // strrchr
#include <unistd.h>  // access, readlink

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <linux/limits.h>  // PATH_MAX
#include <netinet/in.h>

#include <QStandardPaths>  // writableLocation GenericConfigLocation

namespace
{
// Returns directory where binary resides, including slash at the end.
std::optional<std::string> GetExecutableDir()
{
  char path[PATH_MAX] = {};
  if (::readlink("/proc/self/exe", path, ARRAY_SIZE(path)) <= 0)
    return {};
  *(strrchr(path, '/') + 1) = '\0';
  return path;
}

// Returns true if EULA file exists in a directory.
bool IsWelcomeExist(std::string const & dir)
{
  return Platform::IsFileExistsByFullPath(base::JoinPath(dir, "welcome.html"));
}

// Returns string value of an environment variable.
std::optional<std::string> GetEnv(char const * var)
{
  char const * value = ::getenv(var);
  if (value == nullptr)
    return {};
  return value;
}

bool IsDirWritable(std::string const & dir)
{
  return ::access(dir.c_str(), W_OK) == 0;
}
}  // namespace

namespace platform
{
std::unique_ptr<Socket> CreateSocket()
{
  return std::unique_ptr<Socket>();
}
} // namespace platform


Platform::Platform()
{
  pl::SetMaxOpenFileLimit();

  using base::JoinPath;
  // Current executable's path with a trailing slash.
  auto const execDir = GetExecutableDir();
  CHECK(execDir, ("Can't retrieve the path to executable"));
  // Home directory without a trailing slash.
  auto const homeDir = GetEnv("HOME");
  CHECK(homeDir, ("Can't retrieve home directory"));

  // XDG config directory, usually ~/.config/OMaps/
  m_settingsDir = JoinPath(
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation).toStdString(),
      "OMaps");
  if (!IsFileExistsByFullPath(JoinPath(m_settingsDir, SETTINGS_FILE_NAME)) && !MkDirRecursively(m_settingsDir))
    MYTHROW(FileSystemException, ("Can't create directory", m_settingsDir));
  m_settingsDir += '/';

  // Override dirs from the env.
  if (auto const dir = GetEnv("MWM_WRITABLE_DIR"))
    m_writableDir = *dir;

  if (auto const dir = GetEnv("MWM_RESOURCES_DIR"))
    m_resourcesDir = *dir;
  else
  { // Guess the existing resources directory.
    std::string const dirsToScan[] = {
        "./data",  // symlink in the current folder
        "../data",  // 'build' folder inside the repo
        JoinPath(*execDir, "..", "organicmaps", "data"),  // build-omim-{debug,release}
        JoinPath(*execDir, "..", "share"),  // installed version with packages
        JoinPath(*execDir, "..", "OMaps"),  // installed version without packages
        JoinPath(*execDir, "..", "share", "organicmaps", "data"),  // flatpak-build
    };
    for (auto const & dir : dirsToScan)
    {
      if (IsWelcomeExist(dir))
      {
        m_resourcesDir = dir;
        if (m_writableDir.empty() && IsDirWritable(dir))
          m_writableDir = m_resourcesDir;
        break;
      }
    }
  }
  // Use ~/.local/share/OMaps if resources directory was not writable.
  if (!m_resourcesDir.empty() && m_writableDir.empty())
  {
    // The writableLocation does the same for AppDataLocation, AppLocalDataLocation,
    // and GenericDataLocation. Provided, that test mode is not enabled, then
    // first it checks ${XDG_DATA_HOME}, if empty then it falls back to ${HOME}/.local/share
    m_writableDir = JoinPath(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation).toStdString(), "OMaps");

    if (!MkDirRecursively(m_writableDir))
      MYTHROW(FileSystemException, ("Can't create writable directory:", m_writableDir));
  }
  // Here one or both m_resourcesDir and m_writableDir still may be empty.
  // Tests or binary may initialize them later.
  using base::AddSlashIfNeeded;
  if (!m_writableDir.empty())
    m_writableDir = AddSlashIfNeeded(m_writableDir);
  if (!m_resourcesDir.empty())
    m_resourcesDir = AddSlashIfNeeded(m_resourcesDir);

  // Select directory for temporary files.
  for (auto const & dir : { GetEnv("TMPDIR"), GetEnv("TMP"), GetEnv("TEMP"), {"/tmp"}})
  {
    if (dir && IsFileExistsByFullPath(*dir) && IsDirWritable(*dir))
    {
      m_tmpDir = AddSlashIfNeeded(*dir);
      break;
    }
  }

  m_guiThread = std::make_unique<platform::GuiThread>();

  LOG(LDEBUG, ("Resources directory:", m_resourcesDir));
  LOG(LDEBUG, ("Writable directory:", m_writableDir));
  LOG(LDEBUG, ("Tmp directory:", m_tmpDir));
  LOG(LDEBUG, ("Settings directory:", m_settingsDir));
}

std::string Platform::DeviceName() const
{
  return OMIM_OS_NAME;
}

std::string Platform::DeviceModel() const
{
  return {};
}

Platform::EConnectionType Platform::ConnectionStatus()
{
  int socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  SCOPE_GUARD(closeSocket, std::bind(&close, socketFd));
  if (socketFd < 0)
    return EConnectionType::CONNECTION_NONE;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(80);
  inet_pton(AF_INET, DEFAULT_CONNECTION_CHECK_IP, &addr.sin_addr);

  if (connect(socketFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
    return EConnectionType::CONNECTION_NONE;

  return EConnectionType::CONNECTION_WIFI;
}

Platform::ChargingStatus Platform::GetChargingStatus()
{
  return Platform::ChargingStatus::Plugged;
}

uint8_t Platform::GetBatteryLevel()
{
  // This value is always 100 for desktop.
  return 100;
}

void Platform::GetSystemFontNames(FilesList & res) const
{
  char constexpr const * const fontsWhitelist[] = {
    "Roboto-Medium.ttf",
    "Roboto-Regular.ttf",
    "DroidSansFallback.ttf",
    "DroidSansFallbackFull.ttf",
    "DroidSans.ttf",
    "DroidSansArabic.ttf",
    "DroidSansSemc.ttf",
    "DroidSansSemcCJK.ttf",
    "DroidNaskh-Regular.ttf",
    "Lohit-Bengali.ttf",
    "Lohit-Devanagari.ttf",
    "Lohit-Tamil.ttf",
    "PakType Naqsh.ttf",
    "wqy-microhei.ttc",
    "Jomolhari.ttf",
    "Padauk.ttf",
    "KhmerOS.ttf",
    "Umpush.ttf",
    "DroidSansThai.ttf",
    "DroidSansArmenian.ttf",
    "DroidSansEthiopic-Regular.ttf",
    "DroidSansGeorgian.ttf",
    "DroidSansHebrew-Regular.ttf",
    "DroidSansHebrew.ttf",
    "DroidSansJapanese.ttf",
    "LTe50872.ttf",
    "LTe50259.ttf",
    "DevanagariOTS.ttf",
    "FreeSans.ttf",
    "DejaVuSans.ttf",
    "arial.ttf",
    "AbyssinicaSIL-R.ttf",
  };

  std::string const systemFontsPath[] = {
    "/usr/share/fonts/truetype/roboto/",
    "/usr/share/fonts/truetype/droid/",
    "/usr/share/fonts/truetype/dejavu/",
    "/usr/share/fonts/truetype/ttf-dejavu/",
    "/usr/share/fonts/truetype/wqy/",
    "/usr/share/fonts/truetype/freefont/",
    "/usr/share/fonts/truetype/padauk/",
    "/usr/share/fonts/truetype/dzongkha/",
    "/usr/share/fonts/truetype/ttf-khmeros-core/",
    "/usr/share/fonts/truetype/tlwg/",
    "/usr/share/fonts/truetype/abyssinica/",
    "/usr/share/fonts/truetype/paktype/",
  };

  for (auto font : fontsWhitelist)
  {
    for (auto sysPath : systemFontsPath)
    {
      std::string path = sysPath + font;
      if (IsFileExistsByFullPath(path))
        res.push_back(std::move(path));
    }
  }
}

// static
time_t Platform::GetFileCreationTime(std::string const & path)
{
// In older Linux versions there is no reliable way to get file creation time.
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 28
  struct statx st;
  if (0 == statx(AT_FDCWD, path.c_str(), 0, STATX_BTIME, &st))
    return st.stx_btime.tv_sec;
#else
  struct stat st;
  if (0 == stat(path.c_str(), &st))
    return std::min(st.st_atim.tv_sec, st.st_mtim.tv_sec);
#endif
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
