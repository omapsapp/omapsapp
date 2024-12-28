#import "ElevationProfileData.h"
#import "MWMTypes.h"

#include "map/elevation_info.hpp"

NS_ASSUME_NONNULL_BEGIN

@interface ElevationProfileData (Core)

- (instancetype)initWithTrackId:(MWMTrackID)trackId
                  elevationInfo:(ElevationInfo const &)elevationInfo
                    activePoint:(double)activePoint
                     myPosition:(double)myPosition;

@end

NS_ASSUME_NONNULL_END
