#pragma once

#include "platform/battery_tracker.hpp"
#include "platform/country_defines.hpp"
#include "platform/gui_thread.hpp"
#include "platform/secure_storage.hpp"

#include "coding/reader.hpp"

#include "base/exception.hpp"
#include "base/macros.hpp"
#include "base/task_loop.hpp"
#include "base/thread_pool_delayed.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "defines.hpp"

DECLARE_EXCEPTION(FileAbsentException, RootException);
DECLARE_EXCEPTION(FileSystemException, RootException);

namespace platform
{
class LocalCountryFile;
}

class Platform;

extern Platform & GetPlatform();

class Platform
{
public:
  friend class ThreadRunner;

  // ThreadRunner may be subclassed for testing purposes.
  class ThreadRunner
  {
  public:
    ThreadRunner() { GetPlatform().RunThreads(); }
    virtual ~ThreadRunner() { GetPlatform().ShutdownThreads(); }
  };

  enum EError
  {
    ERR_OK = 0,
    ERR_FILE_DOES_NOT_EXIST,
    ERR_ACCESS_FAILED,
    ERR_DIRECTORY_NOT_EMPTY,
    ERR_FILE_ALREADY_EXISTS,
    ERR_NAME_TOO_LONG,
    ERR_NOT_A_DIRECTORY,
    ERR_SYMLINK_LOOP,
    ERR_IO_ERROR,
    ERR_UNKNOWN
  };

  enum EFileType
  {
    FILE_TYPE_UNKNOWN = 0x1,
    FILE_TYPE_REGULAR = 0x2,
    FILE_TYPE_DIRECTORY = 0x4
  };

  enum class EConnectionType : uint8_t
  {
    CONNECTION_NONE,
    CONNECTION_WIFI,
    CONNECTION_WWAN
  };

  enum class ChargingStatus : uint8_t
  {
    Unknown,
    Plugged,
    Unplugged
  };

  enum class Thread : uint8_t
  {
    File,
    Network,
    Gui,
    Background,
  };

  using TFilesWithType = std::vector<std::pair<std::string, EFileType>>;

protected:
  /// Usually read-only directory for application resources
  std::string m_resourcesDir;
  /// Writable directory to store downloaded map data
  /// @note on some systems it can point to external ejectable storage
  std::string m_writableDir;
  /// Application private directory.
  std::string m_privateDir;
  /// Temporary directory, can be cleaned up by the system
  std::string m_tmpDir;
  /// Writable directory to store persistent application data
  std::string m_settingsDir;

  /// Default search scope for resource files.
  /// Used in Android only and initialized according to the market type (Play, Amazon, Samsung).
  std::string m_androidDefResScope;

  /// Used in Android only to get corret GUI elements layout.
  bool m_isTablet;

  /// Returns last system call error as EError.
  static EError ErrnoToError();

  /// Platform-dependent secure storage.
  platform::SecureStorage m_secureStorage;

  std::unique_ptr<base::TaskLoop> m_guiThread;

  std::unique_ptr<base::thread_pool::delayed::ThreadPool> m_networkThread;
  std::unique_ptr<base::thread_pool::delayed::ThreadPool> m_fileThread;
  std::unique_ptr<base::thread_pool::delayed::ThreadPool> m_backgroundThread;

  platform::BatteryLevelTracker m_batteryTracker;

public:
  Platform();
  virtual ~Platform() = default;

  static bool IsFileExistsByFullPath(std::string const & filePath);
  static void DisableBackupForFile(std::string const & filePath);
  static bool RemoveFileIfExists(std::string const & filePath);

  /// @returns path to current working directory.
  /// @note In case of an error returns an empty std::string.
  static std::string GetCurrentWorkingDirectory() noexcept;
  /// @return always the same writable dir for current user with slash at the end
  std::string const & WritableDir() const { return m_writableDir; }
  /// Set writable dir — use for testing and linux stuff only
  void SetWritableDirForTests(std::string const & path);
  /// @return full path to file in user's writable directory
  std::string WritablePathForFile(std::string const & file) const { return WritableDir() + file; }
  /// Uses m_writeableDir [w], m_resourcesDir [r], m_settingsDir [s].
  std::string ReadPathForFile(std::string const & file,
                              std::string searchScope = std::string()) const;

