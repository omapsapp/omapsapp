#include "platform/platform.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"

#include "std/target_os.hpp"

#include <memory>
#include <string>
#include <utility>

#include <Foundation/NSAutoreleasePool.h>
#include <Foundation/NSBundle.h>
#include <Foundation/NSFileManager.h>
#include <Foundation/NSPathUtilities.h>
#include <IOKit/IOKitLib.h>

#include <sys/stat.h>
#include <sys/sysctl.h>

#include <dispatch/dispatch.h>

#import <SystemConfiguration/SystemConfiguration.h>
#import <netinet/in.h>


Platform::Platform()
{
  // OMaps.app/Content/Resources or omim-build-debug for tests.
  std::string const resourcesPath = NSBundle.mainBundle.resourcePath.UTF8String;
  // Omaps.app or omim-build-debug for tests.
  std::string const bundlePath = NSBundle.mainBundle.bundlePath.UTF8String;
  // Current working directory, can be overrided for Xcode projects in the scheme's settings.
  std::string const currentDir = [NSFileManager.defaultManager currentDirectoryPath].UTF8String;

  char const * envResourcesDir = ::getenv("MWM_RESOURCES_DIR");
  char const * envWritableDir = ::getenv("MWM_WRITABLE_DIR");

  if (envResourcesDir && envWritableDir)
  {
    m_resourcesDir = envResourcesDir;
    m_writableDir = envWritableDir;
  }
  else if (resourcesPath == bundlePath)
  {
    // we're the console app, probably unit test, and path is our directory
    m_resourcesDir = bundlePath + "/../../data/";
    if (!IsFileExistsByFullPath(m_resourcesDir))
    {
      // Check development environment without symlink but with git repo
      std::string const repoPath = bundlePath + "/../../../omim/data/";
      if (IsFileExistsByFullPath(repoPath))
        m_resourcesDir = repoPath;
      else
        m_resourcesDir = "./data/";
    }
    m_writableDir = m_resourcesDir;
  }
  else
  {
    m_resourcesDir = resourcesPath + "/";
    std::string const paths[] =
    {
      // Developers can set a symlink to the data folder.
      m_resourcesDir + "../../../data/",
      // Check development environment without a symlink but with a git repo.
      m_resourcesDir + "../../../../omim/data/",
      m_resourcesDir + "../../../../organicmaps/data/",
      // Working directory is set to the data folder or any project's subfolder.
      currentDir + "/../data",
      // Working directory is set to the project's root.
      currentDir + "/data",
      // Working directory is set to the build folder with binaries.
      currentDir + "/../omim/data",
      currentDir + "/../organicmaps/data",
    };
    // Find the writable path.
    for (auto const & path : paths)
    {
      if (IsFileExistsByFullPath(path))
      {
        m_writableDir = path;
        break;
      }
    }

    // Xcode-launched Mac projects are built into a non-standard folder and may need
    // a customized working directory.
    if (m_writableDir.empty())
    {
      for (char const * keyword : {"/omim/", "/organicmaps/"})
      {
        if (auto const p = currentDir.rfind(keyword); p != std::string::npos)
        {
          m_writableDir = m_resourcesDir = currentDir.substr(0, p) + keyword + "data/";
          break;
        }
        if (auto const p = m_resourcesDir.rfind(keyword); p != std::string::npos)
        {
          m_writableDir = m_resourcesDir.substr(0, p) + keyword + "data/";
          break;
        }
      }
    }

    if (m_writableDir.empty())
    {
      NSArray * dirPaths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
      NSString * supportDir = [dirPaths objectAtIndex:0];
      m_writableDir = supportDir.UTF8String;
#ifdef BUILD_DESIGNER
      m_writableDir += "/OMapsData.Designer/";
#else // BUILD_DESIGNER
      m_writableDir += "/OMapsData/";
#endif // BUILD_DESIGNER
      ::mkdir(m_writableDir.c_str(), 0755);
    }
  }

  if (m_resourcesDir.empty())
    m_resourcesDir = ".";
  m_resourcesDir = base::AddSlashIfNeeded(m_resourcesDir);
  m_writableDir = base::AddSlashIfNeeded(m_writableDir);

  m_settingsDir = m_writableDir;

  NSString * tempDir = NSTemporaryDirectory();
  if (tempDir == nil)
      tempDir = @"/tmp";
  m_tmpDir = tempDir.UTF8String;
  base::AddSlashIfNeeded(m_tmpDir);

  m_guiThread = std::make_unique<platform::GuiThread>();

  LOG(LDEBUG, ("Resources Directory:", m_resourcesDir));
  LOG(LDEBUG, ("Writable Directory:", m_writableDir));
  LOG(LDEBUG, ("Tmp Directory:", m_tmpDir));
  LOG(LDEBUG, ("Settings Directory:", m_settingsDir));
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
  struct sockaddr_in zero;
  memset(&zero, 0, sizeof(zero));
  zero.sin_len = sizeof(zero);
  zero.sin_family = AF_INET;
  SCNetworkReachabilityRef reachability = SCNetworkReachabilityCreateWithAddress(kCFAllocatorDefault, reinterpret_cast<const struct sockaddr*>(&zero));
  if (!reachability)
    return EConnectionType::CONNECTION_NONE;
  SCNetworkReachabilityFlags flags;
  bool const gotFlags = SCNetworkReachabilityGetFlags(reachability, &flags);
  CFRelease(reachability);
  if (!gotFlags || ((flags & kSCNetworkReachabilityFlagsReachable) == 0))
    return EConnectionType::CONNECTION_NONE;
  SCNetworkReachabilityFlags userActionRequired = kSCNetworkReachabilityFlagsConnectionRequired | kSCNetworkReachabilityFlagsInterventionRequired;
  if ((flags & userActionRequired) == userActionRequired)
    return EConnectionType::CONNECTION_NONE;
  return EConnectionType::CONNECTION_WIFI;
}

// static
Platform::ChargingStatus Platform::GetChargingStatus()
{
  return Platform::ChargingStatus::Plugged;
}

uint8_t Platform::GetBatteryLevel()
{
  // This value is always 100 for desktop.
  return 100;
}

void Platform::GetSystemFontNames(FilesList &) const
{
}

// static
time_t Platform::GetFileCreationTime(std::string const & path)
{
  struct stat st;
  if (0 == stat(path.c_str(), &st))
    return st.st_birthtimespec.tv_sec;
  return 0;
}

// static
time_t Platform::GetFileModificationTime(std::string const & path)
{
  struct stat st;
  if (0 == stat(path.c_str(), &st))
    return st.st_mtimespec.tv_sec;
  return 0;
}
