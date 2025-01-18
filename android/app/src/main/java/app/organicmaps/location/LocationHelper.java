package app.organicmaps.location;

import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;

import android.annotation.SuppressLint;
import android.app.PendingIntent;
import android.content.Context;
import android.location.Location;
import android.location.LocationManager;
import android.os.Handler;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresPermission;
import androidx.annotation.UiThread;
import androidx.core.content.ContextCompat;
import androidx.core.location.GnssStatusCompat;
import androidx.core.location.LocationManagerCompat;

import org.chromium.base.ObserverList;

import app.organicmaps.Framework;
import app.organicmaps.Map;
import app.organicmaps.MwmApplication;
import app.organicmaps.bookmarks.data.FeatureId;
import app.organicmaps.bookmarks.data.MapObject;
import app.organicmaps.sdk.routing.JunctionInfo;
import app.organicmaps.routing.RoutingController;
import app.organicmaps.util.Config;
import app.organicmaps.util.LocationUtils;
import app.organicmaps.util.NetworkPolicy;
import app.organicmaps.util.log.Logger;

public class LocationHelper implements BaseLocationProvider.Listener
{
  private static final long INTERVAL_FOLLOW_MS = 0;
  private static final long INTERVAL_NOT_FOLLOW_MS = 3000;
  private static final long INTERVAL_NAVIGATION_MS = 0;
  private static final long INTERVAL_TRACK_RECORDING = 0;

  private static final long AGPS_EXPIRATION_TIME_MS = 16 * 60 * 60 * 1000; // 16 hours
  private static final long LOCATION_UPDATE_TIMEOUT_MS = 30 * 1000; // 30 seconds

  @NonNull
  private final Context mContext;

  private static final String TAG = LocationState.LOCATION_TAG;

  private final ObserverList<LocationListener> mListeners = new ObserverList<>();
  private final ObserverList.RewindableIterator<LocationListener> mListenersIterator = mListeners.rewindableIterator();

  @Nullable
  private Location mSavedLocation;
  private MapObject mMyPosition;
  @NonNull
  private BaseLocationProvider mLocationProvider;
  private long mInterval;
  private boolean mInFirstRun;
  private boolean mActive;
  private Handler mHandler;
  private Runnable mLocationTimeoutRunnable = this::notifyLocationUpdateTimeout;

  @NonNull
  private final GnssStatusCompat.Callback mGnssStatusCallback = new GnssStatusCompat.Callback()
  {
    @Override
    public void onStarted()
    {
      Logger.d(TAG);
    }

    @Override
    public void onStopped()
    {
      Logger.d(TAG);
    }

    @Override
    public void onFirstFix(int ttffMillis)
    {
      Logger.d(TAG, "ttffMillis = " + ttffMillis);
    }

    @Override
    public void onSatelliteStatusChanged(@NonNull GnssStatusCompat status)
    {
      int used = 0;
      boolean fixed = false;
      for (int i = 0; i < status.getSatelliteCount(); i++)
      {
        if (status.usedInFix(i))
        {
          used++;
          fixed = true;
        }
      }
      Logger.d(TAG, "total = " + status.getSatelliteCount() + " used = " + used + " fixed = " + fixed);
    }
  };

  @NonNull
  public static LocationHelper from(@NonNull Context context)
  {
    return MwmApplication.from(context).getLocationHelper();
  }

  public LocationHelper(@NonNull Context context)
  {
    mContext = context;
    mLocationProvider = LocationProviderFactory.getProvider(mContext, this);
    mHandler = new Handler();
  }

  /**
   * @return MapObject.MY_POSITION, null if location is not yet determined or "My position" button is switched off.
   */
  @Nullable
  public MapObject getMyPosition()
  {
    if (!isActive())
    {
      mMyPosition = null;
      return null;
    }

    if (mSavedLocation == null)
      return null;

    if (mMyPosition == null)
      mMyPosition = MapObject.createMapObject(FeatureId.EMPTY, MapObject.MY_POSITION, "", "",
                                  mSavedLocation.getLatitude(), mSavedLocation.getLongitude());

    return mMyPosition;
  }

