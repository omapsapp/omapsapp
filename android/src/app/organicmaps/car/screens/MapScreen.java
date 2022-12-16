package app.organicmaps.car.screens;

import androidx.annotation.NonNull;
import androidx.car.app.CarContext;
import androidx.car.app.Screen;

import app.organicmaps.car.SurfaceRenderer;

public abstract class MapScreen extends Screen
{
  @NonNull
  private final SurfaceRenderer mSurfaceRenderer;

  public MapScreen(@NonNull CarContext carContext, @NonNull SurfaceRenderer surfaceRenderer)
  {
    super(carContext);
    mSurfaceRenderer = surfaceRenderer;
  }

  @NonNull
  public SurfaceRenderer getSurfaceRenderer()
  {
    return mSurfaceRenderer;
  }
}
