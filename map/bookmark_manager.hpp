#pragma once

#include "map/bookmark.hpp"
#include "map/bookmark_helpers.hpp"
#include "map/elevation_info.hpp"
#include "map/track.hpp"
#include "map/user_mark_layer.hpp"

#include "search/region_address_getter.hpp"

#include "drape_frontend/drape_engine_safe_ptr.hpp"

#include "platform/safe_callback.hpp"

#include "geometry/any_rect2d.hpp"
#include "geometry/screenbase.hpp"

#include "base/macros.hpp"
#include "base/strings_bundle.hpp"
#include "base/thread_checker.hpp"
#include "base/visitor.hpp"

#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>


namespace storage
{
class CountryInfoGetter;
}  // namespace storage

class DataSource;
class SearchAPI;

class BookmarkManager final
{
  using UserMarkLayers = std::vector<std::unique_ptr<UserMarkLayer>>;
  using CategoriesCollection = std::map<kml::MarkGroupId, std::unique_ptr<BookmarkCategory>>;
  using MarksCollection = std::map<kml::MarkId, std::unique_ptr<UserMark>>;
  using BookmarksCollection = std::map<kml::MarkId, std::unique_ptr<Bookmark>>;
  using TracksCollection = std::map<kml::TrackId, std::unique_ptr<Track>>;

public:
  using KMLDataCollection = std::vector<std::pair<std::string, std::unique_ptr<kml::FileData>>>;
  using KMLDataCollectionPtr = std::shared_ptr<KMLDataCollection>;

  using BookmarksChangedCallback = std::function<void()>;
  using CategoriesChangedCallback = std::function<void()>;
  using ElevationActivePointChangedCallback = std::function<void()>;
  using ElevationMyPositionChangedCallback = std::function<void()>;

  using OnSymbolSizesAcquiredCallback = std::function<void()>;

  using AsyncLoadingStartedCallback = std::function<void()>;
  using AsyncLoadingFinishedCallback = std::function<void()>;
  using AsyncLoadingFileCallback = std::function<void(std::string const &, bool)>;

  struct AsyncLoadingCallbacks
  {
    AsyncLoadingStartedCallback m_onStarted;
    AsyncLoadingFinishedCallback m_onFinished;
    AsyncLoadingFileCallback m_onFileError;
    AsyncLoadingFileCallback m_onFileSuccess;
  };

  struct Callbacks
  {
    using GetStringsBundleFn = std::function<StringsBundle const &()>;
    using GetSeacrhAPIFn = std::function<SearchAPI &()>;
    using CreatedBookmarksCallback = std::function<void(std::vector<BookmarkInfo> const &)>;
    using UpdatedBookmarksCallback = std::function<void(std::vector<BookmarkInfo> const &)>;
    using DeletedBookmarksCallback = std::function<void(std::vector<kml::MarkId> const &)>;
    using AttachedBookmarksCallback = std::function<void(std::vector<BookmarkGroupInfo> const &)>;
    using DetachedBookmarksCallback = std::function<void(std::vector<BookmarkGroupInfo> const &)>;

    template <typename StringsBundleProvider, typename SearchAPIProvider,
              typename CreateListener, typename UpdateListener,
              typename DeleteListener, typename AttachListener, typename DetachListener>
    Callbacks(StringsBundleProvider && stringsBundleProvider,
              SearchAPIProvider && searchAPIProvider,
              CreateListener && createListener, UpdateListener && updateListener,
              DeleteListener && deleteListener, AttachListener && attachListener,
              DetachListener && detachListener)
      : m_getStringsBundle(std::forward<StringsBundleProvider>(stringsBundleProvider))
      , m_getSearchAPI(std::forward<SearchAPIProvider>(searchAPIProvider))
      , m_createdBookmarksCallback(std::forward<CreateListener>(createListener))
      , m_updatedBookmarksCallback(std::forward<UpdateListener>(updateListener))
      , m_deletedBookmarksCallback(std::forward<DeleteListener>(deleteListener))
      , m_attachedBookmarksCallback(std::forward<AttachListener>(attachListener))
      , m_detachedBookmarksCallback(std::forward<DetachListener>(detachListener))
    {}

    GetStringsBundleFn m_getStringsBundle;
    GetSeacrhAPIFn m_getSearchAPI;
    CreatedBookmarksCallback m_createdBookmarksCallback;
    UpdatedBookmarksCallback m_updatedBookmarksCallback;
    DeletedBookmarksCallback m_deletedBookmarksCallback;
    AttachedBookmarksCallback m_attachedBookmarksCallback;
    DetachedBookmarksCallback m_detachedBookmarksCallback;
  };

  class EditSession
  {
  public:
    explicit EditSession(BookmarkManager & bmManager);
    ~EditSession();

    template <typename UserMarkT>
    UserMarkT * CreateUserMark(m2::PointD const & ptOrg)
    {
      return m_bmManager.CreateUserMark<UserMarkT>(ptOrg);
    }

