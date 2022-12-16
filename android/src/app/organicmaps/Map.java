package app.organicmaps;

import android.content.Context;
import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.Surface;

import androidx.annotation.Nullable;

import app.organicmaps.location.LocationHelper;
import app.organicmaps.util.Config;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.concurrency.UiThread;
import app.organicmaps.util.log.Logger;

public final class Map
{
  public interface CallbackUnsupported
  {
    void report();
  }

  public static final String ARG_LAUNCH_BY_DEEP_LINK = "launch_by_deep_link";
  private static final String TAG = Map.class.getSimpleName();

  // Should correspond to android::MultiTouchAction from Framework.cpp
  public static final int NATIVE_ACTION_UP = 0x01;
  public static final int NATIVE_ACTION_DOWN = 0x02;
  public static final int NATIVE_ACTION_MOVE = 0x03;
  public static final int NATIVE_ACTION_CANCEL = 0x04;

  // Should correspond to gui::EWidget from skin.hpp
  public static final int WIDGET_RULER = 0x01;
  public static final int WIDGET_COMPASS = 0x02;
  public static final int WIDGET_COPYRIGHT = 0x04;
  public static final int WIDGET_SCALE_FPS_LABEL = 0x08;

  // Should correspond to dp::Anchor from drape_global.hpp
  public static final int ANCHOR_CENTER = 0x00;
  public static final int ANCHOR_LEFT = 0x01;
  public static final int ANCHOR_RIGHT = (ANCHOR_LEFT << 1);
  public static final int ANCHOR_TOP = (ANCHOR_RIGHT << 1);
  public static final int ANCHOR_BOTTOM = (ANCHOR_TOP << 1);
  public static final int ANCHOR_LEFT_TOP = (ANCHOR_LEFT | ANCHOR_TOP);
  public static final int ANCHOR_RIGHT_TOP = (ANCHOR_RIGHT | ANCHOR_TOP);
  public static final int ANCHOR_LEFT_BOTTOM = (ANCHOR_LEFT | ANCHOR_BOTTOM);
  public static final int ANCHOR_RIGHT_BOTTOM = (ANCHOR_RIGHT | ANCHOR_BOTTOM);

  // Should correspond to df::TouchEvent::INVALID_MASKED_POINTER from user_event_stream.cpp
  public static final int INVALID_POINTER_MASK = 0xFF;
  public static final int INVALID_TOUCH_ID = -1;

  private final long mEngineId;

  private int mCurrentCompassOffsetX;
  private int mCurrentCompassOffsetY;
  private int mBottomWidgetOffsetX;
  private int mBottomWidgetOffsetY;

  private int mHeight;
  private int mWidth;
  private boolean mRequireResize;
  private boolean mSurfaceCreated;
  private boolean mSurfaceAttached;
  private boolean mLaunchByDeepLink;
  @Nullable
  private String mUiThemeOnPause;
  @Nullable
  private MapRenderingListener mMapRenderingListener;
  @Nullable
  private CallbackUnsupported mCallbackUnsupported;

  public Map()
  {
    mEngineId = nativeCreateEngineId();
    Logger.d(TAG, "Created engineId: " + mEngineId);
    onCreate(false);
  }

  /**
   * Moves the map compass using the given offsets.
   *
   * @param context     Context.
   * @param offsetX     Pixel offset from the top. -1 to keep the previous value.
   * @param offsetY     Pixel offset from the right.  -1 to keep the previous value.
   * @param forceRedraw True to force the compass to redraw
   */
  public void setupCompass(final Context context, int offsetX, int offsetY, boolean forceRedraw)
  {
    final int x = offsetX < 0 ? mCurrentCompassOffsetX : offsetX;
    final int y = offsetY < 0 ? mCurrentCompassOffsetY : offsetY;
    final int navPadding = UiUtils.dimen(context, R.dimen.nav_frame_padding);
    final int marginX = UiUtils.dimen(context, R.dimen.margin_compass) + navPadding;
    final int marginY = UiUtils.dimen(context, R.dimen.margin_compass_top) + navPadding;
    nativeSetupWidget(mEngineId, WIDGET_COMPASS, mWidth - x - marginX, y + marginY, ANCHOR_CENTER);
    if (forceRedraw && mSurfaceCreated)
      nativeApplyWidgets(mEngineId);
    mCurrentCompassOffsetX = x;
    mCurrentCompassOffsetY = y;
  }

  public static void onCompassUpdated(double north, boolean forceRedraw)
  {
    nativeCompassUpdated(north, forceRedraw);
  }

  /**
   * Moves the ruler and copyright using the given offsets.
   *
   * @param context Context.
   * @param offsetX Pixel offset from the left.  -1 to keep the previous value.
   * @param offsetY Pixel offset from the bottom. -1 to keep the previous value.
   */
  public void setupBottomWidgetsOffset(final Context context, int offsetX, int offsetY)
  {
    final int x = offsetX < 0 ? mBottomWidgetOffsetX : offsetX;
    final int y = offsetY < 0 ? mBottomWidgetOffsetY : offsetY;
    setupRuler(context, x, y);
    setupAttribution(context, x, y);
    mBottomWidgetOffsetX = x;
    mBottomWidgetOffsetY = y;
  }

