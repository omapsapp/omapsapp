#import <Foundation/Foundation.h>
#import "MWMTypes.h"

@class TrackInfo;
@class ElevationProfileData;

NS_ASSUME_NONNULL_BEGIN

@interface PlacePageTrackData : NSObject

@property(nonatomic, readonly) MWMTrackID trackId;
@property(nonatomic, readonly) MWMMarkGroupID groupId;
@property(nonatomic, readwrite, nonnull) TrackInfo * trackInfo;
@property(nonatomic, readwrite, nullable) ElevationProfileData * elevationProfileData;

- (instancetype)initWithTrackInfo:(TrackInfo * _Nonnull)trackInfo elevationInfo:(ElevationProfileData * _Nullable)elevationInfo;

@end

NS_ASSUME_NONNULL_END
