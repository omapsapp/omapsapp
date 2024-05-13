#include "Framework.hpp"

#include "map/gps_tracker.hpp"

#include <chrono>

extern "C"
{
  JNIEXPORT void JNICALL
  Java_app_organicmaps_location_TrackRecorder_nativeSetEnabled(JNIEnv * env, jclass clazz, jboolean enable)
  {
    GpsTracker::Instance().SetEnabled(enable);
    Framework * const f = frm();
    if (f == nullptr)
      return;
    if (enable)
      f->ConnectToGpsTracker();
    else
      f->DisconnectFromGpsTracker();
  }

  JNIEXPORT jboolean JNICALL
  Java_app_organicmaps_location_TrackRecorder_nativeIsEnabled(JNIEnv * env, jclass clazz)
  {
    return GpsTracker::Instance().IsEnabled();
  }

  JNIEXPORT void JNICALL
  Java_app_organicmaps_location_TrackRecorder_nativeSetDuration(JNIEnv * env, jclass clazz, jint durationHours)
  {
    GpsTracker::Instance().SetDuration(std::chrono::hours(durationHours));
  }

  JNIEXPORT jint JNICALL
  Java_app_organicmaps_location_TrackRecorder_nativeGetDuration(JNIEnv * env, jclass clazz)
  {
    return static_cast<jint>(GpsTracker::Instance().GetDuration().count());
  }
}
