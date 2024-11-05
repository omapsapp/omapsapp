#import "MWMBookmarksManager.h"

#import "MWMBookmark+Core.h"
#import "MWMBookmarksSection.h"
#import "MWMBookmarkGroup.h"
#import "MWMCarPlayBookmarkObject.h"
#import "MWMTrack+Core.h"
#import "RecentlyDeletedCategory+Core.h"

#include "Framework.h"

#include "map/bookmarks_search_params.hpp"

#include "coding/internal/file_data.hpp"

#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <utility>

static kml::PredefinedColor kmlColorFromBookmarkColor(MWMBookmarkColor bookmarkColor) {
  switch (bookmarkColor) {
    case MWMBookmarkColorNone:
      return kml::PredefinedColor::None;
    case MWMBookmarkColorRed:
      return kml::PredefinedColor::Red;
    case MWMBookmarkColorBlue:
      return kml::PredefinedColor::Blue;
    case MWMBookmarkColorPurple:
      return kml::PredefinedColor::Purple;
    case MWMBookmarkColorYellow:
      return kml::PredefinedColor::Yellow;
    case MWMBookmarkColorPink:
      return kml::PredefinedColor::Pink;
    case MWMBookmarkColorBrown:
      return kml::PredefinedColor::Brown;
    case MWMBookmarkColorGreen:
      return kml::PredefinedColor::Green;
    case MWMBookmarkColorOrange:
      return kml::PredefinedColor::Orange;
    case MWMBookmarkColorDeepPurple:
      return kml::PredefinedColor::DeepPurple;
    case MWMBookmarkColorLightBlue:
      return kml::PredefinedColor::LightBlue;
    case MWMBookmarkColorCyan:
      return kml::PredefinedColor::Cyan;
    case MWMBookmarkColorTeal:
      return kml::PredefinedColor::Teal;
    case MWMBookmarkColorLime:
      return kml::PredefinedColor::Lime;
    case MWMBookmarkColorDeepOrange:
      return kml::PredefinedColor::DeepOrange;
    case MWMBookmarkColorGray:
      return kml::PredefinedColor::Gray;
    case MWMBookmarkColorBlueGray:
      return kml::PredefinedColor::BlueGray;
    case MWMBookmarkColorCount:
      return kml::PredefinedColor::Count;
  }
}

static MWMBookmarksSortingType convertSortingType(BookmarkManager::SortingType const &sortingType) {
  switch (sortingType) {
    case BookmarkManager::SortingType::ByType:
      return MWMBookmarksSortingTypeByType;
    case BookmarkManager::SortingType::ByDistance:
      return MWMBookmarksSortingTypeByDistance;
    case BookmarkManager::SortingType::ByTime:
      return MWMBookmarksSortingTypeByTime;
    case BookmarkManager::SortingType::ByName:
      return MWMBookmarksSortingTypeByName;
  }
}

static BookmarkManager::SortingType convertSortingTypeToCore(MWMBookmarksSortingType sortingType) {
  switch (sortingType) {
    case MWMBookmarksSortingTypeByType:
      return BookmarkManager::SortingType::ByType;
    case MWMBookmarksSortingTypeByDistance:
      return BookmarkManager::SortingType::ByDistance;
    case MWMBookmarksSortingTypeByTime:
      return BookmarkManager::SortingType::ByTime;
    case MWMBookmarksSortingTypeByName:
      return BookmarkManager::SortingType::ByName;
  }
}

static KmlFileType convertFileTypeToCore(MWMKmlFileType fileType) {
  switch (fileType) {
    case MWMKmlFileTypeText:
      return KmlFileType::Text;
    case MWMKmlFileTypeBinary:
      return KmlFileType::Binary;
    case MWMKmlFileTypeGpx:
      return KmlFileType::Gpx;
  }
}

@interface MWMBookmarksManager ()

@property(nonatomic, readonly) BookmarkManager & bm;

@property(nonatomic) NSHashTable<id<MWMBookmarksObserver>> * observers;
@property(nonatomic) BOOL areBookmarksLoaded;
@property(nonatomic) NSURL * shareCategoryURL;
@property(nonatomic) NSInteger lastSearchId;
@property(nonatomic) NSInteger lastSortId;

@end

@implementation MWMBookmarksManager

+ (instancetype)sharedManager
{
  static MWMBookmarksManager * manager;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    manager = [[self alloc] initManager];
  });
  return manager;
}

