package app.organicmaps.sdk.settings;

/**
 * Represents storage option.
 */
public class StorageItem
{
  public final String mPath;
  public final long mFreeSize;
  public final long mTotalSize;
  public final String mLabel;
  public final boolean mIsReadonly;

  public StorageItem(String path, long freeSize, long totalSize, final String label, boolean isReadonly)
  {
    mPath = path;
    mFreeSize = freeSize;
    mTotalSize = totalSize;
    mLabel = label;
    mIsReadonly = isReadonly;
  }

  @Override
  public boolean equals(Object o)
  {
    if (o == this)
      return true;
    if (!(o instanceof StorageItem))
      return false;
    StorageItem other = (StorageItem) o;
    return mPath.equals(other.mPath);
  }

  @Override
  public int hashCode()
  {
    // Yes, do not put StorageItem to Hash containers, performance will be awful.
    // At least such hash is compatible with hacky equals.
    return 0;
  }
}
