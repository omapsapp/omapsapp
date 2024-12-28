#import "PlacePageTrackData.h"

#include <CoreApi/Framework.h>

NS_ASSUME_NONNULL_BEGIN

@interface PlacePageTrackData (Core)

- (instancetype)initWithTrack:(Track const &)track;

@end

NS_ASSUME_NONNULL_END
