package app.organicmaps.util;

import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Pair;

import androidx.annotation.NonNull;

import app.organicmaps.MwmApplication;
import app.organicmaps.R;

import java.util.Locale;

public class StringUtils
{
  final public static String kNarrowNonBreakingSpace = "\u202F";

  public static String formatUsingUsLocale(String pattern, Object... args)
  {
    return String.format(Locale.US, pattern, args);
  }

  public static String formatUsingSystemLocale(String pattern, Object... args)
  {
    return String.format(Locale.getDefault(), pattern, args);
  }

  public static native boolean nativeIsHtml(String text);

  public static native boolean nativeContainsNormalized(String str, String substr);
  public static native String[] nativeFilterContainsNormalized(String[] strings, String substr);

  public static native Pair<String, String> nativeFormatSpeedAndUnits(double metersPerSecond);
  public static native Distance nativeFormatDistance(double meters);
  @NonNull
  public static native Pair<String, String> nativeGetLocalizedDistanceUnits();
  @NonNull
  public static native Pair<String, String> nativeGetLocalizedAltitudeUnits();
  @NonNull
  public static native String nativeGetLocalizedSpeedUnits();

  /**
   * Formats size in bytes to "x MB" or "x.x GB" format.
   * Small values rounded to 1 MB without fractions.
   * Decimal separator character depends on system locale.
   *
   * @param context context for getString()
   * @param size size in bytes
   * @return formatted string
   */
  public static String getFileSizeString(@NonNull Context context, long size)
  {
    if (size < Constants.GB)
    {
      int value = (int)((float)size / Constants.MB + 0.5f);
      if (value == 0)
        value = 1;

      return formatUsingUsLocale("%1$d %2$s", value, MwmApplication.from(context).getString(R.string.mb));
    }

    float value = ((float) size / Constants.GB);
    return formatUsingSystemLocale("%1$.1f %2$s", value, MwmApplication.from(context).getString(R.string.gb));
  }

  public static boolean isRtl()
  {
    Locale defLocale = Locale.getDefault();
    return Character.getDirectionality(defLocale.getDisplayName(defLocale).charAt(0)) == Character.DIRECTIONALITY_RIGHT_TO_LEFT;
  }

  @NonNull
  public static String toLowerCase(@NonNull String string)
  {
    return string.toLowerCase(Locale.getDefault());
  }

  @NonNull
  public static String toUpperCase(@NonNull String string)
  {
    return string.toUpperCase(Locale.getDefault());
  }

  public static class SimpleTextWatcher implements TextWatcher
  {
    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) { }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) { }

    @Override
    public void afterTextChanged(Editable s) { }
  }

  private StringUtils() {}
}
