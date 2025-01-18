package app.organicmaps.settings;

import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Environment;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import app.organicmaps.Framework;
import app.organicmaps.R;
import app.organicmaps.downloader.MapManager;
import app.organicmaps.util.Config;
import app.organicmaps.util.StorageUtils;
import app.organicmaps.util.log.Logger;

import java.io.File;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class StoragePathManager
{
  private static final String TAG = StoragePathManager.class.getSimpleName();
  private static final String DATA_FILE_EXT = Framework.nativeGetDataFileExt();
  private static final String[] MOVABLE_EXTS = Framework.nativeGetMovableFilesExts();
  static final FilenameFilter MOVABLE_FILES_FILTER = (dir, filename) -> {
    for (String ext : MOVABLE_EXTS)
      if (filename.endsWith(ext))
        return true;

    return false;
  };

  interface OnStorageListChangedListener
  {
    void onStorageListChanged(List<StorageItem> storageItems, int currentStorageIndex);
  }

  private OnStorageListChangedListener mStoragesChangedListener;
  private BroadcastReceiver mInternalReceiver;
  private final Context mContext;

  public final List<StorageItem> mStorages = new ArrayList<>();
  public int mCurrentStorageIndex = -1;
  private StorageItem mInternalStorage = null;

  public StoragePathManager(@NonNull Context context)
  {
    mContext = context;
  }

  protected void finalize() throws Throwable
  {
    // Make sure watchers are detached.
    stopExternalStorageWatching();
    super.finalize();
  }

  /**
   * Observes status of connected media and retrieves list of available external storages.
   *
   * TODO: ATM its used to update settings UI only - watch and handle storage changes constantly at the app level
   */
  public void startExternalStorageWatching(final @Nullable OnStorageListChangedListener storagesChangedListener)
  {
    mStoragesChangedListener = storagesChangedListener;
    mInternalReceiver = new BroadcastReceiver()
    {
      @Override
      public void onReceive(Context context, Intent intent)
      {
        scanAvailableStorages();

        if (mStoragesChangedListener != null)
          mStoragesChangedListener.onStorageListChanged(mStorages, mCurrentStorageIndex);
      }
    };

    mContext.registerReceiver(mInternalReceiver, getMediaChangesIntentFilter());
  }

  private static IntentFilter getMediaChangesIntentFilter()
  {
    final IntentFilter filter = new IntentFilter();
    filter.addAction(Intent.ACTION_MEDIA_MOUNTED);
    filter.addAction(Intent.ACTION_MEDIA_REMOVED);
    filter.addAction(Intent.ACTION_MEDIA_EJECT);
    filter.addAction(Intent.ACTION_MEDIA_SHARED);
    filter.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
    filter.addAction(Intent.ACTION_MEDIA_BAD_REMOVAL);
    filter.addAction(Intent.ACTION_MEDIA_UNMOUNTABLE);
    filter.addAction(Intent.ACTION_MEDIA_CHECKING);
    filter.addAction(Intent.ACTION_MEDIA_NOFS);
    filter.addDataScheme(ContentResolver.SCHEME_FILE);

    return filter;
  }

  public void stopExternalStorageWatching()
  {
    if (mInternalReceiver != null)
    {
      mContext.unregisterReceiver(mInternalReceiver);
      mInternalReceiver = null;
      mStoragesChangedListener = null;
    }
  }

  /**
   * Adds a storage into the list if it passes sanity checks.
   */
  private void addStorageOption(File dir, boolean isInternal, String configPath)
  {
    // Internal storage must always exists, but Android is unpredictable.
    // External storages can be null in some cases.
    // https://github.com/organicmaps/organicmaps/issues/632
    if (dir == null)
    {
      Logger.w(TAG, "The system returned 'null' " + (isInternal ? "internal" : "external") + " storage");
      return;
    }

    String path;
    try
    {
      path = dir.getCanonicalPath();
    }
    catch (IOException e)
    {
      Logger.e(TAG, "IOException at getCanonicalPath for " + dir.getPath(), e);
      return;
    }
    // Add the trailing separator because the native code assumes that all paths have it.
    path = StorageUtils.addTrailingSeparator(path);
    // Ensure we check the same path we save into storages list later.
    dir = new File(path);

    final boolean isCurrent = path.equals(configPath);
    final long totalSize = dir.getTotalSpace();
    final long freeSize = dir.getUsableSpace();

    String commentedPath = path + (StorageUtils.addTrailingSeparator(dir.getPath()).equals(path)
                                   ? "" : " (" + dir.getPath() + ")") + " - " +
                           (isCurrent ? "currently configured, " : "") +
                           (isInternal ? "internal" : "external") + ", " +
                           freeSize + " available of " + totalSize + " bytes";

    boolean isEmulated = false;
    boolean isRemovable = false;
    boolean isReadonly = false;
    String state = null;
    String label = null;
    if (!isInternal)
    {
      try
      {
        isEmulated = Environment.isExternalStorageEmulated(dir);
        isRemovable = Environment.isExternalStorageRemovable(dir);
        state = Environment.getExternalStorageState(dir);
        commentedPath += (isEmulated ? ", emulated" : "") +
                         (isRemovable ? ", removable" : "") +
                         (state != null ? ", state=" + state : "");
      }
      catch (IllegalArgumentException e)
      {
        // Thrown if the dir is not a valid storage device.
        // https://github.com/organicmaps/organicmaps/issues/538
        Logger.w(TAG, "External storage checks failed for " + commentedPath);
      }

      // Get additional storage information for Android 7+.
      if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N)
      {
        final StorageManager sm = (StorageManager) mContext.getSystemService(Context.STORAGE_SERVICE);
        if (sm != null)
        {
          final StorageVolume sv = sm.getStorageVolume(dir);
          if (sv != null)
          {
            label = sv.getDescription(mContext);
            commentedPath += (sv.isPrimary() ? ", primary" : "") +
                             (!TextUtils.isEmpty(sv.getUuid()) ? ", uuid=" + sv.getUuid() : "") +
                             (!TextUtils.isEmpty(label) ? ", label='" + label + "'" : "");
          }
          else
            Logger.w(TAG, "Can't get StorageVolume for " + commentedPath);
        }
        else
          Logger.w(TAG, "Can't get StorageManager for " + commentedPath);
      }
    }

    if (state != null && !Environment.MEDIA_MOUNTED.equals(state)
        && !Environment.MEDIA_MOUNTED_READ_ONLY.equals(state))
    {
      Logger.w(TAG, "Not mounted: " + commentedPath);
      return;
    }

    if (!dir.exists())
    {
      Logger.w(TAG, "Not exists: " + commentedPath);
      return;
    }
    if (!dir.isDirectory())
    {
      Logger.w(TAG, "Not a directory: " + commentedPath);
      return;
    }

    // For writability check use a test dir creation instead of canWrite() to get more information
    // and avoid possible false negatives.
    if (!StorageUtils.isDirWritable(dir))
    {
      if (Environment.MEDIA_MOUNTED_READ_ONLY.equals(state))
        Logger.w(TAG, "Mounted read-only: " + commentedPath);
      isReadonly = true;
      commentedPath = "read-only " + commentedPath;
    }

    if (TextUtils.isEmpty(label))
      label = isInternal ? mContext.getString(R.string.maps_storage_internal)
                         : (isRemovable ? mContext.getString(R.string.maps_storage_removable)
                                        : (isEmulated ? mContext.getString(R.string.maps_storage_shared)
                                                      : mContext.getString(R.string.maps_storage_external)));

    StorageItem storage = new StorageItem(path, freeSize, totalSize, label, isReadonly);
    mStorages.add(storage);
    if (isCurrent)
      mCurrentStorageIndex = mStorages.size() - 1;
    if (isInternal)
      mInternalStorage = storage;
    Logger.i(TAG, "Accepted " + commentedPath);
  }

  /**
   * Updates the list of available storages.
   */
  public void scanAvailableStorages() throws AssertionError
  {
    // Current configured storage directory, can be empty on the first run.
    final String configPath = Config.getStoragePath();
    Logger.i(TAG, "Currently configured storage: " + (TextUtils.isEmpty(configPath) ? "N/A" : configPath));

    Logger.i(TAG, "Begin scanning storages");
    mStorages.clear();
    mCurrentStorageIndex = -1;
    mInternalStorage = null;

    // External storages (SD cards and other).
    for (File externalDir : mContext.getExternalFilesDirs(null))
    {
      addStorageOption(externalDir, false, configPath);
    }

    File internalDir = mContext.getFilesDir();
    addStorageOption(internalDir, true, configPath);

    Logger.i(TAG, "End scanning storages");

    if (mStorages.isEmpty())
      // Shut down the app.
      throw new AssertionError("Can't find available storages");

    if (!TextUtils.isEmpty(configPath) && mCurrentStorageIndex == -1)
    {
      Logger.w(TAG, "Currently configured storage is not available!");
    }
  }

  /**
   * Determine whether the storage contains map files.
   */
  private static boolean containsMapData(String storagePath)
  {
    return StorageUtils.getDirSizeRecursively(new File(storagePath), (dir, filename) -> filename.endsWith(DATA_FILE_EXT)) > 0;
  }

  /**
   * Get a writable non-internal storage with the most free space.
   * Returns an internal storage if no other options are suitable.
   */
  public StorageItem getDefaultStorage()
  {
    StorageItem res = null;
    for (StorageItem storage : mStorages)
    {
      if ((res == null || res.mFreeSize < storage.mFreeSize)
          && !storage.mIsReadonly && !storage.equals(mInternalStorage))
        res = storage;
    }

    return res != null ? res : mInternalStorage;
  }

  /**
   * Returns an available storage with existing maps files.
   * Checks the currently configured storage first, then scans other storages.
   * If no map files found uses getDefaultStorage().
   */
  public static String findMapsStorage(@NonNull Context context)
  {
    StoragePathManager mgr = new StoragePathManager(context);
    mgr.scanAvailableStorages();
    String path;
    final List<StorageItem> storages = mgr.mStorages;
    final int currentIdx = mgr.mCurrentStorageIndex;

    if (currentIdx != -1)
    {
      path = storages.get(currentIdx).mPath;
      if (containsMapData(path))
      {
        Logger.i(TAG, "Found map files at the currently configured " + path);
        return path;
      }
      else
      {
        Logger.w(TAG, "No map files found at the currenly configured " + path);
      }
    }

    Logger.i(TAG, "Looking for map files in available storages...");
    for (int idx = 0; idx < storages.size(); ++idx)
    {
      if (idx == currentIdx)
        continue;
      path = storages.get(idx).mPath;
      if (containsMapData(path))
      {
        Logger.i(TAG, "Found map files at " + path);
        return path;
      }
      else
      {
        Logger.i(TAG, "No map files found at " + path);
      }
    }

    path = mgr.getDefaultStorage().mPath;
    Logger.i(TAG, "Using default storage " + path);
    return path;
  }

  /**
   * Moves map files.
   */
  @SuppressWarnings("ResultOfMethodCallIgnored")
  public static boolean moveStorage(@NonNull final String newPath, @NonNull final String oldPath)
  {
    Logger.i(TAG, "Begin moving maps from " + oldPath + " to " + newPath);

    final File oldDir = new File(oldPath);
    final File newDir = new File(newPath);

    ArrayList<String> relPaths = new ArrayList<>();
    StorageUtils.listFilesRecursively(oldDir, "", MOVABLE_FILES_FILTER, relPaths);

    File[] oldFiles = new File[relPaths.size()];
    File[] newFiles = new File[relPaths.size()];
    for (int i = 0; i < relPaths.size(); ++i)
    {
      oldFiles[i] = new File(oldDir.getAbsolutePath() + File.separator + relPaths.get(i));
      newFiles[i] = new File(newDir.getAbsolutePath() + File.separator + relPaths.get(i));
    }

    for (int i = 0; i < oldFiles.length; ++i)
    {
      Logger.i(TAG, "Moving " + oldFiles[i].getPath() + " to " + newFiles[i].getPath());
      File parent = newFiles[i].getParentFile();
      if (parent != null)
        parent.mkdirs();
      if (!MapManager.nativeMoveFile(oldFiles[i].getPath(), newFiles[i].getPath()))
      {
        Logger.e(TAG, "Failed to move " + oldFiles[i].getPath() + " to " + newFiles[i].getPath());
        // In the case of failure delete all new files.  Old files will
        // be lost if new files were just moved from old locations.
        // TODO: Delete old files only after all of them were copied to the new location.
        StorageUtils.removeFilesInDirectory(newDir, newFiles);
        return false;
      }
    }
    Logger.i(TAG, "End moving maps");

    return true;
  }
}