  /// @return resource dir (on some platforms it's differ from Writable dir)
  std::string const & ResourcesDir() const { return m_resourcesDir; }
  /// @note! This function is used in generator_tool and unit tests.
  /// Client app should not replace default resource dir.
  void SetResourceDir(std::string const & path);

  /// Creates the directory in the filesystem.
  WARN_UNUSED_RESULT static EError MkDir(std::string const & dirName);

  /// Creates the directory. Returns true on success.
  /// Returns false and logs the reason on failure.
  WARN_UNUSED_RESULT static bool MkDirChecked(std::string const & dirName);

  // Creates the directory path dirName.
  // The function will create all parent directories necessary to create the directory.
  // Returns true if successful; otherwise returns false.
  // If the path already exists when this function is called, it will return true.
  // If it was possible to create only a part of the directories, the function will returns false
  // and will not restore the previous state of the file system.
  WARN_UNUSED_RESULT static bool MkDirRecursively(std::string const & dirName);

  /// Removes empty directory from the filesystem.
  static EError RmDir(std::string const & dirName);

  /// Removes directory from the filesystem.
  /// @note Directory can be non empty.
  /// @note If function fails, directory can be partially removed.
  static bool RmDirRecursively(std::string const & dirName);

  /// @return path for directory with temporary files with slash at the end
  std::string const & TmpDir() const { return m_tmpDir; }
  /// @return full path to file in the temporary directory
  std::string TmpPathForFile(std::string const & file) const { return TmpDir() + file; }
  /// @return full random path to temporary file.
  std::string TmpPathForFile() const;
  /// @return full partially random path to temporary file.
  std::string TmpPathForFile(std::string const & prefix, std::string const & suffix) const;

  /// @return full path to the file where data for unit tests is stored.
  std::string TestsDataPathForFile(std::string const & file) const { return ReadPathForFile(file); }

  /// @return path for directory in the persistent memory, can be the same
  /// as WritableDir, but on some platforms it's different
  std::string const & SettingsDir() const { return m_settingsDir; }
  void SetSettingsDir(std::string const & path);
  /// @return full path to file in the settings directory
  std::string SettingsPathForFile(std::string const & file) const { return SettingsDir() + file; }

  /// Returns application private directory.
  std::string const & PrivateDir() const { return m_privateDir; }

  /// @return reader for file decriptor.
  /// @throws FileAbsentException
  /// @param[in] file name or full path which we want to read
  /// @param[in] searchScope looks for file in dirs in given order: \n
  /// [w]ritable, [r]esources, [s]ettings, by [f]ull path, [e]xternal resources,
  std::unique_ptr<ModelReader> GetReader(std::string const & file,
                                         std::string const & searchScope = std::string()) const;
  /// @return nullptr instead of throw if file not exist.
  std::unique_ptr<ModelReader> GetReaderSafe(std::string const & file,
                                             std::string const & searchScope = std::string()) const;

  /// @name File operations
  //@{
  using FilesList = std::vector<std::string>;
  /// Retrieves files list contained in given directory
  /// @param directory directory path with slash at the end
  //@{
  /// @param ext files extension to find, like ".mwm".
  static void GetFilesByExt(std::string const & directory, std::string const & ext,
                            FilesList & outFiles);
  static void GetFilesByRegExp(std::string const & directory, std::string const & regexp,
                               FilesList & outFiles);
  //@}

  static void GetFilesByType(std::string const & directory, unsigned typeMask,
                             TFilesWithType & outFiles);

  static void GetFilesRecursively(std::string const & directory, FilesList & filesList);

  static bool IsDirectoryEmpty(std::string const & directory);
  // Returns true if |path| refers to a directory. Returns false otherwise or on error.
  static bool IsDirectory(std::string const & path);

  static EError GetFileType(std::string const & path, EFileType & type);

