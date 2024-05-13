package app.organicmaps.sdk.search;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

/**
 * Native search will return results via this interface.
 */
public interface NativeSearchListener
{
  /**
   * @param results Search results.
   * @param timestamp Timestamp of search request.
   */
  // Called by JNI.
  @Keep
  @SuppressWarnings("unused")
  default void onResultsUpdate(@NonNull SearchResult[] results, long timestamp) {}

  /**
   * @param timestamp Timestamp of search request.
   */
  // Called by JNI.
  @Keep
  @SuppressWarnings("unused")
  default void onResultsEnd(long timestamp) {}
}
