#include <jni.h>

#include "app/organicmaps/core/jni_helper.hpp"
#include "app/organicmaps/platform/AndroidPlatform.hpp"

#include "editor/opening_hours_ui.hpp"

#include "editor/ui2oh.hpp"

#include "base/logging.hpp"
#include <algorithm>
#include <set>

#include <vector>
#include "3party/opening_hours/opening_hours.hpp"

namespace
{
using namespace editor;
using namespace editor::ui;
using namespace osmoh;
using THours = std::chrono::hours;
using TMinutes = std::chrono::minutes;

// ID-s for HoursMinutes class
jclass g_clazzHoursMinutes;
jmethodID g_ctorHoursMinutes;
jfieldID g_fidHours;
jfieldID g_fidMinutes;
// ID-s for Timespan class
jclass g_clazzTimespan;
jmethodID g_ctorTimespan;
jfieldID g_fidStart;
jfieldID g_fidEnd;
// ID-s for Timetable class
jclass g_clazzTimetable;
jmethodID g_ctorTimetable;
jfieldID g_fidWorkingTimespan;
jfieldID g_fidClosedTimespans;
jfieldID g_fidIsFullday;
jfieldID g_fidWeekdays;

jobject JavaHoursMinutes(JNIEnv * env, jlong hours, jlong minutes)
{
  static const jclass dateUtilsClass = jni::GetGlobalClassRef(env,
                                                              "app/organicmaps/util/DateUtils");
  static jmethodID const is24HourFormatMethod =
    jni::GetStaticMethodID(env,
                           dateUtilsClass,
                           "is24HourFormat",
                           "(Landroid/content/Context;)Z");
  jobject context = android::Platform::Instance().GetContext();
  jboolean const is24HourFormat = env->CallStaticBooleanMethod(dateUtilsClass,
                                                               is24HourFormatMethod,
                                                               context);
  jobject const hoursMinutes = env->NewObject(g_clazzHoursMinutes,
                                              g_ctorHoursMinutes,
                                              hours,
                                              minutes,
                                              is24HourFormat);
  ASSERT(hoursMinutes, (jni::DescribeException()));
  return hoursMinutes;
}

jobject JavaTimespan(JNIEnv * env, jobject start, jobject end)
{
  jobject const span = env->NewObject(g_clazzTimespan, g_ctorTimespan, start, end);
  ASSERT(span, (jni::DescribeException()));
  return span;
}

jobject JavaTimespan(JNIEnv * env, osmoh::Timespan const & timespan)
{
  auto const start = timespan.GetStart();
  auto const end = timespan.GetEnd();
  return JavaTimespan(env,
                      JavaHoursMinutes(env, start.GetHoursCount(), start.GetMinutesCount()),
                      JavaHoursMinutes(env, end.GetHoursCount(), end.GetMinutesCount()));
}

jobject JavaTimetable(JNIEnv * env, jobject workingHours, jobject closedHours, bool isFullday, jintArray weekdays)
{
  jobject const tt = env->NewObject(g_clazzTimetable, g_ctorTimetable, workingHours, closedHours, isFullday, weekdays);
  ASSERT(tt, (jni::DescribeException()));
  return tt;
}

jobject JavaTimetable(JNIEnv * env, TimeTable const & tt)
{
  auto const excludeSpans = tt.GetExcludeTime();
  std::set<Weekday> weekdays = tt.GetOpeningDays();
  std::vector<int> weekdaysVector;
  weekdaysVector.reserve(weekdays.size());
  std::transform(weekdays.begin(), weekdays.end(), std::back_inserter(weekdaysVector), [](Weekday weekday)
  {
    return static_cast<int>(weekday);
  });
  jintArray jWeekdays = env->NewIntArray(static_cast<jsize>(weekdays.size()));
  env->SetIntArrayRegion(jWeekdays, 0, static_cast<jsize>(weekdaysVector.size()),
                         &weekdaysVector[0]);

  return JavaTimetable(env,
                       JavaTimespan(env, tt.GetOpeningTime()),
                       jni::ToJavaArray(env, g_clazzTimespan, tt.GetExcludeTime(), [](JNIEnv * env, osmoh::Timespan const & timespan)
                       {
                         return JavaTimespan(env, timespan);
                       }),
                       tt.IsTwentyFourHours(),
                       jWeekdays);
}

jobjectArray JavaTimetables(JNIEnv * env, TimeTableSet & tts)
{
  size_t const size = tts.Size();
  jobjectArray const result = env->NewObjectArray(static_cast<jsize>(size), g_clazzTimetable, 0);
  for (size_t i = 0; i < size; i++)
  {
    jni::TScopedLocalRef jTable(env, JavaTimetable(env, tts.Get(i)));
    env->SetObjectArrayElement(result, static_cast<jsize>(i), jTable.get());
  }

  return result;
}

HourMinutes NativeHoursMinutes(JNIEnv * env, jobject jHourMinutes)
{
  jlong const hours = env->GetLongField(jHourMinutes, g_fidHours);
  jlong const minutes = env->GetLongField(jHourMinutes, g_fidMinutes);
  return HourMinutes(THours(hours) + TMinutes(minutes));
}

Timespan NativeTimespan(JNIEnv * env, jobject jTimespan)
{
  Timespan span;
  span.SetStart(NativeHoursMinutes(env, env->GetObjectField(jTimespan, g_fidStart)));
  span.SetEnd(NativeHoursMinutes(env, env->GetObjectField(jTimespan, g_fidEnd)));
  return span;
}

TimeTable NativeTimetable(JNIEnv * env, jobject jTimetable)
{
  TimeTable tt = TimeTable::GetPredefinedTimeTable();
  jintArray const jWeekdays = static_cast<jintArray>(env->GetObjectField(jTimetable, g_fidWeekdays));
  int * weekdaysArr = static_cast<int*>(env->GetIntArrayElements(jWeekdays, nullptr));
  jint size = env->GetArrayLength(jWeekdays);
  std::set<Weekday> weekdays;
  for (int i = 0; i < size; i++)
    weekdays.insert(ToWeekday(weekdaysArr[i]));
  tt.SetOpeningDays(weekdays);
  env->ReleaseIntArrayElements(jWeekdays, weekdaysArr, 0);
  tt.SetTwentyFourHours(env->GetBooleanField(jTimetable, g_fidIsFullday));
  tt.SetOpeningTime(NativeTimespan(env, env->GetObjectField(jTimetable, g_fidWorkingTimespan)));
  jobjectArray jClosedSpans = static_cast<jobjectArray>(env->GetObjectField(jTimetable, g_fidClosedTimespans));
  size = env->GetArrayLength(jClosedSpans);
  for (int i = 0; i < size; i++)
  {
    jni::TScopedLocalRef jSpan(env, env->GetObjectArrayElement(jClosedSpans, i));
    if (jSpan.get())
      tt.AddExcludeTime(NativeTimespan(env, jSpan.get()));
  }
  return tt;
}

TimeTableSet NativeTimetableSet(JNIEnv * env, jobjectArray jTimetables)
{
  TimeTableSet tts;
  int const size = env->GetArrayLength(jTimetables);
  jobject const timetable = env->GetObjectArrayElement(jTimetables, 0);
  tts.Replace(NativeTimetable(env, timetable), 0);

  for (int i = 1; i < size; i++)
  {
    jni::TScopedLocalRef timetable(env, env->GetObjectArrayElement(jTimetables, i));
    tts.Append(NativeTimetable(env, timetable.get()));
  }

  return tts;
}

}  // namespace

