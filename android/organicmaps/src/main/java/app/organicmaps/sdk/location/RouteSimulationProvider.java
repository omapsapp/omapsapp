package app.organicmaps.sdk.location;

import android.content.Context;
import android.location.Location;

import androidx.annotation.NonNull;

import app.organicmaps.routing.JunctionInfo;
import app.organicmaps.util.LocationUtils;
import app.organicmaps.util.concurrency.UiThread;
import app.organicmaps.util.log.Logger;

class RouteSimulationProvider extends BaseLocationProvider
{
  private static final String TAG = RouteSimulationProvider.class.getSimpleName();
  private static final long INTERVAL_MS = 1000;

  private final JunctionInfo[] mPoints;
  private int mCurrentPoint = 0;
  private boolean mActive = false;

  RouteSimulationProvider(@NonNull Context context, @NonNull Listener listener, JunctionInfo[] points)
  {
    super(listener);
    mPoints = points;
  }

  @Override
  public void start(long interval)
  {
    Logger.i(TAG);
    if (mActive)
      throw new IllegalStateException("Already started");
    mActive = true;
    UiThread.runLater(this::nextPoint);
  }

  @Override
  public void stop()
  {
    Logger.i(TAG);
    mActive = false;
  }

  public void nextPoint()
  {
    if (!mActive)
      return;
    if (mCurrentPoint >= mPoints.length)
    {
      Logger.i(TAG, "Finished the final point");
      mActive = false;
      return;
    }

    final Location location = new Location(LocationUtils.FUSED_PROVIDER);
    location.setLatitude(mPoints[mCurrentPoint].mLat);
    location.setLongitude(mPoints[mCurrentPoint].mLon);
    location.setAccuracy(1.0f);
    mListener.onLocationChanged(location);
    mCurrentPoint += 1;
    UiThread.runLater(this::nextPoint, INTERVAL_MS);
  }
}