- (BookmarkManager &)bm
{
  return GetFramework().GetBookmarkManager();
}

- (void)addObserver:(id<MWMBookmarksObserver>)observer
{
  [self.observers addObject:observer];
}

- (void)removeObserver:(id<MWMBookmarksObserver>)observer
{
  [self.observers removeObject:observer];
}

- (instancetype)initManager
{
  self = [super init];
  if (self)
  {
    _observers = [NSHashTable<id<MWMBookmarksObserver>> weakObjectsHashTable];
    [self registerBookmarksObserver];
  }
  return self;
}

- (void)registerBookmarksObserver
{
  BookmarkManager::AsyncLoadingCallbacks bookmarkCallbacks;
  {
    __weak auto wSelf = self;
    bookmarkCallbacks.m_onStarted = [wSelf]() {
      wSelf.areBookmarksLoaded = NO;
    };
  }
  {
    __weak auto wSelf = self;
    bookmarkCallbacks.m_onFinished = [wSelf]() {
      __strong auto self = wSelf;
      if (!self)
        return;
      self.areBookmarksLoaded = YES;
      [self loopObservers:^(id<MWMBookmarksObserver> observer) {
        if ([observer respondsToSelector:@selector(onBookmarksLoadFinished)])
          [observer onBookmarksLoadFinished];
      }];
    };
  }
  {
    __weak auto wSelf = self;
    bookmarkCallbacks.m_onFileSuccess = [wSelf](std::string const & filePath,
                                                bool isTemporaryFile) {
      __strong __typeof(self) self = wSelf;
      [self loopObservers:^(id<MWMBookmarksObserver> observer) {
        if ([observer respondsToSelector:@selector(onBookmarksFileLoadSuccess)])
          [observer onBookmarksFileLoadSuccess];
      }];
    };
  }
  {
    __weak auto wSelf = self;
    bookmarkCallbacks.m_onFileError = [wSelf](std::string const & filePath, bool isTemporaryFile) {
      __strong __typeof(self) self = wSelf;
      [self loopObservers:^(id<MWMBookmarksObserver> observer) {
        if ([observer respondsToSelector:@selector(onBookmarksFileLoadError)])
          [observer onBookmarksFileLoadError];
      }];
    };
  }
  self.bm.SetAsyncLoadingCallbacks(std::move(bookmarkCallbacks));
}

#pragma mark - Bookmarks loading

- (BOOL)areBookmarksLoaded
{
  return _areBookmarksLoaded;
}

- (void)loadBookmarks
{
  self.bm.LoadBookmarks();
}

- (void)loadBookmarkFile:(NSURL *)url
{
  self.bm.LoadBookmark(url.path.UTF8String, false /* isTemporaryFile */);
}

- (void)reloadCategoryAtFilePath:(NSString *)filePath
{
  self.bm.ReloadBookmark(filePath.UTF8String);
}

- (void)deleteCategoryAtFilePath:(NSString *)filePath
{
  auto const groupId = self.bm.GetCategoryByFileName(filePath.UTF8String);
  if (groupId)
    [self deleteCategory:groupId];
}

#pragma mark - Categories

- (BOOL)areAllCategoriesEmpty
{
  return self.bm.AreAllCategoriesEmpty();
}

- (BOOL)isCategoryEmpty:(MWMMarkGroupID)groupId {
  return self.bm.HasBmCategory(groupId) && self.bm.IsCategoryEmpty(groupId);
}

- (void)prepareForSearch:(MWMMarkGroupID)groupId {
  self.bm.PrepareForSearch(groupId);
}

- (NSString *)getCategoryName:(MWMMarkGroupID)groupId
{
  return @(self.bm.GetCategoryName(groupId).c_str());
}

- (uint64_t)getCategoryMarksCount:(MWMMarkGroupID)groupId
{
  return self.bm.GetUserMarkIds(groupId).size();
}

- (uint64_t)getCategoryTracksCount:(MWMMarkGroupID)groupId
{
  return self.bm.GetTrackIds(groupId).size();
}

