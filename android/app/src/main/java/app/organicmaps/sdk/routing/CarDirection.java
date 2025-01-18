package app.organicmaps.sdk.routing;

import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;

import app.organicmaps.R;

/**
 * IMPORTANT : Order of enum values MUST BE the same as native CarDirection enum.
 */
public enum CarDirection
{
  NO_TURN(R.drawable.ic_turn_straight, 0),
  GO_STRAIGHT(R.drawable.ic_turn_straight, 0),

  TURN_RIGHT(R.drawable.ic_turn_right, R.drawable.ic_then_right),
  TURN_SHARP_RIGHT(R.drawable.ic_turn_right_sharp, R.drawable.ic_then_right_sharp),
  TURN_SLIGHT_RIGHT(R.drawable.ic_turn_right_slight, R.drawable.ic_then_right_slight),

  TURN_LEFT(R.drawable.ic_turn_left, R.drawable.ic_then_left),
  TURN_SHARP_LEFT(R.drawable.ic_turn_left_sharp, R.drawable.ic_then_left_sharp),
  TURN_SLIGHT_LEFT(R.drawable.ic_turn_left_slight, R.drawable.ic_then_left_slight),

  U_TURN_LEFT(R.drawable.ic_turn_uleft, R.drawable.ic_then_uleft),
  U_TURN_RIGHT(R.drawable.ic_turn_uright, R.drawable.ic_then_uright),

  ENTER_ROUND_ABOUT(R.drawable.ic_turn_round, R.drawable.ic_then_round),
  LEAVE_ROUND_ABOUT(R.drawable.ic_turn_round, R.drawable.ic_then_round),
  STAY_ON_ROUND_ABOUT(R.drawable.ic_turn_round, R.drawable.ic_then_round),

  START_AT_THE_END_OF_STREET(0, 0),
  REACHED_YOUR_DESTINATION(R.drawable.ic_turn_finish, R.drawable.ic_then_finish),

  EXIT_HIGHWAY_TO_LEFT(R.drawable.ic_exit_highway_to_left, R.drawable.ic_then_exit_highway_to_left),
  EXIT_HIGHWAY_TO_RIGHT(R.drawable.ic_exit_highway_to_right, R.drawable.ic_then_exit_highway_to_right);

  private final int mTurnRes;
  private final int mNextTurnRes;

  CarDirection(@DrawableRes int mainResId, @DrawableRes int nextResId)
  {
    mTurnRes = mainResId;
    mNextTurnRes = nextResId;
  }

  public int getTurnRes()
  {
    return mTurnRes;
  }

  public void setTurnDrawable(@NonNull ImageView imageView)
  {
    imageView.setImageResource(mTurnRes);
    imageView.setRotation(0.0f);
  }

  public void setNextTurnDrawable(@NonNull ImageView imageView)
  {
    imageView.setImageResource(mNextTurnRes);
  }

  public boolean containsNextTurn()
  {
    return mNextTurnRes != 0;
  }

  public static boolean isRoundAbout(CarDirection turn)
  {
    return turn == ENTER_ROUND_ABOUT || turn == LEAVE_ROUND_ABOUT || turn == STAY_ON_ROUND_ABOUT;
  }
}
