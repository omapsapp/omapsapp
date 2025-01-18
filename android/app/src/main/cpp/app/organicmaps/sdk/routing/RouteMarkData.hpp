#pragma once

#include "app/organicmaps/core/jni_helper.hpp"

#include "geometry/point2d.hpp"

#include <vector>

jobjectArray CreateRouteMarkDataArray(JNIEnv * env, std::vector<RouteMarkData> const & points)
{
  static jclass const pointClazz = jni::GetGlobalClassRef(env, "app/organicmaps/sdk/routing/RouteMarkData");
  // Java signature : RouteMarkData(String title, String subtitle, int pointType,
  //                                int intermediateIndex, boolean isVisible, boolean isMyPosition,
  //                                boolean isPassed, double lat, double lon)
  static jmethodID const pointConstructor =
    jni::GetConstructorID(env, pointClazz, "(Ljava/lang/String;Ljava/lang/String;IIZZZDD)V");
  return jni::ToJavaArray(env, pointClazz, points,
                          [&](JNIEnv * jEnv, RouteMarkData const & data)
                          {
                            jni::TScopedLocalRef const title(env, jni::ToJavaString(env, data.m_title));
                            jni::TScopedLocalRef const subtitle(env, jni::ToJavaString(env, data.m_subTitle));
                            return env->NewObject(
                              pointClazz, pointConstructor, title.get(), subtitle.get(),
                              static_cast<jint>(data.m_pointType), static_cast<jint>(data.m_intermediateIndex),
                              static_cast<jboolean>(data.m_isVisible), static_cast<jboolean>(data.m_isMyPosition),
                              static_cast<jboolean>(data.m_isPassed), mercator::YToLat(data.m_position.y),
                              mercator::XToLon(data.m_position.x));
                          });
}
