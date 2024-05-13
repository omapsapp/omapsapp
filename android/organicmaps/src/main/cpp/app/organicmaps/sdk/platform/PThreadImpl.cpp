#include "app/organicmaps/core/jni_helper.hpp"

/// Implements bodies of base/thread.hpp functions for Android

void AndroidThreadAttachToJVM()
{
  JNIEnv * env;
  jni::GetJVM()->AttachCurrentThread(&env, 0);
}

void AndroidThreadDetachFromJVM()
{
  jni::GetJVM()->DetachCurrentThread();
}
