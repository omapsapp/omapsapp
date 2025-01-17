package app.organicmaps.sdk.routing;

import androidx.annotation.Keep;

public interface RoutingLoadPointsListener
{
  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  void onRoutePointsLoaded(boolean success);
}
