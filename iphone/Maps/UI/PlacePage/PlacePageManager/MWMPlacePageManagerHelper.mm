#import "MWMPlacePageManagerHelper.h"
#import "MWMMapViewControlsManager.h"
#import "MWMPlacePageManager.h"

@interface MWMMapViewControlsManager ()

@property(nonatomic) MWMPlacePageManager * placePageManager;

@end

@interface MWMPlacePageManager ()

- (void)updateAvailableArea:(CGRect)frame;
- (void)editPlace;
- (void)addBusiness;
- (void)addPlace:(CLLocationCoordinate2D)coordinate;
- (void)openWebsite:(PlacePageData *)data;
- (void)openWebsiteMenu:(PlacePageData *)data;
- (void)openKayak:(PlacePageData *)data;
- (void)openWikipedia:(PlacePageData *)data;
- (void)openWikimediaCommons:(PlacePageData *)data;
- (void)openEmail:(PlacePageData *)data;
- (void)openFacebook:(PlacePageData *)data;
- (void)openInstagram:(PlacePageData *)data;
- (void)openTwitter:(PlacePageData *)data;
- (void)openVk:(PlacePageData *)data;
- (void)openLine:(PlacePageData *)data;
- (void)call:(PlacePageData *)data;
- (void)showAllFacilities:(PlacePageData *)data;
- (void)showPlaceDescription:(NSString *)htmlString;
- (void)openMoreUrl:(PlacePageData *)data;
- (void)openReviewUrl:(PlacePageData *)data;
- (void)openDescriptionUrl:(PlacePageData *)data;
- (void)openCatalogSingleItem:(PlacePageData *)data atIndex:(NSInteger)index;
- (void)openCatalogMoreItems:(PlacePageData *)data;
- (void)addBookmark:(PlacePageData *)data;
- (void)removeBookmark:(PlacePageData *)data;
- (void)removeTrack:(PlacePageData *)data;
- (void)editBookmark:(PlacePageData *)data;
- (void)editTrack:(PlacePageData *)data;
- (void)searchBookingHotels:(PlacePageData *)data;
- (void)book:(PlacePageData *)data;
- (void)routeFrom:(PlacePageData *)data;
- (void)routeTo:(PlacePageData *)data;
- (void)routeAddStop:(PlacePageData *)data;
- (void)routeRemoveStop:(PlacePageData *)data;
- (void)avoidDirty;
- (void)avoidFerry;
- (void)avoidToll;
- (void)openElevationDifficultPopup:(PlacePageData *)data;

@end

@implementation MWMPlacePageManagerHelper

+ (void)updateAvailableArea:(CGRect)frame
{
  [[MWMMapViewControlsManager manager].placePageManager updateAvailableArea:frame];
}

+ (void)editPlace {
  [[MWMMapViewControlsManager manager].placePageManager editPlace];
}

+ (void)addBusiness {
  [[MWMMapViewControlsManager manager].placePageManager addBusiness];
}

+ (void)addPlace:(CLLocationCoordinate2D)coordinate {
  [[MWMMapViewControlsManager manager].placePageManager addPlace:coordinate];
}

+ (void)openWebsite:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openWebsite:data];
}

+ (void)openWebsiteMenu:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openWebsiteMenu:data];
}

+ (void)openKayak:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openKayak:data];
}

+ (void)openEmail:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openEmail:data];
}

+ (void)openWikipedia:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openWikipedia:data];
}

+ (void)openWikimediaCommons:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openWikimediaCommons:data];
}

+ (void)openFacebook:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openFacebook:data];
}

+ (void)openInstagram:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openInstagram:data];
}

+ (void)openTwitter:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openTwitter:data];
}

+ (void)openVk:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openVk:data];
}

+ (void)openLine:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openLine:data];
}

+ (void)call:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager call:data];
}

+ (void)showAllFacilities:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager showAllFacilities:data];
}

+ (void)showPlaceDescription:(NSString *)htmlString {
  [[MWMMapViewControlsManager manager].placePageManager showPlaceDescription:htmlString];
}

+ (void)openMoreUrl:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openMoreUrl:data];
}

+ (void)openReviewUrl:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openReviewUrl:data];
}

+ (void)openDescriptionUrl:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openDescriptionUrl:data];
}

+ (void)openCatalogSingleItem:(PlacePageData *)data atIndex:(NSInteger)index {
  [[MWMMapViewControlsManager manager].placePageManager openCatalogSingleItem:data atIndex:index];
}

+ (void)openCatalogMoreItems:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openCatalogMoreItems:data];
}

+ (void)addBookmark:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager addBookmark:data];
}

+ (void)removeBookmark:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager removeBookmark:data];
}

+ (void)removeTrack:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager removeTrack:data];
}

+ (void)editBookmark:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager editBookmark:data];
}

+ (void)editTrack:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager editTrack:data];
}

+ (void)searchBookingHotels:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager searchBookingHotels:data];
}

+ (void)book:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager book:data];
}

+ (void)routeFrom:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager routeFrom:data];
}

+ (void)routeTo:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager routeTo:data];
}

+ (void)routeAddStop:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager routeAddStop:data];
}

+ (void)routeRemoveStop:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager routeRemoveStop:data];
}

+ (void)avoidDirty {
  [[MWMMapViewControlsManager manager].placePageManager avoidDirty];
}

+ (void)avoidFerry {
  [[MWMMapViewControlsManager manager].placePageManager avoidFerry];
}

+ (void)avoidToll {
  [[MWMMapViewControlsManager manager].placePageManager avoidToll];
}

+ (void)openElevationDifficultPopup:(PlacePageData *)data {
  [[MWMMapViewControlsManager manager].placePageManager openElevationDifficultPopup:data];
}

@end