- (MWMBookmarkGroupAccessStatus)getCategoryAccessStatus:(MWMMarkGroupID)groupId
{
  switch (self.bm.GetCategoryData(groupId).m_accessRules)
  {
    case kml::AccessRules::Local:
      return MWMBookmarkGroupAccessStatusLocal;
    case kml::AccessRules::Public:
      return MWMBookmarkGroupAccessStatusPublic;
    case kml::AccessRules::DirectLink:
      return MWMBookmarkGroupAccessStatusPrivate;
    case kml::AccessRules::AuthorOnly:
      return MWMBookmarkGroupAccessStatusAuthorOnly;
    case kml::AccessRules::P2P:
    case kml::AccessRules::Paid:
    case kml::AccessRules::Count:
      return MWMBookmarkGroupAccessStatusOther;
  }
}

- (NSString *)getCategoryAnnotation:(MWMMarkGroupID)groupId {
  return @(GetPreferredBookmarkStr(self.bm.GetCategoryData(groupId).m_annotation).c_str());
}

- (NSString *)getCategoryDescription:(MWMMarkGroupID)groupId
{
  return @(GetPreferredBookmarkStr(self.bm.GetCategoryData(groupId).m_description).c_str());
}

- (NSString *)getCategoryAuthorName:(MWMMarkGroupID)groupId
{
  return @(self.bm.GetCategoryData(groupId).m_authorName.c_str());
}

- (NSString *)getCategoryAuthorId:(MWMMarkGroupID)groupId
{
  return @(self.bm.GetCategoryData(groupId).m_authorId.c_str());
}

- (MWMBookmarkGroupType)getCategoryGroupType:(MWMMarkGroupID)groupId {
  if (self.bm.IsCompilation(groupId) == false) {
    return MWMBookmarkGroupTypeRoot;
  }
  switch (self.bm.GetCompilationType(groupId)) {
    case kml::CompilationType::Category:
      return MWMBookmarkGroupTypeCategory;
    case kml::CompilationType::Collection:
      return MWMBookmarkGroupTypeCollection;
    case kml::CompilationType::Day:
      return MWMBookmarkGroupTypeDay;
  }
  return MWMBookmarkGroupTypeRoot;
}

- (nullable NSURL *)getCategoryImageUrl:(MWMMarkGroupID)groupId {
  NSString *urlString = @(self.bm.GetCategoryData(groupId).m_imageUrl.c_str());
  return [NSURL URLWithString:urlString];
}

- (BOOL)hasExtraInfo:(MWMMarkGroupID)groupId {
  auto data = self.bm.GetCategoryData(groupId);
  return !data.m_description.empty() || !data.m_annotation.empty();
}

- (BOOL)isHtmlDescription:(MWMMarkGroupID)groupId {
  auto const description = GetPreferredBookmarkStr(self.bm.GetCategoryData(groupId).m_description);
  return strings::IsHTML(description);
}

- (MWMMarkGroupID)createCategoryWithName:(NSString *)name
{
  auto groupId = self.bm.CreateBookmarkCategory(name.UTF8String);
  self.bm.SetLastEditedBmCategory(groupId);
  return groupId;
}

- (void)setCategory:(MWMMarkGroupID)groupId name:(NSString *)name
{
  self.bm.GetEditSession().SetCategoryName(groupId, name.UTF8String);
}

- (void)setCategory:(MWMMarkGroupID)groupId description:(NSString *)name
{
  self.bm.GetEditSession().SetCategoryDescription(groupId, name.UTF8String);
}

- (BOOL)isCategoryVisible:(MWMMarkGroupID)groupId
{
  return self.bm.IsVisible(groupId);
}

- (void)setCategory:(MWMMarkGroupID)groupId isVisible:(BOOL)isVisible
{
  self.bm.GetEditSession().SetIsVisible(groupId, isVisible);
}

- (void)setUserCategoriesVisible:(BOOL)isVisible {
  self.bm.SetAllCategoriesVisibility(isVisible);
}

- (void)setCatalogCategoriesVisible:(BOOL)isVisible {
  self.bm.SetAllCategoriesVisibility(isVisible);
}

- (void)deleteCategory:(MWMMarkGroupID)groupId
{
  self.bm.GetEditSession().DeleteBmCategory(groupId, false /* move to the Trash */);
  [self loopObservers:^(id<MWMBookmarksObserver> observer) {
    if ([observer respondsToSelector:@selector(onBookmarksCategoryDeleted:)])
      [observer onBookmarksCategoryDeleted:groupId];
  }];
}

- (BOOL)checkCategoryName:(NSString *)name
{
  return !self.bm.IsUsedCategoryName(name.UTF8String);
}

- (BOOL)hasCategory:(MWMMarkGroupID)groupId
{
  return self.bm.HasBmCategory(groupId);
}

