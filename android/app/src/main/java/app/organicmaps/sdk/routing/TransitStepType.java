package app.organicmaps.sdk.routing;

import androidx.annotation.DrawableRes;

import app.organicmaps.R;

public enum TransitStepType
{
  // A specific icon for different intermediate points is calculated dynamically in TransitStepView.
  INTERMEDIATE_POINT(R.drawable.ic_20px_route_planning_walk),
  PEDESTRIAN(R.drawable.ic_20px_route_planning_walk),
  SUBWAY(R.drawable.ic_20px_route_planning_metro),
  TRAIN(R.drawable.ic_20px_route_planning_train),
  LIGHT_RAIL(R.drawable.ic_20px_route_planning_lightrail),
  MONORAIL(R.drawable.ic_20px_route_planning_monorail),
  RULER(R.drawable.ic_ruler_route);

  @DrawableRes
  private final int mDrawable;

  TransitStepType(@DrawableRes int drawable)
  {
    mDrawable = drawable;
  }

  @DrawableRes
  public int getDrawable()
  {
    return mDrawable;
  }
}
