@class PlacePageData;
@class ElevationProfileData;

@interface MWMPlacePageManagerHelper : NSObject

+ (void)updateAvailableArea:(CGRect)frame;
+ (void)editPlace;
+ (void)addBusiness;
+ (void)addPlace:(CLLocationCoordinate2D)coordinate;
+ (void)openWebsite:(PlacePageData *)data;
+ (void)openWebsiteMenu:(PlacePageData *)data;
+ (void)openKayak:(PlacePageData *)data;
+ (void)openWikipedia:(PlacePageData *)data;
+ (void)openWikimediaCommons:(PlacePageData *)data;
+ (void)openEmail:(PlacePageData *)data;
+ (void)openFacebook:(PlacePageData *)data;
+ (void)openInstagram:(PlacePageData *)data;
+ (void)openTwitter:(PlacePageData *)data;
+ (void)openVk:(PlacePageData *)data;
+ (void)openLine:(PlacePageData *)data;
+ (void)call:(PlacePageData *)data;
+ (void)showAllFacilities:(PlacePageData *)data;
+ (void)showPlaceDescription:(NSString *)htmlString;
+ (void)openMoreUrl:(PlacePageData *)data;
+ (void)openReviewUrl:(PlacePageData *)data;
+ (void)openDescriptionUrl:(PlacePageData *)data;
+ (void)openCatalogSingleItem:(PlacePageData *)data atIndex:(NSInteger)index;
+ (void)openCatalogMoreItems:(PlacePageData *)data;
+ (void)addBookmark:(PlacePageData *)data;
+ (void)removeBookmark:(PlacePageData *)data;
+ (void)removeTrack:(PlacePageData *)data;
+ (void)editBookmark:(PlacePageData *)data;
+ (void)editTrack:(PlacePageData *)data;
+ (void)searchBookingHotels:(PlacePageData *)data;
+ (void)book:(PlacePageData *)data;
+ (void)routeFrom:(PlacePageData *)data;
+ (void)routeTo:(PlacePageData *)data;
+ (void)routeAddStop:(PlacePageData *)data;
+ (void)routeRemoveStop:(PlacePageData *)data;
+ (void)avoidDirty;
+ (void)avoidFerry;
+ (void)avoidToll;
+ (void)openElevationDifficultPopup:(PlacePageData *)data;

@end
