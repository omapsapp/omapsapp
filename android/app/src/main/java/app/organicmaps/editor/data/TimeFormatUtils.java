package app.organicmaps.editor.data;

import android.content.res.Resources;

import androidx.annotation.IntRange;
import androidx.annotation.NonNull;

import app.organicmaps.R;
import app.organicmaps.sdk.editor.data.Timespan;
import app.organicmaps.sdk.editor.data.Timetable;
import app.organicmaps.util.Utils;

import java.text.DateFormatSymbols;
import java.util.Locale;

public class TimeFormatUtils
{
  private TimeFormatUtils() {}

  private static String[] sShortWeekdays;
  private static Locale sCurrentLocale;

  private static void refreshWithCurrentLocale()
  {
    if (!Locale.getDefault().equals(sCurrentLocale))
    {
      sCurrentLocale = Locale.getDefault();
      sShortWeekdays = DateFormatSymbols.getInstance().getShortWeekdays();
      for (int i = 0; i < sShortWeekdays.length; i++)
      {
        sShortWeekdays[i] = Utils.capitalize(sShortWeekdays[i]);
      }
    }
  }

  public static String formatShortWeekday(@IntRange(from = 1, to = 7) int day)
  {
    refreshWithCurrentLocale();
    return sShortWeekdays[day];
  }

  public static String formatWeekdaysRange(int startWeekDay, int endWeekDay)
  {
    refreshWithCurrentLocale();
    if (startWeekDay == endWeekDay)
      return sShortWeekdays[startWeekDay];
    else
      return sShortWeekdays[startWeekDay] + "-" + sShortWeekdays[endWeekDay];
  }

  public static String formatWeekdays(@NonNull Timetable timetable)
  {
    return formatWeekdays(timetable.weekdays);
  }

  public static String formatWeekdays(@NonNull int[] weekdays)
  {
    if (weekdays.length == 0)
      return "";

    refreshWithCurrentLocale();
    final StringBuilder builder = new StringBuilder(sShortWeekdays[weekdays[0]]);
    boolean iteratingRange;
    for (int i = 1; i < weekdays.length; )
    {
      iteratingRange = (weekdays[i] == weekdays[i - 1] + 1);
      if (iteratingRange)
      {
        while (i < weekdays.length && weekdays[i] == weekdays[i - 1] + 1)
          i++;
        builder.append("-").append(sShortWeekdays[weekdays[i - 1]]);
        continue;
      }

      if (i < weekdays.length)
        builder.append(", ").append(sShortWeekdays[weekdays[i]]);

      i++;
    }

    return builder.toString();
  }

  public static String formatNonBusinessTime(Timespan[] closedTimespans, String hoursClosedLabel)
  {
    StringBuilder closedTextBuilder = new StringBuilder();
    boolean firstLine = true;

    for (Timespan cts: closedTimespans) {
      if (!firstLine)
        closedTextBuilder.append('\n');

      closedTextBuilder.append(hoursClosedLabel).append(' ').append(cts.toWideString());
      firstLine = false;
    }
    return closedTextBuilder.toString();
  }

  public static String formatTimetables(@NonNull Resources resources, String ohStr, Timetable[] timetables)
  {
    if (timetables == null || timetables.length == 0)
      return ohStr;

    // Generate string "24/7" or "Daily HH:MM - HH:MM".
    if (timetables[0].isFullWeek())
    {
      Timetable tt = timetables[0];
      if (tt.isFullday)
        return resources.getString(R.string.twentyfour_seven);
      if (tt.closedTimespans == null || tt.closedTimespans.length == 0)
        return resources.getString(R.string.daily) + " " + tt.workingTimespan.toWideString();
      return resources.getString(R.string.daily) + " " + tt.workingTimespan.toWideString() +
             "\n" + formatNonBusinessTime(tt.closedTimespans, resources.getString(R.string.editor_hours_closed));
    }

    // Generate full week multiline string. E.g.
    // "Mon-Fri HH:MM - HH:MM
    // Sat HH:MM - HH:MM
    // Non-business Hours HH:MM - HH:MM"
    StringBuilder weekSchedule = new StringBuilder();
    boolean firstRow = true;
    for (Timetable tt : timetables)
    {
      if (!firstRow)
        weekSchedule.append('\n');

      final String weekdays = formatWeekdays(tt);
      final String openTime = tt.isFullday ?
                              Utils.unCapitalize(resources.getString(R.string.editor_time_allday)) :
                              tt.workingTimespan.toWideString();

      weekSchedule.append(weekdays).append(' ').append(openTime);
      if (tt.closedTimespans != null && tt.closedTimespans.length > 0)
        weekSchedule.append('\n')
                    .append(formatNonBusinessTime(tt.closedTimespans, resources.getString(R.string.editor_hours_closed)));

      firstRow = false;
    }

    return weekSchedule.toString();
  }
}
