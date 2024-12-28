#import "AltitudeFormatter.h"

#include "platform/distance.hpp"

@implementation AltitudeFormatter

+ (NSString *)altitudeStringFromMeters:(double)meters {
  auto const altitude = platform::Distance::FormatAltitude(meters);
  return [NSString stringWithUTF8String:altitude.c_str()];
}

@end
