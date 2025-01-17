#include "app/organicmaps/core/jni_helper.hpp"

#include "app/organicmaps/Framework.hpp"

#include "indexer/map_style.hpp"

extern "C"
{
JNIEXPORT void JNICALL Java_app_organicmaps_sdk_MapStyle_nativeSet(JNIEnv *, jclass, jint mapStyle)
{
  auto const val = static_cast<MapStyle>(mapStyle);
  if (val != g_framework->GetMapStyle())
    g_framework->SetMapStyle(val);
}

JNIEXPORT jint JNICALL Java_app_organicmaps_sdk_MapStyle_nativeGet(JNIEnv *, jclass)
{
  return g_framework->GetMapStyle();
}

JNIEXPORT void JNICALL Java_app_organicmaps_sdk_MapStyle_nativeMark(JNIEnv *, jclass, jint mapStyle)
{
  auto const val = static_cast<MapStyle>(mapStyle);
  if (val != g_framework->GetMapStyle())
    g_framework->MarkMapStyle(val);
}
}