    Bookmark * CreateBookmark(kml::BookmarkData && bmData);
    Bookmark * CreateBookmark(kml::BookmarkData && bmData, kml::MarkGroupId groupId);
    Track * CreateTrack(kml::TrackData && trackData);

    template <typename UserMarkT>
    UserMarkT * GetMarkForEdit(kml::MarkId markId)
    {
      return m_bmManager.GetMarkForEdit<UserMarkT>(markId);
    }

    Bookmark * GetBookmarkForEdit(kml::MarkId bmId);

    template <typename UserMarkT, typename F>
    void DeleteUserMarks(UserMark::Type type, F && deletePredicate)
    {
      return m_bmManager.DeleteUserMarks<UserMarkT>(type, std::forward<F>(deletePredicate));
    }

    void DeleteUserMark(kml::MarkId markId);
    void DeleteBookmark(kml::MarkId bmId);
    void DeleteTrack(kml::TrackId trackId);

    void ClearGroup(kml::MarkGroupId groupId);

    void SetIsVisible(kml::MarkGroupId groupId, bool visible);

    void MoveBookmark(kml::MarkId bmID, kml::MarkGroupId curGroupID, kml::MarkGroupId newGroupID);
    void UpdateBookmark(kml::MarkId bmId, kml::BookmarkData const & bm);

    void AttachBookmark(kml::MarkId bmId, kml::MarkGroupId groupId);
    void DetachBookmark(kml::MarkId bmId, kml::MarkGroupId groupId);

    Track * GetTrackForEdit(kml::TrackId trackId);

    void MoveTrack(kml::TrackId trackID, kml::MarkGroupId curGroupID, kml::MarkGroupId newGroupID);
    void AttachTrack(kml::TrackId trackId, kml::MarkGroupId groupId);
    void DetachTrack(kml::TrackId trackId, kml::MarkGroupId groupId);
    void ChangeTrackColor(kml::TrackId trackId, dp::Color color);
    void UpdateTrack(kml::TrackId trackId, kml::TrackData const & trackData);

    void SetCategoryName(kml::MarkGroupId categoryId, std::string const & name);
    void SetCategoryDescription(kml::MarkGroupId categoryId, std::string const & desc);
    void SetCategoryTags(kml::MarkGroupId categoryId, std::vector<std::string> const & tags);
    void SetCategoryAccessRules(kml::MarkGroupId categoryId, kml::AccessRules accessRules);
    void SetCategoryCustomProperty(kml::MarkGroupId categoryId, std::string const & key,
                                   std::string const & value);
    
    /// Removes the category from the list of categories and deletes the related file.
    /// @param permanently If true, the file will be removed from the disk. If false, the file will be marked as deleted and moved into a trash.
    bool DeleteBmCategory(kml::MarkGroupId groupId, bool permanently);
    void NotifyChanges();

  private:
    BookmarkManager & m_bmManager;
  };

  explicit BookmarkManager(Callbacks && callbacks);

  void SetDrapeEngine(ref_ptr<df::DrapeEngine> engine);

  void InitRegionAddressGetter(DataSource const & dataSource,
                               storage::CountryInfoGetter const & infoGetter);
  void ResetRegionAddressGetter();

  void SetBookmarksChangedCallback(BookmarksChangedCallback && callback);
  void SetCategoriesChangedCallback(CategoriesChangedCallback && callback);
  void SetAsyncLoadingCallbacks(AsyncLoadingCallbacks && callbacks);
  bool IsAsyncLoadingInProgress() const { return m_asyncLoadingInProgress; }

  bool AreSymbolSizesAcquired(OnSymbolSizesAcquiredCallback && callback);

  EditSession GetEditSession();

  void UpdateViewport(ScreenBase const & screen);
  void Teardown();

  static bool IsBookmarkCategory(kml::MarkGroupId groupId)
  {
    return groupId != kml::kInvalidMarkGroupId && groupId >= UserMark::USER_MARK_TYPES_COUNT_MAX;
  }
  static bool IsBookmark(kml::MarkId markId)
  {
    return markId != kml::kInvalidMarkId && UserMark::GetMarkType(markId) == UserMark::BOOKMARK;
  }
  static UserMark::Type GetGroupType(kml::MarkGroupId groupId)
  {
    return IsBookmarkCategory(groupId) ? UserMark::BOOKMARK : static_cast<UserMark::Type>(groupId);
  }

  template <typename UserMarkT>
  UserMarkT const * GetMark(kml::MarkId markId) const
  {
    auto * mark = GetUserMark(markId);
    ASSERT(dynamic_cast<UserMarkT const *>(mark) != nullptr, ());
    return static_cast<UserMarkT const *>(mark);
  }

  UserMark const * GetUserMark(kml::MarkId markId) const;
  Bookmark const * GetBookmark(kml::MarkId markId) const;
  Track const * GetTrack(kml::TrackId trackId) const;

