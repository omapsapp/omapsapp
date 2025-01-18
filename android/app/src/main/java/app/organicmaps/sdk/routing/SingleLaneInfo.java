package app.organicmaps.sdk.routing;

import androidx.annotation.NonNull;

public final class SingleLaneInfo
{
  public LaneWay[] mLane;
  public boolean mIsActive;

  public SingleLaneInfo(@NonNull byte[] laneOrdinals, boolean isActive)
  {
    mLane = new LaneWay[laneOrdinals.length];
    final LaneWay[] values = LaneWay.values();
    for (int i = 0; i < mLane.length; i++)
      mLane[i] = values[laneOrdinals[i]];

    mIsActive = isActive;
  }

  @NonNull
  @Override
  public String toString()
  {
    final int initialCapacity = 32;
    StringBuilder sb = new StringBuilder(initialCapacity);
    sb.append("Is the lane active? ").append(mIsActive).append(". The lane directions IDs are");
    for (LaneWay i : mLane)
      sb.append(" ").append(i.ordinal());
    return sb.toString();
  }
}