- (BOOL)hasBookmark:(MWMMarkID)bookmarkId
{
  return self.bm.HasBookmark(bookmarkId);
}

- (BOOL)hasTrack:(MWMTrackID)trackId
{
  return self.bm.HasTrack(trackId);
}

- (NSArray<NSNumber *> *)availableSortingTypes:(MWMMarkGroupID)groupId hasMyPosition:(BOOL)hasMyPosition{
  auto const availableTypes = self.bm.GetAvailableSortingTypes(groupId, hasMyPosition);
  NSMutableArray *result = [NSMutableArray array];
  for (auto const &sortingType : availableTypes) {
    [result addObject:[NSNumber numberWithInteger:convertSortingType(sortingType)]];
  }
  return [result copy];
}

- (void)sortBookmarks:(MWMMarkGroupID)groupId
          sortingType:(MWMBookmarksSortingType)sortingType
             location:(CLLocation *)location
           completion:(SortBookmarksCompletionBlock)completion {
  self.bm.SetLastSortingType(groupId, convertSortingTypeToCore(sortingType));
  m2::PointD myPosition = m2::PointD::Zero();

  if (sortingType == MWMBookmarksSortingTypeByDistance) {
    if (!location) {
      completion(nil);
      return;
    }
    myPosition = mercator::FromLatLon(location.coordinate.latitude, location.coordinate.longitude);
  }

  auto const sortId = ++self.lastSortId;
  __weak auto weakSelf = self;

  BookmarkManager::SortParams sortParams;
  sortParams.m_groupId = groupId;
  sortParams.m_sortingType = convertSortingTypeToCore(sortingType);
  sortParams.m_hasMyPosition = location != nil;
  sortParams.m_myPosition = myPosition;
  sortParams.m_onResults = [weakSelf, sortId, completion] (BookmarkManager::SortedBlocksCollection &&sortedBlocks,
                                                           BookmarkManager::SortParams::Status status) {
    __strong auto self = weakSelf;
    if (!self || sortId != self.lastSortId)
      return;

    switch (status) {
      case BookmarkManager::SortParams::Status::Completed: {
        NSMutableArray *result = [NSMutableArray array];
        for (auto const &sortedBlock : sortedBlocks) {
          NSMutableArray *bookmarks = nil;
          if (sortedBlock.m_markIds.size() > 0) {
            bookmarks = [NSMutableArray array];
            for (auto const &markId : sortedBlock.m_markIds) {
              [bookmarks addObject:[[MWMBookmark alloc] initWithMarkId:markId
                                                          bookmarkData:self.bm.GetBookmark(markId)]];
            }
          }
          NSMutableArray *tracks = nil;
          if (sortedBlock.m_trackIds.size() > 0) {
            tracks = [NSMutableArray array];
            for (auto const &trackId : sortedBlock.m_trackIds) {
              [tracks addObject:[[MWMTrack alloc] initWithTrackId:trackId trackData:self.bm.GetTrack(trackId)]];
            }
          }
          [result addObject:[[MWMBookmarksSection alloc] initWithTitle:@(sortedBlock.m_blockName.c_str())
                                                             bookmarks:bookmarks
                                                                tracks:tracks]];
        }
        completion([result copy]);
        break;
      }
      case BookmarkManager::SortParams::Status::Cancelled:
        completion(nil);
        break;
    }
  };

  self.bm.GetSortedCategory(sortParams);
}

- (BOOL)hasLastSortingType:(MWMMarkGroupID)groupId {
  BookmarkManager::SortingType st;
  return self.bm.GetLastSortingType(groupId, st);
}

- (MWMBookmarksSortingType)lastSortingType:(MWMMarkGroupID)groupId {
  BookmarkManager::SortingType st;
  self.bm.GetLastSortingType(groupId, st);
  return convertSortingType(st);
}

- (void)resetLastSortingType:(MWMMarkGroupID)groupId {
  self.bm.ResetLastSortingType(groupId);
}

#pragma mark - Bookmarks

- (NSArray<MWMCarPlayBookmarkObject *> *)bookmarksForCategory:(MWMMarkGroupID)categoryId
{
  NSMutableArray<MWMCarPlayBookmarkObject *> * result = [NSMutableArray array];
  auto const & bookmarkIds = self.bm.GetUserMarkIds(categoryId);
  for (auto bookmarkId : bookmarkIds)
  {
    MWMCarPlayBookmarkObject *bookmark = [[MWMCarPlayBookmarkObject alloc] initWithBookmarkId:bookmarkId];
    [result addObject:bookmark];
  }
  return [result copy];
}

