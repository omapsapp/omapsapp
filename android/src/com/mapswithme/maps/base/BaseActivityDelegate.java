package com.mapswithme.maps.base;

import android.content.Intent;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.mapswithme.util.Config;
import com.mapswithme.util.CrashlyticsUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.ViewServer;
import com.mapswithme.util.concurrency.UiThread;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

public class BaseActivityDelegate
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.MISC);
  private static final String TAG = BaseActivityDelegate.class.getSimpleName();
  @NonNull
  private final BaseActivity mActivity;
  @Nullable
  private String mThemeName;

  public BaseActivityDelegate(@NonNull BaseActivity activity)
  {
    mActivity = activity;
  }

  public void onNewIntent(@NonNull Intent intent)
  {
    logLifecycleMethod("onNewIntent(" + intent + ")");
  }

  public void onCreate()
  {
    logLifecycleMethod("onCreate()");
    mThemeName = Config.getCurrentUiTheme(mActivity.get().getApplicationContext());
    if (!TextUtils.isEmpty(mThemeName))
      mActivity.get().setTheme(mActivity.getThemeResourceId(mThemeName));
  }

  public void onSafeCreate()
  {
    logLifecycleMethod("onSafeCreate()");
  }

  public void onSafeDestroy()
  {
    logLifecycleMethod("onSafeDestroy()");
  }

  public void onDestroy()
  {
    logLifecycleMethod("onDestroy()");
    ViewServer.get(mActivity.get()).removeWindow(mActivity.get());
  }

  public void onPostCreate()
  {
    logLifecycleMethod("onPostCreate()");
    ViewServer.get(mActivity.get()).addWindow(mActivity.get());
  }

  public void onStart()
  {
    logLifecycleMethod("onStart()");
  }

  public void onStop()
  {
    logLifecycleMethod("onStop()");
  }

  public void onResume()
  {
    logLifecycleMethod("onResume()");
    ViewServer.get(mActivity.get()).setFocusedWindow(mActivity.get());
    Utils.showOnLockScreen(Config.isShowOnLockScreenEnabled(), mActivity.get());
  }

  public void onPause()
  {
    logLifecycleMethod("onPause()");
  }

  public void onPostResume()
  {
    logLifecycleMethod("onPostResume()");
    if (!TextUtils.isEmpty(mThemeName) &&
        mThemeName.equals(Config.getCurrentUiTheme(mActivity.get().getApplicationContext())))
      return;

    // Workaround described in https://code.google.com/p/android/issues/detail?id=93731
    UiThread.runLater(() -> mActivity.get().recreate());
  }

  private void logLifecycleMethod(@NonNull String method)
  {
    String msg = mActivity.getClass().getSimpleName() + ": " + method + " activity: " + mActivity;
    CrashlyticsUtils.INSTANCE.log(Log.INFO, TAG, msg);
    LOGGER.i(TAG, msg);
  }
}
