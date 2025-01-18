package app.organicmaps.sdk.routing;

import androidx.annotation.DrawableRes;

import app.organicmaps.R;

/**
 * IMPORTANT : Order of enum values MUST BE the same
 * with native LaneWay enum (see routing/turns.hpp for details).
 * Information for every lane is composed of some number values below.
 * For example, a lane may have THROUGH and RIGHT values.
 */
public enum LaneWay
{
  NONE(R.drawable.ic_turn_straight),
  REVERSE(R.drawable.ic_turn_uleft),
  SHARP_LEFT(R.drawable.ic_turn_left_sharp),
  LEFT(R.drawable.ic_turn_left),
  SLIGHT_LEFT(R.drawable.ic_turn_left_slight),
  MERGE_TO_RIGHT(R.drawable.ic_turn_right_slight),
  THROUGH(R.drawable.ic_turn_straight),
  MERGE_TO_LEFT(R.drawable.ic_turn_left_slight),
  SLIGHT_RIGHT(R.drawable.ic_turn_right_slight),
  RIGHT(R.drawable.ic_turn_right),
  SHARP_RIGHT(R.drawable.ic_turn_right_sharp);

  public final int mTurnRes;

  LaneWay(@DrawableRes int turnRes)
  {
    mTurnRes = turnRes;
  }
}
