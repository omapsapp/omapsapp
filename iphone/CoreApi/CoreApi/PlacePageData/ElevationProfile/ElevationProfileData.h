#import <Foundation/Foundation.h>
#import "ElevationHeightPoint.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ElevationDifficulty) {
  ElevationDifficultyDisabled,
  ElevationDifficultyEasy,
  ElevationDifficultyMedium,
  ElevationDifficultyHard
};

@interface ElevationProfileData : NSObject

@property(nonatomic, readonly) uint64_t trackId;
@property(nonatomic, readonly) ElevationDifficulty difficulty;
@property(nonatomic, readonly) NSArray<ElevationHeightPoint *> * points;
@property(nonatomic, readonly) double activePoint;
@property(nonatomic, readonly) double myPosition;

@end

NS_ASSUME_NONNULL_END
