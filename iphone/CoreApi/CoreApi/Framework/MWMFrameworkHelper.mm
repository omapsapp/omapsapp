#import "MWMFrameworkHelper.h"
#import "MWMMapSearchResult+Core.h"
#import "ProductsConfiguration+Core.h"
#import "Product+Core.h"
#import "TrackInfo+Core.h"

#include "Framework.h"

#include "base/sunrise_sunset.hpp"

#include "platform/local_country_file_utils.hpp"
#include "platform/network_policy_ios.h"

static Framework::ProductsPopupCloseReason ConvertProductPopupCloseReasonToCore(ProductsPopupCloseReason reason) {
  switch (reason) {
    case ProductsPopupCloseReasonClose:
      return Framework::ProductsPopupCloseReason::Close;
    case ProductsPopupCloseReasonSelectProduct:
      return Framework::ProductsPopupCloseReason::SelectProduct;
    case ProductsPopupCloseReasonAlreadyDonated:
      return Framework::ProductsPopupCloseReason::AlreadyDonated;
    case ProductsPopupCloseReasonRemindLater:
      return Framework::ProductsPopupCloseReason::RemindLater;
  }
}

@implementation MWMFrameworkHelper

+ (void)processFirstLaunch:(BOOL)hasLocation {
  auto &f = GetFramework();
  if (!hasLocation)
    f.SwitchMyPositionNextMode();
  else
    f.RunFirstLaunchAnimation();
}

+ (void)setVisibleViewport:(CGRect)rect scaleFactor:(CGFloat)scale {
  CGFloat const x0 = rect.origin.x * scale;
  CGFloat const y0 = rect.origin.y * scale;
  CGFloat const x1 = x0 + rect.size.width * scale;
  CGFloat const y1 = y0 + rect.size.height * scale;
  GetFramework().SetVisibleViewport(m2::RectD(x0, y0, x1, y1));
}

+ (void)setTheme:(MWMTheme)theme {
  auto &f = GetFramework();

  auto const style = f.GetMapStyle();
  auto const isOutdoor = ^BOOL(MapStyle style) {
    switch (style) {
      case MapStyleOutdoorsLight:
      case MapStyleOutdoorsDark:
        return YES;
      default:
        return NO;
    }
  }(style);
  auto const newStyle = ^MapStyle(MWMTheme theme) {
    switch (theme) {
      case MWMThemeDay:
        return isOutdoor ? MapStyleOutdoorsLight : MapStyleDefaultLight;
      case MWMThemeVehicleDay:
        return MapStyleVehicleLight;
      case MWMThemeNight:
        return isOutdoor ? MapStyleOutdoorsDark : MapStyleDefaultDark;
      case MWMThemeVehicleNight:
        return MapStyleVehicleDark;
      case MWMThemeAuto:
        NSAssert(NO, @"Invalid theme");
        return MapStyleDefaultLight;
    }
  }(theme);

  if (style != newStyle)
    f.SetMapStyle(newStyle);
}

+ (MWMDayTime)daytimeAtLocation:(CLLocation *)location {
  if (!location)
    return MWMDayTimeDay;
  DayTimeType dayTime =
    GetDayTime(NSDate.date.timeIntervalSince1970, location.coordinate.latitude, location.coordinate.longitude);
  switch (dayTime) {
    case DayTimeType::Day:
    case DayTimeType::PolarDay:
      return MWMDayTimeDay;
    case DayTimeType::Night:
    case DayTimeType::PolarNight:
      return MWMDayTimeNight;
  }
}

+ (void)createFramework {
  UNUSED_VALUE(GetFramework());
}

+ (MWMMarkID)invalidBookmarkId {
  return kml::kInvalidMarkId;
}

+ (MWMMarkGroupID)invalidCategoryId {
  return kml::kInvalidMarkGroupId;
}

+ (NSArray<NSString *> *)obtainLastSearchQueries {
  NSMutableArray *result = [NSMutableArray array];
  auto const &queries = GetFramework().GetSearchAPI().GetLastSearchQueries();
  for (auto const &item : queries) {
    [result addObject:@(item.second.c_str())];
  }
  return [result copy];
}

#pragma mark - Map Interaction

+ (void)zoomMap:(MWMZoomMode)mode {
  switch (mode) {
    case MWMZoomModeIn:
      GetFramework().Scale(Framework::SCALE_MAG, true);
      break;
    case MWMZoomModeOut:
      GetFramework().Scale(Framework::SCALE_MIN, true);
      break;
  }
}

+ (void)moveMap:(UIOffset)offset {
  GetFramework().Move(offset.horizontal, offset.vertical, true);
}

+ (void)scrollMap:(double) distanceX :(double) distanceY {
  GetFramework().Scroll(distanceX, distanceY);
}

