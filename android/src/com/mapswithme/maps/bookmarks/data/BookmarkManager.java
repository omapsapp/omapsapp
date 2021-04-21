package com.mapswithme.maps.bookmarks.data;

import androidx.annotation.IntDef;
import androidx.annotation.IntRange;
import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.mapswithme.maps.base.DataChangedListener;
import com.mapswithme.maps.base.Observable;
import com.mapswithme.util.KeyValue;
import com.mapswithme.util.UTM;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

@MainThread
public enum BookmarkManager
{
  INSTANCE;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ CLOUD_BACKUP, CLOUD_RESTORE })
  public @interface SynchronizationType {}

  public static final int CLOUD_BACKUP = 0;
  public static final int CLOUD_RESTORE = 1;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ CLOUD_SUCCESS, CLOUD_AUTH_ERROR, CLOUD_NETWORK_ERROR,
            CLOUD_DISK_ERROR, CLOUD_USER_INTERRUPTED, CLOUD_INVALID_CALL })
  public @interface SynchronizationResult {}

  public static final int CLOUD_SUCCESS = 0;
  public static final int CLOUD_AUTH_ERROR = 1;
  public static final int CLOUD_NETWORK_ERROR = 2;
  public static final int CLOUD_DISK_ERROR = 3;
  public static final int CLOUD_USER_INTERRUPTED = 4;
  public static final int CLOUD_INVALID_CALL = 5;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ CLOUD_BACKUP_EXISTS, CLOUD_NO_BACKUP, CLOUD_NOT_ENOUGH_DISK_SPACE })
  public @interface RestoringRequestResult {}

  public static final int CLOUD_BACKUP_EXISTS = 0;
  public static final int CLOUD_NO_BACKUP = 1;
  public static final int CLOUD_NOT_ENOUGH_DISK_SPACE = 2;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ SORT_BY_TYPE, SORT_BY_DISTANCE, SORT_BY_TIME })
  public @interface SortingType {}

  public static final int SORT_BY_TYPE = 0;
  public static final int SORT_BY_DISTANCE = 1;
  public static final int SORT_BY_TIME = 2;

  // These values have to match the values of kml::CompilationType from kml/types.hpp
  public static final int CATEGORY = 0;

  public static final List<Icon> ICONS = new ArrayList<>();

  @NonNull
  private final BookmarkCategoriesDataProvider mCategoriesCoreDataProvider
      = new CoreBookmarkCategoriesDataProvider();

  @NonNull
  private BookmarkCategoriesDataProvider mCurrentDataProvider = mCategoriesCoreDataProvider;

  private final BookmarkCategoriesCache mBookmarkCategoriesCache = new BookmarkCategoriesCache();

  @NonNull
  private final List<BookmarksLoadingListener> mListeners = new ArrayList<>();

  @NonNull
  private final List<BookmarksSortingListener> mSortingListeners = new ArrayList<>();

  @NonNull
  private final List<KmlConversionListener> mConversionListeners = new ArrayList<>();

  @NonNull
  private final List<BookmarksSharingListener> mSharingListeners = new ArrayList<>();

  @NonNull
  private final List<BookmarksCloudListener> mCloudListeners = new ArrayList<>();

  @NonNull
  private final List<BookmarksCatalogPingListener> mCatalogPingListeners = new ArrayList<>();

  @NonNull
  private final List<BookmarksExpiredCategoriesListener> mExpiredCategoriesListeners = new ArrayList<>();

  @Nullable
  private OnElevationCurrentPositionChangedListener mOnElevationCurrentPositionChangedListener;

  @Nullable
  private OnElevationActivePointChangedListener mOnElevationActivePointChangedListener;

  static
  {
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_RED, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_PINK, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_PURPLE, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_DEEPPURPLE, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_BLUE, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_LIGHTBLUE, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_CYAN, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_TEAL, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_GREEN, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_LIME, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_YELLOW, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_ORANGE, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_DEEPORANGE, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_BROWN, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_GRAY, Icon.BOOKMARK_ICON_TYPE_NONE));
    ICONS.add(new Icon(Icon.PREDEFINED_COLOR_BLUEGRAY, Icon.BOOKMARK_ICON_TYPE_NONE));
  }

  public void toggleCategoryVisibility(@NonNull BookmarkCategory category)
  {
    boolean isVisible = isVisible(category.getId());
    setVisibility(category.getId(), !isVisible);
  }

  @Nullable
  public Bookmark addNewBookmark(double lat, double lon)
  {
    final Bookmark bookmark = nativeAddBookmarkToLastEditedCategory(lat, lon);
    return bookmark;
  }

  public void addLoadingListener(@NonNull BookmarksLoadingListener listener)
  {
    mListeners.add(listener);
  }

  public void removeLoadingListener(@NonNull BookmarksLoadingListener listener)
  {
    mListeners.remove(listener);
  }

  public void addSortingListener(@NonNull BookmarksSortingListener listener)
  {
    mSortingListeners.add(listener);
  }

  public void removeSortingListener(@NonNull BookmarksSortingListener listener)
  {
    mSortingListeners.remove(listener);
  }

  public void addKmlConversionListener(@NonNull KmlConversionListener listener)
  {
    mConversionListeners.add(listener);
  }

  public void removeKmlConversionListener(@NonNull KmlConversionListener listener)
  {
    mConversionListeners.remove(listener);
  }

  public void addSharingListener(@NonNull BookmarksSharingListener listener)
  {
    mSharingListeners.add(listener);
  }

  public void removeSharingListener(@NonNull BookmarksSharingListener listener)
  {
    mSharingListeners.remove(listener);
  }

  public void addCloudListener(@NonNull BookmarksCloudListener listener)
  {
    mCloudListeners.add(listener);
  }

  public void removeCloudListener(@NonNull BookmarksCloudListener listener)
  {
    mCloudListeners.remove(listener);
  }

  public void setElevationActivePointChangedListener(
      @Nullable OnElevationActivePointChangedListener listener)
  {
    if (listener != null)
      nativeSetElevationActiveChangedListener();
    else
      nativeRemoveElevationActiveChangedListener();

    mOnElevationActivePointChangedListener = listener;
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onBookmarksChanged()
  {
    updateCache();
  }

  @SuppressWarnings("unused")
  @MainThread
  public void onBookmarksLoadingStarted()
  {
    for (BookmarksLoadingListener listener : mListeners)
      listener.onBookmarksLoadingStarted();
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onBookmarksLoadingFinished()
  {
    updateCache();
    mCurrentDataProvider = new CacheBookmarkCategoriesDataProvider();
    for (BookmarksLoadingListener listener : mListeners)
      listener.onBookmarksLoadingFinished();
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onBookmarksSortingCompleted(@NonNull SortedBlock[] sortedBlocks, long timestamp)
  {
    for (BookmarksSortingListener listener : mSortingListeners)
      listener.onBookmarksSortingCompleted(sortedBlocks, timestamp);
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onBookmarksSortingCancelled(long timestamp)
  {
    for (BookmarksSortingListener listener : mSortingListeners)
      listener.onBookmarksSortingCancelled(timestamp);
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onBookmarksFileLoaded(boolean success, @NonNull String fileName,
                                    boolean isTemporaryFile)
  {
    // Android could create temporary file with bookmarks in some cases (KML/KMZ file is a blob
    // in the intent, so we have to create a temporary file on the disk). Here we can delete it.
    if (isTemporaryFile)
    {
      File tmpFile = new File(fileName);
      tmpFile.delete();
    }

    for (BookmarksLoadingListener listener : mListeners)
      listener.onBookmarksFileLoaded(success);
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onFinishKmlConversion(boolean success)
  {
    for (KmlConversionListener listener : mConversionListeners)
      listener.onFinishKmlConversion(success);
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onPreparedFileForSharing(BookmarkSharingResult result)
  {
    for (BookmarksSharingListener listener : mSharingListeners)
      listener.onPreparedFileForSharing(result);
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onSynchronizationStarted(@SynchronizationType int type)
  {
    for (BookmarksCloudListener listener : mCloudListeners)
      listener.onSynchronizationStarted(type);
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onSynchronizationFinished(@SynchronizationType int type,
                                        @SynchronizationResult int result,
                                        @NonNull String errorString)
  {
    for (BookmarksCloudListener listener : mCloudListeners)
      listener.onSynchronizationFinished(type, result, errorString);
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onRestoreRequested(@RestoringRequestResult int result, @NonNull String deviceName,
                                 long backupTimestampInMs)
  {
    for (BookmarksCloudListener listener : mCloudListeners)
      listener.onRestoreRequested(result, deviceName, backupTimestampInMs);
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onRestoredFilesPrepared()
  {
    for (BookmarksCloudListener listener : mCloudListeners)
      listener.onRestoredFilesPrepared();
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onElevationCurrentPositionChanged()
  {
    if (mOnElevationCurrentPositionChangedListener != null)
      mOnElevationCurrentPositionChangedListener.onCurrentPositionChanged();
  }

  public void setElevationCurrentPositionChangedListener(@Nullable OnElevationCurrentPositionChangedListener listener)
  {
    if (listener != null)
      nativeSetElevationCurrentPositionChangedListener();
    else
      nativeRemoveElevationCurrentPositionChangedListener();

    mOnElevationCurrentPositionChangedListener = listener;
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @MainThread
  public void onElevationActivePointChanged()
  {
    if (mOnElevationActivePointChangedListener != null)
      mOnElevationActivePointChangedListener.onElevationActivePointChanged();
  }

  public boolean isVisible(long catId)
  {
    return nativeIsVisible(catId);
  }

  public void setVisibility(long catId, boolean visible)
  {
    nativeSetVisibility(catId, visible);
  }

  public void setCategoryName(long catId, @NonNull String name)
  {
    nativeSetCategoryName(catId, name);
  }

  public void setCategoryDescription(long id, @NonNull String categoryDesc)
  {
    nativeSetCategoryDescription(id, categoryDesc);
  }

  @Nullable
  public Bookmark updateBookmarkPlacePage(long bmkId)
  {
    return nativeUpdateBookmarkPlacePage(bmkId);
  }

  @Nullable
  public BookmarkInfo getBookmarkInfo(long bmkId)
  {
    return nativeGetBookmarkInfo(bmkId);
  }

  public long getBookmarkIdByPosition(long catId, int positionInCategory)
  {
    return nativeGetBookmarkIdByPosition(catId, positionInCategory);
  }

  @NonNull
  public Track getTrack(long trackId)
  {
    return nativeGetTrack(trackId, Track.class);
  }

  public long getTrackIdByPosition(long catId, int positionInCategory)
  {
    return nativeGetTrackIdByPosition(catId, positionInCategory);
  }

  public static void loadBookmarks() { nativeLoadBookmarks(); }

  public void deleteCategory(long catId) { nativeDeleteCategory(catId); }

  public void deleteTrack(long trackId)
  {
    nativeDeleteTrack(trackId);
  }

  public void deleteBookmark(long bmkId)
  {
    nativeDeleteBookmark(bmkId);
  }

  public long createCategory(@NonNull String name) { return nativeCreateCategory(name); }

  public void showBookmarkOnMap(long bmkId) { nativeShowBookmarkOnMap(bmkId); }

  public void showBookmarkCategoryOnMap(long catId) { nativeShowBookmarkCategoryOnMap(catId); }

  public long getLastEditedCategory() { return nativeGetLastEditedCategory(); }

  @Icon.PredefinedColor
  public int getLastEditedColor() { return nativeGetLastEditedColor(); }

  public void loadKmzFile(@NonNull String path, boolean isTemporaryFile)
  {
    nativeLoadKmzFile(path, isTemporaryFile);
  }

  public boolean isAsyncBookmarksLoadingInProgress()
  {
    return nativeIsAsyncBookmarksLoadingInProgress();
  }

  @NonNull
  public List<BookmarkCategory> getCategories()
  {
    return mCurrentDataProvider.getCategories();
  }

  @NonNull
  BookmarkCategoriesCache getBookmarkCategoriesCache()
  {
    return mBookmarkCategoriesCache;
  }

  private void updateCache()
  {
    getBookmarkCategoriesCache().update(mCategoriesCoreDataProvider.getCategories());
  }

  public void addCategoriesUpdatesListener(@NonNull DataChangedListener listener)
  {
    getBookmarkCategoriesCache().registerListener(listener);
  }

  public void removeCategoriesUpdatesListener(@NonNull DataChangedListener listener)
  {
    getBookmarkCategoriesCache().unregisterListener(listener);
  }

  @NonNull
  public BookmarkCategory getCategoryById(long categoryId)
  {
    return mCurrentDataProvider.getCategoryById(categoryId);
  }

  public boolean isUsedCategoryName(@NonNull String name)
  {
    return nativeIsUsedCategoryName(name);
  }

  public boolean isEditableBookmark(long bmkId) { return nativeIsEditableBookmark(bmkId); }

  public boolean isSearchAllowed(long catId) { return nativeIsSearchAllowed(catId); }

  public void prepareForSearch(long catId) { nativePrepareForSearch(catId); }

  public boolean areAllCategoriesVisible()
  {
    return nativeAreAllCategoriesVisible();
  }

  public boolean areAllCategoriesInvisible()
  {
    return nativeAreAllCategoriesInvisible();
  }

  public void setAllCategoriesVisibility(boolean visible)
  {
    nativeSetAllCategoriesVisibility(visible);
  }

  public void setChildCategoriesVisibility(long catId, boolean visible)
  {
    nativeSetChildCategoriesVisibility(catId, visible);
  }

  public int getKmlFilesCountForConversion()
  {
    return nativeGetKmlFilesCountForConversion();
  }

  public void convertAllKmlFiles()
  {
    nativeConvertAllKmlFiles();
  }

  public void prepareCategoryForSharing(long catId)
  {
    nativePrepareFileForSharing(catId);
  }

  public void setNotificationsEnabled(boolean enabled)
  {
    nativeSetNotificationsEnabled(enabled);
  }

  public boolean areNotificationsEnabled()
  {
    return nativeAreNotificationsEnabled();
  }

  @NonNull
  public String getCatalogFrontendUrl(@UTM.UTMType int utm)
  {
    return nativeGetCatalogFrontendUrl(utm);
  }

  public void requestRouteTags()
  {
    nativeRequestCatalogTags();
  }

  public boolean hasLastSortingType(long catId) { return nativeHasLastSortingType(catId); }

  @SortingType
  public int getLastSortingType(long catId) { return nativeGetLastSortingType(catId); }

  public void setLastSortingType(long catId, @SortingType int sortingType)
  {
    nativeSetLastSortingType(catId, sortingType);
  }

  public void resetLastSortingType(long catId) { nativeResetLastSortingType(catId); }

  @NonNull
  @SortingType
  public int[] getAvailableSortingTypes(long catId, boolean hasMyPosition)
  {
    return nativeGetAvailableSortingTypes(catId, hasMyPosition);
  }

  public void getSortedCategory(long catId, @SortingType int sortingType,
                                boolean hasMyPosition, double lat, double lon,
                                long timestamp)
  {
    nativeGetSortedCategory(catId, sortingType, hasMyPosition, lat, lon, timestamp);
  }

  @NonNull
  public List<BookmarkCategory> getChildrenCategories(long catId)
  {
    return mCurrentDataProvider.getChildrenCategories(catId);
  }

  @NonNull
  native BookmarkCategory nativeGetBookmarkCategory(long catId);
  @NonNull
  native BookmarkCategory[] nativeGetBookmarkCategories();
  @NonNull
  native BookmarkCategory[] nativeGetChildrenCategories(long catId);

  @NonNull
  public String getBookmarkName(@IntRange(from = 0) long bookmarkId)
  {
    return nativeGetBookmarkName(bookmarkId);
  }

  @NonNull
  public String getBookmarkFeatureType(@IntRange(from = 0) long bookmarkId)
  {
    return nativeGetBookmarkFeatureType(bookmarkId);
  }

  @NonNull
  public ParcelablePointD getBookmarkXY(@IntRange(from = 0) long bookmarkId)
  {
    return nativeGetBookmarkXY(bookmarkId);
  }

  @Icon.PredefinedColor
  public int getBookmarkColor(@IntRange(from = 0) long bookmarkId)
  {
    return nativeGetBookmarkColor(bookmarkId);
  }

  @Icon.BookmarkIconType
  public int getBookmarkIcon(@IntRange(from = 0) long bookmarkId)
  {
    return nativeGetBookmarkIcon(bookmarkId);
  }

  @NonNull
  public String getBookmarkDescription(@IntRange(from = 0) long bookmarkId)
  {
    return nativeGetBookmarkDescription(bookmarkId);
  }

  public double getBookmarkScale(@IntRange(from = 0) long bookmarkId)
  {
    return nativeGetBookmarkScale(bookmarkId);
  }

  @NonNull
  public String encode2Ge0Url(@IntRange(from = 0) long bookmarkId, boolean addName)
  {
    return nativeEncode2Ge0Url(bookmarkId, addName);
  }

  public void setBookmarkParams(@IntRange(from = 0) long bookmarkId, @NonNull String name,
                                @Icon.PredefinedColor int color, @NonNull String descr)
  {
    nativeSetBookmarkParams(bookmarkId, name, color, descr);
  }

  public void changeBookmarkCategory(@IntRange(from = 0) long oldCatId,
                                     @IntRange(from = 0) long newCatId,
                                     @IntRange(from = 0) long bookmarkId)
  {
    nativeChangeBookmarkCategory(oldCatId, newCatId, bookmarkId);
  }

  @NonNull
  public String getBookmarkAddress(@IntRange(from = 0) long bookmarkId)
  {
    return nativeGetBookmarkAddress(bookmarkId);
  }

  public void notifyCategoryChanging(@NonNull BookmarkInfo bookmarkInfo,
                                     @IntRange(from = 0) long catId)
  {
    if (catId == bookmarkInfo.getCategoryId())
      return;

    changeBookmarkCategory(bookmarkInfo.getCategoryId(), catId, bookmarkInfo.getBookmarkId());
  }

  public void notifyCategoryChanging(@NonNull Bookmark bookmark, @IntRange(from = 0) long catId)
  {
    if (catId == bookmark.getCategoryId())
      return;

    changeBookmarkCategory(bookmark.getCategoryId(), catId, bookmark.getBookmarkId());
  }

  public void notifyParametersUpdating(@NonNull BookmarkInfo bookmarkInfo, @NonNull String name,
                                       @Nullable Icon icon, @NonNull String description)
  {
    if (icon == null)
      icon = bookmarkInfo.getIcon();

    if (!name.equals(bookmarkInfo.getName()) || !icon.equals(bookmarkInfo.getIcon()) ||
        !description.equals(getBookmarkDescription(bookmarkInfo.getBookmarkId())))
    {
      setBookmarkParams(bookmarkInfo.getBookmarkId(), name, icon.getColor(), description);
    }
  }

  public void notifyParametersUpdating(@NonNull Bookmark bookmark, @NonNull String name,
                                       @Nullable Icon icon, @NonNull String description)
  {
    if (icon == null)
      icon = bookmark.getIcon();

    if (!name.equals(bookmark.getName()) || !icon.equals(bookmark.getIcon()) ||
        !description.equals(getBookmarkDescription(bookmark.getBookmarkId())))
    {
      setBookmarkParams(bookmark.getBookmarkId(), name,
                        icon != null ? icon.getColor() : getLastEditedColor(), description);
    }
  }

  public double getElevationCurPositionDistance(long trackId)
  {
   return nativeGetElevationCurPositionDistance(trackId);
  }

  public void setElevationActivePoint(long trackId, double distance)
  {
    nativeSetElevationActivePoint(trackId, distance);
  }

  public double getElevationActivePointDistance(long trackId)
  {
    return nativeGetElevationActivePointDistance(trackId);
  }

  @Nullable
  private native Bookmark nativeUpdateBookmarkPlacePage(long bmkId);

  @Nullable
  private native BookmarkInfo nativeGetBookmarkInfo(long bmkId);

  private native long nativeGetBookmarkIdByPosition(long catId, int position);

  @NonNull
  private native Track nativeGetTrack(long trackId, Class<Track> trackClazz);

  private native long nativeGetTrackIdByPosition(long catId, int position);

  private native boolean nativeIsVisible(long catId);

  private native void nativeSetVisibility(long catId, boolean visible);

  private native void nativeSetCategoryName(long catId, @NonNull String n);

  private native void nativeSetCategoryDescription(long catId, @NonNull String desc);

  private native void nativeSetCategoryTags(long catId, @NonNull String[] tagsIds);

  private native void nativeSetCategoryAccessRules(long catId, int accessRules);

  private native void nativeSetCategoryCustomProperty(long catId, String key, String value);

  private static native void nativeLoadBookmarks();

  private native boolean nativeDeleteCategory(long catId);

  private native void nativeDeleteTrack(long trackId);

  private native void nativeDeleteBookmark(long bmkId);

  /**
   * @return category Id
   */
  private native long nativeCreateCategory(@NonNull String name);

  private native void nativeShowBookmarkOnMap(long bmkId);

  private native void nativeShowBookmarkCategoryOnMap(long catId);

  @Nullable
  private native Bookmark nativeAddBookmarkToLastEditedCategory(double lat, double lon);

  private native long nativeGetLastEditedCategory();

  @Icon.PredefinedColor
  private native int nativeGetLastEditedColor();

  private static native void nativeLoadKmzFile(@NonNull String path, boolean isTemporaryFile);

  private static native boolean nativeIsAsyncBookmarksLoadingInProgress();

  private static native boolean nativeIsUsedCategoryName(@NonNull String name);

  private static native boolean nativeIsEditableBookmark(long bmkId);

  private static native boolean nativeIsEditableCategory(long catId);

  private static native boolean nativeIsSearchAllowed(long catId);

  private static native void nativePrepareForSearch(long catId);

  private static native boolean nativeAreAllCategoriesVisible();

  private static native boolean nativeAreAllCategoriesInvisible();

  private static native void nativeSetChildCategoriesVisibility(long catId, boolean visible);

  private static native void nativeSetAllCategoriesVisibility(boolean visible);

  private static native int nativeGetKmlFilesCountForConversion();

  private static native void nativeConvertAllKmlFiles();

  private static native void nativePrepareFileForSharing(long catId);

  private static native boolean nativeIsCategoryEmpty(long catId);

  private static native void nativeSetNotificationsEnabled(boolean enabled);

  private static native boolean nativeAreNotificationsEnabled();

  @NonNull
  private static native String nativeGetCatalogDeeplink(long catId);

  @NonNull
  private static native String nativeGetCatalogPublicLink(long catId);

  @NonNull
  private static native String nativeGetWebEditorUrl(@NonNull String serverId);

  @NonNull
  private static native String nativeGetCatalogFrontendUrl(@UTM.UTMType int utm);

  @NonNull
  private static native KeyValue[] nativeGetCatalogHeaders();

  @NonNull
  private static native String nativeInjectCatalogUTMContent(@NonNull String url,
                                                             @UTM.UTMContentType int content);

  private static native void nativeRequestCatalogTags();

  private static native void nativeRequestCatalogCustomProperties();

  private native boolean nativeHasLastSortingType(long catId);

  @SortingType
  private native int nativeGetLastSortingType(long catId);

  private native void nativeSetLastSortingType(long catId, @SortingType int sortingType);

  private native void nativeResetLastSortingType(long catId);

  @NonNull
  @SortingType
  private native int[] nativeGetAvailableSortingTypes(long catId, boolean hasMyPosition);

  private native boolean nativeGetSortedCategory(long catId, @SortingType int sortingType,
                                                 boolean hasMyPosition, double lat, double lon,
                                                 long timestamp);

  @NonNull
  private static native String nativeGetBookmarkName(@IntRange(from = 0) long bookmarkId);

  @NonNull
  private static native String nativeGetBookmarkFeatureType(@IntRange(from = 0) long bookmarkId);

  @NonNull
  private static native ParcelablePointD nativeGetBookmarkXY(@IntRange(from = 0) long bookmarkId);

  @Icon.PredefinedColor
  private static native int nativeGetBookmarkColor(@IntRange(from = 0) long bookmarkId);

  @Icon.BookmarkIconType
  private static native int nativeGetBookmarkIcon(@IntRange(from = 0) long bookmarkId);

  @NonNull
  private static native String nativeGetBookmarkDescription(@IntRange(from = 0) long bookmarkId);

  private static native double nativeGetBookmarkScale(@IntRange(from = 0) long bookmarkId);

  @NonNull
  private static native String nativeEncode2Ge0Url(@IntRange(from = 0) long bookmarkId,
                                                   boolean addName);

  private static native void nativeSetBookmarkParams(@IntRange(from = 0) long bookmarkId,
                                                     @NonNull String name,
                                                     @Icon.PredefinedColor int color,
                                                     @NonNull String descr);

  private static native void nativeChangeBookmarkCategory(@IntRange(from = 0) long oldCatId,
                                                          @IntRange(from = 0) long newCatId,
                                                          @IntRange(from = 0) long bookmarkId);

  @NonNull
  private static native String nativeGetBookmarkAddress(@IntRange(from = 0) long bookmarkId);

  private static native double nativeGetElevationCurPositionDistance(long trackId);

  private static native void nativeSetElevationCurrentPositionChangedListener();

  public static native void nativeRemoveElevationCurrentPositionChangedListener();

  private static native void nativeSetElevationActivePoint(long trackId, double distanceInMeters);

  private static native double nativeGetElevationActivePointDistance(long trackId);

  private static native void nativeSetElevationActiveChangedListener();

  public static native void nativeRemoveElevationActiveChangedListener();

  public interface ElevationActivePointChangedListener
  {
    void onElevationActivePointChanged();
  }

  public interface BookmarksLoadingListener
  {
    void onBookmarksLoadingStarted();
    void onBookmarksLoadingFinished();
    void onBookmarksFileLoaded(boolean success);
  }

  public interface BookmarksSortingListener
  {
    void onBookmarksSortingCompleted(@NonNull SortedBlock[] sortedBlocks, long timestamp);
    void onBookmarksSortingCancelled(long timestamp);
  }

  public interface KmlConversionListener
  {
    void onFinishKmlConversion(boolean success);
  }

  public interface BookmarksSharingListener
  {
    void onPreparedFileForSharing(@NonNull BookmarkSharingResult result);
  }

  public interface BookmarksCloudListener
  {
    /**
     * The method is called when the synchronization started.
     *
     * @param type determines type of synchronization (backup or restoring).
     */
    void onSynchronizationStarted(@SynchronizationType int type);

    /**
     * The method is called when the synchronization finished.
     *
     * @param type determines type of synchronization (backup or restoring).
     * @param result is one of possible results of the synchronization.
     * @param errorString contains detailed description in case of unsuccessful completion.
     */
    void onSynchronizationFinished(@SynchronizationType int type,
                                   @SynchronizationResult int result,
                                   @NonNull String errorString);

    /**
     * The method is called after restoring request.
     *
     * @param result By result you can determine if the restoring is possible.
     * @param deviceName The name of device which was the source of the backup.
     * @param backupTimestampInMs contains timestamp of the backup on the server (in milliseconds).
     */
    void onRestoreRequested(@RestoringRequestResult int result, @NonNull String deviceName,
                            long backupTimestampInMs);

    /**
     * Restored bookmark files are prepared to substitute for the current ones.
     * After this callback any cached bookmarks data become invalid. Also after this
     * callback the restoring process can not be cancelled.
     */
    void onRestoredFilesPrepared();
  }

  public interface BookmarksCatalogPingListener
  {
    void onPingFinished(boolean isServiceAvailable);
  }

  public interface BookmarksExpiredCategoriesListener
  {
    void onCheckExpiredCategories(boolean hasExpiredCategories);
  }

  public interface OnElevationActivePointChangedListener
  {
    void onElevationActivePointChanged();
  }

  public interface OnElevationCurrentPositionChangedListener
  {
    void onCurrentPositionChanged();
  }

  public enum UploadResult
  {
    UPLOAD_RESULT_SUCCESS,
    UPLOAD_RESULT_NETWORK_ERROR,
    UPLOAD_RESULT_SERVER_ERROR,
    UPLOAD_RESULT_AUTH_ERROR,
    /* Broken file */
    UPLOAD_RESULT_MALFORMED_DATA_ERROR,
    /* Edit on web */
    UPLOAD_RESULT_ACCESS_ERROR,
    UPLOAD_RESULT_INVALID_CALL
  }

  static class BookmarkCategoriesCache extends Observable<DataChangedListener>
  {
    @NonNull
    private final List<BookmarkCategory> mCategories = new ArrayList<>();

    void update(@NonNull List<BookmarkCategory> categories)
    {
      mCategories.clear();
      mCategories.addAll(categories);
      notifyChanged();
    }

    @NonNull
    public List<BookmarkCategory> getCategories()
    {
      return Collections.unmodifiableList(mCategories);
    }
  }
}
