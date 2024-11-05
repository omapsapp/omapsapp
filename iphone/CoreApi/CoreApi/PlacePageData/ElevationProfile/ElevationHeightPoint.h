#import <Foundation/Foundation.h>
#import <CoreLocation/CoreLocation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ElevationHeightPoint : NSObject

@property(nonatomic, readonly) CLLocationCoordinate2D coordinates;
@property(nonatomic, readonly) double distance;
@property(nonatomic, readonly) double altitude;

- (instancetype)initWithCoordinates:(CLLocationCoordinate2D)coordinates distance:(double)distance andAltitude:(double)altitude;

@end

NS_ASSUME_NONNULL_END