  /// @return false if file is not exist
  /// @note Check files in Writable dir first, and in ReadDir if not exist in Writable dir
  bool GetFileSizeByName(std::string const & fileName, uint64_t & size) const;
  /// @return false if file is not exist
  /// @note Try do not use in client production code
  static bool GetFileSizeByFullPath(std::string const & filePath, uint64_t & size);
  //@}

  /// Used to check available free storage space for downloading.
  enum TStorageStatus
  {
    STORAGE_OK = 0,
    STORAGE_DISCONNECTED,
    NOT_ENOUGH_SPACE
  };
  TStorageStatus GetWritableStorageStatus(uint64_t neededSize) const;
  uint64_t GetWritableStorageSpace() const;

  // Please note, that number of active cores can vary at runtime.
  // DO NOT assume for the same return value between calls.
  unsigned CpuCores() const;

  void GetFontNames(FilesList & res) const;

  int VideoMemoryLimit() const;

  int PreCachingDepth() const;

  std::string DeviceName() const;

  std::string DeviceModel() const;

  /// @return url for clients to download maps
  //@{
  std::string MetaServerUrl() const;
  std::string ResourcesMetaServerUrl() const;
  //@}

  /// @return JSON-encoded list of urls if metaserver is unreachable
  std::string DefaultUrlsJSON() const;

  bool IsTablet() const { return m_isTablet; }

  /// @return information about kinds of memory which are relevant for a platform.
  /// This method is implemented for iOS and Android only.
  /// @TODO Add implementation
  std::string GetMemoryInfo() const;

  static EConnectionType ConnectionStatus();
  static bool IsConnected() { return ConnectionStatus() != EConnectionType::CONNECTION_NONE; }

  static ChargingStatus GetChargingStatus();

  // Returns current battery level. Possible values are from 0 to 100.
  // Returns 100 when actual level is unknown.
  static uint8_t GetBatteryLevel();

  void SetupMeasurementSystem() const;

  platform::SecureStorage & GetSecureStorage() { return m_secureStorage; }

  /// \brief Placing an executable object |task| on a queue of |thread|. Then the object will be
  /// executed on |thread|.
  /// \note |task| cannot be moved in case of |Thread::Gui|. This way unique_ptr cannot be used
  /// in |task|. Use shared_ptr instead.
  template <typename Task>
  base::TaskLoop::PushResult RunTask(Thread thread, Task && task)
  {
    ASSERT(m_networkThread && m_fileThread && m_backgroundThread, ());
    switch (thread)
    {
    case Thread::File: return m_fileThread->Push(std::forward<Task>(task));
    case Thread::Network: return m_networkThread->Push(std::forward<Task>(task));
    case Thread::Gui: return m_guiThread->Push(std::forward<Task>(task));
    case Thread::Background: return m_backgroundThread->Push(std::forward<Task>(task));
    }
    UNREACHABLE();
  }

  template <typename Task>
  base::TaskLoop::PushResult RunDelayedTask(
      Thread thread, base::thread_pool::delayed::ThreadPool::Duration const & delay, Task && task)
  {
    ASSERT(m_networkThread && m_fileThread && m_backgroundThread, ());
    switch (thread)
    {
    case Thread::File: return m_fileThread->PushDelayed(delay, std::forward<Task>(task));
    case Thread::Network: return m_networkThread->PushDelayed(delay, std::forward<Task>(task));
    case Thread::Gui:
      CHECK(false, ("Delayed tasks for gui thread are not supported yet"));
      return {};
    case Thread::Background:
      return m_backgroundThread->PushDelayed(delay, std::forward<Task>(task));
    }
    UNREACHABLE();
  }

  void CancelTask(Thread thread, base::TaskLoop::TaskId id);

  // Use this method for testing purposes only.
  void SetGuiThread(std::unique_ptr<base::TaskLoop> guiThread);

  platform::BatteryLevelTracker & GetBatteryTracker() { return m_batteryTracker; }

private:
  void RunThreads();
  void ShutdownThreads();

  void GetSystemFontNames(FilesList & res) const;
};

std::string DebugPrint(Platform::EError err);
std::string DebugPrint(Platform::ChargingStatus status);
