#include "platform/platform_ios.h"
#include "platform/constants.hpp"
#include "platform/gui_thread.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/platform_unix_impl.hpp"
#include "platform/settings.hpp"

#include "coding/file_reader.hpp"

#include "std/target_os.hpp"

#include <utility>

#include <ifaddrs.h>

#include <mach/mach.h>

#include <net/if.h>
#include <net/if_dl.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/xattr.h>

#import <CoreFoundation/CFURL.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <UIKit/UIKit.h>
#import <netinet/in.h>

#include <memory>
#include <sstream>
#include <string>
#include <utility>

Platform::Platform()
{
  m_isTablet = (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad);

  NSBundle * bundle = NSBundle.mainBundle;
  NSString * path = [bundle resourcePath];
  m_resourcesDir = path.UTF8String;
  m_resourcesDir += "/";

  NSArray * dirPaths =
      NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  NSString * docsDir = dirPaths.firstObject;
  m_writableDir = docsDir.UTF8String;
  m_writableDir += "/";
  m_settingsDir = m_writableDir;

  NSString * tmpDir = NSTemporaryDirectory();
  if (tmpDir)
    m_tmpDir = tmpDir.UTF8String;
  else
  {
    m_tmpDir = NSHomeDirectory().UTF8String;
    m_tmpDir += "/tmp/";
  }

  m_guiThread = std::make_unique<platform::GuiThread>();

  UIDevice * device = UIDevice.currentDevice;
  device.batteryMonitoringEnabled = YES;

  LOG(LINFO, ("Device:", device.model.UTF8String, "SystemName:",
              device.systemName.UTF8String, "SystemVersion:",
              device.systemVersion.UTF8String));
}

//static
void Platform::DisableBackupForFile(std::string const & filePath)
{
  // We need to disable iCloud backup for downloaded files.
  // This is the reason for rejecting from the AppStore
  // https://developer.apple.com/library/iOS/qa/qa1719/_index.html
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                                         reinterpret_cast<unsigned char const *>(filePath.c_str()),
                                                         filePath.size(),
                                                         0);
  CFErrorRef err;
  BOOL valueRaw = YES;
  CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberCharType, &valueRaw);
  if (!CFURLSetResourcePropertyForKey(url, kCFURLIsExcludedFromBackupKey, value, &err))
    NSLog(@"Error while disabling iCloud backup for file: %s", filePath.c_str());

  CFRelease(value);
  CFRelease(url);
}

// static
Platform::EError Platform::MkDir(std::string const & dirName)
{
  if (::mkdir(dirName.c_str(), 0755))
    return ErrnoToError();
  return Platform::ERR_OK;
}

void Platform::GetFilesByRegExp(std::string const & directory, std::string const & regexp, FilesList & res)
{
  pl::EnumerateFilesByRegExp(directory, regexp, res);
}

bool Platform::GetFileSizeByName(std::string const & fileName, uint64_t & size) const
{
  try
  {
    return GetFileSizeByFullPath(ReadPathForFile(fileName), size);
  }
  catch (RootException const &)
  {
    return false;
  }
}

std::unique_ptr<ModelReader> Platform::GetReader(std::string const & file, std::string searchScope) const
{
  return std::make_unique<FileReader>(ReadPathForFile(file, std::move(searchScope)), READER_CHUNK_LOG_SIZE,
                                      READER_CHUNK_LOG_COUNT);
}

int Platform::VideoMemoryLimit() const { return 8 * 1024 * 1024; }

int Platform::PreCachingDepth() const { return 2; }

std::string Platform::GetMemoryInfo() const
{
  struct task_basic_info info;
  mach_msg_type_number_t size = sizeof(info);
  kern_return_t const kerr =
      task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &size);
  std::stringstream ss;
  if (kerr == KERN_SUCCESS)
  {
    ss << "Memory info: Resident_size = " << info.resident_size / 1024
       << "KB; virtual_size = " << info.resident_size / 1024
       << "KB; suspend_count = " << info.suspend_count << " policy = " << info.policy;
  }
  else
  {
    ss << "Error with task_info(): " << mach_error_string(kerr);
  }
  return ss.str();
}

std::string Platform::DeviceName() const { return UIDevice.currentDevice.name.UTF8String; }

