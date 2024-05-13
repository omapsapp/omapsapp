package app.organicmaps.sdk.search;

import androidx.annotation.Keep;
import androidx.annotation.Nullable;

/**
 * Native search will return results via this interface.
 */
public interface NativeBookmarkSearchListener
{
  /**
   * @param bookmarkIds Founded bookmark ids.
   * @param timestamp Timestamp of search request.
   */
  // Used by JNI.
  @Keep
  @SuppressWarnings("unused")
  void onBookmarkSearchResultsUpdate(@Nullable long[] bookmarkIds, long timestamp);

  /**
   * @param bookmarkIds Founded bookmark ids.
   * @param timestamp Timestamp of search request.
   */
  // Used by JNI.
  @Keep
  @SuppressWarnings("unused")
  void onBookmarkSearchResultsEnd(@Nullable long[] bookmarkIds, long timestamp);
}
