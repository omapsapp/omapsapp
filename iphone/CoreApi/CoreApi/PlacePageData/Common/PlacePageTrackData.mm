#import "PlacePageTrackData+Core.h"
#import "ElevationProfileData+Core.h"

@implementation PlacePageTrackData

@end

@implementation PlacePageTrackData (Core)

- (instancetype)initWithTrack:(Track const &)track {
  self = [super init];
  if (self) {
    _trackId = track.GetData().m_id;
    if (track.HasAltitudes()) {
      auto const & bm = GetFramework().GetBookmarkManager();
      auto const & elevationInfo = track.GetElevationInfo();
      _elevationProfileData = [[ElevationProfileData alloc] initWithTrackId:_trackId
                                                              elevationInfo:*elevationInfo
                                                                activePoint:bm.GetElevationActivePoint(_trackId)
                                                                 myPosition:bm.GetElevationMyPosition(_trackId)];
    }
  }
  return self;
}

@end