+ (void)deactivateMapSelection {
  GetFramework().DeactivateMapSelection();
}

+ (void)switchMyPositionMode {
  GetFramework().SwitchMyPositionNextMode();
}

+ (void)stopLocationFollow {
  GetFramework().StopLocationFollow();
}

+ (void)rotateMap:(double)azimuth animated:(BOOL)isAnimated {
  GetFramework().Rotate(azimuth, isAnimated);
}

+ (void)updatePositionArrowOffset:(BOOL)useDefault offset:(int)offsetY {
  GetFramework().UpdateMyPositionRoutingOffset(useDefault, offsetY);
}

+ (int64_t)dataVersion {
  return GetFramework().GetCurrentDataVersion();
}

+ (void)searchInDownloader:(NSString *)query
               inputLocale:(NSString *)locale
                completion:(SearchInDownloaderCompletions)completion {
  storage::DownloaderSearchParams params{
    query.UTF8String,
    locale.precomposedStringWithCompatibilityMapping.UTF8String,
    // m_onResults
    [completion](storage::DownloaderSearchResults const & results)
    {
      NSMutableArray *resultsArray = [NSMutableArray arrayWithCapacity:results.m_results.size()];
      for (auto const & res : results.m_results)
      {
        MWMMapSearchResult *result = [[MWMMapSearchResult alloc] initWithSearchResult:res];
        [resultsArray addObject:result];
      }
      completion(resultsArray, results.m_endMarker);
    }
  };

  GetFramework().GetSearchAPI().SearchInDownloader(std::move(params));
}

+ (BOOL)canEditMapAtViewportCenter {
  auto const &f = GetFramework();
  return f.CanEditMapForPosition(f.GetViewportCenter());
}

+ (void)showOnMap:(MWMMarkGroupID)categoryId {
  GetFramework().ShowBookmarkCategory(categoryId);
}

+ (void)showBookmark:(MWMMarkID)bookmarkId {
  GetFramework().ShowBookmark(bookmarkId);
}

+ (void)showTrack:(MWMTrackID)trackId {
  GetFramework().ShowTrack(trackId);
}

+ (void)updatePlacePageData {
  GetFramework().UpdatePlacePageInfoForCurrentSelection();
}

+ (void)updateAfterDeleteBookmark {
  auto & frm = GetFramework();
  auto buildInfo = frm.GetCurrentPlacePageInfo().GetBuildInfo();
  buildInfo.m_match = place_page::BuildInfo::Match::FeatureOnly;
  buildInfo.m_userMarkId = kml::kInvalidMarkId;
  buildInfo.m_source = place_page::BuildInfo::Source::Other;
  frm.UpdatePlacePageInfoForCurrentSelection(buildInfo);
}

+ (int)currentZoomLevel {
  return GetFramework().GetDrawScale();
}

// MARK: - TrackRecorder

+ (void)startTrackRecording {
  GetFramework().StartTrackRecording();
}

+ (void)setTrackRecordingUpdateHandler:(TrackRecordingUpdatedHandler _Nullable)trackRecordingDidUpdate {
  if (!trackRecordingDidUpdate)
  {
    GetFramework().SetTrackRecordingUpdateHandler(nullptr);
    return;
  }
  GetFramework().SetTrackRecordingUpdateHandler([trackRecordingDidUpdate](GpsTrackInfo const & gpsTrackInfo) {
    TrackInfo * info = [[TrackInfo alloc] initWithGpsTrackInfo:gpsTrackInfo];
    trackRecordingDidUpdate(info);
  });
}

+ (void)stopTrackRecording {
  GetFramework().StopTrackRecording();
}

+ (void)saveTrackRecordingWithName:(nullable NSString *)name {
  GetFramework().SaveTrackRecordingWithName(name == nil ? "" : name.UTF8String);
}

+ (BOOL)isTrackRecordingEnabled {
  return GetFramework().IsTrackRecordingEnabled();
}

+ (BOOL)isTrackRecordingEmpty {
  return GetFramework().IsTrackRecordingEmpty();
}

// MARK: - ProductsManager

+ (nullable ProductsConfiguration *)getProductsConfiguration {
  auto const & config = GetFramework().GetProductsConfiguration();
  return config.has_value() ? [[ProductsConfiguration alloc] init:config.value()] : nil;
}

+ (void)didCloseProductsPopupWithReason:(ProductsPopupCloseReason)reason {
  GetFramework().DidCloseProductsPopup(ConvertProductPopupCloseReasonToCore(reason));
}

+ (void)didSelectProduct:(Product *)product {
  GetFramework().DidSelectProduct({product.title.UTF8String, product.link.UTF8String});
}

@end
