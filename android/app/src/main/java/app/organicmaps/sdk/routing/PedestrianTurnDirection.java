package app.organicmaps.sdk.routing;

import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;

import app.organicmaps.R;

public enum PedestrianTurnDirection
{
  NO_TURN(R.drawable.ic_turn_straight, 0),
  GO_STRAIGHT(R.drawable.ic_turn_straight, 0),

  TURN_RIGHT(R.drawable.ic_turn_right, R.drawable.ic_then_right),
  TURN_LEFT(R.drawable.ic_turn_left, R.drawable.ic_then_left),

  REACHED_YOUR_DESTINATION(R.drawable.ic_turn_finish, R.drawable.ic_then_finish);

  private final int mTurnRes;
  private final int mNextTurnRes;

  PedestrianTurnDirection(@DrawableRes int mainResId, @DrawableRes int nextResId)
  {
    mTurnRes = mainResId;
    mNextTurnRes = nextResId;
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
}
