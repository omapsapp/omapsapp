#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface DurationFormatter: NSObject

+ (NSString *)durationStringFromTimeInterval:(NSTimeInterval)timeInterval NS_SWIFT_NAME(durationString(from:));

@end

NS_ASSUME_NONNULL_END