  kml::MarkIdSet const & GetUserMarkIds(kml::MarkGroupId groupId) const;
  kml::TrackIdSet const & GetTrackIds(kml::MarkGroupId groupId) const;

  // Do not change the order.
  enum class SortingType
  {
    ByType,
    ByDistance,
    ByTime,
    ByName
  };

  struct SortedBlock
  {
    bool operator==(SortedBlock const & other) const;

    std::string m_blockName;
    kml::MarkIdCollection m_markIds;
    kml::MarkIdCollection m_trackIds;
  };
  using SortedBlocksCollection = std::vector<SortedBlock>;

  struct SortParams
  {
    enum class Status
    {
      Completed,
      Cancelled
    };

    using OnResults = std::function<void(SortedBlocksCollection && sortedBlocks, Status status)>;

    kml::MarkGroupId m_groupId = kml::kInvalidMarkGroupId;
    SortingType m_sortingType = SortingType::ByType;
    bool m_hasMyPosition = false;
    m2::PointD m_myPosition = {0.0, 0.0};
    OnResults m_onResults;
  };

  std::vector<SortingType> GetAvailableSortingTypes(kml::MarkGroupId groupId,
                                                    bool hasMyPosition) const;
  void GetSortedCategory(SortParams const & params);

  bool GetLastSortingType(kml::MarkGroupId groupId, SortingType & sortingType) const;
  void SetLastSortingType(kml::MarkGroupId groupId, SortingType sortingType);
  void ResetLastSortingType(kml::MarkGroupId groupId);

  void PrepareForSearch(kml::MarkGroupId groupId);

  bool IsVisible(kml::MarkGroupId groupId) const;

  kml::MarkGroupId CreateBookmarkCategory(kml::CategoryData && data, bool autoSave = true);
  kml::MarkGroupId CreateBookmarkCategory(std::string const & name, bool autoSave = true);
  void UpdateBookmarkCategory(kml::MarkGroupId & groupId, kml::CategoryData && data, bool autoSave);

  BookmarkCategory * CreateBookmarkCompilation(kml::CategoryData && data);

  std::string GetCategoryName(kml::MarkGroupId categoryId) const;
  std::string GetCategoryFileName(kml::MarkGroupId categoryId) const;
  kml::MarkGroupId GetCategoryByFileName(std::string const & fileName) const;
  m2::RectD GetCategoryRect(kml::MarkGroupId categoryId, bool addIconsSize) const;
  kml::CategoryData const & GetCategoryData(kml::MarkGroupId categoryId) const;

  kml::MarkGroupId GetCategoryId(std::string const & name) const;

  kml::GroupIdCollection const & GetUnsortedBmGroupsIdList() const { return m_unsortedBmGroupsIdList; }
  kml::GroupIdCollection GetSortedBmGroupIdList() const;
  size_t GetBmGroupsCount() const { return m_unsortedBmGroupsIdList.size(); };
  bool HasBmCategory(kml::MarkGroupId groupId) const;
  bool HasBookmark(kml::MarkId markId) const;
  bool HasTrack(kml::TrackId trackId) const;
  kml::MarkGroupId LastEditedBMCategory();
  kml::PredefinedColor LastEditedBMColor() const;

  void SetLastEditedBmCategory(kml::MarkGroupId groupId);
  void SetLastEditedBmColor(kml::PredefinedColor color);

  using TTouchRectHolder = std::function<m2::AnyRectD(UserMark::Type)>;
  using TFindOnlyVisibleChecker = std::function<bool(UserMark::Type)>;
  UserMark const * FindNearestUserMark(TTouchRectHolder const & holder,
                                       TFindOnlyVisibleChecker const & findOnlyVisible) const;
  UserMark const * FindNearestUserMark(m2::AnyRectD const & rect) const;
  UserMark const * FindMarkInRect(kml::MarkGroupId groupId, m2::AnyRectD const & rect, bool findOnlyVisible,
                                  double & d) const;

  /// Scans and loads all kml files with bookmarks.
  void LoadBookmarks();
  void LoadBookmark(std::string const & filePath, bool isTemporaryFile);
  void ReloadBookmark(std::string const & filePath);

  /// Uses the same file name from which was loaded, or
  /// creates unique file name on first save and uses it every time.
  void SaveBookmarks(kml::GroupIdCollection const & groupIdCollection);

  StaticMarkPoint & SelectionMark() { return *m_selectionMark; }
  StaticMarkPoint const & SelectionMark() const { return *m_selectionMark; }

  MyPositionMarkPoint & MyPositionMark() { return *m_myPositionMark; }
  MyPositionMarkPoint const & MyPositionMark() const { return *m_myPositionMark; }

  struct SharingResult
  {
    enum class Code
    {
      Success = 0,
      EmptyCategory,
      ArchiveError,
      FileError
    };

