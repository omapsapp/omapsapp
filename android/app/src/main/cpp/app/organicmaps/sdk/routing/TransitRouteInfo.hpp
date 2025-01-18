#pragma once

#include "app/organicmaps/core/jni_helper.hpp"

#include "app/organicmaps/sdk/routing/TransitStepInfo.hpp"

#include "map/transit/transit_display.hpp"

jobject CreateTransitRouteInfo(JNIEnv * env, TransitRouteInfo const & routeInfo)
{
  jobjectArray steps = CreateTransitStepInfoArray(env, routeInfo.m_steps);

  static jclass const transitRouteInfoClass =
    jni::GetGlobalClassRef(env, "app/organicmaps/sdk/routing/TransitRouteInfo");
  // Java signature : TransitRouteInfo(@NonNull String totalDistance, @NonNull String totalDistanceUnits,
  //                                   int totalTimeInSec, @NonNull String totalPedestrianDistance, @NonNull String
  //                                   totalPedestrianDistanceUnits, int totalPedestrianTimeInSec, @NonNull
  //                                   TransitStepInfo[] steps)
  static jmethodID const transitRouteInfoConstructor =
    jni::GetConstructorID(env, transitRouteInfoClass,
                          "(Ljava/lang/String;Ljava/lang/String;I"
                          "Ljava/lang/String;Ljava/lang/String;I"
                          "[Lapp/organicmaps/sdk/routing/TransitStepInfo;)V");
  jni::TScopedLocalRef const distance(env, jni::ToJavaString(env, routeInfo.m_totalDistanceStr));
  jni::TScopedLocalRef const distanceUnits(env, jni::ToJavaString(env, routeInfo.m_totalDistanceUnitsSuffix));
  jni::TScopedLocalRef const distancePedestrian(env, jni::ToJavaString(env, routeInfo.m_totalPedestrianDistanceStr));
  jni::TScopedLocalRef const distancePedestrianUnits(env,
                                                     jni::ToJavaString(env, routeInfo.m_totalPedestrianUnitsSuffix));
  return env->NewObject(transitRouteInfoClass, transitRouteInfoConstructor, distance.get(), distanceUnits.get(),
                        static_cast<jint>(routeInfo.m_totalTimeInSec), distancePedestrian.get(),
                        distancePedestrianUnits.get(), static_cast<jint>(routeInfo.m_totalPedestrianTimeInSec), steps);
}
