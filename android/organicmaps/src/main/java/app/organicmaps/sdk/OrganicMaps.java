package app.organicmaps.sdk;

import static app.organicmaps.sdk.location.LocationState.LOCATION_TAG;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Message;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.LifecycleOwner;

import app.organicmaps.sdk.bookmarks.data.BookmarkManager;
import app.organicmaps.sdk.display.DisplayManager;
import app.organicmaps.sdk.downloader.CountryItem;
import app.organicmaps.sdk.downloader.MapManager;
import app.organicmaps.sdk.location.LocationHelper;
import app.organicmaps.sdk.location.LocationState;
import app.organicmaps.sdk.location.SensorHelper;
import app.organicmaps.sdk.maplayer.isolines.IsolinesManager;
import app.organicmaps.sdk.maplayer.subway.SubwayManager;
import app.organicmaps.sdk.maplayer.traffic.TrafficManager;
import app.organicmaps.sdk.routing.RoutingController;
import app.organicmaps.sdk.util.Config;
import app.organicmaps.sdk.util.ConnectionState;
import app.organicmaps.sdk.util.StorageUtils;
import app.organicmaps.sdk.util.log.Logger;
import app.organicmaps.sdk.util.log.LogsManager;

import java.io.IOException;
import java.util.List;

public class OrganicMaps extends Application
{
  @NonNull
  private static final String TAG = OrganicMaps.class.getSimpleName();

  private volatile boolean mFrameworkInitialized;
  private volatile boolean mPlatformInitialized;

  @NonNull
  private final MapManager.StorageCallback mStorageCallbacks = new StorageCallbackImpl();

  private Handler mMainLoopHandler;
  private final Object mMainQueueToken = new Object();

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private SubwayManager mSubwayManager;

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private IsolinesManager mIsolinesManager;

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private LocationHelper mLocationHelper;

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private SensorHelper mSensorHelper;

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private DisplayManager mDisplayManager;

  @NonNull
  public SubwayManager getSubwayManager()
  {
    return mSubwayManager;
  }

  @NonNull
  public IsolinesManager getIsolinesManager()
  {
    return mIsolinesManager;
  }

  @NonNull
  public LocationHelper getLocationHelper()
  {
    return mLocationHelper;
  }

  @NonNull
  public SensorHelper getSensorHelper()
  {
    return mSensorHelper;
  }

  @NonNull
  public DisplayManager getDisplayManager()
  {
    return mDisplayManager;
  }

  @NonNull
  public static SharedPreferences prefs(@NonNull Context context)
  {
    return context.getSharedPreferences(context.getString(R.string.pref_file_name), MODE_PRIVATE);
  }

  @NonNull
  public static OrganicMaps from(@NonNull Context context)
  {
    return (OrganicMaps) context.getApplicationContext();
  }

  @Override
  public void onCreate()
  {
    super.onCreate();
    Logger.i(TAG, "Initializing OrganicMaps core");
    LogsManager.INSTANCE.initFileLogging(this);

    // Set configuration directory as early as possible.
    // Other methods may explicitly use Config, which requires settingsDir to be set.
    final String settingsPath = StorageUtils.getSettingsPath(this);
    if (!StorageUtils.createDirectory(settingsPath))
      throw new AssertionError("Can't create settingsDir " + settingsPath);
    Logger.d(TAG, "Settings path = " + settingsPath);
    nativeSetSettingsDir(settingsPath);

    Config.init(this);

    mMainLoopHandler = new Handler(getMainLooper());
    ConnectionState.INSTANCE.initialize(this);

    mDisplayManager = new DisplayManager();
    mSubwayManager = new SubwayManager(this);
    mIsolinesManager = new IsolinesManager(this);
    mLocationHelper = new LocationHelper(this);
    mSensorHelper = new SensorHelper(this);
  }

  /**
   * Initialize native core of application: platform and framework.
   *
   * @throws IOException - if failed to create directories. Caller must handle
   *                     the exception and do nothing with native code if initialization is failed.
   */
  public boolean init(@NonNull Runnable onComplete) throws IOException
  {
    initNativePlatform();
    return initNativeFramework(onComplete);
  }

  public boolean arePlatformAndCoreInitialized()
  {
    return mFrameworkInitialized && mPlatformInitialized;
  }

  private void initNativePlatform() throws IOException
  {
    if (mPlatformInitialized)
      return;

    final String apkPath = StorageUtils.getApkPath(this);
    Logger.d(TAG, "Apk path = " + apkPath);
    // Note: StoragePathManager uses Config, which requires SettingsDir to be set.
    final String writablePath = StoragePathManager.findMapsStorage(this);
    Logger.d(TAG, "Writable path = " + writablePath);
    final String privatePath = StorageUtils.getPrivatePath(this);
    Logger.d(TAG, "Private path = " + privatePath);
    final String tempPath = StorageUtils.getTempPath(this);
    Logger.d(TAG, "Temp path = " + tempPath);

    // If platform directories are not created it means that native part of app will not be able
    // to work at all. So, we just ignore native part initialization in this case, e.g. when the
    // external storage is damaged or not available (read-only).
    createPlatformDirectories(writablePath, privatePath, tempPath);

    nativeInitPlatform(apkPath,
        writablePath,
        privatePath,
        tempPath,
        app.organicmaps.BuildConfig.FLAVOR,
        app.organicmaps.BuildConfig.BUILD_TYPE, UiUtils.isTablet(this));
    Config.setStoragePath(writablePath);
    Config.setStatisticsEnabled(SharedPropertiesUtils.isStatisticsEnabled(this));

    mPlatformInitialized = true;
    Logger.i(TAG, "Platform initialized");
  }

