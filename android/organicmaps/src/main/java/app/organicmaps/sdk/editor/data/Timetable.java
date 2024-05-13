package app.organicmaps.sdk.editor.data;

import androidx.annotation.IntRange;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;

// Called from JNI.
@Keep
@SuppressWarnings("unused")
public class Timetable
{
  public final Timespan workingTimespan;
  public final Timespan[] closedTimespans;
  public final boolean isFullday;
  public final int[] weekdays;

  public Timetable(@NonNull Timespan workingTime, @NonNull Timespan[] closedHours, boolean isFullday, @NonNull int[] weekdays)
  {
    this.workingTimespan = workingTime;
    this.closedTimespans = closedHours;
    this.isFullday = isFullday;
    this.weekdays = weekdays;
  }

  public boolean containsWeekday(@IntRange(from = 1, to = 7) int day)
  {
    for (int workingDay : weekdays)
    {
      if (workingDay == day)
        return true;
    }

    return false;
  }

  public boolean isFullWeek()
  {
    return weekdays.length == 7;
  }

  @Override
  public String toString()
  {
    StringBuilder stringBuilder = new StringBuilder();
    stringBuilder.append("Working timespan : ").append(workingTimespan).append("\n")
                 .append("Closed timespans : ");
    for (Timespan timespan : closedTimespans)
      stringBuilder.append(timespan).append("   ");
    stringBuilder.append("\n");
    stringBuilder.append("Fullday : ").append(isFullday).append("\n")
                 .append("Weekdays : ");
    for (int i : weekdays)
      stringBuilder.append(i);
    return stringBuilder.toString();
  }
}
