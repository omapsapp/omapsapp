#import "ElevationHeightPoint.h"

@implementation ElevationHeightPoint

- (instancetype)initWithCoordinates:(CLLocationCoordinate2D)coordinates distance:(double)distance andAltitude:(double)altitude {
  self = [super init];
  if (self) {
    _coordinates = coordinates;
    _distance = distance;
    _altitude = altitude;
  }
  return self;
}

@end
