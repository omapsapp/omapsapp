#import "TrackInfo.h"

#include <CoreApi/Framework.h>
#include "map/gps_track_collection.hpp"
#include "map/elevation_info.hpp"

@interface TrackInfo (Core)

- (instancetype)initWithGpsTrackInfo:(GpsTrackInfo const &)info;
- (instancetype)initWithDistance:(double)distance duration:(double)duration;

- (void)setElevationInfo:(ElevationInfo const &)elevationInfo;

@end
