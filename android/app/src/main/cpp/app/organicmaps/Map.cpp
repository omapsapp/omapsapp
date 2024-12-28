#include "Framework.hpp"

#include "app/organicmaps/core/jni_helper.hpp"

#include "app/organicmaps/platform/AndroidPlatform.hpp"

#include "storage/storage_defines.hpp"

#include "base/logging.hpp"

#include "platform/settings.hpp"

namespace
{
void OnRenderingInitializationFinished(std::shared_ptr<jobject> const & listener)
{
  JNIEnv * env = jni::GetEnv();
  env->CallVoidMethod(*listener, jni::GetMethodID(env, *listener.get(),
                      "onRenderingInitializationFinished", "()V"));
}
}  // namespace

extern "C"
{
JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Map_nativeCreateEngine(JNIEnv * env, jclass,
                                            jobject surface, jint density,
                                            jboolean firstLaunch,
                                            jboolean isLaunchByDeepLink,
                                            jint appVersionCode,
                                            jboolean isCustomROM)
{
  return g_framework->CreateDrapeEngine(env, surface, density, firstLaunch, isLaunchByDeepLink,
                                        base::asserted_cast<uint32_t>(appVersionCode), isCustomROM);
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Map_nativeIsEngineCreated(JNIEnv *, jclass)
{
  return g_framework->IsDrapeEngineCreated();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeUpdateEngineDpi(JNIEnv *, jclass, jint dpi)
{
  return g_framework->UpdateDpi(dpi);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeExecuteMapApiRequest(JNIEnv * env, jclass)
{
  return g_framework->ExecuteMapApiRequest();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeSetRenderingInitializationFinishedListener(
  JNIEnv *, jclass, jobject listener)
{
  if (listener)
  {
    g_framework->NativeFramework()->SetGraphicsContextInitializationHandler(
      std::bind(&OnRenderingInitializationFinished, jni::make_global_ref(listener)));
  }
  else
  {
    g_framework->NativeFramework()->SetGraphicsContextInitializationHandler(nullptr);
  }
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Map_nativeAttachSurface(JNIEnv * env, jclass, jobject surface)
{
  return g_framework->AttachSurface(env, surface);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeDetachSurface(JNIEnv *, jclass, jboolean destroySurface)
{
  g_framework->DetachSurface(destroySurface);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeSurfaceChanged(JNIEnv * env, jclass, jobject surface, jint w, jint h)
{
  g_framework->Resize(env, surface, w, h);
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Map_nativeDestroySurfaceOnDetach(JNIEnv *, jclass)
{
  return g_framework->DestroySurfaceOnDetach();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativePauseSurfaceRendering(JNIEnv *, jclass)
{
  g_framework->PauseSurfaceRendering();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeResumeSurfaceRendering(JNIEnv *, jclass)
{
  g_framework->ResumeSurfaceRendering();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeUpdateMyPositionRoutingOffset(JNIEnv * env, jclass clazz, int offsetY)
{
  g_framework->UpdateMyPositionRoutingOffset(offsetY);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeApplyWidgets(JNIEnv *, jclass)
{
  g_framework->ApplyWidgets();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeCleanWidgets(JNIEnv *, jclass)
{
  g_framework->CleanWidgets();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeSetupWidget(
  JNIEnv *, jclass, jint widget, jfloat x, jfloat y, jint anchor)
{
  g_framework->SetupWidget(static_cast<gui::EWidget>(widget), x, y, static_cast<dp::Anchor>(anchor));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeCompassUpdated(JNIEnv *, jclass, jdouble north, jboolean forceRedraw)
{
  location::CompassInfo info;
  info.m_bearing = north;

  g_framework->OnCompassUpdated(info, forceRedraw);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeScalePlus(JNIEnv *, jclass)
{
  g_framework->Scale(::Framework::SCALE_MAG);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeScaleMinus(JNIEnv *, jclass)
{
  g_framework->Scale(::Framework::SCALE_MIN);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeOnScroll(
  JNIEnv *, jclass, jdouble distanceX, jdouble distanceY)
{
  g_framework->Scroll(distanceX, distanceY);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeOnScale(
  JNIEnv *, jclass, jdouble factor, jdouble focusX, jdouble focusY, jboolean isAnim)
{
  g_framework->Scale(factor, {focusX, focusY}, isAnim);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeOnTouch(JNIEnv *, jclass, jint action,
                                       jint id1, jfloat x1, jfloat y1,
                                       jint id2, jfloat x2, jfloat y2,
                                       jint maskedPointer)
{
  g_framework->Touch(action,
                     android::Framework::Finger(id1, x1, y1),
                     android::Framework::Finger(id2, x2, y2), maskedPointer);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeStorageConnected(JNIEnv *, jclass)
{
  android::Platform::Instance().OnExternalStorageStatusChanged(true);
  g_framework->AddLocalMaps();
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeStorageDisconnected(JNIEnv *, jclass)
{
  android::Platform::Instance().OnExternalStorageStatusChanged(false);
  g_framework->RemoveLocalMaps();
}
} // extern "C"
