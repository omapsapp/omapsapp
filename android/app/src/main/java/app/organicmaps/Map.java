package app.organicmaps;

import android.content.Context;
import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.Surface;

import androidx.annotation.Nullable;

import app.organicmaps.display.DisplayType;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.util.Config;
import app.organicmaps.util.ROMUtils;
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

  private final DisplayType mDisplayType;

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

  private static int sCurrentDpi = 0;

  public Map(DisplayType mapType)
  {
    mDisplayType = mapType;
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
  public void updateCompassOffset(final Context context, int offsetX, int offsetY, boolean forceRedraw)
  {
    final int x = offsetX < 0 ? mCurrentCompassOffsetX : offsetX;
    final int y = offsetY < 0 ? mCurrentCompassOffsetY : offsetY;
    final int navPadding = UiUtils.dimen(context, R.dimen.nav_frame_padding);
    final int marginX = UiUtils.dimen(context, R.dimen.margin_compass) + navPadding;
    final int marginY = UiUtils.dimen(context, R.dimen.margin_compass_top) + navPadding;
    nativeSetupWidget(WIDGET_COMPASS, mWidth - x - marginX, y + marginY, ANCHOR_CENTER);
    if (forceRedraw && mSurfaceCreated)
      nativeApplyWidgets();
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
  public void updateBottomWidgetsOffset(final Context context, int offsetX, int offsetY)
  {
    final int x = offsetX < 0 ? mBottomWidgetOffsetX : offsetX;
    final int y = offsetY < 0 ? mBottomWidgetOffsetY : offsetY;
    updateRulerOffset(context, x, y);
    updateAttributionOffset(context, x, y);
    mBottomWidgetOffsetX = x;
    mBottomWidgetOffsetY = y;
  }

  /**
   * Moves my position arrow to the given offset.
   *
   * @param offsetY Pixel offset from the bottom.
   */
  public void updateMyPositionRoutingOffset(int offsetY)
  {
    nativeUpdateMyPositionRoutingOffset(offsetY);
  }

  public void onSurfaceCreated(final Context context, final Surface surface, Rect surfaceFrame, int surfaceDpi)
  {
    if (isThemeChangingProcess(context))
    {
      Logger.d(TAG, "Theme changing process, skip 'onSurfaceCreated' callback");
      return;
    }

    Logger.d(TAG, "mSurfaceCreated = " + mSurfaceCreated);
    if (nativeIsEngineCreated())
    {
      if (sCurrentDpi != surfaceDpi)
      {
        nativeUpdateEngineDpi(surfaceDpi);
        sCurrentDpi = surfaceDpi;

        setupWidgets(context, surfaceFrame.width(), surfaceFrame.height());
      }
      if (!nativeAttachSurface(surface))
      {
        if (mCallbackUnsupported != null)
          mCallbackUnsupported.report();
        return;
      }
      mSurfaceCreated = true;
      mSurfaceAttached = true;
      mRequireResize = true;
      nativeResumeSurfaceRendering();
      return;
    }

    mRequireResize = false;
    setupWidgets(context, surfaceFrame.width(), surfaceFrame.height());

    final LocationHelper locationHelper = LocationHelper.from(context);

    final boolean firstStart = locationHelper.isInFirstRun();
    if (!nativeCreateEngine(surface, surfaceDpi, firstStart, mLaunchByDeepLink,
                            BuildConfig.VERSION_CODE, ROMUtils.isCustomROM()))
    {
      if (mCallbackUnsupported != null)
        mCallbackUnsupported.report();
      return;
    }
    sCurrentDpi = surfaceDpi;

    if (firstStart)
      UiThread.runLater(locationHelper::onExitFromFirstRun);

    mSurfaceCreated = true;
    mSurfaceAttached = true;
    nativeResumeSurfaceRendering();
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

    nativeSurfaceChanged(surface, surfaceFrame.width(), surfaceFrame.height());

    mRequireResize = false;
    setupWidgets(context, surfaceFrame.width(), surfaceFrame.height());
    nativeApplyWidgets();
    if (mMapRenderingListener != null)
      mMapRenderingListener.onRenderingRestored();
  }

  public void onSurfaceDestroyed(boolean activityIsChangingConfigurations, boolean isAdded)
  {
    Logger.d(TAG, "mSurfaceCreated = " + mSurfaceCreated + ", mSurfaceAttached = " + mSurfaceAttached + ", isAdded = " + isAdded);
    if (!mSurfaceCreated || !mSurfaceAttached || !isAdded)
      return;

    nativeDetachSurface(!activityIsChangingConfigurations);
    mSurfaceCreated = !nativeDestroySurfaceOnDetach();
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
    nativeSetRenderingInitializationFinishedListener(mMapRenderingListener);
  }

  public void onStop()
  {
    nativeSetRenderingInitializationFinishedListener(null);
  }

  public void onPause(final Context context)
  {
    mUiThemeOnPause = Config.getCurrentUiTheme(context);

    // Pause/Resume can be called without surface creation/destroy.
    if (mSurfaceAttached)
      nativePauseSurfaceRendering();
  }

  public void onResume()
  {
    // Pause/Resume can be called without surface creation/destroy.
    if (mSurfaceAttached)
      nativeResumeSurfaceRendering();
  }

  boolean isContextCreated()
  {
    return mSurfaceCreated;
  }

  public void onScroll(double distanceX, double distanceY)
  {
    Map.nativeOnScroll(distanceX, distanceY);
  }

  public static void zoomIn()
  {
    nativeScalePlus();
  }

  public static void zoomOut()
  {
    nativeScaleMinus();
  }

  public static void onScale(double factor, double focusX, double focusY, boolean isAnim)
  {
    nativeOnScale(factor, focusX, focusY, isAnim);
  }

  public static void onTouch(int actionType, MotionEvent event, int pointerIndex)
  {
    if (event.getPointerCount() == 1)
    {
      nativeOnTouch(actionType, event.getPointerId(0), event.getX(), event.getY(), Map.INVALID_TOUCH_ID, 0, 0, 0);
    }
    else
    {
      nativeOnTouch(actionType,
          event.getPointerId(0), event.getX(0), event.getY(0),
          event.getPointerId(1), event.getX(1), event.getY(1), pointerIndex);
    }
  }

  public static void onClick(float x, float y)
  {
    nativeOnTouch(NATIVE_ACTION_DOWN, 0, x, y, Map.INVALID_TOUCH_ID, 0, 0, 0);
    nativeOnTouch(NATIVE_ACTION_UP, 0, x, y, Map.INVALID_TOUCH_ID, 0, 0, 0);
  }

  public static boolean isEngineCreated()
  {
    return nativeIsEngineCreated();
  }

  public static void executeMapApiRequest()
  {
    nativeExecuteMapApiRequest();
  }

  private void setupWidgets(final Context context, int width, int height)
  {
    mHeight = height;
    mWidth = width;

    nativeCleanWidgets();
    updateBottomWidgetsOffset(context, mBottomWidgetOffsetX, mBottomWidgetOffsetY);
    if (mDisplayType == DisplayType.Device)
    {
      nativeSetupWidget(WIDGET_SCALE_FPS_LABEL, UiUtils.dimen(context, R.dimen.margin_base), UiUtils.dimen(context, R.dimen.margin_base) * 2, ANCHOR_LEFT_TOP);
      updateCompassOffset(context, mCurrentCompassOffsetX, mCurrentCompassOffsetY, false);
    }
    else
    {
      nativeSetupWidget(WIDGET_SCALE_FPS_LABEL, (float) mWidth / 2 + UiUtils.dimen(context, R.dimen.margin_base) * 2, UiUtils.dimen(context, R.dimen.margin_base), ANCHOR_LEFT_TOP);
      updateCompassOffset(context, mWidth, mCurrentCompassOffsetY, true);
    }
  }

  private void updateRulerOffset(final Context context, int offsetX, int offsetY)
  {
    nativeSetupWidget(WIDGET_RULER,
        UiUtils.dimen(context, R.dimen.margin_ruler) + offsetX,
        mHeight - UiUtils.dimen(context, R.dimen.margin_ruler) - offsetY,
        ANCHOR_LEFT_BOTTOM);
    if (mSurfaceCreated)
      nativeApplyWidgets();
  }

  private void updateAttributionOffset(final Context context, int offsetX, int offsetY)
  {
    nativeSetupWidget(WIDGET_COPYRIGHT,
        UiUtils.dimen(context, R.dimen.margin_ruler) + offsetX,
        mHeight - UiUtils.dimen(context, R.dimen.margin_ruler) - offsetY,
        ANCHOR_LEFT_BOTTOM);
    if (mSurfaceCreated)
      nativeApplyWidgets();
  }

  private boolean isThemeChangingProcess(final Context context)
  {
    return mUiThemeOnPause != null && !mUiThemeOnPause.equals(Config.getCurrentUiTheme(context));
  }

  // Engine
  private static native boolean nativeCreateEngine(Surface surface, int density,
                                                   boolean firstLaunch,
                                                   boolean isLaunchByDeepLink,
                                                   int appVersionCode,
                                                   boolean isCustomROM);

  private static native boolean nativeIsEngineCreated();

  private static native void nativeUpdateEngineDpi(int dpi);

  private static native void nativeSetRenderingInitializationFinishedListener(
      @Nullable MapRenderingListener listener);

  private static native void nativeExecuteMapApiRequest();

  // Surface
  private static native boolean nativeAttachSurface(Surface surface);

  private static native void nativeDetachSurface(boolean destroySurface);

  private static native void nativeSurfaceChanged(Surface surface, int w, int h);

  private static native boolean nativeDestroySurfaceOnDetach();

  private static native void nativePauseSurfaceRendering();

  private static native void nativeResumeSurfaceRendering();

  // Widgets
  private static native void nativeApplyWidgets();

  private static native void nativeCleanWidgets();

  private static native void nativeUpdateMyPositionRoutingOffset(int offsetY);

  private static native void nativeSetupWidget(int widget, float x, float y, int anchor);

  private static native void nativeCompassUpdated(double north, boolean forceRedraw);

  // Events
  private static native void nativeScalePlus();

  private static native void nativeScaleMinus();

  private static native void nativeOnScroll(double distanceX, double distanceY);

  private static native void nativeOnScale(double factor, double focusX, double focusY, boolean isAnim);

  private static native void nativeOnTouch(int actionType, int id1, float x1, float y1, int id2, float x2, float y2, int maskedPointer);
}
