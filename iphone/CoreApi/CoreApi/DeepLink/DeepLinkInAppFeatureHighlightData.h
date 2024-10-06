#import <Foundation/Foundation.h>
#import "DeepLinkParser.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, InAppFeatureHighlightType) {
  InAppFeatureHighlightTypeNone,
  InAppFeatureHighlightTypeTrackRecorder,
  InAppFeatureHighlightTypeICloud,
};

@interface DeepLinkInAppFeatureHighlightData : NSObject

@property(nonatomic, readonly) DeeplinkUrlType urlType;
@property(nonatomic, readonly) InAppFeatureHighlightType feature;

- (instancetype)init:(DeeplinkUrlType)urlType;

@end

NS_ASSUME_NONNULL_END
