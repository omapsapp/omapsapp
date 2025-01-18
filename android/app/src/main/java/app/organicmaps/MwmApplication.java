package app.organicmaps;

import static app.organicmaps.location.LocationState.LOCATION_TAG;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
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
import app.organicmaps.routing.NavigationService;
import app.organicmaps.routing.RoutingController;
import app.organicmaps.sdk.OrganicMaps;
import app.organicmaps.util.Config;
import app.organicmaps.util.ConnectionState;
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
  private OrganicMaps mOrganicMaps;

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
  private final MapManager.StorageCallback mStorageCallbacks = new StorageCallbackImpl();

  @Nullable
  private WeakReference<Activity> mTopActivity;

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

  public OrganicMaps getOrganicMaps()
  {
    return mOrganicMaps;
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

    mOrganicMaps = new OrganicMaps(getApplicationContext());

    LogsManager.INSTANCE.initFileLogging(this);

    Android7RootCertificateWorkaround.initializeIfNeeded(this);

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

  public boolean initOrganicMaps(@NonNull Runnable onComplete) throws IOException
  {
    return mOrganicMaps.init(() -> {
      MapManager.nativeSubscribe(mStorageCallbacks);
      ProcessLifecycleOwner.get().getLifecycle().addObserver(mProcessLifecycleObserver);
      onComplete.run();
    });
  }

  private final LifecycleObserver mProcessLifecycleObserver = new DefaultLifecycleObserver()
  {
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

    mLocationHelper.resumeLocationInForeground();
  }

  private void onBackground()
  {
    Logger.d(TAG);

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