  private boolean initNativeFramework(@NonNull Runnable onComplete)
  {
    if (mFrameworkInitialized)
      return false;

    nativeInitFramework(onComplete);

    MapManager.nativeSubscribe(mStorageCallbacks);

    initNativeStrings();
    ThemeSwitcher.INSTANCE.initialize(this);
    SearchEngine.INSTANCE.initialize();
    BookmarkManager.loadBookmarks();
    TtsPlayer.INSTANCE.initialize(this);
    ThemeSwitcher.INSTANCE.restart(false);
    RoutingController.get().initialize(this);
    TrafficManager.INSTANCE.initialize();
    SubwayManager.from(this).initialize();
    IsolinesManager.from(this).initialize();
    ProcessLifecycleOwner.get().getLifecycle().addObserver(mProcessLifecycleObserver);

    Logger.i(TAG, "Framework initialized");
    mFrameworkInitialized = true;
    return true;
  }

  private void initNativeStrings()
  {
    nativeAddLocalization("core_entrance", getString(R.string.core_entrance));
    nativeAddLocalization("core_exit", getString(R.string.core_exit));
    nativeAddLocalization("core_my_places", getString(R.string.core_my_places));
    nativeAddLocalization("core_my_position", getString(R.string.core_my_position));
    nativeAddLocalization("core_placepage_unknown_place", getString(R.string.core_placepage_unknown_place));
    nativeAddLocalization("postal_code", getString(R.string.postal_code));
    nativeAddLocalization("wifi", getString(R.string.category_wifi));
  }

  private void createPlatformDirectories(@NonNull String writablePath,
                                         @NonNull String privatePath,
                                         @NonNull String tempPath) throws IOException
  {
    SharedPropertiesUtils.emulateBadExternalStorage(this);

    StorageUtils.requireDirectory(writablePath);
    StorageUtils.requireDirectory(privatePath);
    StorageUtils.requireDirectory(tempPath);
  }

  private class StorageCallbackImpl implements MapManager.StorageCallback
  {
    @Override
    public void onStatusChanged(List<MapManager.StorageCallbackData> data)
    {
      for (MapManager.StorageCallbackData item : data)
        if (item.isLeafNode && item.newStatus == CountryItem.STATUS_FAILED)
        {
          if (MapManager.nativeIsAutoretryFailed())
          {
            DownloaderNotifier.notifyDownloadFailed(MwmApplication.this, item.countryId);
          }

          return;
        }
    }

    @Override
    public void onProgress(String countryId, long localSize, long remoteSize) {}
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  private void forwardToMainThread(final long taskPointer)
  {
    Message m = Message.obtain(mMainLoopHandler, () -> nativeProcessTask(taskPointer));
    m.obj = mMainQueueToken;
    mMainLoopHandler.sendMessage(m);
  }

  private final LifecycleObserver mProcessLifecycleObserver = new DefaultLifecycleObserver() {
    @Override
    public void onStart(@NonNull LifecycleOwner owner)
    {
      OrganicMaps.this.onForeground();
    }

    @Override
    public void onStop(@NonNull LifecycleOwner owner)
    {
      OrganicMaps.this.onBackground();
    }
  };

  private void onForeground()
  {
    Logger.d(TAG);

    nativeOnTransit(true);

    mLocationHelper.resumeLocationInForeground();
  }

  private void onBackground()
  {
    Logger.d(TAG);

    nativeOnTransit(false);

    OsmUploadWork.startActionUploadOsmChanges(this);

    if (!mDisplayManager.isDeviceDisplayUsed())
      Logger.i(LOCATION_TAG, "Android Auto is active, keeping location in the background");
    else if (RoutingController.get().isNavigating())
      Logger.i(LOCATION_TAG, "Navigation is in progress, keeping location in the background");
    else if (!Map.isEngineCreated() || LocationState.getMode() == LocationState.PENDING_POSITION)
      Logger.i(LOCATION_TAG, "PENDING_POSITION mode, keeping location in the background");
    else
    {
      Logger.i(LOCATION_TAG, "Stopping location in the background");
      mLocationHelper.stop();
    }
  }

  static
  {
    System.loadLibrary("organicmaps");
  }

  private static native void nativeSetSettingsDir(String settingsPath);

  private native void nativeInitPlatform(String apkPath, String writablePath, String privatePath,
                                         String tmpPath, String flavorName, String buildType,
                                         boolean isTablet);

  private static native void nativeInitFramework(@NonNull Runnable onComplete);

  private static native void nativeProcessTask(long taskPointer);

  private static native void nativeAddLocalization(String name, String value);

  private static native void nativeOnTransit(boolean foreground);
}