    SharingResult(kml::GroupIdCollection && categoriesIds, std::string && sharingPath, const std::string & mimeType)
      : m_categoriesIds(categoriesIds)
      , m_code(Code::Success)
      , m_sharingPath(std::move(sharingPath))
      , m_mimeType(mimeType)
    {}

    SharingResult(kml::GroupIdCollection && categoriesIds, Code code)
      : m_categoriesIds(std::move(categoriesIds))
      , m_code(code)
    {}

    SharingResult(kml::GroupIdCollection && categoriesIds, Code code, std::string && errorString)
      : m_categoriesIds(std::move(categoriesIds))
      , m_code(code)
      , m_errorString(std::move(errorString))
    {}

    kml::MarkIdCollection m_categoriesIds;
    Code m_code;
    std::string m_sharingPath;
    std::string m_mimeType;
    std::string m_errorString;
  };

  using SharingHandler = platform::SafeCallback<void(SharingResult const & result)>;
  void PrepareFileForSharing(kml::GroupIdCollection && categoriesIds, SharingHandler && handler, KmlFileType kmlFileType);
  void PrepareAllFilesForSharing(SharingHandler && handler);

  bool AreAllCategoriesEmpty() const;
  bool IsCategoryEmpty(kml::MarkGroupId categoryId) const;

  bool IsUsedCategoryName(std::string const & name) const;
  bool AreAllCategoriesVisible() const;
  bool AreAllCategoriesInvisible() const;
  void SetAllCategoriesVisibility(bool visible);
  bool AreAllCompilationsVisible(kml::MarkGroupId categoryId, kml::CompilationType compilationType) const;
  bool AreAllCompilationsInvisible(kml::MarkGroupId categoryId, kml::CompilationType compilationType) const;
  void SetChildCategoriesVisibility(kml::MarkGroupId categoryId, kml::CompilationType compilationType,
                                    bool visible);

  void SetNotificationsEnabled(bool enabled);
  bool AreNotificationsEnabled() const;

  void FilterInvalidBookmarks(kml::MarkIdCollection & bookmarks) const;
  void FilterInvalidTracks(kml::TrackIdCollection & tracks) const;

  void EnableTestMode(bool enable);
  bool SaveBookmarkCategory(kml::MarkGroupId groupId);
  bool SaveBookmarkCategory(kml::MarkGroupId groupId, Writer & writer, KmlFileType fileType) const;

  bool HasRecentlyDeletedBookmark() const { return m_recentlyDeletedBookmark.operator bool(); };
  void ResetRecentlyDeletedBookmark();
  
  size_t GetRecentlyDeletedCategoriesCount() const;
  BookmarkManager::KMLDataCollectionPtr GetRecentlyDeletedCategories();
  bool IsRecentlyDeletedCategory(std::string const & filePath) const;

  void RecoverRecentlyDeletedCategoriesAtPaths(std::vector<std::string> const & filePaths);
  void DeleteRecentlyDeletedCategoriesAtPaths(std::vector<std::string> const & filePaths);

  // Used for LoadBookmarks() and unit tests only. Does *not* update last modified time.
  void CreateCategories(KMLDataCollection && dataCollection, bool autoSave = false);

  static std::string GetTracksSortedBlockName();
  static std::string GetBookmarksSortedBlockName();
  static std::string GetOthersSortedBlockName();
  static std::string GetNearMeSortedBlockName();
  enum class SortedByTimeBlockType : uint32_t
  {
    WeekAgo,
    MonthAgo,
    MoreThanMonthAgo,
    MoreThanYearAgo,
    Others
  };
  static std::string GetSortedByTimeBlockName(SortedByTimeBlockType blockType);
  std::string GetLocalizedRegionAddress(m2::PointD const & pt);

  void SetElevationActivePoint(kml::TrackId const & trackId, m2::PointD pt, double distanceInMeters);
  // Returns distance from the start of the track to active point in meters.
  double GetElevationActivePoint(kml::TrackId const & trackId) const;

  void UpdateElevationMyPosition(kml::TrackId const & trackId);
  // Returns distance from the start of the track to my position in meters.
  // Returns negative value if my position is not on the track.
  double GetElevationMyPosition(kml::TrackId const & trackId) const;

  void SetElevationActivePointChangedCallback(ElevationActivePointChangedCallback const & cb);
  void SetElevationMyPositionChangedCallback(ElevationMyPositionChangedCallback const & cb);

  using TracksFilter = std::function<bool(Track const * track)>;
  Track::TrackSelectionInfo FindNearestTrack(
      m2::RectD const & touchRect, TracksFilter const & tracksFilter = nullptr) const;
  Track::TrackSelectionInfo GetTrackSelectionInfo(kml::TrackId const & trackId) const;

  void SetTrackSelectionInfo(Track::TrackSelectionInfo const & trackSelectionInfo, bool notifyListeners);
  void OnTrackSelected(kml::TrackId trackId);
  void OnTrackDeselected();

  kml::GroupIdCollection GetChildrenCategories(kml::MarkGroupId parentCategoryId) const;
  kml::GroupIdCollection GetChildrenCollections(kml::MarkGroupId parentCategoryId) const;