  /**
   * Obtains last known location.
   * @return {@code null} if no location is saved.
   */
  @Nullable
  public Location getSavedLocation() { return mSavedLocation; }

  /**
   * Indicates about whether a location provider is polling location updates right now or not.
   */
  public boolean isActive()
  {
    return mActive;
  }

  private void notifyLocationUpdated()
  {
    if (mSavedLocation == null)
      throw new IllegalStateException("No saved location");

    mHandler.removeCallbacks(mLocationTimeoutRunnable);
    mHandler.postDelayed(mLocationTimeoutRunnable, LOCATION_UPDATE_TIMEOUT_MS); // Reset the timeout.

    mListenersIterator.rewind();
    while (mListenersIterator.hasNext())
      mListenersIterator.next().onLocationUpdated(mSavedLocation);

    // If we are still in the first run mode, i.e. user is staying on the first run screens,
    // not on the map, we mustn't post location update to the core. Only this preserving allows us
    // to play nice zoom animation once a user will leave first screens and will see a map.
    if (mInFirstRun)
    {
      Logger.d(TAG, "Location update is obtained and must be ignored, because the app is in a first run mode");
      return;
    }

    LocationState.nativeLocationUpdated(mSavedLocation.getTime(),
        mSavedLocation.getLatitude(),
        mSavedLocation.getLongitude(),
        mSavedLocation.getAccuracy(),
        mSavedLocation.getAltitude(),
        mSavedLocation.getSpeed(),
        mSavedLocation.getBearing());
  }

  private void notifyLocationUpdateTimeout()
  {
    mHandler.removeCallbacks(mLocationTimeoutRunnable);
    if (!isActive())
    {
      Logger.w(TAG, "Provider is not active");
      return;
    }

    Logger.d(TAG);
    mListenersIterator.rewind();
    while (mListenersIterator.hasNext())
      mListenersIterator.next().onLocationUpdateTimeout();
  }

  @Override
  public void onLocationChanged(@NonNull Location location)
  {
    Logger.d(TAG, "provider = " + mLocationProvider.getClass().getSimpleName() + " location = " + location);

    if (!isActive())
    {
      Logger.w(TAG, "Provider is not active");
      return;
    }

    if (!LocationUtils.isAccuracySatisfied(location))
    {
      Logger.w(TAG, "Unsatisfied accuracy for location = " + location);
      return;
    }

    if (mSavedLocation != null)
    {
      if (!LocationUtils.isFromFusedProvider(location) && !LocationUtils.isLocationBetterThanLast(location, mSavedLocation))
      {
        Logger.d(TAG, "The new " + location + " is worse than the last " + mSavedLocation);
        return;
      }
    }

    mSavedLocation = location;
    mMyPosition = null;
    notifyLocationUpdated();
  }

  // Used by GoogleFusedLocationProvider.
  @SuppressWarnings("unused")
  @Override
  @UiThread
  public void onLocationResolutionRequired(@NonNull PendingIntent pendingIntent)
  {
    Logger.d(TAG);

    if (!isActive())
    {
      Logger.w(TAG, "Provider is not active");
      return;
    }

    // Stop provider until location resolution is granted.
    stop();
    LocationState.nativeOnLocationError(LocationState.ERROR_GPS_OFF);

    mListenersIterator.rewind();
    while (mListenersIterator.hasNext())
      mListenersIterator.next().onLocationResolutionRequired(pendingIntent);
  }

  // Used by GoogleFusedLocationProvider.
  @SuppressWarnings("unused")
  @RequiresPermission(anyOf = {ACCESS_COARSE_LOCATION, ACCESS_FINE_LOCATION})
  @Override
  @UiThread
  public void onFusedLocationUnsupported()
  {
    // Try to downgrade to the native provider first and restart the service before notifying the user.
    Logger.d(TAG, "provider = " + mLocationProvider.getClass().getSimpleName() + " is not supported," +
        " downgrading to use native provider");
    mLocationProvider.stop();
    mLocationProvider = new AndroidNativeProvider(mContext, this);
    mActive = true;
    mLocationProvider.start(mInterval);
  }

