#import "DistanceFormatter.h"

#include "platform/distance.hpp"

@implementation DistanceFormatter

+ (NSString *)distanceStringFromMeters:(double)meters {
  auto const coreDistance = platform::Distance::CreateFormatted(meters);
  return [NSString stringWithUTF8String:coreDistance.ToString().c_str()];
}

@end