  bool IsCompilation(kml::MarkGroupId id) const;
  kml::CompilationType GetCompilationType(kml::MarkGroupId id) const;

  void SaveTrackRecording(std::string trackName);
  std::string GenerateTrackRecordingName() const;
  dp::Color GenerateTrackRecordingColor() const;

private:
  class MarksChangesTracker : public df::UserMarksProvider
  {
  public:
    explicit MarksChangesTracker(BookmarkManager * bmManager)
      : m_bmManager(bmManager)
    {
      CHECK(m_bmManager != nullptr, ());
    }

    void OnAddMark(kml::MarkId markId);
    void OnDeleteMark(kml::MarkId markId);
    void OnUpdateMark(kml::MarkId markId);

    void OnAddLine(kml::TrackId lineId);
    void OnDeleteLine(kml::TrackId lineId);
    void OnUpdateLine(kml::TrackId lineId);

    void OnAddGroup(kml::MarkGroupId groupId);
    void OnDeleteGroup(kml::MarkGroupId groupId);

    void OnAttachBookmark(kml::MarkId markId, kml::MarkGroupId catId);
    void OnDetachBookmark(kml::MarkId markId, kml::MarkGroupId catId);

    void AcceptDirtyItems();
    bool HasChanges() const;
    bool HasBookmarksChanges() const;
    bool HasCategoriesChanges() const;
    void ResetChanges();
    void AddChanges(MarksChangesTracker const & changes);

    using GroupMarkIdSet = std::map<kml::MarkGroupId, kml::MarkIdSet>;
    GroupMarkIdSet const & GetAttachedBookmarks() const { return m_attachedBookmarks; }
    GroupMarkIdSet const & GetDetachedBookmarks() const { return m_detachedBookmarks; }

    // UserMarksProvider
    kml::GroupIdSet GetAllGroupIds() const override;
    kml::GroupIdSet const & GetUpdatedGroupIds() const override { return m_updatedGroups; }
    kml::GroupIdSet const & GetRemovedGroupIds() const override { return m_removedGroups; }
    kml::MarkIdSet const & GetCreatedMarkIds() const override { return m_createdMarks; }
    kml::MarkIdSet const & GetRemovedMarkIds() const override { return m_removedMarks; }
    kml::MarkIdSet const & GetUpdatedMarkIds() const override { return m_updatedMarks; }
    kml::TrackIdSet const & GetCreatedLineIds() const override { return m_createdLines; }
    kml::TrackIdSet const & GetRemovedLineIds() const override { return m_removedLines; }
    kml::TrackIdSet const & GetUpdatedLineIds() const override { return m_updatedLines; }
    kml::GroupIdSet const & GetBecameVisibleGroupIds() const override { return m_becameVisibleGroups; }
    kml::GroupIdSet const & GetBecameInvisibleGroupIds() const override { return m_becameInvisibleGroups; }
    bool IsGroupVisible(kml::MarkGroupId groupId) const override;
    kml::MarkIdSet const & GetGroupPointIds(kml::MarkGroupId groupId) const override;
    kml::TrackIdSet const & GetGroupLineIds(kml::MarkGroupId groupId) const override;
    df::UserPointMark const * GetUserPointMark(kml::MarkId markId) const override;
    df::UserLineMark const * GetUserLineMark(kml::TrackId lineId) const override;

  private:
    void OnUpdateGroup(kml::MarkGroupId groupId);
    void OnBecomeVisibleGroup(kml::MarkGroupId groupId);
    void OnBecomeInvisibleGroup(kml::MarkGroupId groupId);

    static void InsertBookmark(kml::MarkId markId, kml::MarkGroupId catId,
                               GroupMarkIdSet & setToInsert, GroupMarkIdSet & setToErase);
    static bool HasBookmarkCategories(kml::GroupIdSet const & groupIds);

    void InferVisibility(BookmarkCategory * const group);

    BookmarkManager * m_bmManager;

    kml::MarkIdSet m_createdMarks;
    kml::MarkIdSet m_removedMarks;
    kml::MarkIdSet m_updatedMarks;

    kml::TrackIdSet m_createdLines;
    kml::TrackIdSet m_removedLines;
    kml::TrackIdSet m_updatedLines;

    kml::GroupIdSet m_createdGroups;
    kml::GroupIdSet m_removedGroups;

    kml::GroupIdSet m_updatedGroups;
    kml::GroupIdSet m_becameVisibleGroups;
    kml::GroupIdSet m_becameInvisibleGroups;

    GroupMarkIdSet m_attachedBookmarks;
    GroupMarkIdSet m_detachedBookmarks;
  };

