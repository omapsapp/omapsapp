package app.organicmaps.sdk.util;

import android.content.Context;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;

import app.organicmaps.sdk.R;

// Used by JNI.
@Keep
@SuppressWarnings("unused")
public final class Distance
{
  public static final Distance EMPTY = new Distance(0.0, "", (byte) 0);

  /**
   * IMPORTANT : Order of enum values MUST BE the same
   * with native Distance::Units enum (see platform/distance.hpp for details).
   */
  public enum Units
  {
    Meters(R.string.m),
    Kilometers(R.string.km),
    Feet(R.string.ft),
    Miles(R.string.mi);

    @StringRes
    public final int mStringRes;

    Units(@StringRes int stringRes)
    {
      mStringRes = stringRes;
    }
  }

  /// @todo What is the difference with cpp: kNarrowNonBreakingSpace = "\u202F" ?
  private static final char NON_BREAKING_SPACE = '\u00A0';

  public final double mDistance;
  @NonNull
  public final String mDistanceStr;
  public final Units mUnits;

  public Distance(double distance, @NonNull String distanceStr, byte unitsIndex)
  {
    mDistance = distance;
    mDistanceStr = distanceStr;
    mUnits = Units.values()[unitsIndex];
  }

  public boolean isValid()
  {
    return mDistance >= 0.0;
  }

  @NonNull
  public String getUnitsStr(@NonNull final Context context)
  {
    return context.getString(mUnits.mStringRes);
  }

  @NonNull
  public String toString(@NonNull final Context context)
  {
    if (!isValid())
      return "";

    return mDistanceStr + NON_BREAKING_SPACE + getUnitsStr(context);
  }

  @NonNull
  @Override
  public String toString()
  {
    if (!isValid())
      return "";

    return mDistanceStr + NON_BREAKING_SPACE + mUnits.toString();
  }
}