  public void onSurfaceCreated(final Context context, final Surface surface, Rect surfaceFrame, int surfaceDpi)
  {
    if (nativeIsEngineCreated(mEngineId))
      nativeDetachSurface(mEngineId, true);

    if (isThemeChangingProcess(context))
    {
      Logger.d(TAG, "Theme changing process, skip 'onSurfaceCreated' callback");
      return;
    }

    Logger.d(TAG, "mSurfaceCreated = " + mSurfaceCreated);
    if (nativeIsEngineCreated(mEngineId))
    {
      if (!nativeAttachSurface(mEngineId, surface))
      {
        if (mCallbackUnsupported != null)
          mCallbackUnsupported.report();
        return;
      }
      mSurfaceCreated = true;
      mSurfaceAttached = true;
      mRequireResize = true;
      nativeResumeSurfaceRendering(mEngineId);
      return;
    }

    mRequireResize = false;
    setupWidgets(context, surfaceFrame.width(), surfaceFrame.height());

    final boolean firstStart = LocationHelper.INSTANCE.isInFirstRun();
    if (!nativeCreateEngine(mEngineId, surface, surfaceDpi, firstStart, mLaunchByDeepLink, BuildConfig.VERSION_CODE))
    {
      if (mCallbackUnsupported != null)
        mCallbackUnsupported.report();
      return;
    }

    if (firstStart)
      UiThread.runLater(LocationHelper.INSTANCE::onExitFromFirstRun);

    mSurfaceCreated = true;
    mSurfaceAttached = true;
    nativeResumeSurfaceRendering(mEngineId);
    if (mMapRenderingListener != null)
      mMapRenderingListener.onRenderingCreated();
  }

  public void onSurfaceChanged(final Context context, final Surface surface, Rect surfaceFrame, boolean isSurfaceCreating)
  {
    if (isThemeChangingProcess(context))
    {
      Logger.d(TAG, "Theme changing process, skip 'onSurfaceChanged' callback");
      return;
    }

    Logger.d(TAG, "mSurfaceCreated = " + mSurfaceCreated);
    if (!mSurfaceCreated || (!mRequireResize && isSurfaceCreating))
      return;

    nativeSurfaceChanged(mEngineId, surface, surfaceFrame.width(), surfaceFrame.height());

    mRequireResize = false;
    setupWidgets(context, surfaceFrame.width(), surfaceFrame.height());
    nativeApplyWidgets(mEngineId);
    if (mMapRenderingListener != null)
      mMapRenderingListener.onRenderingRestored();
  }

  public void onSurfaceDestroyed(boolean activityIsChangingConfigurations, boolean isAdded)
  {
    Logger.d(TAG, "mSurfaceCreated = " + mSurfaceCreated + ", mSurfaceAttached = " + mSurfaceAttached + ", isAdded = " + isAdded);
    if (!mSurfaceCreated || !mSurfaceAttached || !isAdded)
      return;

    nativeDetachSurface(mEngineId, !activityIsChangingConfigurations);
    mSurfaceCreated = !nativeDestroySurfaceOnDetach(mEngineId);
    mSurfaceAttached = false;
  }

  public void setMapRenderingListener(MapRenderingListener mapRenderingListener)
  {
    mMapRenderingListener = mapRenderingListener;
  }

  public void setCallbackUnsupported(CallbackUnsupported callback)
  {
    mCallbackUnsupported = callback;
  }

  public void onCreate(boolean launchByDeeplink)
  {
    mLaunchByDeepLink = launchByDeeplink;
    mCurrentCompassOffsetX = 0;
    mCurrentCompassOffsetY = 0;
    mBottomWidgetOffsetX = 0;
    mBottomWidgetOffsetY = 0;
  }

  public void onStart()
  {
    nativeSetRenderingInitializationFinishedListener(mEngineId, mMapRenderingListener);
  }

  public void onStop()
  {
    nativeSetRenderingInitializationFinishedListener(mEngineId, null);
  }

  public void onPause(final Context context)
  {
    mUiThemeOnPause = Config.getCurrentUiTheme(context);

    // Pause/Resume can be called without surface creation/destroy.
    if (mSurfaceAttached)
      nativePauseSurfaceRendering(mEngineId);
  }

  public void onResume()
  {
    // Pause/Resume can be called without surface creation/destroy.
    if (mSurfaceAttached)
      nativeResumeSurfaceRendering(mEngineId);
  }

  boolean isContextCreated()
  {
    return mSurfaceCreated;
  }

