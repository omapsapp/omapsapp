#import "TrackInfo+Core.h"
#import "AltitudeFormatter.h"
#import "DistanceFormatter.h"
#import "DurationFormatter.h"

#include "map/elevation_info.hpp"

@implementation TrackInfo

- (BOOL)hasElevationInfo {
  return _ascent != 0 || _descent != 0 || _maxElevation != 0 || _minElevation != 0;
}

+ (TrackInfo *)emptyInfo {
  return [[TrackInfo alloc] init];
}

@end

@implementation TrackInfo (Core)

- (instancetype)initWithGpsTrackInfo:(GpsTrackInfo const &)trackInfo {
  if (self = [super init]) {
    _distance = trackInfo.m_length;
    _duration = trackInfo.m_duration;
    _ascent = trackInfo.m_ascent;
    _descent = trackInfo.m_descent;
    _maxElevation = trackInfo.m_maxElevation;
    _minElevation = trackInfo.m_minElevation;
  }
  return self;
}

- (instancetype)initWithDistance:(double)distance duration:(double)duration {
  if (self = [super init]) {
    _distance = distance;
    _duration = duration;
  }
  return self;
}

- (void)setElevationInfo:(ElevationInfo const &)elevationInfo {
  _ascent = elevationInfo.GetAscent();
  _descent = elevationInfo.GetDescent();
  _maxElevation = elevationInfo.GetMaxAltitude();
  _minElevation = elevationInfo.GetMinAltitude();
}

@end
