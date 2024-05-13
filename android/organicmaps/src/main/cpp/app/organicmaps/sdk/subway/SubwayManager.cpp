#include <jni.h>
#include <android/organicmaps/src/main/cpp/app/organicmaps/Framework.hpp>
#include "app/organicmaps/core/jni_helper.hpp"
#include "app/organicmaps/platform/AndroidPlatform.hpp"

extern "C"
{
static void TransitSchemeStateChanged(TransitReadManager::TransitSchemeState state,
                                      std::shared_ptr<jobject> const & listener)
{
  JNIEnv * env = jni::GetEnv();
  env->CallVoidMethod(*listener,
                      jni::GetMethodID(env, *listener, "onTransitStateChanged", "(I)V"),
                      static_cast<jint>(state));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_maplayer_subway_SubwayManager_nativeAddListener(JNIEnv *env, jclass clazz, jobject listener)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  g_framework->SetTransitSchemeListener(std::bind(&TransitSchemeStateChanged,
                                                  std::placeholders::_1,
                                                  jni::make_global_ref(listener)));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_maplayer_subway_SubwayManager_nativeRemoveListener(JNIEnv * env, jclass clazz)
{
  CHECK(g_framework, ("Framework isn't created yet!"));
  g_framework->SetTransitSchemeListener(TransitReadManager::TransitStateChangedFn());
}
}
