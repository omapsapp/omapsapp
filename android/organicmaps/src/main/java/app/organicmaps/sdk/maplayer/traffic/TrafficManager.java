package app.organicmaps.sdk.maplayer.traffic;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;

import app.organicmaps.sdk.util.log.Logger;

import java.util.ArrayList;
import java.util.List;

@SuppressWarnings("unused")
@MainThread
public enum TrafficManager
{
  INSTANCE;

  private static final String TAG = TrafficManager.class.getSimpleName();

  @NonNull
  private final TrafficState.StateChangeListener mStateChangeListener = new TrafficStateListener();

  @NonNull
  private TrafficState mState = TrafficState.DISABLED;

  @NonNull
  private final List<TrafficCallback> mCallbacks = new ArrayList<>();

  private boolean mInitialized = false;

  public void initialize()
  {
    Logger.d(TAG, "Initialization of traffic manager and setting the listener for traffic state changes");
    TrafficState.nativeSetListener(mStateChangeListener);
    mInitialized = true;
  }

  public void toggle()
  {
    checkInitialization();

    if (isEnabled())
      disable();
    else
      enable();
  }

  private void enable()
  {
    Logger.d(TAG, "Enable traffic");
    TrafficState.nativeEnable();
  }

  private void disable()
  {
    checkInitialization();

    Logger.d(TAG, "Disable traffic");
    TrafficState.nativeDisable();
  }
  
  public boolean isEnabled()
  {
    checkInitialization();
    return TrafficState.nativeIsEnabled();
  }

  public void attach(@NonNull TrafficCallback callback)
  {
    checkInitialization();

    if (mCallbacks.contains(callback))
    {
      throw new IllegalStateException("A callback '" + callback
                                      + "' is already attached. Check that the 'detachAll' method was called.");
    }
    Logger.d(TAG, "Attach callback '" + callback + "'");
    mCallbacks.add(callback);
    postPendingState();
  }

  private void postPendingState()
  {
    mStateChangeListener.onTrafficStateChanged(mState.ordinal());
  }

  public void detachAll()
  {
    checkInitialization();

    if (mCallbacks.isEmpty())
    {
      Logger.w(TAG, "There are no attached callbacks. Invoke the 'detachAll' method " +
               "only when it's really needed!", new Throwable());
      return;
    }

    for (TrafficCallback callback : mCallbacks)
      Logger.d(TAG, "Detach callback '" + callback + "'");
    mCallbacks.clear();
  }

  private void checkInitialization()
  {
    if (!mInitialized)
      throw new AssertionError("Traffic manager is not initialized!");
  }

  public void setEnabled(boolean enabled)
  {
    checkInitialization();

    if (isEnabled() == enabled)
      return;

    if (enabled)
      enable();
    else
      disable();
  }

  private class TrafficStateListener implements TrafficState.StateChangeListener
  {
    @Override
    @MainThread
    public void onTrafficStateChanged(int index)
    {
      TrafficState newTrafficState = TrafficState.values()[index];
      Logger.d(TAG, "onTrafficStateChanged current state = " + mState +
               " new value = " + newTrafficState);

      if (mState == newTrafficState)
        return;

      mState = newTrafficState;
      mState.activate(mCallbacks);
    }
  }

  public interface TrafficCallback
  {
    void onEnabled();
    void onDisabled();
    void onWaitingData();
    void onOutdated();
    void onNetworkError();
    void onNoData();
    void onExpiredData();
    void onExpiredApp();
  }
}
