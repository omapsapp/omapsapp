package app.organicmaps.sdk.util;

import androidx.annotation.NonNull;

import app.organicmaps.sdk.bookmarks.data.ParcelablePointD;

public class GeoUtils
{
  @NonNull
  static public ParcelablePointD toLatLon(double merX, double merY)
  {
    return nativeToLatLon(merX, merY);
  }

  @NonNull
  private static native ParcelablePointD nativeToLatLon(double merX, double merY);
}
