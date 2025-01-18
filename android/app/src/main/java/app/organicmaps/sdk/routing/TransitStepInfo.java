package app.organicmaps.sdk.routing;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Represents TransitStepInfo from core.
 */
// Used by JNI.
@Keep
@SuppressWarnings("unused")
public final class TransitStepInfo
{
  @NonNull
  private final TransitStepType mType;
  @Nullable
  private final String mDistance;
  @Nullable
  private final String mDistanceUnits;
  private final int mTimeInSec;
  @Nullable
  private final String mNumber;
  private final int mColor;
  private final int mIntermediateIndex;

  private TransitStepInfo(int type, @Nullable String distance, @Nullable String distanceUnits,
                          int timeInSec, @Nullable String number, int color, int intermediateIndex)
  {
    mType = TransitStepType.values()[type];
    mDistance = distance;
    mDistanceUnits = distanceUnits;
    mTimeInSec = timeInSec;
    mNumber = number;
    mColor = color;
    mIntermediateIndex = intermediateIndex;
  }

  @NonNull
  public static TransitStepInfo intermediatePoint(int intermediateIndex)
  {
    return new TransitStepInfo(TransitStepType.INTERMEDIATE_POINT.ordinal(), null, null, 0, null, 0, intermediateIndex);
  }

  @NonNull
  public static TransitStepInfo ruler(@NonNull String distance, @NonNull String distanceUnits)
  {
    return new TransitStepInfo(TransitStepType.RULER.ordinal(), distance, distanceUnits, 0, null, 0, -1);
  }

  @NonNull
  public TransitStepType getType()
  {
    return mType;
  }

  @Nullable
  public String getDistance()
  {
    return mDistance;
  }

  @Nullable
  public String getDistanceUnits()
  {
    return mDistanceUnits;
  }

  public int getTimeInSec()
  {
    return mTimeInSec;
  }

  @Nullable
  public String getNumber()
  {
    return mNumber;
  }

  public int getColor()
  {
    return mColor;
  }

  public int getIntermediateIndex()
  {
    return mIntermediateIndex;
  }
}
