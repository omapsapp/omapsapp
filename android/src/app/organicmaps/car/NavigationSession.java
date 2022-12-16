package app.organicmaps.car;

import android.content.Intent;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.car.app.Screen;
import androidx.car.app.Session;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;

import app.organicmaps.MwmApplication;
import app.organicmaps.car.screens.ErrorScreen;
import app.organicmaps.car.screens.NavigationScreen;

import java.io.IOException;

public final class NavigationSession extends Session implements DefaultLifecycleObserver
{
  private static final String TAG = NavigationSession.class.getSimpleName();

  private final SurfaceRenderer mSurfaceRenderer;
  private boolean mInitFailed = false;

  public NavigationSession()
  {
    getLifecycle().addObserver(this);
    mSurfaceRenderer = new SurfaceRenderer(getCarContext(), getLifecycle());
  }

  @NonNull
  @Override
  public Screen onCreateScreen(@NonNull Intent intent)
  {
    Log.d(TAG, "onCreateScreen()");
    if (mInitFailed)
      return new ErrorScreen(getCarContext());

    return new NavigationScreen(getCarContext(), mSurfaceRenderer);
  }

  @Override
  public void onCreate(@NonNull LifecycleOwner owner)
  {
    Log.d(TAG, "onCreate()");
    init();
  }

  @Override
  public void onResume(@NonNull LifecycleOwner owner)
  {
    Log.d(TAG, "onResume()");
    init();
  }

  private void init()
  {
    mInitFailed = false;
    MwmApplication app = MwmApplication.from(getCarContext());
    try
    {
      app.init();
    } catch (IOException e)
    {
      mInitFailed = true;
      Log.e(TAG, "Failed to initialize the app.");
    }
  }
}