  public void onScroll(float distanceX, float distanceY)
  {
    nativeMove(mEngineId, -distanceX / ((float) mWidth), distanceY / ((float) mHeight), false);
  }

  public void zoomIn()
  {
    nativeScalePlus(mEngineId);
  }

  public void zoomOut()
  {
    nativeScaleMinus(mEngineId);
  }

  public void onScale(double factor, double focusX, double focusY, boolean isAnim)
  {
    nativeScale(mEngineId, factor, focusX, focusY, isAnim);
  }

  public void onTouch(int actionType, MotionEvent event, int pointerIndex)
  {
    if (event.getPointerCount() == 1)
    {
      nativeOnTouch(mEngineId, actionType, event.getPointerId(0), event.getX(), event.getY(), Map.INVALID_TOUCH_ID, 0, 0, 0);
    }
    else
    {
      nativeOnTouch(mEngineId, actionType,
          event.getPointerId(0), event.getX(0), event.getY(0),
          event.getPointerId(1), event.getX(1), event.getY(1), pointerIndex);
    }
  }

  public void onTouch(float x, float y)
  {
    nativeOnTouch(mEngineId, Map.NATIVE_ACTION_UP, 0, x, y, Map.INVALID_TOUCH_ID, 0, 0, 0);
  }

  public static boolean isEngineCreated()
  {
    return nativeIsEngineCreated(0);
  }

  public static boolean showMapForUrl(String url)
  {
    return nativeShowMapForUrl(url);
  }

  private void setupWidgets(final Context context, int width, int height)
  {
    mHeight = height;
    mWidth = width;

    nativeCleanWidgets(mEngineId);
    setupBottomWidgetsOffset(context, mBottomWidgetOffsetX, mBottomWidgetOffsetY);
    nativeSetupWidget(mEngineId, WIDGET_SCALE_FPS_LABEL, UiUtils.dimen(context, R.dimen.margin_base), UiUtils.dimen(context, R.dimen.margin_base), ANCHOR_LEFT_TOP);
    setupCompass(context, mCurrentCompassOffsetX, mCurrentCompassOffsetY, false);
  }

  private void setupRuler(final Context context, int offsetX, int offsetY)
  {
    nativeSetupWidget(mEngineId, WIDGET_RULER,
        UiUtils.dimen(context, R.dimen.margin_ruler) + offsetX,
        mHeight - UiUtils.dimen(context, R.dimen.margin_ruler) - offsetY,
        ANCHOR_LEFT_BOTTOM);
    if (mSurfaceCreated)
      nativeApplyWidgets(mEngineId);
  }

  private void setupAttribution(final Context context, int offsetX, int offsetY)
  {
    nativeSetupWidget(mEngineId, WIDGET_COPYRIGHT,
        UiUtils.dimen(context, R.dimen.margin_ruler) + offsetX,
        mHeight - UiUtils.dimen(context, R.dimen.margin_ruler) - offsetY,
        ANCHOR_LEFT_BOTTOM);
    if (mSurfaceCreated)
      nativeApplyWidgets(mEngineId);
  }

  private boolean isThemeChangingProcess(final Context context)
  {
    return mUiThemeOnPause != null && !mUiThemeOnPause.equals(Config.getCurrentUiTheme(context));
  }

  // Engine
  private static native long nativeCreateEngineId();
  private static native boolean nativeCreateEngine(long engineId, Surface surface,
                                                   int density, boolean firstLaunch,
                                                   boolean isLaunchByDeepLink,
                                                   int appVersionCode);
  private static native boolean nativeIsEngineCreated(long engineId);
  private static native void nativeSetRenderingInitializationFinishedListener(
      long engineId, @Nullable MapRenderingListener listener);
  private static native boolean nativeShowMapForUrl(String url);

  // Surface
  private static native boolean nativeAttachSurface(long engineId, Surface surface);
  private static native void nativeDetachSurface(long engineId, boolean destroySurface);
  private static native void nativeSurfaceChanged(long engineId, Surface surface, int w, int h);
  private static native boolean nativeDestroySurfaceOnDetach(long engineId);
  private static native void nativePauseSurfaceRendering(long engineId);
  private static native void nativeResumeSurfaceRendering(long engineId);

  // Widgets
  private static native void nativeApplyWidgets(long engineId);
  private static native void nativeCleanWidgets(long engineId);
  private static native void nativeSetupWidget(long engineId, int widget, float x, float y, int anchor);
  private static native void nativeCompassUpdated(double north, boolean forceRedraw);

  // Events
  private static native void nativeMove(long engineId, double factorX, double factorY, boolean isAnim);
  private static native void nativeScalePlus(long engineId);
  private static native void nativeScaleMinus(long engineId);
  private static native void nativeScale(long engineId, double factor, double focusX, double focusY, boolean isAnim);
  private static native void nativeOnTouch(long engineId, int actionType, int id1, float x1, float y1, int id2, float x2, float y2, int maskedPointer);
}
