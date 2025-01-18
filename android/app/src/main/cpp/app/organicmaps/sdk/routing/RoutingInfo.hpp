#pragma once

#include "app/organicmaps/core/jni_helper.hpp"
#include "app/organicmaps/sdk/routing/SingleLaneInfo.hpp"

#include "map/routing_manager.hpp"

jobject CreateRoutingInfo(JNIEnv * env, routing::FollowingInfo const & info, RoutingManager & rm)
{
  static jclass const klass = jni::GetGlobalClassRef(env, "app/organicmaps/sdk/routing/RoutingInfo");
  // Java signature : RoutingInfo(Distance distToTarget, Distance distToTurn,
  //                              String currentStreet, String nextStreet, String nextNextStreet,
  //                              double completionPercent, int vehicleTurnOrdinal, int
  //                              vehicleNextTurnOrdinal, int pedestrianTurnOrdinal, int exitNum,
  //                              int totalTime, SingleLaneInfo[] lanes)
  static jmethodID const ctorRouteInfoID =
    jni::GetConstructorID(env, klass,
                          "(Lapp/organicmaps/util/Distance;Lapp/organicmaps/util/Distance;"
                          "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;DIIIII"
                          "[Lapp/organicmaps/sdk/routing/SingleLaneInfo;DZZ)V");

  jobjectArray jLanes = CreateLanesInfo(env, info.m_lanes);

  auto const isSpeedCamLimitExceeded = rm.IsSpeedCamLimitExceeded();
  auto const shouldPlaySignal = rm.GetSpeedCamManager().ShouldPlayBeepSignal();
  jobject const result = env->NewObject(
    klass, ctorRouteInfoID, ToJavaDistance(env, info.m_distToTarget), ToJavaDistance(env, info.m_distToTurn),
    jni::ToJavaString(env, info.m_currentStreetName), jni::ToJavaString(env, info.m_nextStreetName),
    jni::ToJavaString(env, info.m_nextNextStreetName), info.m_completionPercent, info.m_turn, info.m_nextTurn,
    info.m_pedestrianTurn, info.m_exitNum, info.m_time, jLanes, info.m_speedLimitMps,
    static_cast<jboolean>(isSpeedCamLimitExceeded), static_cast<jboolean>(shouldPlaySignal));
  ASSERT(result, (jni::DescribeException()));
  return result;
}