  // RouteSimulationProvider doesn't really require location permissions.
  @SuppressLint("MissingPermission")
  public void startNavigationSimulation(JunctionInfo[] points)
  {
    Logger.i(TAG);
    mLocationProvider.stop();
    mLocationProvider = new RouteSimulationProvider(mContext, this, points);
    mActive = true;
    mLocationProvider.start(mInterval);
  }

  @Override
  @UiThread
  public void onLocationDisabled()
  {
    Logger.d(TAG, "provider = " + mLocationProvider.getClass().getSimpleName() +
        " settings = " + LocationUtils.areLocationServicesTurnedOn(mContext));

    stop();
    LocationState.nativeOnLocationError(LocationState.ERROR_GPS_OFF);

    mListenersIterator.rewind();
    while (mListenersIterator.hasNext())
      mListenersIterator.next().onLocationDisabled();
  }

  /**
   * Registers listener to obtain location updates.
   *
   * @param listener    listener to be registered.
   */
  @UiThread
  public void addListener(@NonNull LocationListener listener)
  {
    Logger.d(TAG, "listener: " + listener + " count was: " + mListeners.size());

    mListeners.addObserver(listener);
    if (mSavedLocation != null)
      listener.onLocationUpdated(mSavedLocation);
  }

  /**
   * Removes given location listener.
   * @param listener listener to unregister.
   */
  @UiThread
  public void removeListener(@NonNull LocationListener listener)
  {
    Logger.d(TAG, "listener: " + listener + " count was: " + mListeners.size());
    mListeners.removeObserver(listener);
  }

  private long calcLocationUpdatesInterval()
  {
    if (RoutingController.get().isNavigating())
      return INTERVAL_NAVIGATION_MS;

    if (TrackRecorder.nativeIsTrackRecordingEnabled())
      return INTERVAL_TRACK_RECORDING;

    final int mode = Map.isEngineCreated() ? LocationState.getMode() : LocationState.NOT_FOLLOW_NO_POSITION;
    return switch (mode)
    {
      case LocationState.PENDING_POSITION, LocationState.FOLLOW, LocationState.FOLLOW_AND_ROTATE ->
          INTERVAL_FOLLOW_MS;
      case LocationState.NOT_FOLLOW, LocationState.NOT_FOLLOW_NO_POSITION -> INTERVAL_NOT_FOLLOW_MS;
      default -> throw new IllegalArgumentException("Unsupported location mode: " + mode);
    };
  }

  /**
   * Restart the location with a new refresh interval if changed.
   */
  @RequiresPermission(anyOf = {ACCESS_COARSE_LOCATION, ACCESS_FINE_LOCATION})
  public void restartWithNewMode()
  {
    if (!isActive())
    {
      start();
      return;
    }

    final long newInterval = calcLocationUpdatesInterval();
    if (newInterval == mInterval)
      return;

    Logger.i(TAG, "update refresh interval: old = " + mInterval + " new = " + newInterval);
    mLocationProvider.stop();
    mInterval = newInterval;
    mLocationProvider.start(newInterval);
  }

  /**
   * Starts polling location updates.
   */
  @RequiresPermission(anyOf = {ACCESS_COARSE_LOCATION, ACCESS_FINE_LOCATION})
  public void start()
  {
    if (isActive())
    {
      Logger.d(TAG, "Already started");
      return;
    }

    Logger.i(TAG);
    checkForAgpsUpdates();

    if (LocationUtils.checkFineLocationPermission(mContext))
      SensorHelper.from(mContext).start();

    final long oldInterval = mInterval;
    mInterval = calcLocationUpdatesInterval();
    Logger.i(TAG, "provider = " + mLocationProvider.getClass().getSimpleName() +
        " mInFirstRun = " + mInFirstRun + " oldInterval = " + oldInterval + " interval = " + mInterval);
    mActive = true;
    mLocationProvider.start(mInterval);
    mHandler.postDelayed(mLocationTimeoutRunnable, LOCATION_UPDATE_TIMEOUT_MS);
    subscribeToGnssStatusUpdates();
  }

