#include "app/organicmaps/Framework.hpp"

#include "platform/settings.hpp"

extern "C"
{
JNIEXPORT void JNICALL
Java_app_organicmaps_settings_MapLanguageCode_setMapLanguageCode(JNIEnv * env, jobject, jstring languageCode)
{
  g_framework->SetMapLanguageCode(jni::ToNativeString(env, languageCode));
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_settings_MapLanguageCode_getMapLanguageCode(JNIEnv * env, jobject)
{
  return jni::ToJavaString(env, g_framework->GetMapLanguageCode());
}
}
