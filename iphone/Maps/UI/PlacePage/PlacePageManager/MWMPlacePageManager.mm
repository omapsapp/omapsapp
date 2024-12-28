#import "MWMPlacePageManager.h"
#import "CLLocation+Mercator.h"
#import "MWMActivityViewController.h"
#import "MWMLocationHelpers.h"
#import "MWMLocationObserver.h"
#import "MWMRoutePoint+CPP.h"
#import "MWMStorage+UI.h"
#import "SwiftBridge.h"
#import "MWMMapViewControlsManager+AddPlace.h"
#import "location_util.h"

#import <CoreApi/Framework.h>
#import <CoreApi/StringUtils.h>

#include "platform/downloader_defines.hpp"

#include "indexer/validate_and_format_contacts.hpp"

using namespace storage;

@interface MWMPlacePageManager ()

@property(nonatomic) storage::NodeStatus currentDownloaderStatus;

@end

@implementation MWMPlacePageManager

- (BOOL)isPPShown {
  return GetFramework().HasPlacePageInfo();
}

- (void)closePlacePage { GetFramework().DeactivateMapSelection(); }

- (void)routeFrom:(PlacePageData *)data {
  MWMRoutePoint *point = [self routePoint:data withType:MWMRoutePointTypeStart intermediateIndex:0];
  [MWMRouter buildFromPoint:point bestRouter:YES];
  [self closePlacePage];
}

- (void)routeTo:(PlacePageData *)data {
  if ([MWMRouter isOnRoute]) {
    [MWMRouter stopRouting];
  }

  if ([MWMMapOverlayManager transitEnabled]) {
    [MWMRouter setType:MWMRouterTypePublicTransport];
  }

  MWMRoutePoint *point = [self routePoint:data withType:MWMRoutePointTypeFinish intermediateIndex:0];
  [MWMRouter buildToPoint:point bestRouter:YES];
  [self closePlacePage];
}

- (void)routeAddStop:(PlacePageData *)data {
  MWMRoutePoint *point = [self routePoint:data withType:MWMRoutePointTypeIntermediate intermediateIndex:0];
  [MWMRouter addPointAndRebuild:point];
  [self closePlacePage];
}

- (void)routeRemoveStop:(PlacePageData *)data {
  MWMRoutePoint *point = nil;
  auto const intermediateIndex = GetFramework().GetCurrentPlacePageInfo().GetIntermediateIndex();
  switch (GetFramework().GetCurrentPlacePageInfo().GetRouteMarkType()) {
  case RouteMarkType::Start:
    point = [self routePoint:data withType:MWMRoutePointTypeStart intermediateIndex:intermediateIndex];
    break;
  case RouteMarkType::Finish:
    point = [self routePoint:data withType:MWMRoutePointTypeFinish intermediateIndex:intermediateIndex];
    break;
  case RouteMarkType::Intermediate:
    point = [self routePoint:data withType:MWMRoutePointTypeIntermediate intermediateIndex:intermediateIndex];
    break;
  }
  [MWMRouter removePointAndRebuild:point];
  [self closePlacePage];
}

- (MWMRoutePoint *)routePointWithData:(PlacePageData *)data
                                 pointType:(MWMRoutePointType)type
                    intermediateIndex:(size_t)intermediateIndex
{
  if (data.isMyPosition) {
    return [[MWMRoutePoint alloc] initWithLastLocationAndType:type intermediateIndex:intermediateIndex];
  }

  NSString *title = nil;
  if (data.previewData.title.length > 0) {
    title = data.previewData.title;
  } else if (data.previewData.secondarySubtitle.length > 0) {
    title = data.previewData.secondarySubtitle;
  } else if (data.previewData.subtitle.length > 0) {
    title = data.previewData.subtitle;
  } else if (data.bookmarkData != nil) {
    title = data.bookmarkData.externalTitle;
  } else {
    title = L(@"core_placepage_unknown_place");
  }

  NSString * subtitle = nil;
  if (data.previewData.subtitle.length > 0 && ![title isEqualToString:data.previewData.subtitle]) {
    subtitle = data.previewData.subtitle;
  }

  return [[MWMRoutePoint alloc] initWithPoint:location_helpers::ToMercator(data.locationCoordinate)
                                        title:title
                                     subtitle:subtitle
                                         type:type
                            intermediateIndex:intermediateIndex];
}