  /**
   * Stops the polling location updates.
   */
  public void stop()
  {
    if (!isActive())
    {
      Logger.d(TAG, "Already stopped");
      return;
    }

    Logger.i(TAG);
    mLocationProvider.stop();
    unsubscribeFromGnssStatusUpdates();
    SensorHelper.from(mContext).stop();
    mHandler.removeCallbacks(mLocationTimeoutRunnable);
    mActive = false;
  }

  /**
   * Resume location services when entering the foreground.
   */
  public void resumeLocationInForeground()
  {
    if (isActive())
      return;
    else if (!Map.isEngineCreated())
    {
      // LocationState.nativeGetMode() is initialized only after drape creation.
      // https://github.com/organicmaps/organicmaps/issues/1128#issuecomment-1784435190
      Logger.d(TAG, "Engine is not created yet.");
      return;
    }
    else if (LocationState.getMode() == LocationState.NOT_FOLLOW_NO_POSITION)
    {
      Logger.i(TAG, "Location updates are stopped by the user manually.");
      return;
    }
    else if (!LocationUtils.checkLocationPermission(mContext))
    {
      Logger.i(TAG, "Permissions ACCESS_FINE_LOCATION and ACCESS_COARSE_LOCATION are not granted");
      return;
    }

    start();
  }

  private void checkForAgpsUpdates()
  {
    if (!NetworkPolicy.getCurrentNetworkUsageStatus())
      return;

    long previousTimestamp = Config.getAgpsTimestamp();
    long currentTimestamp = System.currentTimeMillis();
    if (previousTimestamp + AGPS_EXPIRATION_TIME_MS > currentTimestamp)
    {
      Logger.d(TAG, "A-GPS should be up to date");
      return;
    }

    Logger.d(TAG, "Requesting new A-GPS data");
    Config.setAgpsTimestamp(currentTimestamp);
    final LocationManager manager = (LocationManager) mContext.getSystemService(Context.LOCATION_SERVICE);
    manager.sendExtraCommand(LocationManager.GPS_PROVIDER, "force_xtra_injection", null);
    manager.sendExtraCommand(LocationManager.GPS_PROVIDER, "force_time_injection", null);
  }

  private void subscribeToGnssStatusUpdates()
  {
    // Subscribe to the low-level GNSS status to keep the green dot location indicator always firing.
    // https://github.com/organicmaps/organicmaps/issues/5999#issuecomment-1793713369
    if (!LocationUtils.checkFineLocationPermission(mContext))
      return;
    final LocationManager locationManager = (LocationManager) mContext.getSystemService(Context.LOCATION_SERVICE);
    LocationManagerCompat.registerGnssStatusCallback(locationManager, ContextCompat.getMainExecutor(mContext),
        mGnssStatusCallback);
  }

  private void unsubscribeFromGnssStatusUpdates()
  {
    final LocationManager locationManager = (LocationManager) mContext.getSystemService(Context.LOCATION_SERVICE);
    LocationManagerCompat.unregisterGnssStatusCallback(locationManager, mGnssStatusCallback);
  }

  @UiThread
  public boolean isInFirstRun()
  {
    return mInFirstRun;
  }

  @UiThread
  public void onEnteredIntoFirstRun()
  {
    Logger.i(TAG);
    mInFirstRun = true;
  }

  @UiThread
  public void onExitFromFirstRun()
  {
    Logger.i(TAG);
    if (!mInFirstRun)
      throw new AssertionError("Must be called only after 'onEnteredIntoFirstRun' method!");

    mInFirstRun = false;

    // If there is a location we need just to pass it to the listeners, so that
    // my position state machine will be switched to the FOLLOW state.
    if (mSavedLocation != null)
    {
      notifyLocationUpdated();
      Logger.d(TAG, "Current location is available, so play the nice zoom animation");
      Framework.nativeRunFirstLaunchAnimation();
    }
  }
}
