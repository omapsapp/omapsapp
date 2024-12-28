#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface TrackInfo : NSObject

@property (nonatomic, readonly) double distance;
@property (nonatomic, readonly) double duration;
@property (nonatomic, readonly) NSUInteger ascent;
@property (nonatomic, readonly) NSUInteger descent;
@property (nonatomic, readonly) NSUInteger maxElevation;
@property (nonatomic, readonly) NSUInteger minElevation;

- (BOOL)hasElevationInfo;

+ (TrackInfo *)emptyInfo;

@end

NS_ASSUME_NONNULL_END
