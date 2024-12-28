#import <CoreLocation/CoreLocation.h>

#import "MWMTypes.h"

#import "MWMBookmarksObserver.h"
#import "PlacePageBookmarkData.h"

@class MWMBookmark;
@class MWMBookmarkGroup;
@class MWMBookmarksSection;
@class MWMCarPlayBookmarkObject;
@class MWMTrack;
@class RecentlyDeletedCategory;
@class UIColor;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, MWMBookmarksSortingType) {
  MWMBookmarksSortingTypeByType,
  MWMBookmarksSortingTypeByDistance,
  MWMBookmarksSortingTypeByTime,
  MWMBookmarksSortingTypeByName
} NS_SWIFT_NAME(BookmarksSortingType);

typedef void (^PingCompletionBlock)(BOOL success);
typedef void (^ElevationPointChangedBlock)(double distance);
typedef void (^SearchBookmarksCompletionBlock)(NSArray<MWMBookmark *> *bookmarks);
typedef void (^SortBookmarksCompletionBlock)(NSArray<MWMBookmarksSection *> * _Nullable sortedSections);
typedef void (^SharingResultCompletionHandler)(MWMBookmarksShareStatus status, NSURL * _Nullable urlToALocalFile);

@protocol RecentlyDeletedCategoriesManager <NSObject>
- (uint64_t)recentlyDeletedCategoriesCount;
- (NSArray<RecentlyDeletedCategory *> *)getRecentlyDeletedCategories;
- (void)deleteRecentlyDeletedCategoryAtURLs:(NSArray<NSURL *> *)urls;
- (void)recoverRecentlyDeletedCategoriesAtURLs:(NSArray<NSURL *> *)urls;
@end

NS_SWIFT_NAME(BookmarksManager)
@interface MWMBookmarksManager : NSObject<BookmarksObservable, RecentlyDeletedCategoriesManager>

+ (MWMBookmarksManager *)sharedManager;

- (BOOL)areBookmarksLoaded;
- (void)loadBookmarks;
- (void)loadBookmarkFile:(NSURL *)url;
- (void)reloadCategoryAtFilePath:(NSString *)filePath;
- (void)deleteCategoryAtFilePath:(NSString *)filePath;

- (BOOL)areAllCategoriesEmpty;
- (BOOL)isCategoryEmpty:(MWMMarkGroupID)groupId;
- (void)prepareForSearch:(MWMMarkGroupID)groupId;
- (NSString *)getCategoryName:(MWMMarkGroupID)groupId;
- (uint64_t)getCategoryMarksCount:(MWMMarkGroupID)groupId;
- (uint64_t)getCategoryTracksCount:(MWMMarkGroupID)groupId;
- (MWMBookmarkGroupAccessStatus)getCategoryAccessStatus:(MWMMarkGroupID)groupId;
- (NSString *)getCategoryAnnotation:(MWMMarkGroupID)groupId;
- (NSString *)getCategoryDescription:(MWMMarkGroupID)groupId;
- (NSString *)getCategoryAuthorName:(MWMMarkGroupID)groupId;
- (NSString *)getCategoryAuthorId:(MWMMarkGroupID)groupId;
- (MWMBookmarkGroupType)getCategoryGroupType:(MWMMarkGroupID)groupId;
- (nullable NSURL *)getCategoryImageUrl:(MWMMarkGroupID)groupId;
- (BOOL)hasExtraInfo:(MWMMarkGroupID)groupId;
- (BOOL)isHtmlDescription:(MWMMarkGroupID)groupId;

- (MWMMarkGroupID)createCategoryWithName:(NSString *)name;
- (void)setCategory:(MWMMarkGroupID)groupId name:(NSString *)name;
- (void)setCategory:(MWMMarkGroupID)groupId description:(NSString *)name;
- (BOOL)isCategoryVisible:(MWMMarkGroupID)groupId;
- (void)setCategory:(MWMMarkGroupID)groupId isVisible:(BOOL)isVisible;
- (void)setUserCategoriesVisible:(BOOL)isVisible;
- (void)deleteCategory:(MWMMarkGroupID)groupId;
- (BOOL)checkCategoryName:(NSString *)name;
- (BOOL)hasCategory:(MWMMarkGroupID)groupId;
- (BOOL)hasBookmark:(MWMMarkID)bookmarkId;
- (BOOL)hasTrack:(MWMTrackID)trackId;
- (NSArray<NSNumber *> *)availableSortingTypes:(MWMMarkGroupID)groupId hasMyPosition:(BOOL)hasMyPosition;
- (void)sortBookmarks:(MWMMarkGroupID)groupId
          sortingType:(MWMBookmarksSortingType)sortingType
             location:(CLLocation * _Nullable)location
           completion:(SortBookmarksCompletionBlock)completionBlock;
- (BOOL)hasLastSortingType:(MWMMarkGroupID)groupId;
- (MWMBookmarksSortingType)lastSortingType:(MWMMarkGroupID)groupId;
- (void)resetLastSortingType:(MWMMarkGroupID)groupId;

