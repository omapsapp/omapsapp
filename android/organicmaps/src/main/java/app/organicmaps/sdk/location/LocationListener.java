package app.organicmaps.sdk.location;

import android.app.PendingIntent;
import android.location.Location;

import androidx.annotation.NonNull;

public interface LocationListener
{
  void onLocationUpdated(@NonNull Location location);

  default void onLocationDisabled()
  {
    // No op.
  }

  default void onLocationResolutionRequired(@NonNull PendingIntent pendingIntent)
  {
    // No op.
  }
}
