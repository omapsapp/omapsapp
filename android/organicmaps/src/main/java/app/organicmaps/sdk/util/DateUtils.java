package app.organicmaps.sdk.util;

import android.content.Context;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import java.text.DateFormat;
import java.util.Locale;

public final class DateUtils
{
  private DateUtils()
  {
  }

  @NonNull
  public static DateFormat getShortDateFormatter()
  {
    return DateFormat.getDateInstance(DateFormat.SHORT, Locale.getDefault());
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  public static boolean is24HourFormat(@NonNull Context context)
  {
    return android.text.format.DateFormat.is24HourFormat(context);
  }
}
