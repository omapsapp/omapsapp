package app.organicmaps.sdk.routing;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Represents TransitRouteInfo from core.
 */
// Used by JNI.
@Keep
@SuppressWarnings("unused")
public class TransitRouteInfo
{
  @NonNull
  private final String mTotalDistance;
  @NonNull
  private final String mTotalDistanceUnits;
  private final int mTotalTimeInSec;
  @NonNull
  private final String mTotalPedestrianDistance;
  @NonNull
  private final String mTotalPedestrianDistanceUnits;
  private final int mTotalPedestrianTimeInSec;
  @NonNull
  private final TransitStepInfo[] mSteps;

  public TransitRouteInfo(@NonNull String totalDistance, @NonNull String totalDistanceUnits, int totalTimeInSec,
                          @NonNull String totalPedestrianDistance, @NonNull String totalPedestrianDistanceUnits,
                          int totalPedestrianTimeInSec, @NonNull TransitStepInfo[] steps)
  {
    mTotalDistance = totalDistance;
    mTotalDistanceUnits = totalDistanceUnits;
    mTotalTimeInSec = totalTimeInSec;
    mTotalPedestrianDistance = totalPedestrianDistance;
    mTotalPedestrianDistanceUnits = totalPedestrianDistanceUnits;
    mTotalPedestrianTimeInSec = totalPedestrianTimeInSec;
    mSteps = steps;
  }

  @NonNull
  String getTotalPedestrianDistance()
  {
    return mTotalPedestrianDistance;
  }

  int getTotalPedestrianTimeInSec()
  {
    return mTotalPedestrianTimeInSec;
  }

  @NonNull
  String getTotalPedestrianDistanceUnits()
  {
    return mTotalPedestrianDistanceUnits;
  }

  int getTotalTime()
  {
    return mTotalTimeInSec;
  }

  @NonNull
  List<TransitStepInfo> getTransitSteps()
  {
    return new ArrayList<>(Arrays.asList(mSteps));
  }
}