  template <typename UserMarkT>
  UserMarkT * CreateUserMark(m2::PointD const & ptOrg)
  {
    CHECK_THREAD_CHECKER(m_threadChecker, ());
    auto mark = std::make_unique<UserMarkT>(ptOrg);
    auto * m = mark.get();
    auto const markId = m->GetId();
    auto const groupId = static_cast<kml::MarkGroupId>(m->GetMarkType());
    CHECK_EQUAL(m_userMarks.count(markId), 0, ());
    ASSERT_GREATER(groupId, 0, ());
    ASSERT_LESS(groupId - 1, m_userMarkLayers.size(), ());
    m_userMarks.emplace(markId, std::move(mark));
    m_changesTracker.OnAddMark(markId);
    m_userMarkLayers[static_cast<size_t>(groupId - 1)]->AttachUserMark(markId);
    return m;
  }

  template <typename UserMarkT>
  UserMarkT * GetMarkForEdit(kml::MarkId markId)
  {
    CHECK_THREAD_CHECKER(m_threadChecker, ());
    auto * mark = GetUserMarkForEdit(markId);
    ASSERT(dynamic_cast<UserMarkT *>(mark) != nullptr, ());
    return static_cast<UserMarkT *>(mark);
  }

  template <typename UserMarkT, typename F>
  void DeleteUserMarks(UserMark::Type type, F && deletePredicate)
  {
    CHECK_THREAD_CHECKER(m_threadChecker, ());
    std::list<kml::MarkId> marksToDelete;
    for (auto markId : GetUserMarkIds(type))
    {
      if (deletePredicate(GetMark<UserMarkT>(markId)))
        marksToDelete.push_back(markId);
    }
    // Delete after iterating to avoid iterators invalidation issues.
    for (auto markId : marksToDelete)
      DeleteUserMark(markId);
  }

  UserMark * GetUserMarkForEdit(kml::MarkId markId);
  void DeleteUserMark(kml::MarkId markId);

  Bookmark * CreateBookmark(kml::BookmarkData && bmData);
  Bookmark * CreateBookmark(kml::BookmarkData && bmData, kml::MarkGroupId groupId);

  Bookmark * GetBookmarkForEdit(kml::MarkId markId);
  void AttachBookmark(kml::MarkId bmId, kml::MarkGroupId groupId);
  void DetachBookmark(kml::MarkId bmId, kml::MarkGroupId groupId);
  void DeleteBookmark(kml::MarkId bmId);
  void DetachUserMark(kml::MarkId bmId, kml::MarkGroupId catId);
  void DeleteCompilations(kml::GroupIdCollection const & compilations);

  Track * CreateTrack(kml::TrackData && trackData);

  Track * GetTrackForEdit(kml::TrackId trackId);
  void AttachTrack(kml::TrackId trackId, kml::MarkGroupId groupId);
  void DetachTrack(kml::TrackId trackId, kml::MarkGroupId groupId);
  void DeleteTrack(kml::TrackId trackId);
  void MoveTrack(kml::TrackId trackID, kml::MarkGroupId curGroupID, kml::MarkGroupId newGroupID);
  void ChangeTrackColor(kml::TrackId trackId, dp::Color color);
  void UpdateTrack(kml::TrackId trackId, kml::TrackData const & trackData);

  void ClearGroup(kml::MarkGroupId groupId);
  void SetIsVisible(kml::MarkGroupId groupId, bool visible);

  void SetCategoryName(kml::MarkGroupId categoryId, std::string const & name);
  void SetCategoryDescription(kml::MarkGroupId categoryId, std::string const & desc);
  void SetCategoryTags(kml::MarkGroupId categoryId, std::vector<std::string> const & tags);
  void SetCategoryAccessRules(kml::MarkGroupId categoryId, kml::AccessRules accessRules);
  void SetCategoryCustomProperty(kml::MarkGroupId categoryId, std::string const & key, std::string const & value);
  bool DeleteBmCategory(kml::MarkGroupId groupId, bool permanently);
  void ClearCategories();

  void MoveBookmark(kml::MarkId bmID, kml::MarkGroupId curGroupID, kml::MarkGroupId newGroupID);
  void UpdateBookmark(kml::MarkId bmId, kml::BookmarkData const & bm);

  UserMark const * GetMark(kml::MarkId markId) const;

  UserMarkLayer * GetGroup(kml::MarkGroupId groupId) const;
  BookmarkCategory * GetBmCategorySafe(kml::MarkGroupId categoryId) const;
  BookmarkCategory * GetBmCategory(kml::MarkGroupId categoryId) const
  {
    auto * res = GetBmCategorySafe(categoryId);
    CHECK(res, (categoryId));
    return res;
  }

  Bookmark * AddBookmark(std::unique_ptr<Bookmark> && bookmark);
  Track * AddTrack(std::unique_ptr<Track> && track);

  void OnEditSessionOpened();
  void OnEditSessionClosed();
  void NotifyChanges(bool saveChangesOnDisk);

  void SaveState() const;
  void LoadState();

  void SaveMetadata();
  void LoadMetadata();
  void CleanupInvalidMetadata();
  std::string GetMetadataEntryName(kml::MarkGroupId groupId) const;

