package app.organicmaps.car;

import android.graphics.Rect;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.car.app.AppManager;
import androidx.car.app.CarContext;
import androidx.car.app.CarToast;
import androidx.car.app.SurfaceCallback;
import androidx.car.app.SurfaceContainer;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleOwner;

import app.organicmaps.Map;
import app.organicmaps.R;

public class SurfaceRenderer implements DefaultLifecycleObserver, SurfaceCallback
{
  private static final String TAG = SurfaceRenderer.class.getSimpleName();

  private final CarContext mCarContext;
  private final Map mMap;

  @Nullable
  private Rect mVisibleArea;
  @Nullable
  private Rect mStableArea;

  public SurfaceRenderer(@NonNull CarContext carContext, @NonNull Lifecycle lifecycle)
  {
    Log.d(TAG, "SurfaceRenderer()");
    mCarContext = carContext;
    mMap = new Map();
    lifecycle.addObserver(this);
  }

  @Override
  public void onSurfaceAvailable(@NonNull SurfaceContainer surfaceContainer)
  {
    Log.d(TAG, "Surface available " + surfaceContainer);
    mMap.onSurfaceCreated(
        mCarContext,
        surfaceContainer.getSurface(),
        new Rect(0, 0, surfaceContainer.getWidth(), surfaceContainer.getHeight()),
        surfaceContainer.getDpi()
    );
  }

  @Override
  public void onVisibleAreaChanged(@NonNull Rect visibleArea)
  {
    Log.d(TAG, "Visible area changed. stableArea: " + mStableArea + " visibleArea:" + visibleArea);
    mVisibleArea = visibleArea;
  }

  @Override
  public void onStableAreaChanged(@NonNull Rect stableArea)
  {
    Log.d(TAG, "Stable area changed. stableArea: " + mStableArea + " visibleArea:" + mVisibleArea);
    mStableArea = stableArea;
  }

  @Override
  public void onSurfaceDestroyed(@NonNull SurfaceContainer surfaceContainer)
  {
    Log.d(TAG, "Surface destroyed");
    mMap.onSurfaceDestroyed(false, true);
  }

  @Override
  public void onCreate(@NonNull LifecycleOwner owner)
  {
    Log.d(TAG, "onCreate");
    mCarContext.getCarService(AppManager.class).setSurfaceCallback(this);

    // TODO: Properly process deep links from other apps on AA.
    boolean launchByDeepLink = false;
    mMap.onCreate(launchByDeepLink);
  }

  @Override
  public void onStart(@NonNull LifecycleOwner owner)
  {
    Log.d(TAG, "onStart");
    mMap.onStart();
    mMap.setCallbackUnsupported(this::reportUnsupported);
  }

  @Override
  public void onStop(@NonNull LifecycleOwner owner)
  {
    Log.d(TAG, "onStop");
    mMap.onStop();
    mMap.setCallbackUnsupported(null);
  }

  @Override
  public void onPause(@NonNull LifecycleOwner owner)
  {
    Log.d(TAG, "onPause");
    mMap.onPause(mCarContext);
  }

  @Override
  public void onResume(@NonNull LifecycleOwner owner)
  {
    Log.d(TAG, "onResume");
    mMap.onResume();
  }

  @Override
  public void onScroll(float distanceX, float distanceY)
  {
    Log.d(TAG, "onScroll: distanceX: " + distanceX + ", distanceY: " + distanceY);
    mMap.onScroll(distanceX, distanceY);
  }

  @Override
  public void onFling(float velocityX, float velocityY)
  {
    Log.d(TAG, "onFling: velocityX: " + velocityX + ", velocityY: " + velocityY);
  }

  public void onZoomIn()
  {
    mMap.zoomIn();
  }

  public void onZoomOut()
  {
    mMap.zoomOut();
  }

  @Override
  public void onScale(float focusX, float focusY, float scaleFactor)
  {
    Log.d(TAG, "onScale: focusX: " + focusX + ", focusY: " + focusY + ", scaleFactor: " + scaleFactor);
    float x = focusX;
    float y = focusY;

    Rect visibleArea = mVisibleArea;
    if (visibleArea != null)
    {
      // If a focal point value is negative, use the center point of the visible area.
      if (x < 0)
        x = visibleArea.centerX();
      if (y < 0)
        y = visibleArea.centerY();
    }

    final boolean animated = Float.compare(scaleFactor, 2f) == 0;

    mMap.onScale(scaleFactor, x, y, animated);
  }

  @Override
  public void onClick(float x, float y)
  {
    Log.d(TAG, "onClick: x: " + x + ", y: " + y);
    mMap.onTouch(x, y);
  }

  private void reportUnsupported()
  {
    String message = mCarContext.getString(R.string.unsupported_phone);
    Log.e(TAG, mCarContext.getString(R.string.unsupported_phone));
    CarToast.makeText(mCarContext, message, CarToast.LENGTH_LONG).show();
  }
}
