#import "MapsAppDelegate.h"
#import "MWMSettings.h"

#include "platform/platform.hpp"

int main(int argc, char * argv[])
{
  [MWMSettings initializeLogging];
  auto & p = GetPlatform();
  LOG(LINFO, ("Organic Maps", p.Version(), "started, detected CPU cores:", p.CpuCores()));

  int retVal;
  @autoreleasepool
  {
    retVal = UIApplicationMain(argc, argv, nil, NSStringFromClass([MapsAppDelegate class]));
  }
  return retVal;
}
