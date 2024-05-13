package app.organicmaps.sdk.location;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;

import app.organicmaps.MwmApplication;
import app.organicmaps.util.LocationUtils;
import app.organicmaps.util.log.Logger;

import java.util.LinkedHashSet;
import java.util.Set;

public class SensorHelper implements SensorEventListener
{
  private static final String TAG = SensorHelper.class.getSimpleName();

  @NonNull
  private final SensorManager mSensorManager;
  @Nullable
  private Sensor mRotationVectorSensor;

  private final float[] mRotationMatrix = new float[9];
  private final float[] mRotationValues = new float[3];
  // Initialized with purposely invalid value.
  private int mLastAccuracy = -42;
  private double mSavedNorth = Double.NaN;
  private int mRotation = 0;

  @NonNull
  private final Set<SensorListener> mListeners = new LinkedHashSet<>();

  @NonNull
  public static SensorHelper from(@NonNull Context context)
  {
    return MwmApplication.from(context).getSensorHelper();
  }

  @Override
  public void onSensorChanged(SensorEvent event)
  {
    // Here we can have events from one out of these two sensors:
    // TYPE_GEOMAGNETIC_ROTATION_VECTOR
    // TYPE_ROTATION_VECTOR

    if (mLastAccuracy != event.accuracy)
    {
      mLastAccuracy = event.accuracy;
      switch (mLastAccuracy)
      {
        case SensorManager.SENSOR_STATUS_ACCURACY_HIGH:
          break;
        case SensorManager.SENSOR_STATUS_ACCURACY_MEDIUM:
          for (SensorListener listener : mListeners)
            listener.onCompassCalibrationRecommended();
          break;
        case SensorManager.SENSOR_STATUS_ACCURACY_LOW:
        case SensorManager.SENSOR_STATUS_UNRELIABLE:
        default:
          for (SensorListener listener : mListeners)
            listener.onCompassCalibrationRequired();
      }
    }

    if (event.accuracy == SensorManager.SENSOR_STATUS_UNRELIABLE)
      return;

    SensorManager.getRotationMatrixFromVector(mRotationMatrix, event.values);
    SensorManager.getOrientation(mRotationMatrix, mRotationValues);

    // mRotationValues indexes: 0 - yaw (azimuth), 1 - pitch, 2 - roll.
    mSavedNorth = LocationUtils.correctCompassAngle(mRotation, mRotationValues[0]);
    for (SensorListener listener : mListeners)
      listener.onCompassUpdated(mSavedNorth);
  }

  @Override
  public void onAccuracyChanged(Sensor sensor, int accuracy)
  {
    Log.w("onAccuracyChanged", "Sensor " + sensor.getStringType() + " has changed accuracy to " + accuracy);
    // This method is called _only_ when accuracy changes. To know the initial startup accuracy,
    // and to show calibration warning toast if necessary, we check it in onSensorChanged().
    // Looks like modern Androids can send this event after starting the sensor.
  }

  public SensorHelper(@NonNull Context context)
  {
    mSensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
  }

  public double getSavedNorth()
  {
    return mSavedNorth;
  }

  public void setRotation(int rotation)
  {
    Logger.i(TAG, "rotation = " + rotation);
    mRotation = rotation;
  }

  /**
   * Registers listener to obtain compass updates.
   * @param listener listener to be registered.
   */
  @UiThread
  public void addListener(@NonNull SensorListener listener)
  {
    Logger.d(TAG, "listener: " + listener + " count was: " + mListeners.size());

    mListeners.add(listener);
    if (!Double.isNaN(mSavedNorth))
      listener.onCompassUpdated(mSavedNorth);
  }

  /**
   * Removes given compass listener.
   * @param listener listener to unregister.
   */
  @UiThread
  public void removeListener(@NonNull SensorListener listener)
  {
    Logger.d(TAG, "listener: " + listener + " count was: " + mListeners.size());
    mListeners.remove(listener);
  }

  public void start()
  {
    if (mRotationVectorSensor != null)
    {
      Logger.d(TAG, "Already started");
      return;
    }

    mRotationVectorSensor = mSensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR);
    if (mRotationVectorSensor == null)
    {
      Logger.w(TAG, "There is no ROTATION_VECTOR sensor, requesting GEOMAGNETIC_ROTATION_VECTOR");
      mRotationVectorSensor = mSensorManager.getDefaultSensor(Sensor.TYPE_GEOMAGNETIC_ROTATION_VECTOR);
      if (mRotationVectorSensor == null)
      {
        // Can be null in rare cases on devices without magnetic sensors.
        Logger.w(TAG, "There is no GEOMAGNETIC_ROTATION_VECTOR sensor, device orientation can not be calculated");
        return;
      }
    }

    Logger.d(TAG);
    mSensorManager.registerListener(this, mRotationVectorSensor, SensorManager.SENSOR_DELAY_UI);
  }

  public void stop()
  {
    if (mRotationVectorSensor == null)
      return;
    Logger.d(TAG);

    mSensorManager.unregisterListener(this);
    mRotationVectorSensor = null;
  }
}
