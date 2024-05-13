package app.organicmaps.sdk.routing;

import androidx.annotation.IntDef;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Represents TransitStepInfo from core.
 */
// Used by JNI.
@Keep
@SuppressWarnings("unused")
public class TransitStepInfo
{
  private static final int TRANSIT_TYPE_INTERMEDIATE_POINT = 0;
  private static final int TRANSIT_TYPE_PEDESTRIAN = 1;
  private static final int TRANSIT_TYPE_SUBWAY = 2;
  private static final int TRANSIT_TYPE_TRAIN = 3;
  private static final int TRANSIT_TYPE_LIGHT_RAIL = 4;
  private static final int TRANSIT_TYPE_MONORAIL = 5;
  private static final int TRANSIT_TYPE_RULER = 6;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ TRANSIT_TYPE_INTERMEDIATE_POINT, TRANSIT_TYPE_PEDESTRIAN, TRANSIT_TYPE_SUBWAY,
            TRANSIT_TYPE_TRAIN, TRANSIT_TYPE_LIGHT_RAIL, TRANSIT_TYPE_MONORAIL, TRANSIT_TYPE_RULER})
  @interface TransitType {}

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

  TransitStepInfo(@TransitType int type, @Nullable String distance, @Nullable String distanceUnits,
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
    return new TransitStepInfo(TRANSIT_TYPE_INTERMEDIATE_POINT, null, null, 0, null, 0, intermediateIndex);
  }

  @NonNull
  public static TransitStepInfo ruler(@NonNull String distance, @NonNull String distanceUnits)
  {
    return new TransitStepInfo(TRANSIT_TYPE_RULER, distance, distanceUnits, 0, null, 0, -1);
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
