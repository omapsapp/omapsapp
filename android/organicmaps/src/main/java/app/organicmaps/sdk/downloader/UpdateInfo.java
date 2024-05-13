package app.organicmaps.sdk.downloader;

import androidx.annotation.Keep;

/**
 * Info about data to be updated. Created by native code.
 */
// Called from JNI.
@Keep
@SuppressWarnings("unused")
public final class UpdateInfo
{
  public final int filesCount;
  public final long totalSize;

  public UpdateInfo(int filesCount, long totalSize)
  {
    this.filesCount = filesCount;
    this.totalSize = totalSize;
  }
}