- (MWMMarkIDCollection)bookmarkIdsForCategory:(MWMMarkGroupID)categoryId {
  auto const &bookmarkIds = self.bm.GetUserMarkIds(categoryId);
  NSMutableArray<NSNumber *> *collection = [[NSMutableArray alloc] initWithCapacity:bookmarkIds.size()];
  for (auto bookmarkId : bookmarkIds)
    [collection addObject:@(bookmarkId)];
  return [collection copy];
}

- (void)deleteBookmark:(MWMMarkID)bookmarkId
{
  self.bm.GetEditSession().DeleteBookmark(bookmarkId);
  [self loopObservers:^(id<MWMBookmarksObserver> observer) {
    if ([observer respondsToSelector:@selector(onBookmarkDeleted:)])
      [observer onBookmarkDeleted:bookmarkId];
  }];
}

- (void)deleteTrack:(MWMTrackID)trackId {
  self.bm.GetEditSession().DeleteTrack(trackId);
}

- (MWMBookmark *)bookmarkWithId:(MWMMarkID)bookmarkId {
  return [[MWMBookmark alloc] initWithMarkId:bookmarkId bookmarkData:self.bm.GetBookmark(bookmarkId)];
}

- (MWMTrack *)trackWithId:(MWMTrackID)trackId {
  return [[MWMTrack alloc] initWithTrackId:trackId trackData:self.bm.GetTrack(trackId)];
}

- (MWMBookmarkGroup *)categoryForBookmarkId:(MWMMarkID)bookmarkId {
  auto const groupId = self.bm.GetBookmark(bookmarkId)->GetGroupId();
  return [self categoryWithId:groupId];
}

- (MWMBookmarkGroup *)categoryForTrackId:(MWMTrackID)trackId {
  auto const groupId = self.bm.GetTrack(trackId)->GetGroupId();
  return [self categoryWithId:groupId];
}

- (NSString *)descriptionForBookmarkId:(MWMMarkID)bookmarkId {
  auto const description = self.bm.GetBookmark(bookmarkId)->GetDescription();
  return [NSString stringWithUTF8String:description.c_str()];
}

- (NSArray<MWMBookmark *> *)bookmarksForGroup:(MWMMarkGroupID)groupId {
  auto const &bookmarkIds = self.bm.GetUserMarkIds(groupId);
  NSMutableArray *result = [NSMutableArray array];
  for (auto bookmarkId : bookmarkIds) {
    [result addObject:[[MWMBookmark alloc] initWithMarkId:bookmarkId bookmarkData:self.bm.GetBookmark(bookmarkId)]];
  }
  return [result copy];
}

- (void)searchBookmarksGroup:(MWMMarkGroupID)groupId
                        text:(NSString *)text
                  completion:(SearchBookmarksCompletionBlock)completion
{
  auto const searchId = ++self.lastSearchId;
  __weak auto weakSelf = self;

  using search::BookmarksSearchParams;
  BookmarksSearchParams params{
    text.UTF8String,
    groupId,
    // m_onResults
    [weakSelf, searchId, completion](BookmarksSearchParams::Results results, BookmarksSearchParams::Status status)
    {
      __strong auto self = weakSelf;
      if (!self || searchId != self.lastSearchId)
        return;

      self.bm.FilterInvalidBookmarks(results);

      NSMutableArray *result = [NSMutableArray array];
      for (auto bookmarkId : results)
        [result addObject:[[MWMBookmark alloc] initWithMarkId:bookmarkId bookmarkData:self.bm.GetBookmark(bookmarkId)]];

      completion(result);
    }
  };

  GetFramework().GetSearchAPI().SearchInBookmarks(std::move(params));
}


#pragma mark - Tracks

- (MWMTrackIDCollection)trackIdsForCategory:(MWMMarkGroupID)categoryId {
  auto const & trackIds = self.bm.GetTrackIds(categoryId);
  NSMutableArray<NSNumber *> * collection = [[NSMutableArray alloc] initWithCapacity:trackIds.size()];

  for (auto trackId : trackIds)
    [collection addObject:@(trackId)];
  return collection;
}