std::string Platform::DeviceModel() const
{
  utsname systemInfo;
  uname(&systemInfo);
  NSString * deviceModel = @(systemInfo.machine);
  if (auto m = platform::kDeviceModelsBeforeMetalDriver[deviceModel])
    deviceModel = m;
  else if (auto m = platform::kDeviceModelsWithiOS10MetalDriver[deviceModel])
    deviceModel = m;
  else if (auto m = platform::kDeviceModelsWithMetalDriver[deviceModel])
    deviceModel = m;
  return deviceModel.UTF8String;
}

std::string Platform::Version() const
{
  /// @note Do not change version format, it is parsed on server side.
  NSBundle * mainBundle = [NSBundle mainBundle];
  NSString * version = [mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
  NSString * build = [mainBundle objectForInfoDictionaryKey:@"CFBundleVersion"];
  return std::string{version.UTF8String} + '-' + build.UTF8String + '-' + OMIM_OS_NAME;
}

int32_t Platform::IntVersion() const
{
  NSString * version = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
  int year = 0;
  int month = 0;
  int day = 0;
  int rc = sscanf(version.UTF8String, "%d.%d.%d", &year, &month, &day);
  CHECK_EQUAL(rc, 3, ("Failed to parse version"));
  CHECK(year > 2000 && year < 3000, ("Invalid year"));
  CHECK(month > 0 && month <= 12, ("Invalid month"));
  CHECK(day > 0 && day <= 31, ("Invalid day"));
  return (int32_t)(year - 2000) * 10000 + month * 100 + day;
}

Platform::EConnectionType Platform::ConnectionStatus()
{
  struct sockaddr_in zero;
  bzero(&zero, sizeof(zero));
  zero.sin_len = sizeof(zero);
  zero.sin_family = AF_INET;
  SCNetworkReachabilityRef reachability =
      SCNetworkReachabilityCreateWithAddress(kCFAllocatorDefault, (const struct sockaddr *)&zero);
  if (!reachability)
    return EConnectionType::CONNECTION_NONE;
  SCNetworkReachabilityFlags flags;
  bool const gotFlags = SCNetworkReachabilityGetFlags(reachability, &flags);
  CFRelease(reachability);
  if (!gotFlags || ((flags & kSCNetworkReachabilityFlagsReachable) == 0))
    return EConnectionType::CONNECTION_NONE;
  SCNetworkReachabilityFlags userActionRequired = kSCNetworkReachabilityFlagsConnectionRequired |
                                                  kSCNetworkReachabilityFlagsInterventionRequired;
  if ((flags & userActionRequired) == userActionRequired)
    return EConnectionType::CONNECTION_NONE;
  if ((flags & kSCNetworkReachabilityFlagsIsWWAN) == kSCNetworkReachabilityFlagsIsWWAN)
    return EConnectionType::CONNECTION_WWAN;
  else
    return EConnectionType::CONNECTION_WIFI;
}

Platform::ChargingStatus Platform::GetChargingStatus()
{
  switch (UIDevice.currentDevice.batteryState)
  {
  case UIDeviceBatteryStateUnknown: return Platform::ChargingStatus::Unknown;
  case UIDeviceBatteryStateUnplugged: return Platform::ChargingStatus::Unplugged;
  case UIDeviceBatteryStateCharging:
  case UIDeviceBatteryStateFull: return Platform::ChargingStatus::Plugged;
  }
}

uint8_t Platform::GetBatteryLevel()
{
  auto const level = UIDevice.currentDevice.batteryLevel;

  ASSERT_GREATER_OR_EQUAL(level, -1.0, ());
  ASSERT_LESS_OR_EQUAL(level, 1.0, ());

  if (level == -1.0)
    return 100;

  auto const result = static_cast<uint8_t>(level * 100);

  CHECK_LESS_OR_EQUAL(result, 100, ());

  return result;
}

void Platform::SetupMeasurementSystem() const
{
  auto units = measurement_utils::Units::Metric;
  if (settings::Get(settings::kMeasurementUnits, units))
    return;
  BOOL const isMetric =
      [[[NSLocale autoupdatingCurrentLocale] objectForKey:NSLocaleUsesMetricSystem] boolValue];
  units = isMetric ? measurement_utils::Units::Metric : measurement_utils::Units::Imperial;
  settings::Set(settings::kMeasurementUnits, units);
}

void Platform::GetSystemFontNames(FilesList & res) const
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

////////////////////////////////////////////////////////////////////////
extern Platform & GetPlatform()
{
  static Platform platform;
  return platform;
}
