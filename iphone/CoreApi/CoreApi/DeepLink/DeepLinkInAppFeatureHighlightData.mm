#import "DeepLinkInAppFeatureHighlightData.h"
#import <CoreApi/Framework.h>

static inline InAppFeatureHighlightType FeatureTypeFrom(url_scheme::InAppFeatureHighlightRequest::InAppFeatureType type)
{
  using namespace url_scheme;
  switch (type)
  {
    case InAppFeatureHighlightRequest::InAppFeatureType::None: return InAppFeatureHighlightTypeNone;
    case InAppFeatureHighlightRequest::InAppFeatureType::TrackRecorder: return InAppFeatureHighlightTypeTrackRecorder;
    case InAppFeatureHighlightRequest::InAppFeatureType::iCloud: return InAppFeatureHighlightTypeICloud;
  }
}

@implementation DeepLinkInAppFeatureHighlightData

- (instancetype)init:(DeeplinkUrlType)urlType {
  self = [super init];
  if (self) {
    _urlType = urlType;
    _feature = FeatureTypeFrom(GetFramework().GetInAppFeatureHighlightRequest().m_feature);
  }
  return self;
}

@end
