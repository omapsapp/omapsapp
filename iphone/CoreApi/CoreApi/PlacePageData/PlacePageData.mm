#import "PlacePageData.h"

#import "PlacePageButtonsData+Core.h"
#import "PlacePagePreviewData+Core.h"
#import "PlacePageInfoData+Core.h"
#import "PlacePageBookmarkData+Core.h"
#import "PlacePageTrackData+Core.h"
#import "MWMMapNodeAttributes.h"

#include <CoreApi/CoreApi.h>
#include "platform/network_policy.hpp"

static place_page::Info & rawData() { return GetFramework().GetCurrentPlacePageInfo(); }

static PlacePageRoadType convertRoadType(RoadWarningMarkType roadType) {
  switch (roadType) {
    case RoadWarningMarkType::Toll:
      return PlacePageRoadTypeToll;
    case RoadWarningMarkType::Ferry:
      return PlacePageRoadTypeFerry;
    case RoadWarningMarkType::Dirty:
      return PlacePageRoadTypeDirty;
    case RoadWarningMarkType::Count:
      return PlacePageRoadTypeNone;
  }
}

@interface PlacePageData () <MWMStorageObserver> {
  FeatureID m_featureID;
  m2::PointD m_mercator;
  std::vector<std::string> m_rawTypes;
}

@end

@implementation PlacePageData

- (instancetype)initWithLocalizationProvider:(id<IOpeningHoursLocalization>)localization {
  self = [super init];
  if (self) {
    _buttonsData = [[PlacePageButtonsData alloc] initWithRawData:rawData()];
    _infoData = [[PlacePageInfoData alloc] initWithRawData:rawData() ohLocalization:localization];

    if (rawData().IsBookmark()) {
      _objectType = PlacePageObjectTypeBookmark;
      _bookmarkData = [[PlacePageBookmarkData alloc] initWithRawData:rawData()];
    }

    if (auto const & wikiDescription = rawData().GetWikiDescription(); !wikiDescription.empty()) {
      _wikiDescriptionHtml = @(("<html><body>" + wikiDescription + "</body></html>").c_str());
    }

    _roadType = convertRoadType(rawData().GetRoadType());

    _isMyPosition = rawData().IsMyPosition();
    _isRoutePoint = rawData().IsRoutePoint();
    _isPreviewPlus = rawData().GetOpeningMode() == place_page::OpeningMode::PreviewPlus;
    auto latlon = rawData().GetLatLon();
    _locationCoordinate = CLLocationCoordinate2DMake(latlon.m_lat, latlon.m_lon);

    NSMutableArray *tagsArray = [NSMutableArray array];
    for (auto const & s : rawData().GetRawTypes()) {
      [tagsArray addObject:@(s.c_str())];
    }

    if (rawData().IsTrack()) {
      _objectType = PlacePageObjectTypeTrack;
      auto const & track = GetFramework().GetBookmarkManager().GetTrack(rawData().GetTrackId());
      _trackData = [[PlacePageTrackData alloc] initWithTrack:*track];
      _isPreviewPlus = track->HasAltitudes();
    }
    _previewData = [[PlacePagePreviewData alloc] initWithRawData:rawData()];

    auto const &countryId = rawData().GetCountryId();
    if (!countryId.empty()) {
      _mapNodeAttributes = [[MWMStorage sharedStorage] attributesForCountry:@(rawData().GetCountryId().c_str())];
      [[MWMStorage sharedStorage] addObserver:self];
    }

    m_featureID = rawData().GetID();
    m_mercator = rawData().GetMercator();
    m_rawTypes = rawData().GetRawTypes();
  }
  return self;
}

- (void)dealloc {
  if (self.mapNodeAttributes != nil) {
    [[MWMStorage sharedStorage] removeObserver:self];
  }
}

+ (BOOL)hasData {
  return GetFramework().HasPlacePageInfo();
}

#pragma mark - Private

- (void)updateBookmarkStatus {
  if (!GetFramework().HasPlacePageInfo()) {
    return;
  }
  if (rawData().IsBookmark()) {
    _bookmarkData = [[PlacePageBookmarkData alloc] initWithRawData:rawData()];
  } else {
    _bookmarkData = nil;
  }
  _previewData = [[PlacePagePreviewData alloc] initWithRawData:rawData()];
  if (self.onBookmarkStatusUpdate != nil) {
    self.onBookmarkStatusUpdate();
  }
}

#pragma mark - MWMStorageObserver

- (void)processCountryEvent:(NSString *)countryId {
  if ([countryId isEqualToString:self.mapNodeAttributes.countryId]) {
    _mapNodeAttributes = [[MWMStorage sharedStorage] attributesForCountry:countryId];
    if (self.onMapNodeStatusUpdate != nil) {
      self.onMapNodeStatusUpdate();
    }
  }
}

- (void)processCountry:(NSString *)countryId downloadedBytes:(uint64_t)downloadedBytes totalBytes:(uint64_t)totalBytes {
  if ([countryId isEqualToString:self.mapNodeAttributes.countryId] && self.onMapNodeProgressUpdate != nil) {
    self.onMapNodeProgressUpdate(downloadedBytes, totalBytes);
  }
}

@end
