#pragma once

#include "app/organicmaps/core/jni_helper.hpp"

#include "routing/following_info.hpp"

#include <vector>

jobjectArray CreateLanesInfo(JNIEnv * env, std::vector<routing::FollowingInfo::SingleLaneInfoClient> const & lanes)
{
  if (lanes.empty())
    return nullptr;

  static jclass const laneClass = jni::GetGlobalClassRef(env, "app/organicmaps/sdk/routing/SingleLaneInfo");
  auto const lanesSize = static_cast<jsize>(lanes.size());
  jobjectArray jLanes = env->NewObjectArray(lanesSize, laneClass, nullptr);
  ASSERT(jLanes, (jni::DescribeException()));
  static jmethodID const ctorSingleLaneInfoID = jni::GetConstructorID(env, laneClass, "([BZ)V");

  for (jsize j = 0; j < lanesSize; ++j)
  {
    auto const laneSize = static_cast<jsize>(lanes[j].m_lane.size());
    jni::TScopedLocalByteArrayRef singleLane(env, env->NewByteArray(laneSize));
    ASSERT(singleLane.get(), (jni::DescribeException()));
    env->SetByteArrayRegion(singleLane.get(), 0, laneSize, lanes[j].m_lane.data());

    jni::TScopedLocalRef singleLaneInfo(
      env, env->NewObject(laneClass, ctorSingleLaneInfoID, singleLane.get(), lanes[j].m_isRecommended));
    ASSERT(singleLaneInfo.get(), (jni::DescribeException()));
    env->SetObjectArrayElement(jLanes, j, singleLaneInfo.get());
  }

  return jLanes;
}
