package app.organicmaps.sdk.maplayer.isolines;

import android.app.Application;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

class OnIsolinesChangedListener
{
  @NonNull
  private final Application mApp;
  private IsolinesErrorDialogListener mListener;

  OnIsolinesChangedListener(@NonNull Application app)
  {
    mApp = app;
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  public void onStateChanged(int type)
  {
    IsolinesState state = IsolinesState.values()[type];
    if (mListener == null)
    {
      state.activate(mApp, null, null);
      return;
    }

    mListener.onStateChanged(state);
  }

  public void attach(@NonNull IsolinesErrorDialogListener listener)
  {
    mListener = listener;
  }

  public void detach()
  {
    mListener = null;
  }
}
