package app.organicmaps.sdk.display;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import app.organicmaps.sdk.OrganicMaps;
import app.organicmaps.sdk.util.log.Logger;

import java.util.Objects;

public class DisplayManager
{
  private static final String TAG = DisplayManager.class.getSimpleName();

  private interface TaskWithCallback
  {
    void start(@NonNull Runnable onTaskFinishedCallback);
  }

  private static class DisplayHolder
  {
    boolean notify = false;
    DisplayChangedListener listener;

    public void destroy()
    {
      notify = false;
      listener = null;
    }
  }

  private final Handler mHandler = new Handler(Looper.getMainLooper());

  @NonNull
  private DisplayType mCurrentDisplayType = DisplayType.Device;
  @Nullable
  private DisplayHolder mDevice;
  @Nullable
  private DisplayHolder mCar;

  @NonNull
  public static DisplayManager from(@NonNull Context context)
  {
    final OrganicMaps app = (OrganicMaps) context.getApplicationContext();
    return app.getDisplayManager();
  }

  public boolean isCarConnected()
  {
    return mCar != null;
  }

  public boolean isDeviceConnected()
  {
    return mDevice != null;
  }

  public boolean isCarDisplayUsed()
  {
    return mCurrentDisplayType == DisplayType.Car;
  }

  public boolean isDeviceDisplayUsed()
  {
    return mCurrentDisplayType == DisplayType.Device;
  }

  public void addListener(@NonNull final DisplayType displayType, @NonNull final DisplayChangedListener listener)
  {
    Logger.d(TAG, "displayType = " + displayType + ", listener = " + listener);

    if (displayType == DisplayType.Device)
    {
      mDevice = new DisplayHolder();
      mDevice.notify = true;
      mDevice.listener = listener;
    }
    else if (displayType == DisplayType.Car && mCar == null)
    {
      mCar = new DisplayHolder();
      mCar.notify = true;
      mCar.listener = listener;
    }

    if (isCarConnected() && !isDeviceConnected())
      mCurrentDisplayType = displayType;
  }

  public void removeListener(@NonNull final DisplayType displayType)
  {
    Logger.d(TAG, "displayType = " + displayType);

    if (displayType == DisplayType.Device && mDevice != null)
    {
      mDevice.destroy();
      mDevice = null;
      if (isCarConnected() && !isCarDisplayUsed())
        changeDisplay(DisplayType.Car);
    }
    else if (displayType == DisplayType.Car && mCar != null)
    {
      mCar.destroy();
      mCar = null;
      if (isDeviceConnected() && !isDeviceDisplayUsed())
        changeDisplay(DisplayType.Device);
    }
  }

  public void changeDisplay(@NonNull final DisplayType newDisplayType)
  {
    Logger.d(TAG, "newDisplayType = " + newDisplayType);

    if (mCurrentDisplayType == newDisplayType)
      return;

    if (mCar != null)
      mCar.notify = true;
    if (mDevice != null)
      mDevice.notify = true;

    mCurrentDisplayType = newDisplayType;

    if (mCurrentDisplayType == DisplayType.Device)
      onDisplayTypeChangedToDevice();
    else if (mCurrentDisplayType == DisplayType.Car)
      onDisplayTypeChangedToCar();
  }

  private void onDisplayTypeChangedToDevice()
  {
    Logger.d(TAG);

    TaskWithCallback firstTask = null;
    TaskWithCallback secondTask = null;

    if (mCar != null && mCar.notify)
    {
      firstTask = mCar.listener::onDisplayChangedToDevice;
      mCar.notify = false;
    }
    if (mDevice != null && mDevice.notify)
    {
      if (firstTask == null)
        firstTask = mDevice.listener::onDisplayChangedToDevice;
      else
        secondTask = mDevice.listener::onDisplayChangedToDevice;
      mDevice.notify = false;
    }

    postTask(Objects.requireNonNull(firstTask), secondTask);
  }

  private void onDisplayTypeChangedToCar()
  {
    Logger.d(TAG);

    TaskWithCallback firstTask = null;
    TaskWithCallback secondTask = null;

    if (mDevice != null && mDevice.notify)
    {
      firstTask = mDevice.listener::onDisplayChangedToCar;
      mDevice.notify = false;
    }
    if (mCar != null && mCar.notify)
    {
      if (firstTask == null)
        firstTask = mCar.listener::onDisplayChangedToCar;
      else
        secondTask = mCar.listener::onDisplayChangedToCar;
      mCar.notify = false;
    }

    postTask(Objects.requireNonNull(firstTask), secondTask);
  }

  private void postTask(@NonNull TaskWithCallback firstTask, @Nullable TaskWithCallback secondTask)
  {
    mHandler.post(() -> firstTask.start(() -> {
      if (secondTask != null)
        mHandler.post(() -> secondTask.start(() -> {
        }));
    }));
  }
}
