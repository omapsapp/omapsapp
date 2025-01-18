#pragma once

#include "app/organicmaps/core/jni_helper.hpp"

jobject GetRebuildAfterPointsLoading(JNIEnv * env)
{
  static jobject rebuildAfterPointsLoading = nullptr;
  if (rebuildAfterPointsLoading)
    return rebuildAfterPointsLoading;

  // Find the RouteRecommendationType class
  jclass routeRecommendationTypeClass = env->FindClass("app/organicmaps/sdk/routing/RouteRecommendationType");
  ASSERT(routeRecommendationTypeClass, ());

  // Get the values() method of RouteRecommendationType
  jmethodID valuesMethod = env->GetStaticMethodID(routeRecommendationTypeClass, "values",
                                                  "()[Lapp/organicmaps/sdk/routing/RouteRecommendationType;");
  ASSERT(valuesMethod, ());

  // Call values() to get all enum constants
  jobjectArray enumConstants = (jobjectArray)env->CallStaticObjectMethod(routeRecommendationTypeClass, valuesMethod);
  ASSERT(enumConstants, ());

  // Retrieve the first (and only) constant, RebuildAfterPointsLoading
  rebuildAfterPointsLoading = env->NewGlobalRef(env->GetObjectArrayElement(enumConstants, 0));
  ASSERT(rebuildAfterPointsLoading, ());

  return rebuildAfterPointsLoading;
}

jobject GetRouteRecommendationType(JNIEnv * env, RoutingManager::Recommendation recommendation)
{
  switch (recommendation)
  {
  case RoutingManager::Recommendation::RebuildAfterPointsLoading: return GetRebuildAfterPointsLoading(env);
  default: ASSERT_FAIL("Unknown recommendation type");
  }
}
