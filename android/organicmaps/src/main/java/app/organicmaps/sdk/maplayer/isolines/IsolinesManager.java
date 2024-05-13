package app.organicmaps.sdk.maplayer.isolines;

import android.app.Application;
import android.content.Context;

import androidx.annotation.NonNull;

import app.organicmaps.sdk.Framework;
import app.organicmaps.sdk.OrganicMaps;

public class IsolinesManager
{
  @NonNull
  private final OnIsolinesChangedListener mListener;

  public IsolinesManager(@NonNull Application application)
  {
    mListener = new OnIsolinesChangedListener(application);
  }

  public boolean isEnabled()
  {
    return Framework.nativeIsIsolinesLayerEnabled();
  }

  private void registerListener()
  {
    nativeAddListener(mListener);
  }

  public void setEnabled(boolean isEnabled)
  {
    if (isEnabled == isEnabled())
      return;

    Framework.nativeSetIsolinesLayerEnabled(isEnabled);
  }

  public void initialize()
  {
    registerListener();
  }

  @NonNull
  public static IsolinesManager from(@NonNull Context context)
  {
    OrganicMaps app = (OrganicMaps) context.getApplicationContext();
    return app.getIsolinesManager();
  }

  private static native void nativeAddListener(@NonNull OnIsolinesChangedListener listener);
  private static native void nativeRemoveListener(@NonNull OnIsolinesChangedListener listener);
  private static native boolean nativeShouldShowNotification();

  public void attach(@NonNull IsolinesErrorDialogListener listener)
  {
    mListener.attach(listener);
  }

  public void detach()
  {
    mListener.detach();
  }

  public boolean shouldShowNotification()
  {
    return nativeShouldShowNotification();
  }
}