- (MWMRoutePoint *)routePoint:(PlacePageData *)data
                     withType:(MWMRoutePointType)type
                    intermediateIndex:(size_t)intermediateIndex
{
  if (data.isMyPosition) {
    return [[MWMRoutePoint alloc] initWithLastLocationAndType:type
                                            intermediateIndex:intermediateIndex];
  }

  NSString *title = nil;
  if (data.previewData.title.length > 0) {
    title = data.previewData.title;
  } else if (data.previewData.secondarySubtitle.length > 0) {
    title = data.previewData.secondarySubtitle;
  } else if (data.previewData.subtitle.length > 0) {
    title = data.previewData.subtitle;
  } else if (data.bookmarkData != nil) {
    title = data.bookmarkData.externalTitle;
  } else {
    title = L(@"core_placepage_unknown_place");
  }

  NSString * subtitle = nil;
  if (data.previewData.subtitle.length > 0 && ![title isEqualToString:data.previewData.subtitle]) {
    subtitle = data.previewData.subtitle;
  }

  return [[MWMRoutePoint alloc] initWithPoint:location_helpers::ToMercator(data.locationCoordinate)
                                        title:title
                                     subtitle:subtitle
                                         type:type
                            intermediateIndex:intermediateIndex];
}

- (void)editPlace
{
  [self.ownerViewController openEditor];
}

- (void)addBusiness
{
  [[MWMMapViewControlsManager manager] addPlace:YES position:nullptr];
}

- (void)addPlace:(CLLocationCoordinate2D)coordinate
{
  auto const position = location_helpers::ToMercator(coordinate);
  [[MWMMapViewControlsManager manager] addPlace:NO position:&position];
}

- (void)addBookmark:(PlacePageData *)data {
  auto &f = GetFramework();
  auto &bmManager = f.GetBookmarkManager();
  auto &info = f.GetCurrentPlacePageInfo();
  auto const categoryId = f.LastEditedBMCategory();
  kml::BookmarkData bmData;
  bmData.m_name = info.FormatNewBookmarkName();
  bmData.m_color.m_predefinedColor = f.LastEditedBMColor();
  bmData.m_point = info.GetMercator();
  if (info.IsFeature()) {
    SaveFeatureTypes(info.GetTypes(), bmData);
  }
  auto editSession = bmManager.GetEditSession();
  auto const *bookmark = editSession.CreateBookmark(std::move(bmData), categoryId);

  auto buildInfo = info.GetBuildInfo();
  buildInfo.m_match = place_page::BuildInfo::Match::Everything;
  buildInfo.m_userMarkId = bookmark->GetId();
  f.UpdatePlacePageInfoForCurrentSelection(buildInfo);
  [data updateBookmarkStatus];
}

- (void)removeBookmark:(PlacePageData *)data
{
  auto &f = GetFramework();
  f.GetBookmarkManager().GetEditSession().DeleteBookmark(data.bookmarkData.bookmarkId);
  [MWMFrameworkHelper updateAfterDeleteBookmark];
  [data updateBookmarkStatus];
}

- (void)removeTrack:(PlacePageData *)data
{
  auto &f = GetFramework();
  f.GetBookmarkManager().GetEditSession().DeleteTrack(data.trackData.trackId);
}

- (void)call:(PlacePageData *)data {
  if (data.infoData.phoneUrl && [UIApplication.sharedApplication canOpenURL:data.infoData.phoneUrl]) {
    [UIApplication.sharedApplication openURL:data.infoData.phoneUrl options:@{} completionHandler:nil];
  }
}

- (void)editBookmark:(PlacePageData *)data {
  MWMEditBookmarkController *editBookmarkController = [[UIStoryboard instance:MWMStoryboardMain]
                                                       instantiateViewControllerWithIdentifier:@"MWMEditBookmarkController"];
  [editBookmarkController configureWithPlacePageData:data];
  [[MapViewController sharedController].navigationController pushViewController:editBookmarkController animated:YES];
}

