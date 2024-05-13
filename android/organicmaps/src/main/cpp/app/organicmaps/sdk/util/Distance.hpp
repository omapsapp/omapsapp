#pragma once

#include "app/organicmaps/core/jni_helper.hpp"

#include "platform/distance.hpp"

inline jobject ToJavaDistance(JNIEnv * env, platform::Distance const & distance)
{
  static jclass const distanceClass = jni::GetGlobalClassRef(env, "app/organicmaps/util/Distance");

  static jmethodID const distanceConstructor = jni::GetConstructorID(env, distanceClass, "(DLjava/lang/String;B)V");

  jobject distanceObject = env->NewObject(
      distanceClass, distanceConstructor,
      distance.GetDistance(), jni::ToJavaString(env, distance.GetDistanceString()), static_cast<uint8_t>(distance.GetUnits()));

  return distanceObject;
}
