package app.organicmaps;

import static app.organicmaps.location.LocationState.LOCATION_TAG;

import android.app.Activity;
import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.ProcessLifecycleOwner;

import app.organicmaps.background.OsmUploadWork;
import app.organicmaps.downloader.Android7RootCertificateWorkaround;
import app.organicmaps.downloader.DownloaderNotifier;
import app.organicmaps.bookmarks.data.BookmarkManager;
import app.organicmaps.display.DisplayManager;
import app.organicmaps.downloader.CountryItem;
import app.organicmaps.downloader.MapManager;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.location.LocationState;
import app.organicmaps.location.SensorHelper;
import app.organicmaps.location.TrackRecorder;
import app.organicmaps.location.TrackRecordingService;
import app.organicmaps.maplayer.isolines.IsolinesManager;
import app.organicmaps.maplayer.subway.SubwayManager;
import app.organicmaps.maplayer.traffic.TrafficManager;
import app.organicmaps.routing.NavigationService;
import app.organicmaps.routing.RoutingController;
import app.organicmaps.search.SearchEngine;
import app.organicmaps.settings.StoragePathManager;
import app.organicmaps.sound.TtsPlayer;
import app.organicmaps.util.Config;
import app.organicmaps.util.ConnectionState;
import app.organicmaps.util.Language;
import app.organicmaps.util.SharedPropertiesUtils;
import app.organicmaps.util.StorageUtils;
import app.organicmaps.util.ThemeSwitcher;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.util.log.Logger;
import app.organicmaps.util.log.LogsManager;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.List;

public class MwmApplication extends Application implements Application.ActivityLifecycleCallbacks
{
  @NonNull
  private static final String TAG = MwmApplication.class.getSimpleName();

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

  private volatile boolean mFrameworkInitialized;
  private volatile boolean mPlatformInitialized;

  @NonNull
  private final MapManager.StorageCallback mStorageCallbacks = new StorageCallbackImpl();

  @Nullable
  private WeakReference<Activity> mTopActivity;

  private BroadcastReceiver mLocaleChangeReceiver;

  @UiThread
  @Nullable
  public Activity getTopActivity()
  {
    return mTopActivity != null ? mTopActivity.get() : null;
  }

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
  public static MwmApplication from(@NonNull Context context)
  {
    return (MwmApplication) context.getApplicationContext();
  }

  @NonNull
  public static SharedPreferences prefs(@NonNull Context context)
  {
    return context.getSharedPreferences(context.getString(R.string.pref_file_name), MODE_PRIVATE);
  }

  @Override
  public void onCreate()
  {
    super.onCreate();
    Logger.i(TAG, "Initializing application");
    LogsManager.INSTANCE.initFileLogging(this);

    Android7RootCertificateWorkaround.initializeIfNeeded(this);

    // Set configuration directory as early as possible.
    // Other methods may explicitly use Config, which requires settingsDir to be set.
    final String settingsPath = StorageUtils.getSettingsPath(this);
    if (!StorageUtils.createDirectory(settingsPath))
      throw new AssertionError("Can't create settingsDir " + settingsPath);
    Logger.d(TAG, "Settings path = " + settingsPath);
    nativeSetSettingsDir(settingsPath);

    Config.init(this);

    ConnectionState.INSTANCE.initialize(this);

    DownloaderNotifier.createNotificationChannel(this);
    NavigationService.createNotificationChannel(this);
    TrackRecordingService.createNotificationChannel(this);

    registerActivityLifecycleCallbacks(this);
    mSubwayManager = new SubwayManager(this);
    mIsolinesManager = new IsolinesManager(this);
    mLocationHelper = new LocationHelper(this);
    mSensorHelper = new SensorHelper(this);
    mDisplayManager = new DisplayManager();
  }

  /**
   * Initialize native core of application: platform and framework.
   *
   * @throws IOException - if failed to create directories. Caller must handle
   * the exception and do nothing with native code if initialization is failed.
   */
  public boolean init(@NonNull Runnable onComplete) throws IOException
  {
    initNativePlatform();
    return initNativeFramework(onComplete);
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

    nativeInitPlatform(getApplicationContext(),
                       apkPath,
                       writablePath,
                       privatePath,
                       tempPath,
                       app.organicmaps.BuildConfig.FLAVOR,
                       app.organicmaps.BuildConfig.BUILD_TYPE, UiUtils.isTablet(this));
    Config.setStoragePath(writablePath);
    Config.setStatisticsEnabled(SharedPropertiesUtils.isStatisticsEnabled(this));

    // Force native system locale initialization at app start-up.
    Language.nativeRefreshSystemLocale();

    // Setup BroadcastReceiver to receive changes of system locale.
    mLocaleChangeReceiver = new BroadcastReceiver()
    {
      @Override
      public void onReceive(Context context, Intent intent)
      {
        // Refresh the native c++ system locale.
        Language.nativeRefreshSystemLocale();
      }
    };

    registerReceiver(mLocaleChangeReceiver, new IntentFilter(Intent.ACTION_LOCALE_CHANGED));

    mPlatformInitialized = true;
    Logger.i(TAG, "Platform initialized");
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

  public boolean arePlatformAndCoreInitialized()
  {
    return mFrameworkInitialized && mPlatformInitialized;
  }

  static
  {
    System.loadLibrary("organicmaps");
  }

  private static native void nativeSetSettingsDir(String settingsPath);
  private static native void nativeInitPlatform(Context context, String apkPath, String writablePath,
                                                String privatePath, String tmpPath, String flavorName,
                                                String buildType, boolean isTablet);
  private static native void nativeInitFramework(@NonNull Runnable onComplete);
  private static native void nativeAddLocalization(String name, String value);
  private static native void nativeOnTransit(boolean foreground);

  private final LifecycleObserver mProcessLifecycleObserver = new DefaultLifecycleObserver() {
    @Override
    public void onStart(@NonNull LifecycleOwner owner)
    {
      MwmApplication.this.onForeground();
    }

    @Override
    public void onStop(@NonNull LifecycleOwner owner)
    {
      MwmApplication.this.onBackground();
    }
  };

  @Override
  public void onActivityCreated(@NonNull Activity activity, @Nullable Bundle savedInstanceState)
  {}

  @Override
  public void onActivityStarted(@NonNull Activity activity)
  {}

  @Override
  public void onActivityResumed(@NonNull Activity activity)
  {
    Logger.d(TAG, "activity = " + activity);
    Utils.showOnLockScreen(Config.isShowOnLockScreenEnabled(), activity);
    mSensorHelper.setRotation(activity.getWindowManager().getDefaultDisplay().getRotation());
    mTopActivity = new WeakReference<>(activity);
  }

  @Override
  public void onActivityPaused(@NonNull Activity activity)
  {
    Logger.d(TAG, "activity = " + activity);
    mTopActivity = null;
  }

  @Override
  public void onActivityStopped(@NonNull Activity activity)
  {}

  @Override
  public void onActivitySaveInstanceState(@NonNull Activity activity, @NonNull Bundle outState)
  {
    Logger.d(TAG, "activity = " + activity + " outState = " + outState);
  }

  @Override
  public void onActivityDestroyed(@NonNull Activity activity)
  {
    Logger.d(TAG, "activity = " + activity);
  }

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
    else if (TrackRecorder.nativeIsTrackRecordingEnabled())
      Logger.i(LOCATION_TAG, "Track Recordr is active, keeping location in the background");
    else
    {
      Logger.i(LOCATION_TAG, "Stopping location in the background");
      mLocationHelper.stop();
    }
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
}
