#pragma once

#include "app/organicmaps/core/jni_helper.hpp"

#include "geometry/point2d.hpp"

#include <vector>

jobjectArray CreateJunctionInfoArray(JNIEnv * env, std::vector<m2::PointD> const & junctionPoints)
{
  static jclass const junctionClazz = jni::GetGlobalClassRef(env, "app/organicmaps/sdk/routing/JunctionInfo");
  // Java signature : JunctionInfo(double lat, double lon)
  static jmethodID const junctionConstructor = jni::GetConstructorID(env, junctionClazz, "(DD)V");

  return jni::ToJavaArray(env, junctionClazz, junctionPoints,
                          [](JNIEnv * env, m2::PointD const & point)
                          {
                            return env->NewObject(junctionClazz, junctionConstructor, mercator::YToLat(point.y),
                                                  mercator::XToLon(point.x));
                          });
}
