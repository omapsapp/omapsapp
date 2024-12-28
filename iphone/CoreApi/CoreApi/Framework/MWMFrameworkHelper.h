#import <CoreLocation/CoreLocation.h>
#import <UIKit/UIKit.h>

#import "MWMTypes.h"

@class MWMMapSearchResult;
@class TrackInfo;

typedef NS_ENUM(NSUInteger, MWMZoomMode) { MWMZoomModeIn = 0, MWMZoomModeOut };

typedef NS_ENUM(NSInteger, ProductsPopupCloseReason) {
  ProductsPopupCloseReasonClose,
  ProductsPopupCloseReasonSelectProduct,
  ProductsPopupCloseReasonAlreadyDonated,
  ProductsPopupCloseReasonRemindLater
};

NS_ASSUME_NONNULL_BEGIN

typedef void (^SearchInDownloaderCompletions)(NSArray<MWMMapSearchResult *> *results, BOOL finished);
typedef void (^TrackRecordingUpdatedHandler)(TrackInfo * _Nonnull trackInfo);

@protocol TrackRecorder <NSObject>

+ (void)startTrackRecording;
+ (void)setTrackRecordingUpdateHandler:(TrackRecordingUpdatedHandler _Nullable)trackRecordingDidUpdate;
+ (void)stopTrackRecording;
+ (void)saveTrackRecordingWithName:(nullable NSString *)name;
+ (BOOL)isTrackRecordingEnabled;
+ (BOOL)isTrackRecordingEmpty;

@end

@class ProductsConfiguration;
@class Product;

@protocol ProductsManager <NSObject>

+ (nullable ProductsConfiguration *)getProductsConfiguration;
+ (void)didCloseProductsPopupWithReason:(ProductsPopupCloseReason)reason;
+ (void)didSelectProduct:(Product *)product;

@end

NS_SWIFT_NAME(FrameworkHelper)
@interface MWMFrameworkHelper : NSObject<TrackRecorder, ProductsManager>

+ (void)processFirstLaunch:(BOOL)hasLocation;
+ (void)setVisibleViewport:(CGRect)rect scaleFactor:(CGFloat)scale;
+ (void)setTheme:(MWMTheme)theme;
+ (MWMDayTime)daytimeAtLocation:(nullable CLLocation *)location;
+ (void)createFramework;
+ (MWMMarkID)invalidBookmarkId;
+ (MWMMarkGroupID)invalidCategoryId;
+ (void)zoomMap:(MWMZoomMode)mode;
+ (void)moveMap:(UIOffset)offset;
+ (void)scrollMap:(double)distanceX :(double) distanceY;
+ (void)deactivateMapSelection;
+ (void)switchMyPositionMode;
+ (void)stopLocationFollow;
+ (NSArray<NSString *> *)obtainLastSearchQueries;
+ (void)rotateMap:(double)azimuth animated:(BOOL)isAnimated;
+ (void)updatePositionArrowOffset:(BOOL)useDefault offset:(int)offsetY;
+ (int64_t)dataVersion;
+ (void)searchInDownloader:(NSString *)query
               inputLocale:(NSString *)locale
                completion:(SearchInDownloaderCompletions)completion;
+ (BOOL)canEditMapAtViewportCenter;
+ (void)showOnMap:(MWMMarkGroupID)categoryId;
+ (void)showBookmark:(MWMMarkID)bookmarkId;
+ (void)showTrack:(MWMTrackID)trackId;
+ (void)updatePlacePageData;
+ (void)updateAfterDeleteBookmark;
+ (int)currentZoomLevel;

@end

NS_ASSUME_NONNULL_END
