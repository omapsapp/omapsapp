#pragma once

#include "app/organicmaps/core/jni_helper.hpp"

#include "map/transit/transit_display.hpp"

#include <vector>

jobjectArray CreateTransitStepInfoArray(JNIEnv * env, std::vector<TransitStepInfo> const & steps)
{
  static jclass const transitStepClass = jni::GetGlobalClassRef(env, "app/organicmaps/sdk/routing/TransitStepInfo");
  // Java signature : TransitStepInfo(int type, @Nullable String distance, @Nullable String distanceUnits,
  //                                  int timeInSec, @Nullable String number, int color, int intermediateIndex)
  static jmethodID const transitStepConstructor =
    jni::GetConstructorID(env, transitStepClass, "(ILjava/lang/String;Ljava/lang/String;ILjava/lang/String;II)V");

  return jni::ToJavaArray(
    env, transitStepClass, steps,
    [&](JNIEnv * jEnv, TransitStepInfo const & stepInfo)
    {
      jni::TScopedLocalRef const distance(env, jni::ToJavaString(env, stepInfo.m_distanceStr));
      jni::TScopedLocalRef const distanceUnits(env, jni::ToJavaString(env, stepInfo.m_distanceUnitsSuffix));
      jni::TScopedLocalRef const number(env, jni::ToJavaString(env, stepInfo.m_number));
      return env->NewObject(transitStepClass, transitStepConstructor, static_cast<jint>(stepInfo.m_type),
                            distance.get(), distanceUnits.get(), static_cast<jint>(stepInfo.m_timeInSec), number.get(),
                            static_cast<jint>(stepInfo.m_colorARGB), static_cast<jint>(stepInfo.m_intermediateIndex));
    });
}
