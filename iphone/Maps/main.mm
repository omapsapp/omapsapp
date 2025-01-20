#import "MapsAppDelegate.h"
#import "MWMSettings.h"

#include "platform/measurement_utils.hpp"
#include "platform/platform.hpp"

int main(int argc, char * argv[])
{
  [MWMSettings initializeLogging];

  NSBundle * mainBundle = [NSBundle mainBundle];
  NSString * appName = [mainBundle objectForInfoDictionaryKey:@"CFBundleName"];
  NSString * bundleId = mainBundle.bundleIdentifier;
  auto & p = GetPlatform();
  LOG(LINFO, (appName.UTF8String, bundleId.UTF8String, p.Version(), "started, detected CPU cores:", p.CpuCores()));

  // Force system locale initialization at app start-up.
  measurement_utils::RefreshSystemLocale();

  int retVal;
  @autoreleasepool
  {
    retVal = UIApplicationMain(argc, argv, nil, NSStringFromClass([MapsAppDelegate class]));
  }
  return retVal;
}
