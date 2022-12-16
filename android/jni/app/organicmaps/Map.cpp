#include "Framework.hpp"

#include "app/organicmaps/core/jni_helper.hpp"

#include "app/organicmaps/platform/Platform.hpp"

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
JNIEXPORT jlong JNICALL
Java_app_organicmaps_Map_nativeCreateEngineId(JNIEnv * env, jclass)
{
    return g_framework->CreateDrapeEngineId();
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Map_nativeCreateEngine(JNIEnv * env, jclass,
                                            jlong engineId, jobject surface,
                                            jint density, jboolean firstLaunch,
                                            jboolean isLaunchByDeepLink,
                                            jint appVersionCode)
{
  return g_framework->CreateDrapeEngine(env, engineId, surface, density, firstLaunch, isLaunchByDeepLink,
                                        base::asserted_cast<uint32_t>(appVersionCode));
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Map_nativeIsEngineCreated(JNIEnv *, jclass, jlong engineId)
{
  return g_framework->IsDrapeEngineCreated(static_cast<df::DrapeEngineId>(engineId));
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Map_nativeShowMapForUrl(JNIEnv * env, jclass, jstring url)
{
  return g_framework->ShowMapForURL(jni::ToNativeString(env, url));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeSetRenderingInitializationFinishedListener(
  JNIEnv *, jclass, jlong engineId, jobject listener)
{
  if (listener)
  {
    g_framework->NativeFramework()->SetGraphicsContextInitializationHandler(engineId,
      std::bind(&OnRenderingInitializationFinished, jni::make_global_ref(listener)));
  }
  else
  {
    g_framework->NativeFramework()->SetGraphicsContextInitializationHandler(engineId, nullptr);
  }
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Map_nativeAttachSurface(JNIEnv * env, jclass, jlong engineId, jobject surface)
{
  return g_framework->AttachSurface(env, engineId, surface);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeDetachSurface(JNIEnv *, jclass, jlong engineId, jboolean destroySurface)
{
  g_framework->DetachSurface(engineId, destroySurface);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeSurfaceChanged(JNIEnv * env, jclass, jlong engineId, jobject surface, jint w, jint h)
{
  g_framework->Resize(env, engineId, surface, w, h);
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_Map_nativeDestroySurfaceOnDetach(JNIEnv *, jclass, jlong engineId)
{
  return g_framework->DestroySurfaceOnDetach(engineId);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativePauseSurfaceRendering(JNIEnv *, jclass, jlong engineId)
{
  g_framework->PauseSurfaceRendering(engineId);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeResumeSurfaceRendering(JNIEnv *, jclass, jlong engineId)
{
  g_framework->ResumeSurfaceRendering(engineId);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeApplyWidgets(JNIEnv *, jclass, jlong engineId)
{
  g_framework->ApplyWidgets(engineId);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeCleanWidgets(JNIEnv *, jclass, jlong engineId)
{
  g_framework->CleanWidgets(engineId);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeSetupWidget(
  JNIEnv *, jclass, jlong engineId, jint widget, jfloat x, jfloat y, jint anchor)
{
  g_framework->SetupWidget(engineId, static_cast<gui::EWidget>(widget), x, y, static_cast<dp::Anchor>(anchor));
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeCompassUpdated(JNIEnv *, jclass, jdouble north, jboolean forceRedraw)
{
  location::CompassInfo info;
  info.m_bearing = north;

  g_framework->OnCompassUpdated(info, forceRedraw);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeMove(
  JNIEnv *, jclass, jlong engineId, jdouble factorX, jdouble factorY, jboolean isAnim)
{
  g_framework->Move(engineId, factorX, factorY, isAnim);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeScalePlus(JNIEnv *, jclass, jlong engineId)
{
  g_framework->Scale(engineId, ::Framework::SCALE_MAG);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeScaleMinus(JNIEnv *, jclass, jlong engineId)
{
  g_framework->Scale(engineId, ::Framework::SCALE_MIN);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeScale(
  JNIEnv *, jclass, jlong engineId, jdouble factor, jdouble focusX, jdouble focusY, jboolean isAnim)
{
  g_framework->Scale(static_cast<df::DrapeEngineId>(engineId), factor, {focusX, focusY}, isAnim);
}

JNIEXPORT void JNICALL
Java_app_organicmaps_Map_nativeOnTouch(JNIEnv *, jclass, jlong engineId, jint action,
                                       jint id1, jfloat x1, jfloat y1,
                                       jint id2, jfloat x2, jfloat y2,
                                       jint maskedPointer)
{
  g_framework->Touch(engineId, action,
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
