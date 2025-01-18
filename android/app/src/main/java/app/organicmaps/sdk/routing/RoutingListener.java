package app.organicmaps.sdk.routing;

import androidx.annotation.Keep;
import androidx.annotation.MainThread;

public interface RoutingListener
{
  // Called from JNI
  @Keep
  @SuppressWarnings("unused")
  @MainThread
  void onRoutingEvent(int resultCode, String[] missingMaps);
}
