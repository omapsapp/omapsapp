package app.organicmaps.util;

import android.content.Context;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import java.text.DateFormat;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

import app.organicmaps.R;

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

  public static String getEstimateTimeString(@NonNull Context context, long seconds)
  {
    final String format = android.text.format.DateFormat.is24HourFormat(context)? "HH:mm" : "h:mm a";
    final LocalTime localTime = LocalTime.now().plusSeconds(seconds);
    return localTime.format(DateTimeFormatter.ofPattern(format));
  }

  public static String getRemainingTimeString(@NonNull Context context, long seconds)
  {
    final long hours = TimeUnit.SECONDS.toHours(seconds);
    final long minutes = TimeUnit.SECONDS.toMinutes(seconds) % 60;

    String timeString = "";

    if (hours != 0)
      timeString = String.valueOf(hours) + StringUtils.kNarrowNonBreakingSpace +
                   context.getResources().getString(R.string.hour) + " ";

    timeString += String.valueOf(minutes) + StringUtils.kNarrowNonBreakingSpace +
                  context.getResources().getString(R.string.minute);

    return timeString;
  }
}
