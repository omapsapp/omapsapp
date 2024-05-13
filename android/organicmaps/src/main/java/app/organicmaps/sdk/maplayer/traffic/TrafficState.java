package app.organicmaps.sdk.maplayer.traffic;

import androidx.annotation.Keep;
import androidx.annotation.MainThread;
import androidx.annotation.NonNull;

import java.util.List;

@SuppressWarnings("unused")
enum TrafficState
{
  DISABLED
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onDisabled();
        }
      },

  ENABLED
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onEnabled();
        }
      },

  WAITING_DATA
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onWaitingData();
        }
      },

  OUTDATED
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onOutdated();
        }
      },

  NO_DATA
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onNoData();
        }
      },

  NETWORK_ERROR
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onNetworkError();
        }
      },

  EXPIRED_DATA
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onExpiredData();
        }
      },

  EXPIRED_APP
      {
        @Override
        protected void activateInternal(@NonNull TrafficManager.TrafficCallback callback)
        {
          callback.onExpiredApp();
        }
      };

  public void activate(@NonNull List<TrafficManager.TrafficCallback> trafficCallbacks)
  {
    for (TrafficManager.TrafficCallback callback : trafficCallbacks)
      activateInternal(callback);
  }

  protected abstract void activateInternal(@NonNull TrafficManager.TrafficCallback callback);

  interface StateChangeListener
  {
    // Called from JNI.
    @Keep
    @SuppressWarnings("unused")
    @MainThread
    void onTrafficStateChanged(int state);
  }

  @MainThread
  static native void nativeSetListener(@NonNull StateChangeListener listener);

  static native void nativeRemoveListener();

  static native void nativeEnable();

  static native void nativeDisable();

  static native boolean nativeIsEnabled();
}
