#import "DurationFormatter.h"

#include "platform/duration.hpp"

@implementation DurationFormatter

+ (NSString *)durationStringFromTimeInterval:(NSTimeInterval)timeInterval {
  auto const duration = platform::Duration(static_cast<int>(timeInterval));
  return [NSString stringWithCString:duration.GetPlatformLocalizedString().c_str() encoding:NSUTF8StringEncoding];
}

@end
