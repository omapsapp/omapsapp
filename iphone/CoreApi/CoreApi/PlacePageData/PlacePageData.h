#import <Foundation/Foundation.h>
#import <CoreLocation/CoreLocation.h>

#import "MWMTypes.h"

@class PlacePageButtonsData;
@class PlacePageTrackData;
@class PlacePagePreviewData;
@class PlacePageInfoData;
@class PlacePageBookmarkData;
@class MWMMapNodeAttributes;
@class TrackInfo;
@class ElevationProfileData;

typedef NS_ENUM(NSInteger, PlacePageRoadType) {
  PlacePageRoadTypeToll,
  PlacePageRoadTypeFerry,
  PlacePageRoadTypeDirty,
  PlacePageRoadTypeNone
};

typedef NS_ENUM(NSInteger, PlacePageObjectType) {
  PlacePageObjectTypePOI,
  PlacePageObjectTypeBookmark,
  PlacePageObjectTypeTrack,
  PlacePageObjectTypeTrackRecording
};

@protocol IOpeningHoursLocalization;

NS_ASSUME_NONNULL_BEGIN

@interface PlacePageData : NSObject

@property(class, nonatomic, readonly) BOOL hasData;

@property(nonatomic, readonly, nullable) PlacePageButtonsData *buttonsData;
@property(nonatomic, readonly) PlacePagePreviewData *previewData;
@property(nonatomic, readonly, nullable) PlacePageInfoData *infoData;
@property(nonatomic, readonly, nullable) PlacePageBookmarkData *bookmarkData;
@property(nonatomic, readonly) PlacePageRoadType roadType;
@property(nonatomic, readonly, nullable) NSString *wikiDescriptionHtml;
@property(nonatomic, readonly, nullable) PlacePageTrackData *trackData;
@property(nonatomic, readonly, nullable) MWMMapNodeAttributes *mapNodeAttributes;
@property(nonatomic, readonly, nullable) NSString *bookingSearchUrl;
@property(nonatomic, readonly) BOOL isMyPosition;
@property(nonatomic, readonly) BOOL isPreviewPlus;
@property(nonatomic, readonly) BOOL isRoutePoint;
@property(nonatomic, readonly) PlacePageObjectType objectType;
@property(nonatomic, readonly) CLLocationCoordinate2D locationCoordinate;
@property(nonatomic, copy, nullable) MWMVoidBlock onBookmarkStatusUpdate;
@property(nonatomic, copy, nullable) MWMVoidBlock onMapNodeStatusUpdate;
@property(nonatomic, copy, nullable) MWMVoidBlock onTrackRecordingProgressUpdate;
@property(nonatomic, copy, nullable) void (^onMapNodeProgressUpdate)(uint64_t downloadedBytes, uint64_t totalBytes);

- (instancetype)initWithLocalizationProvider:(id<IOpeningHoursLocalization>)localization;
- (instancetype)initWithTrackInfo:(TrackInfo * _Nonnull)trackInfo elevationInfo:(ElevationProfileData * _Nullable)elevationInfo;
- (instancetype)init NS_UNAVAILABLE;

- (void)updateBookmarkStatus;
- (void)updateWithTrackInfo:(TrackInfo * _Nonnull)trackInfo elevationInfo:(ElevationProfileData * _Nullable)elevationInfo;

@end

NS_ASSUME_NONNULL_END
