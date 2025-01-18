package app.organicmaps.sdk.routing;

import androidx.annotation.Keep;
import androidx.annotation.Nullable;

/**
 * Represents RouteMarkData from core.
 */
// Called from JNI.
@Keep
@SuppressWarnings("unused")
public final class RouteMarkData
{
  @Nullable
  public final String mTitle;
  @Nullable
  public final String mSubtitle;
  public final RouteMarkType mPointType;
  public final int mIntermediateIndex;
  public final boolean mIsVisible;
  public final boolean mIsMyPosition;
  public final boolean mIsPassed;
  public final double mLat;
  public final double mLon;

  private RouteMarkData(@Nullable String title, @Nullable String subtitle,
                        int pointType, int intermediateIndex, boolean isVisible,
                        boolean isMyPosition, boolean isPassed, double lat, double lon)
  {
    mTitle = title;
    mSubtitle = subtitle;
    mPointType = RouteMarkType.values()[pointType];
    mIntermediateIndex = intermediateIndex;
    mIsVisible = isVisible;
    mIsMyPosition = isMyPosition;
    mIsPassed = isPassed;
    mLat = lat;
    mLon = lon;
  }
}
