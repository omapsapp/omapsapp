package app.organicmaps.routing;

import androidx.annotation.DrawableRes;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import app.organicmaps.R;
import app.organicmaps.util.log.Logger;

import java.util.ArrayList;
import java.util.List;

public class SingleLaneInfo
{
  @NonNull
  public final LaneWay[] mLane;
  @Nullable
  public final LaneWay[] mRecommendedLaneWays;

  public final boolean mIsActive;

  /**
   * IMPORTANT : Order of enum values MUST BE the same
   * with native LaneWay union (see routing/lanes.hpp for details).
   * Information for every lane is composed of some number values below.
   * For example, a lane may have THROUGH and RIGHT values.
   */
  public enum LaneWay
  {
    LEFT(R.drawable.ic_turn_left),
    SLIGHT_LEFT(R.drawable.ic_turn_left_slight),
    SHARP_LEFT(R.drawable.ic_turn_left_sharp),
    THROUGH(R.drawable.ic_turn_straight),
    RIGHT(R.drawable.ic_turn_right),
    SLIGHT_RIGHT(R.drawable.ic_turn_right_slight),
    SHARP_RIGHT(R.drawable.ic_turn_right_sharp),
    REVERSE(R.drawable.ic_turn_uleft),
    MERGE_TO_LEFT(R.drawable.ic_turn_left_slight),
    MERGE_TO_RIGHT(R.drawable.ic_turn_right_slight),
    SLIDE_LEFT(R.drawable.ic_turn_left_slight),
    SLIDE_RIGHT(R.drawable.ic_turn_right_slight),
    NEXT_RIGHT(R.drawable.ic_turn_right);

    public final int mTurnRes;

    LaneWay(@DrawableRes int turnRes)
    {
      mTurnRes = turnRes;
    }
  }

  // Called from JNI.
  @Keep
  SingleLaneInfo(final short laneWays, final short recommendedLaneWays)
  {
    if (laneWays == 0)
      mLane = new LaneWay[]{LaneWay.THROUGH};
    else
      mLane = createLaneWays(laneWays);
    if (recommendedLaneWays == 0)
      mRecommendedLaneWays = null;
    else
      mRecommendedLaneWays = createLaneWays(recommendedLaneWays);
    mIsActive = recommendedLaneWays != 0;
  }

  @NonNull
  @Override
  public String toString()
  {
    StringBuilder sb = new StringBuilder();
    sb.append("SingleLaneInfo[mLaneWays=[");
    for (LaneWay i : mLane)
      sb.append(i.toString()).append(" ");
    sb.append("], mRecommendedLaneWays=[");
    if (mRecommendedLaneWays != null)
      for (LaneWay i : mRecommendedLaneWays)
        sb.append(i.toString()).append(" ");
    else
      sb.append("null]");
    sb.append("]");

    return sb.toString();
  }

  @NonNull
  private static LaneWay[] createLaneWays(final short laneWays)
  {
    final LaneWay[] values = LaneWay.values();
    final List<LaneWay> result = new ArrayList<>();
    for (int index = 0, ways = laneWays; ways > 0; ways >>= 1, index++)
    {
      if ((ways & 1) != 1)
        continue;
      if (index >= values.length)
      {
        Logger.w(SingleLaneInfo.class.getSimpleName(), "Index >= LaneWay.size(); Probably, unused bits are set");
        continue;
      }
      result.add(values[index]);
    }
    return result.toArray(new LaneWay[0]);
  }
}
