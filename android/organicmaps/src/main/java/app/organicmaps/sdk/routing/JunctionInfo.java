package app.organicmaps.sdk.routing;

import androidx.annotation.Keep;

// Used by JNI.
@Keep
@SuppressWarnings("unused")
public class JunctionInfo
{
  public final double mLat;
  public final double mLon;

  public JunctionInfo(double lat, double lon)
  {
    mLat = lat;
    mLon = lon;
  }
}