- (NSArray<MWMTrack *> *)tracksForGroup:(MWMMarkGroupID)groupId {
  auto const & trackIds = self.bm.GetTrackIds(groupId);
  NSMutableArray * result = [[NSMutableArray alloc] initWithCapacity:trackIds.size()];

  for (auto trackId : trackIds)
    [result addObject:[[MWMTrack alloc] initWithTrackId:trackId trackData:self.bm.GetTrack(trackId)]];
  return result;
}

- (NSArray<MWMBookmarkGroup *> *)collectionsForGroup:(MWMMarkGroupID)groupId {
  auto const & collectionIds = self.bm.GetChildrenCollections(groupId);
  NSMutableArray * result = [[NSMutableArray alloc] initWithCapacity:collectionIds.size()];

  for (auto collectionId : collectionIds)
    [result addObject:[[MWMBookmarkGroup alloc] initWithCategoryId:collectionId bookmarksManager:self]];
  return result;
}

- (NSArray<MWMBookmarkGroup *> *)categoriesForGroup:(MWMMarkGroupID)groupId {
  auto const & categoryIds = self.bm.GetChildrenCategories(groupId);
  NSMutableArray * result = [[NSMutableArray alloc] initWithCapacity:categoryIds.size()];

  for (auto categoryId : categoryIds)
    [result addObject:[[MWMBookmarkGroup alloc] initWithCategoryId:categoryId bookmarksManager:self]];
  return result;
}

#pragma mark - Category sharing

- (void)shareCategory:(MWMMarkGroupID)groupId fileType:(MWMKmlFileType)fileType completion:(SharingResultCompletionHandler)completion {
  self.bm.PrepareFileForSharing({groupId}, [self, completion](auto sharingResult) {
    [self handleSharingResult:sharingResult completion:completion];
  }, convertFileTypeToCore(fileType));
}

- (void)shareAllCategoriesWithCompletion:(SharingResultCompletionHandler)completion {
  self.bm.PrepareAllFilesForSharing([self, completion](auto sharingResult) {
    [self handleSharingResult:sharingResult completion:completion];
  });
}

- (void)handleSharingResult:(BookmarkManager::SharingResult)sharingResult completion:(SharingResultCompletionHandler)completion  {
  NSURL *urlToALocalFile = nil;
  MWMBookmarksShareStatus status;
  switch (sharingResult.m_code) {
    case BookmarkManager::SharingResult::Code::Success:
      urlToALocalFile = [NSURL fileURLWithPath:@(sharingResult.m_sharingPath.c_str()) isDirectory:NO];
      ASSERT(urlToALocalFile, ("Invalid share category URL"));
      self.shareCategoryURL = urlToALocalFile;
      status = MWMBookmarksShareStatusSuccess;
      break;
    case BookmarkManager::SharingResult::Code::EmptyCategory:
      status = MWMBookmarksShareStatusEmptyCategory;
      break;
    case BookmarkManager::SharingResult::Code::ArchiveError:
      status = MWMBookmarksShareStatusArchiveError;
      break;
    case BookmarkManager::SharingResult::Code::FileError:
      status = MWMBookmarksShareStatusFileError;
      break;
  }
  completion(status, urlToALocalFile);
}

- (void)finishShareCategory {
  if (!self.shareCategoryURL)
    return;

  base::DeleteFileX(self.shareCategoryURL.path.UTF8String);
  self.shareCategoryURL = nil;
}

#pragma mark - Notifications

- (void)setNotificationsEnabled:(BOOL)enabled {
  self.bm.SetNotificationsEnabled(enabled);
}

- (BOOL)areNotificationsEnabled {
  return self.bm.AreNotificationsEnabled();
}

#pragma mark - Catalog

- (NSArray<MWMBookmarkGroup *> *)sortedUserCategories {
  auto const & list = self.bm.GetSortedBmGroupIdList();
  NSMutableArray<MWMBookmarkGroup *> * result = [[NSMutableArray alloc] initWithCapacity:list.size()];

  for (auto const & groupId : list)
    [result addObject:[self categoryWithId:groupId]];
  return result;
}

- (MWMBookmarkGroup *)categoryWithId:(MWMMarkGroupID)groupId {
  return [[MWMBookmarkGroup alloc] initWithCategoryId:groupId bookmarksManager:self];
}

- (size_t)userCategoriesCount {
  return self.bm.GetBmGroupsCount();
}

