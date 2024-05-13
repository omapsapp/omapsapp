package app.organicmaps.sdk.util;

import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;

import android.content.Context;
import android.location.Location;
import android.location.LocationManager;
import android.os.Build;
import android.provider.Settings;
import android.view.Surface;

import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;

public class LocationUtils
{
  private LocationUtils() {}

  private static final double DEFAULT_SPEED_MPS = 5;
  public static final String FUSED_PROVIDER = "fused";

  /**
   * Correct compass angles due to display orientation.
   */
  public static double correctCompassAngle(int displayOrientation, double angle)
  {
    double correction = 0;
    switch (displayOrientation)
    {
      case Surface.ROTATION_0 ->
      {
        return angle;
      }
      case Surface.ROTATION_90 -> correction = Math.PI / 2.0;
      case Surface.ROTATION_180 -> correction = Math.PI;
      case Surface.ROTATION_270 -> correction = (3.0 * Math.PI / 2.0);
    }

    return correctAngle(angle, correction);
  }

  public static double correctAngle(double angle, double correction)
  {
    double res = angle + correction;

    final double twoPI = 2.0 * Math.PI;
    res %= twoPI;

    // normalize angle into [0, 2PI]
    if (res < 0.0)
      res += twoPI;

    return res;
  }

  public static double getTimeDiff(@NonNull Location lastLocation, @NonNull Location newLocation)
  {
    return (newLocation.getElapsedRealtimeNanos() - lastLocation.getElapsedRealtimeNanos()) * 1.0E-9;
  }

  public static boolean isFromGpsProvider(@NonNull Location location)
  {
    return LocationManager.GPS_PROVIDER.equals(location.getProvider());
  }

  public static boolean isFromFusedProvider(@NonNull Location location)
  {
    return FUSED_PROVIDER.equals(location.getProvider());
  }

  public static boolean isAccuracySatisfied(@NonNull Location location)
  {
    // If it's a gps location then we completely ignore an accuracy checking,
    // because there are cases on some devices (https://jira.mail.ru/browse/MAPSME-3789)
    // when location is good, but it doesn't contain an accuracy for some reasons.
    if (isFromGpsProvider(location))
      return true;

    // Completely ignore locations without lat and lon.
    return location.getAccuracy() > 0.0f;
  }

  public static boolean isLocationBetterThanLast(@NonNull Location newLocation, @NonNull Location lastLocation)
  {
    // As described in isAccuracySatisfied, GPS may have zero accuracy "for some reasons".
    if (isFromGpsProvider(lastLocation) && lastLocation.getAccuracy() == 0.0f)
      return true;

    double speed = Math.max(DEFAULT_SPEED_MPS, (newLocation.getSpeed() + lastLocation.getSpeed()) / 2.0);
    double lastAccuracy = lastLocation.getAccuracy() + speed * LocationUtils.getTimeDiff(lastLocation, newLocation);
    return newLocation.getAccuracy() < lastAccuracy;
  }

  public static boolean areLocationServicesTurnedOn(@NonNull Context context)
  {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
    {
      final LocationManager lm = (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
      return lm.isLocationEnabled();
    }

    try
    {
      return Settings.Secure.getInt(context.getContentResolver(), Settings.Secure.LOCATION_MODE)
             != Settings.Secure.LOCATION_MODE_OFF;
    }
    catch (Settings.SettingNotFoundException e)
    {
      e.printStackTrace();
      return false;
    }
  }

  public static boolean checkFineLocationPermission(@NonNull Context context)
  {
    return ContextCompat.checkSelfPermission(context, ACCESS_FINE_LOCATION) == PERMISSION_GRANTED;
  }

  public static boolean checkCoarseLocationPermission(@NonNull Context context)
  {
    return ContextCompat.checkSelfPermission(context, ACCESS_COARSE_LOCATION) == PERMISSION_GRANTED;
  }

  public static boolean checkLocationPermission(@NonNull Context context)
  {
    return checkFineLocationPermission(context) || checkCoarseLocationPermission(context);
  }
}