extern "C"
{
JNIEXPORT void JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeInit(JNIEnv * env, jclass clazz)
{
  g_clazzHoursMinutes = jni::GetGlobalClassRef(env, "app/organicmaps/editor/data/HoursMinutes");
  // Java signature : HoursMinutes(@IntRange(from = 0, to = 24) long hours, @IntRange(from = 0, to = 60) long minutes)
  g_ctorHoursMinutes = env->GetMethodID(g_clazzHoursMinutes, "<init>", "(JJZ)V");
  ASSERT(g_ctorHoursMinutes, (jni::DescribeException()));
  g_fidHours = env->GetFieldID(g_clazzHoursMinutes, "hours", "J");
  ASSERT(g_fidHours, (jni::DescribeException()));
  g_fidMinutes = env->GetFieldID(g_clazzHoursMinutes, "minutes", "J");
  ASSERT(g_fidMinutes, (jni::DescribeException()));

  g_clazzTimespan = jni::GetGlobalClassRef(env, "app/organicmaps/editor/data/Timespan");
  // Java signature : Timespan(HoursMinutes start, HoursMinutes end)
  g_ctorTimespan =
      env->GetMethodID(g_clazzTimespan, "<init>","(Lapp/organicmaps/editor/data/HoursMinutes;Lapp/organicmaps/editor/data/HoursMinutes;)V");
  ASSERT(g_ctorTimespan, (jni::DescribeException()));
  g_fidStart = env->GetFieldID(g_clazzTimespan, "start", "Lapp/organicmaps/editor/data/HoursMinutes;");
  ASSERT(g_fidStart, (jni::DescribeException()));
  g_fidEnd = env->GetFieldID(g_clazzTimespan, "end", "Lapp/organicmaps/editor/data/HoursMinutes;");
  ASSERT(g_fidEnd, (jni::DescribeException()));

  g_clazzTimetable = jni::GetGlobalClassRef(env, "app/organicmaps/editor/data/Timetable");
  // Java signature : Timetable(Timespan workingTime, Timespan[] closedHours, boolean isFullday, int weekdays[])
  g_ctorTimetable =
      env->GetMethodID(g_clazzTimetable, "<init>","(Lapp/organicmaps/editor/data/Timespan;[Lapp/organicmaps/editor/data/Timespan;Z[I)V");
  ASSERT(g_ctorTimetable, (jni::DescribeException()));
  g_fidWorkingTimespan = env->GetFieldID(g_clazzTimetable, "workingTimespan", "Lapp/organicmaps/editor/data/Timespan;");
  ASSERT(g_fidWorkingTimespan, (jni::DescribeException()));
  g_fidClosedTimespans = env->GetFieldID(g_clazzTimetable, "closedTimespans", "[Lapp/organicmaps/editor/data/Timespan;");
  ASSERT(g_fidClosedTimespans, (jni::DescribeException()));
  g_fidIsFullday = env->GetFieldID(g_clazzTimetable, "isFullday", "Z");
  ASSERT(g_fidIsFullday, (jni::DescribeException()));
  g_fidWeekdays = env->GetFieldID(g_clazzTimetable, "weekdays", "[I");
  ASSERT(g_fidWeekdays, (jni::DescribeException()));
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeGetDefaultTimetables(JNIEnv * env, jclass clazz)
{
  TimeTableSet tts;
  return JavaTimetables(env, tts);
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeGetComplementTimetable(JNIEnv * env, jclass clazz, jobjectArray timetables)
{
  TimeTableSet const tts = NativeTimetableSet(env, timetables);
  return JavaTimetable(env, tts.GetComplementTimeTable());
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeRemoveWorkingDay(JNIEnv * env, jclass clazz,
                                                                    jobjectArray timetables, jint ttIndex, jint dayIndex)
{
  TimeTableSet tts = NativeTimetableSet(env, timetables);
  auto tt = tts.Get(ttIndex);
  tt.RemoveWorkingDay(ToWeekday(dayIndex));
  tt.Commit();
  return JavaTimetables(env, tts);
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeAddWorkingDay(JNIEnv * env, jclass clazz,
                                                                 jobjectArray timetables, jint ttIndex, jint dayIndex)
{
  TimeTableSet tts = NativeTimetableSet(env, timetables);
  auto tt = tts.Get(ttIndex);
  tt.AddWorkingDay(ToWeekday(dayIndex));
  tt.Commit();
  return JavaTimetables(env, tts);
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeSetIsFullday(JNIEnv * env, jclass clazz,
                                                                jobject jTimetable, jboolean jIsFullday)
{
  TimeTable tt = NativeTimetable(env, jTimetable);
  if (jIsFullday)
    tt.SetTwentyFourHours(true);
  else
  {
    tt.SetTwentyFourHours(false);
    tt.SetOpeningTime(tt.GetPredefinedOpeningTime());
  }
  return JavaTimetable(env, tt);
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeSetOpeningTime(JNIEnv * env, jclass clazz,
                                                                  jobject jTimetable, jobject jOpeningTime)
{
  TimeTable tt = NativeTimetable(env, jTimetable);
  tt.SetOpeningTime(NativeTimespan(env, jOpeningTime));
  return JavaTimetable(env, tt);
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeAddClosedSpan(JNIEnv * env, jclass clazz,
                                                                 jobject jTimetable, jobject jClosedSpan)
{
  TimeTable tt = NativeTimetable(env, jTimetable);
  tt.AddExcludeTime(NativeTimespan(env, jClosedSpan));
  return JavaTimetable(env, tt);
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeRemoveClosedSpan(JNIEnv * env, jclass clazz,
                                                                    jobject jTimetable, jint jClosedSpanIndex)
{
  TimeTable tt = NativeTimetable(env, jTimetable);
  tt.RemoveExcludeTime(static_cast<size_t>(jClosedSpanIndex));
  return JavaTimetable(env, tt);
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeTimetablesFromString(JNIEnv * env, jclass clazz, jstring jSource)
{
  TimeTableSet tts;
  std::string const source = jni::ToNativeString(env, jSource);
  if (!source.empty()  && MakeTimeTableSet(OpeningHours(source), tts))
    return JavaTimetables(env, tts);

  return nullptr;
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeTimetablesToString(JNIEnv * env, jclass clazz, jobjectArray jTts)
{
  TimeTableSet tts = NativeTimetableSet(env, jTts);
  std::stringstream sstr;
  sstr << MakeOpeningHours(tts).GetRule();
  return jni::ToJavaString(env, sstr.str());
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_editor_OpeningHours_nativeIsTimetableStringValid(JNIEnv * env, jclass clazz, jstring jSource)
{
  return OpeningHours(jni::ToNativeString(env, jSource)).IsValid();
}
} // extern "C"
