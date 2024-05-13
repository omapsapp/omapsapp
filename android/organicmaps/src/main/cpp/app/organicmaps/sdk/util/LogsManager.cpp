#include <jni.h>
#include "app/organicmaps/core/logging.hpp"

extern "C" {
JNIEXPORT void JNICALL
Java_app_organicmaps_util_log_LogsManager_nativeToggleCoreDebugLogs(
    JNIEnv * /*env*/, jclass /*clazz*/, jboolean enabled)
{
  jni::ToggleDebugLogs(enabled);
}
}  // extern "C"