  void NotifyAboutStartAsyncLoading();
  void NotifyAboutFinishAsyncLoading(KMLDataCollectionPtr && collection);
  void NotifyAboutFile(bool success, std::string const & filePath, bool isTemporaryFile);
  void LoadBookmarkRoutine(std::string const & filePath, bool isTemporaryFile);
  void ReloadBookmarkRoutine(std::string const & filePath);

  using BookmarksChecker = std::function<bool(kml::FileData const &)>;
  KMLDataCollectionPtr LoadBookmarks(std::string const & dir, std::string_view ext,
                                     KmlFileType fileType, BookmarksChecker const & checker);

  void GetDirtyGroups(kml::GroupIdSet & dirtyGroups) const;
  void UpdateBmGroupIdList();

  void NotifyBookmarksChanged();
  void NotifyCategoriesChanged();

  void SendBookmarksChanges(MarksChangesTracker const & changesTracker);
  void GetBookmarksInfo(kml::MarkIdSet const & marks, std::vector<BookmarkInfo> & bookmarks) const;
  static void GetBookmarkGroupsInfo(MarksChangesTracker::GroupMarkIdSet const & groups,
                                    std::vector<BookmarkGroupInfo> & groupsInfo);

  kml::MarkGroupId CheckAndCreateDefaultCategory();
  void CheckAndResetLastIds();

  std::unique_ptr<kml::FileData> CollectBmGroupKMLData(BookmarkCategory const * group) const;
  KMLDataCollectionPtr PrepareToSaveBookmarks(kml::GroupIdCollection const & groupIdCollection);

  bool HasDuplicatedIds(kml::FileData const & fileData) const;
  template <typename UniquityChecker>
  void SetUniqueName(kml::CategoryData & data, UniquityChecker checker);
  bool CheckVisibility(bool isVisible) const;
  bool CheckCompilationsVisibility(kml::MarkGroupId categoryId, kml::CompilationType compilationType, bool isVisible) const;

  struct SortBookmarkData
  {
    SortBookmarkData(kml::BookmarkData const & bmData, search::ReverseGeocoder::RegionAddress const & address)
      : m_id(bmData.m_id)
      , m_name(GetPreferredBookmarkName(bmData))
      , m_point(bmData.m_point)
      , m_type(GetBookmarkBaseType(bmData.m_featureTypes))
      , m_timestamp(bmData.m_timestamp)
      , m_address(address)
    {}

    kml::MarkId m_id;
    std::string m_name;
    m2::PointD m_point;
    BookmarkBaseType m_type;
    kml::Timestamp m_timestamp;
    search::ReverseGeocoder::RegionAddress m_address;
  };

  struct SortTrackData
  {
    explicit SortTrackData(kml::TrackData const & trackData)
      : m_id(trackData.m_id)
      , m_name(GetPreferredBookmarkStr(trackData.m_name))
      , m_timestamp(trackData.m_timestamp)
    {}

    kml::TrackId m_id;
    std::string m_name;
    kml::Timestamp m_timestamp;
  };

  void GetSortedCategoryImpl(SortParams const & params,
                             std::vector<SortBookmarkData> const & bookmarksForSort,
                             std::vector<SortTrackData> const & tracksForSort,
                             SortedBlocksCollection & sortedBlocks);

  void SortByDistance(std::vector<SortBookmarkData> const & bookmarksForSort,
                      std::vector<SortTrackData> const & tracksForSort,
                      m2::PointD const & myPosition, SortedBlocksCollection & sortedBlocks);
  static void SortByTime(std::vector<SortBookmarkData> const & bookmarksForSort,
                         std::vector<SortTrackData> const & tracksForSort,
                         SortedBlocksCollection & sortedBlocks);
  static void SortByType(std::vector<SortBookmarkData> const & bookmarksForSort,
                         std::vector<SortTrackData> const & tracksForSort,
                         SortedBlocksCollection & sortedBlocks);
  static void SortByName(std::vector<SortBookmarkData> const & bookmarksForSort,
                         std::vector<SortTrackData> const & tracksForSort,
                         SortedBlocksCollection & sortedBlocks);

  using AddressesCollection = std::vector<std::pair<kml::MarkId, search::ReverseGeocoder::RegionAddress>>;
  void PrepareBookmarksAddresses(std::vector<SortBookmarkData> & bookmarksForSort, AddressesCollection & newAddresses);
  void FilterInvalidData(SortedBlocksCollection & sortedBlocks, AddressesCollection & newAddresses) const;
  void SetBookmarksAddresses(AddressesCollection const & addresses);
  static void AddTracksSortedBlock(std::vector<SortTrackData> const & sortedTracks, SortedBlocksCollection & sortedBlocks);
  static void SortTracksByTime(std::vector<SortTrackData> & tracks);
  static void SortTracksByName(std::vector<SortTrackData> & tracks);

