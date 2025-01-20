package app.organicmaps.search;

import android.content.Context;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;

import app.organicmaps.Framework;
import app.organicmaps.util.Language;
import app.organicmaps.util.concurrency.UiThread;

import java.nio.charset.StandardCharsets;

public enum SearchEngine implements NativeSearchListener,
                                    NativeMapSearchListener,
                                    NativeBookmarkSearchListener
{
  INSTANCE;

  // Query, which results are shown on the map.
  @Nullable
  private String mQuery;

  @Override
  public void onResultsUpdate(@NonNull final SearchResult[] results, final long timestamp)
  {
    UiThread.run(
        () ->
        {
          for (NativeSearchListener listener : mListeners)
            listener.onResultsUpdate(results, timestamp);
        });
  }

  @Override
  public void onResultsEnd(final long timestamp)
  {
    UiThread.run(
        () ->
        {
          for (NativeSearchListener listener : mListeners)
            listener.onResultsEnd(timestamp);
        });
  }

  @Override
  public void onMapSearchResults(final NativeMapSearchListener.Result[] results, final long timestamp, final boolean isLast)
  {
    UiThread.run(
        () ->
        {
          for (NativeMapSearchListener listener : mMapListeners)
            listener.onMapSearchResults(results, timestamp, isLast);
        });
  }

  @Override
  public void onBookmarkSearchResultsUpdate(@Nullable long[] bookmarkIds, long timestamp)
  {
    for (NativeBookmarkSearchListener listener : mBookmarkListeners)
      listener.onBookmarkSearchResultsUpdate(bookmarkIds, timestamp);
  }

  @Override
  public void onBookmarkSearchResultsEnd(@Nullable long[] bookmarkIds, long timestamp)
  {
    for (NativeBookmarkSearchListener listener : mBookmarkListeners)
      listener.onBookmarkSearchResultsEnd(bookmarkIds, timestamp);
  }

  private final ObserverList<NativeSearchListener> mListeners = new ObserverList<>();

  private final ObserverList<NativeMapSearchListener> mMapListeners = new ObserverList<>();

  private final ObserverList<NativeBookmarkSearchListener> mBookmarkListeners = new ObserverList<>();

  public void addListener(NativeSearchListener listener)
  {
    mListeners.addObserver(listener);
  }

  public void removeListener(NativeSearchListener listener)
  {
    mListeners.removeObserver(listener);
  }

  public void addMapListener(NativeMapSearchListener listener)
  {
    mMapListeners.addObserver(listener);
  }

  public void removeMapListener(NativeMapSearchListener listener)
  {
    mMapListeners.removeObserver(listener);
  }

  public void addBookmarkListener(NativeBookmarkSearchListener listener)
  {
    mBookmarkListeners.addObserver(listener);
  }

  public void removeBookmarkListener(NativeBookmarkSearchListener listener)
  {
    mBookmarkListeners.removeObserver(listener);
  }

  private native void nativeInit();

  /**
   *
   * @param context
   * @param timestamp Search results are filtered according to it after multiple requests.
   * @return whether search was actually started.
   */
  @MainThread
  public boolean search(@NonNull Context context, String query, boolean isCategory,
                        long timestamp, boolean hasLocation, double lat, double lon)
  {
    return nativeRunSearch(query.getBytes(StandardCharsets.UTF_8), isCategory,
            Language.getKeyboardLocale(context), timestamp, hasLocation, lat, lon);
  }

  @MainThread
  public void searchInteractive(@NonNull String query, boolean isCategory, @NonNull String locale,
                                long timestamp, boolean isMapAndTable, boolean hasLocation, double lat, double lon)
  {
    nativeRunInteractiveSearch(query.getBytes(StandardCharsets.UTF_8), isCategory,
            locale, timestamp, isMapAndTable, hasLocation, lat, lon);
  }

  @MainThread
  public void searchInteractive(@NonNull String query, boolean isCategory, @NonNull String locale,
                                long timestamp, boolean isMapAndTable)
  {
    searchInteractive(query, isCategory, locale, timestamp, isMapAndTable, false, 0, 0);
  }

  @MainThread
  public void searchInteractive(@NonNull Context context, @NonNull String query, boolean isCategory,
                                long timestamp, boolean isMapAndTable)
  {
    searchInteractive(query, isCategory, Language.getKeyboardLocale(context), timestamp, isMapAndTable, false, 0, 0);
  }

  @MainThread
  public static void searchMaps(@NonNull Context context, String query, long timestamp)
  {
    nativeRunSearchMaps(query.getBytes(StandardCharsets.UTF_8), Language.getKeyboardLocale(context),
                        timestamp);
  }

  @MainThread
  public boolean searchInBookmarks(@NonNull String query, long categoryId, long timestamp)
  {
    return nativeRunSearchInBookmarks(query.getBytes(StandardCharsets.UTF_8), categoryId, timestamp);
  }

  public void setQuery(@Nullable String query)
  {
    mQuery = query;
  }

  @Nullable
  public String getQuery()
  {
    return mQuery;
  }

  @MainThread
  public void cancel()
  {
    cancelApiCall();
    cancelAllSearches();
  }

  @MainThread
  private static void cancelApiCall()
  {
    Framework.nativeClearApiPoints();
  }

  @MainThread
  public void cancelInteractiveSearch()
  {
    mQuery = "";
    nativeCancelInteractiveSearch();
  }

  @MainThread
  private void cancelAllSearches()
  {
    mQuery = "";
    nativeCancelAllSearches();
  }

  @MainThread
  public void showResult(int index)
  {
    mQuery = "";
    nativeShowResult(index);
  }

  public void initialize()
  {
    nativeInit();
  }

  /**
   * @param bytes utf-8 formatted bytes of query.
   */
  private static native boolean nativeRunSearch(byte[] bytes, boolean isCategory,
                                                String language, long timestamp, boolean hasLocation,
                                                double lat, double lon);

  /**
   * @param bytes utf-8 formatted query bytes
   */
  private static native void nativeRunInteractiveSearch(byte[] bytes, boolean isCategory,
                                                        String language, long timestamp,
                                                        boolean isMapAndTable, boolean hasLocation,
                                                        double lat, double lon);

  /**
   * @param bytes utf-8 formatted query bytes
   */
  private static native void nativeRunSearchMaps(byte[] bytes, String language, long timestamp);

  private static native boolean nativeRunSearchInBookmarks(byte[] bytes, long categoryId, long timestamp);

  private static native void nativeShowResult(int index);

  private static native void nativeCancelInteractiveSearch();

  private static native void nativeCancelEverywhereSearch();

  private static native void nativeCancelAllSearches();
}