- (void)updateBookmark:(MWMMarkID)bookmarkId
            setGroupId:(MWMMarkGroupID)groupId
                 title:(NSString *)title
                 color:(MWMBookmarkColor)color
           description:(NSString *)description {
  ASSERT_NOT_EQUAL(groupId, kml::kInvalidMarkGroupId, ());
  auto const currentGroupId = self.bm.GetBookmark(bookmarkId)->GetGroupId();
  auto editSession = self.bm.GetEditSession();
  if (currentGroupId != groupId)
    editSession.MoveBookmark(bookmarkId, currentGroupId, groupId);

  auto bookmark = editSession.GetBookmarkForEdit(bookmarkId);
  ASSERT(bookmark, ("Invalid bookmark id:", bookmarkId));

  auto kmlColor = kmlColorFromBookmarkColor(color);
  if (kmlColor != bookmark->GetColor())
    self.bm.SetLastEditedBmColor(kmlColor);

  bookmark->SetColor(kmlColor);
  bookmark->SetDescription(description.UTF8String);
  if (title.UTF8String != bookmark->GetPreferredName())
    bookmark->SetCustomName(title.UTF8String);
}

- (void)updateBookmark:(MWMMarkID)bookmarkId setColor:(MWMBookmarkColor)color {
  auto editSession = self.bm.GetEditSession();

  auto bookmark = editSession.GetBookmarkForEdit(bookmarkId);
  ASSERT(bookmark, ("Invalid bookmark id:", bookmarkId));

  auto kmlColor = kmlColorFromBookmarkColor(color);
  if (kmlColor != bookmark->GetColor())
    self.bm.SetLastEditedBmColor(kmlColor);

  bookmark->SetColor(kmlColor);
}

- (void)moveBookmark:(MWMMarkID)bookmarkId toGroupId:(MWMMarkGroupID)groupId {
  ASSERT_NOT_EQUAL(groupId, kml::kInvalidMarkGroupId, ());
  auto const currentGroupId = self.bm.GetBookmark(bookmarkId)->GetGroupId();
  if (currentGroupId != groupId) {
    auto editSession = self.bm.GetEditSession();
    editSession.MoveBookmark(bookmarkId, currentGroupId, groupId);
  }
}

- (void)updateTrack:(MWMTrackID)trackId
         setGroupId:(MWMMarkGroupID)groupId
              color:(UIColor *)color
              title:(NSString *)title {
  ASSERT_NOT_EQUAL(groupId, kml::kInvalidMarkGroupId, ());
  auto const currentGroupId = self.bm.GetTrack(trackId)->GetGroupId();
  auto editSession = self.bm.GetEditSession();
  if (currentGroupId != groupId)
    editSession.MoveTrack(trackId, currentGroupId, groupId);

  auto track = editSession.GetTrackForEdit(trackId);
  ASSERT(track, ("Invalid track id:", trackId));

  auto const currentColor = track->GetColor(0);
  auto const newColor = [MWMBookmarksManager getColorFromUIColor:color];

  if (newColor != currentColor)
    track->SetColor(newColor);

  track->SetName(title.UTF8String);
}

- (void)updateTrack:(MWMTrackID)trackId setColor:(UIColor *)color {
  auto editSession = self.bm.GetEditSession();

  auto track = editSession.GetTrackForEdit(trackId);
  ASSERT(track, ("Invalid track id:", trackId));

  auto const currentColor = track->GetColor(0);
  auto const newColor = [MWMBookmarksManager getColorFromUIColor:color];

  if (newColor != currentColor)
    track->SetColor(newColor);
}

- (void)moveTrack:(MWMTrackID)trackId toGroupId:(MWMMarkGroupID)groupId {
  ASSERT_NOT_EQUAL(groupId, kml::kInvalidMarkGroupId, ());
  auto const currentGroupId = self.bm.GetTrack(trackId)->GetGroupId();
  if (currentGroupId != groupId) {
    auto editSession = self.bm.GetEditSession();
    editSession.MoveTrack(trackId, currentGroupId, groupId);
  }
}

- (BOOL)hasRecentlyDeletedBookmark {
  return self.bm.HasRecentlyDeletedBookmark();
}

