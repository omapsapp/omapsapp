#include "app/organicmaps/Framework.hpp"

#include "app/organicmaps/platform/GuiThread.hpp"
#include "app/organicmaps/platform/AndroidPlatform.hpp"

#include "app/organicmaps/core/jni_helper.hpp"

extern "C"
{
  // static void nativeSetSettingsDir(String settingsPath);
  JNIEXPORT void JNICALL
  Java_app_organicmaps_MwmApplication_nativeSetSettingsDir(JNIEnv * env, jclass clazz, jstring settingsPath)
  {
    android::Platform::Instance().SetSettingsDir(jni::ToNativeString(env, settingsPath));
  }

  // void nativeInitPlatform(String apkPath, String storagePath, String privatePath, String tmpPath,
  // String flavorName, String buildType, boolean isTablet);
  JNIEXPORT void JNICALL
  Java_app_organicmaps_MwmApplication_nativeInitPlatform(JNIEnv * env, jobject thiz,
                                                             jstring apkPath, jstring writablePath,
                                                             jstring privatePath, jstring tmpPath,
                                                             jstring flavorName, jstring buildType,
                                                             jboolean isTablet)
  {
    android::Platform::Instance().Initialize(env, thiz, apkPath, writablePath, privatePath, tmpPath,
                                             flavorName, buildType, isTablet);
  }

  // static void nativeInitFramework(@NonNull Runnable onComplete);
  JNIEXPORT void JNICALL
  Java_app_organicmaps_MwmApplication_nativeInitFramework(JNIEnv * env, jclass clazz, jobject onComplete)
  {
    if (!g_framework)
    {
      g_framework = std::make_unique<android::Framework>([onComplete = jni::make_global_ref(onComplete)]()
      {
        JNIEnv * env = jni::GetEnv();
        jmethodID const methodId = jni::GetMethodID(env, *onComplete, "run", "()V");
        env->CallVoidMethod(*onComplete, methodId);
      });
    }
  }

  // static void nativeProcessTask(long taskPointer);
  JNIEXPORT void JNICALL
  Java_app_organicmaps_MwmApplication_nativeProcessTask(JNIEnv * env, jclass clazz, jlong taskPointer)
  {
    android::GuiThread::ProcessTask(taskPointer);
  }

  // static void nativeAddLocalization(String name, String value);
  JNIEXPORT void JNICALL
  Java_app_organicmaps_MwmApplication_nativeAddLocalization(JNIEnv * env, jclass clazz, jstring name, jstring value)
  {
    g_framework->AddString(jni::ToNativeString(env, name),
                           jni::ToNativeString(env, value));
  }

  JNIEXPORT void JNICALL
  Java_app_organicmaps_MwmApplication_nativeOnTransit(JNIEnv *, jclass, jboolean foreground)
  {
    if (static_cast<bool>(foreground))
      g_framework->NativeFramework()->EnterForeground();
    else
      g_framework->NativeFramework()->EnterBackground();
  }
}
