#include "app/organicmaps/core/jni_helper.hpp"

#include "app/organicmaps/Framework.hpp"

#include "indexer/map_style.hpp"

extern "C"
{
JNIEXPORT void JNICALL Java_app_organicmaps_sdk_ChoosePositionMode_nativeSet(JNIEnv *, jclass, jint mode,
                                                                             jboolean isBusiness,
                                                                             jboolean applyPosition)
{
  // TODO(AB): Move this code into the Framework to share with iOS and other platforms.
  auto const f = frm();
  if (applyPosition && f->HasPlacePageInfo())
    g_framework->SetChoosePositionMode(static_cast<android::ChoosePositionMode>(mode), isBusiness,
                                       &f->GetCurrentPlacePageInfo().GetMercator());
  else
    g_framework->SetChoosePositionMode(static_cast<android::ChoosePositionMode>(mode), isBusiness, nullptr);
}

JNIEXPORT jint JNICALL Java_app_organicmaps_sdk_ChoosePositionMode_nativeGet(JNIEnv *, jclass)
{
  return static_cast<jint>(g_framework->GetChoosePositionMode());
}
}