- (NSArray<MWMCarPlayBookmarkObject *> *)bookmarksForCategory:(MWMMarkGroupID)categoryId;
- (MWMMarkIDCollection)bookmarkIdsForCategory:(MWMMarkGroupID)categoryId;
- (void)deleteBookmark:(MWMMarkID)bookmarkId;
- (void)deleteTrack:(MWMTrackID)trackId;
- (MWMBookmark *)bookmarkWithId:(MWMMarkID)bookmarkId;
- (MWMTrack *)trackWithId:(MWMTrackID)trackId;
- (NSArray<MWMBookmark *> *)bookmarksForGroup:(MWMMarkGroupID)groupId;
- (NSArray<MWMTrack *> *)tracksForGroup:(MWMMarkGroupID)groupId;
- (NSArray<MWMBookmarkGroup *> *)collectionsForGroup:(MWMMarkGroupID)groupId;
- (NSArray<MWMBookmarkGroup *> *)categoriesForGroup:(MWMMarkGroupID)groupId;
- (void)searchBookmarksGroup:(MWMMarkGroupID)groupId
                        text:(NSString *)text
                  completion:(SearchBookmarksCompletionBlock)completion;

- (MWMTrackIDCollection)trackIdsForCategory:(MWMMarkGroupID)categoryId;

/**
 Shares a specific category with the given group ID.

 @param groupId The identifier for the category to be shared.
 @param fileType Text/Binary/GPX
 @param completion A block that handles the result of the share operation and takes two parameters:
                   - status: The status of the share operation, of type `MWMBookmarksShareStatus`.
                   - urlToALocalFile: The local file URL containing the shared data. This parameter is guaranteed to be non-nil only if `status` is `MWMBookmarksShareStatusSuccess`. In other cases, it will be nil.
*/
- (void)shareCategory:(MWMMarkGroupID)groupId fileType:(MWMKmlFileType)fileType completion:(SharingResultCompletionHandler)completion;
/**
 Shares all categories.

 @param completion A block that handles the result of the share operation and takes two parameters:
                   - status: The status of the share operation, of type `MWMBookmarksShareStatus`.
                   - urlToALocalFile: The local file URL containing the shared data. This parameter is guaranteed to be non-nil only if `status` is `MWMBookmarksShareStatusSuccess`.  In other cases, it will be nil.
*/
- (void)shareAllCategoriesWithCompletion:(SharingResultCompletionHandler)completion;
- (void)finishShareCategory;

- (void)setNotificationsEnabled:(BOOL)enabled;
- (BOOL)areNotificationsEnabled;

- (NSArray<MWMBookmarkGroup *> *)sortedUserCategories;
- (size_t)userCategoriesCount;
- (MWMBookmarkGroup *)categoryWithId:(MWMMarkGroupID)groupId;
- (MWMBookmarkGroup *)categoryForBookmarkId:(MWMMarkID)bookmarkId;
- (MWMBookmarkGroup *)categoryForTrackId:(MWMTrackID)trackId;
- (NSString *)descriptionForBookmarkId:(MWMMarkID)bookmarkId;
- (void)updateBookmark:(MWMMarkID)bookmarkId
            setGroupId:(MWMMarkGroupID)groupId
                 title:(NSString *)title
                 color:(MWMBookmarkColor)color
           description:(NSString *)description;

- (void)updateBookmark:(MWMMarkID)bookmarkId
              setColor:(MWMBookmarkColor)color;

- (void)moveBookmark:(MWMMarkID)bookmarkId
           toGroupId:(MWMMarkGroupID)groupId;

- (void)updateTrack:(MWMTrackID)trackId
         setGroupId:(MWMMarkGroupID)groupId
              color:(UIColor *)color
              title:(NSString *)title;

- (void)updateTrack:(MWMTrackID)trackId
           setColor:(UIColor *)color;

- (void)moveTrack:(MWMTrackID)trackId
        toGroupId:(MWMMarkGroupID)groupId;

- (BOOL)hasRecentlyDeletedBookmark;

- (instancetype)init __attribute__((unavailable("call +manager instead")));
- (instancetype)copy __attribute__((unavailable("call +manager instead")));
- (instancetype)copyWithZone:(NSZone *)zone __attribute__((unavailable("call +manager instead")));
+ (instancetype)allocWithZone:(struct _NSZone *)zone __attribute__((unavailable("call +manager instead")));
+ (instancetype) new __attribute__((unavailable("call +manager instead")));

- (void)setElevationActivePoint:(CLLocationCoordinate2D)point distance:(double)distance trackId:(uint64_t)trackId;
- (void)setElevationActivePointChanged:(uint64_t)trackId callback:(ElevationPointChangedBlock)callback;
- (void)resetElevationActivePointChanged;
- (void)setElevationMyPositionChanged:(uint64_t)trackId callback:(ElevationPointChangedBlock)callback;
- (void)resetElevationMyPositionChanged;

@end
NS_ASSUME_NONNULL_END
