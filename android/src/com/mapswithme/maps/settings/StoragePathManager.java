package com.mapswithme.maps.settings;

import android.app.Application;
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
import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.downloader.MapManager;
import com.mapswithme.util.Config;
import com.mapswithme.util.StorageUtils;
import com.mapswithme.util.concurrency.UiThread;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

import java.io.File;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class StoragePathManager
{
  static final String TAG = StoragePathManager.class.getName();
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.STORAGE);
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
  private Context mContext;

  public final List<StorageItem> mStorages = new ArrayList<>();
  public int mCurrentStorageIndex = -1;
  private StorageItem mEmulatedStorage = null;

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
   * Internal storage is omitted if it backs an emulated one (unless internal is the current one),
   * hence internal should be fed to this method last.
   */
  private void addStorageOption(File dir, boolean isInternal, String configPath)
  {
    // Internal storage must always exists, but Android is unpredictable.
    // External storages can be null in some cases.
    // https://github.com/organicmaps/organicmaps/issues/632
    if (dir == null)
    {
      LOGGER.w(TAG, "The system returned 'null' " + (isInternal ? "internal" : "external") + " storage");
      return;
    }

    String commentedPath = null;
    try
    {
      // Add the trailing separator because the native code assumes that all paths have it.
      final String path = StorageUtils.addTrailingSeparator(dir.getCanonicalPath());
      final boolean isCurrent = path.equals(configPath);
      final long totalSize = dir.getTotalSpace();
      final long freeSize = dir.getUsableSpace();

      commentedPath = path + (StorageUtils.addTrailingSeparator(dir.getPath()).equals(path)
                              ? "" : " (" + dir.getPath() + ")") + " - " +
                      (isCurrent ? "currently configured, " : "") +
                      (isInternal ? "internal" : "external") + ", " +
                      freeSize + " available out of " + totalSize + " bytes";

      // Check if internal and emulated are the same physical device.
      // Allow for some divergence in freeSize because there could have been file operations inbetween free space checks.
      if (isInternal && mEmulatedStorage != null && mEmulatedStorage.mTotalSize == totalSize
          && mEmulatedStorage.mFreeSize > freeSize - 1024 * 1024
          && mEmulatedStorage.mFreeSize < freeSize + 1024 * 1024)
      {
        final String emulated = ", backs emulated (" + mEmulatedStorage.mPath + ")";
        // Allow duplicating internal storage if its the current one (for migration purposes).
        if (!isCurrent)
        {
          LOGGER.i(TAG, "Duplicate" + emulated + ": " + commentedPath);
          return;
        }
        commentedPath += emulated;
      }

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
          LOGGER.w(TAG, "isExternalStorage checks failed for " + commentedPath);
        }

        // Get additional storage information for Android 7+.
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N)
        {
          final StorageManager sm = (StorageManager) mContext.getSystemService(mContext.STORAGE_SERVICE);
          if (sm != null)
          {
            final StorageVolume sv = sm.getStorageVolume(dir);
            if (sv != null)
            {
              label = sv.getDescription(mContext);
              commentedPath += (sv.isPrimary() ? ", primary" : "") +
                               (!TextUtils.isEmpty(sv.getUuid()) ? ", uuid=" + sv.getUuid() : "") +
                               (!TextUtils.isEmpty(label) ? ", label='" + label + "'": "");
            }
            else
              LOGGER.w(TAG, "Can't get StorageVolume for " + commentedPath);
          }
          else
            LOGGER.w(TAG, "Can't get StorageManager for " + commentedPath);
        }
      }

      if (state != null && !Environment.MEDIA_MOUNTED.equals(state)
          && !Environment.MEDIA_MOUNTED_READ_ONLY.equals(state))
      {
        LOGGER.w(TAG, "Not mounted: " + commentedPath);
        return;
      }
      if (!dir.exists())
      {
        LOGGER.w(TAG, "Not exists: " + commentedPath);
        return;
      }
      if (!dir.isDirectory())
      {
        LOGGER.w(TAG, "Not a directory: " + commentedPath);
        return;
      }
      if (!dir.canWrite() || Environment.MEDIA_MOUNTED_READ_ONLY.equals(state))
      {
        isReadonly = true;
        LOGGER.w(TAG, "Not writable: " + commentedPath);
        // Keep using currently configured storage even if its read-only.
        if (isCurrent)
          commentedPath += ", read-only";
        else
          return;
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
      if (isEmulated)
        mEmulatedStorage = storage;
      LOGGER.i(TAG, "Accepted " + commentedPath);
    }
    catch (SecurityException | IOException ex)
    {
      LOGGER.e(TAG, "Error: " + (commentedPath != null ? commentedPath : "(" + dir.getPath() + ")"), ex);
    }
  }

  /**
   * Updates the list of available storages.
   */
  public void scanAvailableStorages() throws AssertionError
  {
    // Current configured storage directory, can be empty on the first run.
    final String configPath = Config.getStoragePath();
    LOGGER.i(TAG, "Currently configured storage: " + (TextUtils.isEmpty(configPath) ? "N/A" : configPath));

    LOGGER.i(TAG, "Begin scanning storages");
    mStorages.clear();
    mCurrentStorageIndex = -1;
    mEmulatedStorage = null;

    // External storages (SD cards and other).
    for (File externalDir : mContext.getExternalFilesDirs(null))
    {
      addStorageOption(externalDir, false, configPath);
    }

    File internalDir = mContext.getFilesDir();
    addStorageOption(internalDir, true, configPath);

    LOGGER.i(TAG, "End scanning storages");

    if (mStorages.isEmpty())
      // Shut down the app.
      throw new AssertionError("Can't find available storages");

    if (!TextUtils.isEmpty(configPath) && mCurrentStorageIndex == -1)
    {
      LOGGER.w(TAG, "Currently configured storage is not available!");
    }
  }

  /**
   * Determine whether the storage contains map files
   * by checking for non-empty directories with version-like names (e.g. "220415").
   */
  private static boolean containsMapData(String storagePath)
  {
    File path = new File(storagePath);
    File[] candidates = path.listFiles((pathname) -> {
      if (!pathname.isDirectory())
        return false;

      try
      {
        String name = pathname.getName();
        if (name.length() != 6)
          return false;

        int version = Integer.valueOf(name);
        return (version > 120000 && version <= 999999);
      }
      catch (NumberFormatException ignored)
      {
      }

      return false;
    });

    return (candidates != null && candidates.length > 0 &&
            candidates[0].list().length > 0);
  }

  /**
   * Get storage with the most free space.
   */
  public StorageItem getBiggestStorage()
  {
    StorageItem res = null;
    for (StorageItem storage : mStorages)
    {
      if (res == null || res.mFreeSize < storage.mFreeSize)
      {
        res = storage;
      }
    }
    return res;
  }

  /**
   * Returns an available storage with existing maps files.
   * Checks the currently configured storage first,
   * then scans other storages. If no maps files found
   * defaults to the storage with the most free space.
   */
  public static String findMapsStorage(@NonNull Application application)
  {
    StoragePathManager mgr = new StoragePathManager(application);
    mgr.scanAvailableStorages();
    String path = null;
    final List<StorageItem> storages = mgr.mStorages;
    final int currentIdx = mgr.mCurrentStorageIndex;

    if (currentIdx != -1)
    {
      path = storages.get(currentIdx).mPath;
      if (containsMapData(path))
      {
        LOGGER.i(TAG, "Found map files at the currently configured " + path);
        return path;
      }
      else
      {
        LOGGER.w(TAG, "No map files found at the currenly configured " + path);
      }
    }

    LOGGER.i(TAG, "Looking for map files in available storages...");
    for (int idx = 0; idx < storages.size(); ++idx)
    {
      if (idx == currentIdx)
        continue;
      path = storages.get(idx).mPath;
      if (containsMapData(path))
      {
        LOGGER.i(TAG, "Found map files at " + path);
        return path;
      }
      else
      {
        LOGGER.i(TAG, "No map files found at " + path);
      }
    }

    path = mgr.getBiggestStorage().mPath;
    LOGGER.i(TAG, "Defaulting to a storage with the most free space: " + path);
    return path;
  }

  /**
   * Moves map files.
   */
  @SuppressWarnings("ResultOfMethodCallIgnored")
  public static boolean moveStorage(@NonNull final String newPath, @NonNull final String oldPath)
  {
    LOGGER.i(TAG, "Begin moving maps from " + oldPath + " to " + newPath);

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
      LOGGER.i(TAG, "Moving " + oldFiles[i].getPath() + " to " + newFiles[i].getPath());
      File parent = newFiles[i].getParentFile();
      if (parent != null)
        parent.mkdirs();
      if (!MapManager.nativeMoveFile(oldFiles[i].getPath(), newFiles[i].getPath()))
      {
        LOGGER.e(TAG, "Failed to move " + oldFiles[i].getPath() + " to " + newFiles[i].getPath());
        // In the case of failure delete all new files.  Old files will
        // be lost if new files were just moved from old locations.
        // TODO: Delete old files only after all of them were copied to the new location.
        StorageUtils.removeFilesInDirectory(newDir, newFiles);
        return false;
      }
    }
    LOGGER.i(TAG, "End moving maps");

    UiThread.run(() -> Framework.nativeSetWritableDir(newPath));

    return true;
  }
}