  kml::MarkId GetTrackSelectionMarkId(kml::TrackId trackId) const;
  int GetTrackSelectionMarkMinZoom(kml::TrackId trackId) const;
  void SetTrackSelectionMark(kml::TrackId trackId, m2::PointD const & pt, double distance);
  void DeleteTrackSelectionMark(kml::TrackId trackId);
  void SetTrackInfoMark(kml::TrackId trackId, m2::PointD const & pt);
  void ResetTrackInfoMark(kml::TrackId trackId);

  void UpdateTrackMarksMinZoom();
  void UpdateTrackMarksVisibility(kml::MarkGroupId groupId);
  void RequestSymbolSizes();

  kml::GroupIdCollection GetCompilationOfType(kml::MarkGroupId parentId, kml::CompilationType type) const;

  ThreadChecker m_threadChecker;

  Callbacks m_callbacks;
  MarksChangesTracker m_changesTracker;
  MarksChangesTracker m_bookmarksChangesTracker;
  MarksChangesTracker m_drapeChangesTracker;
  df::DrapeEngineSafePtr m_drapeEngine;

  std::unique_ptr<search::RegionAddressGetter> m_regionAddressGetter;
  std::mutex m_regionAddressMutex;

  BookmarksChangedCallback m_bookmarksChangedCallback;
  CategoriesChangedCallback m_categoriesChangedCallback;
  ElevationActivePointChangedCallback m_elevationActivePointChanged;
  ElevationMyPositionChangedCallback m_elevationMyPositionChanged;
  m2::PointD m_lastElevationMyPosition = m2::PointD::Zero();

  OnSymbolSizesAcquiredCallback m_onSymbolSizesAcquiredFn;
  bool m_symbolSizesAcquired = false;

  AsyncLoadingCallbacks m_asyncLoadingCallbacks;
  std::atomic<bool> m_needTeardown;
  size_t m_openedEditSessionsCount = 0;
  bool m_loadBookmarksCalled = false;
  bool m_loadBookmarksFinished = false;
  bool m_firstDrapeNotification = false;
  bool m_notificationsEnabled = true;

  ScreenBase m_viewport;

  CategoriesCollection m_categories;
  kml::GroupIdCollection m_unsortedBmGroupsIdList;

  std::string m_lastCategoryUrl;
  kml::MarkGroupId m_lastEditedGroupId = kml::kInvalidMarkGroupId;
  kml::PredefinedColor m_lastColor = kml::PredefinedColor::Red;
  UserMarkLayers m_userMarkLayers;

  MarksCollection m_userMarks;
  BookmarksCollection m_bookmarks;
  TracksCollection m_tracks;

  StaticMarkPoint * m_selectionMark = nullptr;
  MyPositionMarkPoint * m_myPositionMark = nullptr;

  kml::MarkId m_trackInfoMarkId = kml::kInvalidMarkId;
  kml::TrackId m_selectedTrackId = kml::kInvalidTrackId;
  m2::PointF m_maxBookmarkSymbolSize;

  std::unique_ptr<Bookmark> m_recentlyDeletedBookmark;

  bool m_asyncLoadingInProgress = false;
  struct BookmarkLoaderInfo
  {
    BookmarkLoaderInfo(std::string const & filename, bool isTemporaryFile, bool isReloading)
      : m_filename(filename), m_isTemporaryFile(isTemporaryFile), m_isReloading(isReloading)
    {}

    std::string m_filename;
    bool m_isTemporaryFile = false;
    bool m_isReloading = false;
  };
  std::list<BookmarkLoaderInfo> m_bookmarkLoadingQueue;

  struct RestoringCache
  {
    std::string m_serverId;
    kml::AccessRules m_accessRules;
  };
  std::map<std::string, RestoringCache> m_restoringCache;

  struct ExpiredCategory
  {
    ExpiredCategory(kml::MarkGroupId id, std::string const & serverId)
      : m_id(id), m_serverId(serverId) {}

    kml::MarkGroupId m_id;
    std::string m_serverId;
  };
  std::vector<ExpiredCategory> m_expiredCategories;


  struct Properties
  {
    DECLARE_VISITOR_AND_DEBUG_PRINT(Properties, visitor(m_values, "values"))

    bool GetProperty(std::string const & propertyName, std::string & value) const;

    std::map<std::string, std::string> m_values;
  };

  struct Metadata
  {
    DECLARE_VISITOR_AND_DEBUG_PRINT(Metadata, visitor(m_entriesProperties, "entriesProperties"),
                                    visitor(m_commonProperties, "commonProperties"))

    bool GetEntryProperty(std::string const & entryName, std::string const & propertyName,
                          std::string & value) const;

    std::map<std::string, Properties> m_entriesProperties;
    Properties m_commonProperties;
  };

  Metadata m_metadata;

  bool m_testModeEnabled = false;

  CategoriesCollection m_compilations;

  DISALLOW_COPY_AND_MOVE(BookmarkManager);
};
