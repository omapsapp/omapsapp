package app.organicmaps.sdk.routing;

import androidx.annotation.Keep;
import androidx.annotation.Nullable;

/**
 * Represents RouteMarkData from core.
 */
// Called from JNI.
@Keep
@SuppressWarnings("unused")
public class RouteMarkData
{
  @Nullable
  public final String mTitle;
  @Nullable
  public final String mSubtitle;
  @RoutePointInfo.RouteMarkType
  public final int mPointType;
  public final int mIntermediateIndex;
  public final boolean mIsVisible;
  public final boolean mIsMyPosition;
  public final boolean mIsPassed;
  public final double mLat;
  public final double mLon;

  public RouteMarkData(@Nullable String title, @Nullable String subtitle,
                       @RoutePointInfo.RouteMarkType int pointType,
                       int intermediateIndex, boolean isVisible, boolean isMyPosition,
                       boolean isPassed, double lat, double lon)
  {
    mTitle = title;
    mSubtitle = subtitle;
    mPointType = pointType;
    mIntermediateIndex = intermediateIndex;
    mIsVisible = isVisible;
    mIsMyPosition = isMyPosition;
    mIsPassed = isPassed;
    mLat = lat;
    mLon = lon;
  }
}
