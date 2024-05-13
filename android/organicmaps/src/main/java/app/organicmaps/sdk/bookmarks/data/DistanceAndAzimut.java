package app.organicmaps.sdk.bookmarks.data;

import androidx.annotation.Keep;

import app.organicmaps.util.Distance;

// Used by JNI.
@Keep
@SuppressWarnings("unused")
public class DistanceAndAzimut
{
  private final Distance mDistance;
  private final double mAzimuth;

  public Distance getDistance()
  {
    return mDistance;
  }

  public double getAzimuth()
  {
    return mAzimuth;
  }

  public DistanceAndAzimut(Distance distance, double azimuth)
  {
    mDistance = distance;
    mAzimuth = azimuth;
  }
}