- (void)setCategory:(MWMMarkGroupID)groupId authorType:(MWMBookmarkGroupAuthorType)author
{
  switch (author)
  {
    case MWMBookmarkGroupAuthorTypeLocal:
      self.bm.GetEditSession().SetCategoryCustomProperty(groupId, @"author_type".UTF8String, @"local".UTF8String);
      break;
    case MWMBookmarkGroupAuthorTypeTraveler:
      self.bm.GetEditSession().SetCategoryCustomProperty(groupId, @"author_type".UTF8String, @"tourist".UTF8String);
  }
}

// MARK: - RecentlyDeletedCategoriesManager
- (uint64_t)recentlyDeletedCategoriesCount {
  return self.bm.GetRecentlyDeletedCategoriesCount();
}

- (NSArray<RecentlyDeletedCategory *> *)getRecentlyDeletedCategories {
  auto const categoriesCollection = self.bm.GetRecentlyDeletedCategories();
  NSMutableArray<RecentlyDeletedCategory *> * recentlyDeletedCategories = [[NSMutableArray alloc] initWithCapacity:categoriesCollection->size()];

  for (auto const & [filePath, categoryPtr] : * categoriesCollection) {
    ASSERT(categoryPtr, ("Recently deleted category shouldn't be nil."));
    RecentlyDeletedCategory * category = [[RecentlyDeletedCategory alloc] initWithCategoryData:categoryPtr->m_categoryData filePath:filePath];
    [recentlyDeletedCategories addObject:category];
  }
  return recentlyDeletedCategories;
}

- (void)deleteRecentlyDeletedCategoryAtURLs:(NSArray<NSURL *> *)urls {
  std::vector<std::string> filePaths;
  for (NSURL * url in urls)
    filePaths.push_back(url.filePathURL.path.UTF8String);
  self.bm.DeleteRecentlyDeletedCategoriesAtPaths(filePaths);
}

- (void)recoverRecentlyDeletedCategoriesAtURLs:(NSArray<NSURL *> *)urls {
  std::vector<std::string> filePaths;
  for (NSURL * url in urls)
    filePaths.push_back(url.filePathURL.path.UTF8String);
  self.bm.RecoverRecentlyDeletedCategoriesAtPaths(filePaths);
}

#pragma mark - Helpers

- (void)loopObservers:(void (^)(id<MWMBookmarksObserver> observer))block
{
  for (id<MWMBookmarksObserver> observer in [self.observers copy])
  {
    if (observer)
      block(observer);
  }
}

- (void)setElevationActivePoint:(CLLocationCoordinate2D)point distance:(double)distance trackId:(uint64_t)trackId {
  self.bm.SetElevationActivePoint(trackId, mercator::FromLatLon(point.latitude, point.longitude), distance);
}

- (void)setElevationActivePointChanged:(uint64_t)trackId callback:(ElevationPointChangedBlock)callback {
  __weak __typeof(self) ws = self;
  self.bm.SetElevationActivePointChangedCallback([callback, trackId, ws] () {
    callback(ws.bm.GetElevationActivePoint(trackId));
  });
}

- (void)resetElevationActivePointChanged {
  self.bm.SetElevationActivePointChangedCallback(nullptr);
}

- (void)setElevationMyPositionChanged:(uint64_t)trackId callback:(ElevationPointChangedBlock)callback {
  __weak __typeof(self) ws = self;
  self.bm.SetElevationMyPositionChangedCallback([callback, trackId, ws] () {
    callback(ws.bm.GetElevationMyPosition(trackId));
  });
}

- (void)resetElevationMyPositionChanged {
  self.bm.SetElevationMyPositionChangedCallback(nullptr);
}

+ (dp::Color)getColorFromUIColor:(UIColor *)color {
  CGFloat fRed, fGreen, fBlue, fAlpha;
  [color getRed:&fRed green:&fGreen blue:&fBlue alpha:&fAlpha];

  const uint8_t red = [self convertColorComponentToHex:fRed];
  const uint8_t green = [self convertColorComponentToHex:fGreen];
  const uint8_t blue = [self convertColorComponentToHex:fBlue];
  const uint8_t alpha = [self convertColorComponentToHex:fAlpha];

  return dp::Color(red, green, blue, alpha);
}

+ (uint8_t)convertColorComponentToHex:(CGFloat)color {
  ASSERT_LESS_OR_EQUAL(color, 1.f, ("Extended sRGB color space is not supported"));
  ASSERT_GREATER_OR_EQUAL(color, 0.f, ("Extended sRGB color space is not supported"));
  static constexpr uint8_t kMaxChannelValue = 255;
  return color * kMaxChannelValue;
}

@end
