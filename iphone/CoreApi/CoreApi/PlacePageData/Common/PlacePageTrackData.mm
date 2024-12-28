#import "PlacePageTrackData+Core.h"
#import "ElevationProfileData+Core.h"
#import "TrackInfo+Core.h"

@implementation PlacePageTrackData

@end

@implementation PlacePageTrackData (Core)

- (instancetype)initWithTrack:(Track const &)track {
  self = [super init];
  if (self) {
    _trackId = track.GetData().m_id;
    _trackInfo = [[TrackInfo alloc] initWithDistance:track.GetLengthMeters()
                                                     duration:track.GetDurationInSeconds()];
    auto const & elevationInfo = track.GetElevationInfo();
    if (track.HasAltitudes() && elevationInfo.has_value()) {
      [_trackInfo setElevationInfo:elevationInfo.value()];
      auto const & bm = GetFramework().GetBookmarkManager();
      _elevationProfileData = [[ElevationProfileData alloc] initWithTrackId:_trackId
                                                              elevationInfo:elevationInfo.value()
                                                                activePoint:bm.GetElevationActivePoint(_trackId)
                                                                 myPosition:bm.GetElevationMyPosition(_trackId)];
    }
  }
  return self;
}

@end
