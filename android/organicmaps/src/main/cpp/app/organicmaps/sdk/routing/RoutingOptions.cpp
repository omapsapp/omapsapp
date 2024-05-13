#include <jni.h>
#include <android/organicmaps/src/main/cpp/app/organicmaps/Framework.hpp>
#include <android/organicmaps/src/main/cpp/app/organicmaps/core/jni_helper.hpp>
#include "routing/routing_options.hpp"

routing::RoutingOptions::Road makeValue(jint option)
{
  uint8_t const road = static_cast<uint8_t>(1u << static_cast<int>(option));
  CHECK_LESS(road, static_cast<uint8_t>(routing::RoutingOptions::Road::Max), ());
  return static_cast<routing::RoutingOptions::Road>(road);
}

extern "C"
{

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_routing_RoutingOptions_nativeHasOption(JNIEnv * env, jclass clazz, jint option)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  routing::RoutingOptions routingOptions = routing::RoutingOptions::LoadCarOptionsFromSettings();
  routing::RoutingOptions::Road road = makeValue(option);
  return static_cast<jboolean>(routingOptions.Has(road));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_routing_RoutingOptions_nativeAddOption(JNIEnv * env, jclass clazz, jint option)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  routing::RoutingOptions routingOptions = routing::RoutingOptions::LoadCarOptionsFromSettings();
  routing::RoutingOptions::Road road = makeValue(option);
  routingOptions.Add(road);
  routing::RoutingOptions::SaveCarOptionsToSettings(routingOptions);
}


JNIEXPORT void JNICALL
Java_app_organicmaps_routing_RoutingOptions_nativeRemoveOption(JNIEnv * env, jclass clazz, jint option)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  routing::RoutingOptions routingOptions = routing::RoutingOptions::LoadCarOptionsFromSettings();
  routing::RoutingOptions::Road road = makeValue(option);
  routingOptions.Remove(road);
  routing::RoutingOptions::SaveCarOptionsToSettings(routingOptions);
}
}
