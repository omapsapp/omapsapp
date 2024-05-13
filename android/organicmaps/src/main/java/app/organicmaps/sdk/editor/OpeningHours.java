package app.organicmaps.sdk.editor;

import androidx.annotation.IntRange;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import app.organicmaps.sdk.editor.data.Timespan;
import app.organicmaps.sdk.editor.data.Timetable;

public final class OpeningHours
{
  private OpeningHours() {}

  static
  {
    nativeInit();
  }

  private static native void nativeInit();

  @NonNull
  public static native Timetable[] nativeGetDefaultTimetables();

  @NonNull
  public static native Timetable nativeGetComplementTimetable(Timetable[] timetableSet);

  @NonNull
  public static native Timetable[] nativeAddTimetable(Timetable[] timetableSet);

  @NonNull
  public static native Timetable[] nativeRemoveTimetable(Timetable[] timetableSet, int timetableIndex);

  @NonNull
  public static native Timetable nativeSetIsFullday(Timetable timetable, boolean isFullday);

  @NonNull
  public static native Timetable[] nativeAddWorkingDay(Timetable[] timetables, int timetableIndex, @IntRange(from = 1, to = 7) int day);

  @NonNull
  public static native Timetable[] nativeRemoveWorkingDay(Timetable[] timetables, int timetableIndex, @IntRange(from = 1, to = 7) int day);

  @NonNull
  public static native Timetable nativeSetOpeningTime(Timetable timetable, Timespan openingTime);

  @NonNull
  public static native Timetable nativeAddClosedSpan(Timetable timetable, Timespan closedSpan);

  @NonNull
  public static native Timetable nativeRemoveClosedSpan(Timetable timetable, int spanIndex);

  @Nullable
  public static native Timetable[] nativeTimetablesFromString(String source);

  @NonNull
  public static native String nativeTimetablesToString(@NonNull Timetable[] timetables);

  /**
   * Sometimes timetables cannot be parsed with {@link #nativeTimetablesFromString} (hence can't be displayed in UI),
   * but still are valid OSM timetables.
   * @return true if timetable string is valid OSM timetable.
   */
  public static native boolean nativeIsTimetableStringValid(String source);
}
