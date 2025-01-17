package app.organicmaps.sdk.routing;

import androidx.annotation.Keep;
import androidx.annotation.MainThread;

public interface RoutingProgressListener
{
  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @MainThread
  void onRouteBuildingProgress(float progress);
}
