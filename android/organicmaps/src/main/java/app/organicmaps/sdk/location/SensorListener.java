package app.organicmaps.sdk.location;

public interface SensorListener
{
  void onCompassUpdated(double north);

  default void onCompassCalibrationRecommended()
  {
    // No op.
  }

  default void onCompassCalibrationRequired()
  {
    // No op.
  }
}