- (void)editTrack:(PlacePageData *)data {
  if (data.objectType != PlacePageObjectTypeTrack) {
    ASSERT_FAIL("editTrack called for non-track object");
    return;
  }
  EditTrackViewController * editTrackController = [[EditTrackViewController alloc] initWithTrackId:data.trackData.trackId editCompletion:^(BOOL edited) {
    if (!edited)
      return;
    [MWMFrameworkHelper updatePlacePageData];
    [data updateBookmarkStatus];
  }];
  [[MapViewController sharedController].navigationController pushViewController:editTrackController animated:YES];
}

- (void)showPlaceDescription:(NSString *)htmlString
{
  [self.ownerViewController openFullPlaceDescriptionWithHtml:htmlString];
}

- (void)avoidDirty {
  [MWMRouter avoidRoadTypeAndRebuild:MWMRoadTypeDirty];
  [self closePlacePage];
}

- (void)avoidFerry {
  [MWMRouter avoidRoadTypeAndRebuild:MWMRoadTypeFerry];
  [self closePlacePage];
}

- (void)avoidToll {
  [MWMRouter avoidRoadTypeAndRebuild:MWMRoadTypeToll];
  [self closePlacePage];
}

- (void)openWebsite:(PlacePageData *)data {
  [self.ownerViewController openUrl:data.infoData.website externally:YES];
}

- (void)openWebsiteMenu:(PlacePageData *)data {
  [self.ownerViewController openUrl:data.infoData.websiteMenu externally:YES];
}

- (void)openKayak:(PlacePageData *)data {
  [self.ownerViewController openUrl:data.infoData.kayak externally:YES];
}

- (void)openWikipedia:(PlacePageData *)data {
  [self.ownerViewController openUrl:data.infoData.wikipedia externally:YES];
}

- (void)openWikimediaCommons:(PlacePageData *)data {
  [self.ownerViewController openUrl:data.infoData.wikimediaCommons externally:YES];
}

- (void)openFacebook:(PlacePageData *)data {
  std::string const fullUrl = osm::socialContactToURL(osm::MapObject::MetadataID::FMD_CONTACT_FACEBOOK, [data.infoData.facebook UTF8String]);
  [self.ownerViewController openUrl:ToNSString(fullUrl) externally:YES];
}

- (void)openInstagram:(PlacePageData *)data {
  std::string const fullUrl = osm::socialContactToURL(osm::MapObject::MetadataID::FMD_CONTACT_INSTAGRAM, [data.infoData.instagram UTF8String]);
  [self.ownerViewController openUrl:ToNSString(fullUrl) externally:YES];
}

- (void)openTwitter:(PlacePageData *)data {
  std::string const fullUrl = osm::socialContactToURL(osm::MapObject::MetadataID::FMD_CONTACT_TWITTER, [data.infoData.twitter UTF8String]);
  [self.ownerViewController openUrl:ToNSString(fullUrl) externally:YES];
}

- (void)openVk:(PlacePageData *)data {
  std::string const fullUrl = osm::socialContactToURL(osm::MapObject::MetadataID::FMD_CONTACT_VK, [data.infoData.vk UTF8String]);
  [self.ownerViewController openUrl:ToNSString(fullUrl) externally:YES];
}

- (void)openLine:(PlacePageData *)data {
  std::string const fullUrl = osm::socialContactToURL(osm::MapObject::MetadataID::FMD_CONTACT_LINE, [data.infoData.line UTF8String]);
  [self.ownerViewController openUrl:ToNSString(fullUrl) externally:YES];
}

- (void)openEmail:(PlacePageData *)data {
  [MailComposer sendEmailWithSubject:nil body:nil toRecipients:@[data.infoData.email] attachmentFileURL:nil];
}

- (void)openElevationDifficultPopup:(PlacePageData *)data {
  auto difficultyPopup = [ElevationDetailsBuilder buildWithData:data];
  [[MapViewController sharedController] presentViewController:difficultyPopup animated:YES completion:nil];
}

#pragma mark - AvailableArea / PlacePageArea

- (void)updateAvailableArea:(CGRect)frame
{
//  auto data = self.data;
//  if (data)
//    [self.layout updateAvailableArea:frame];
}

#pragma mark - MWMFeatureHolder

- (FeatureID const &)featureId { return GetFramework().GetCurrentPlacePageInfo().GetID(); }

- (MapViewController *)ownerViewController { return [MapViewController sharedController]; }

@end
