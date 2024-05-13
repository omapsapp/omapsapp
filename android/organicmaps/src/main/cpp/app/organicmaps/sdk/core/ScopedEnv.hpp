#pragma once

#include <jni.h>

// Scoped environment which can attach to any thread and automatically detach
class ScopedEnv final
{
public:
  ScopedEnv(JavaVM * vm)
  {
    JNIEnv * env;
    auto result = vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    if (result == JNI_EDETACHED)
    {
      result = vm->AttachCurrentThread(&env, nullptr);
      m_needToDetach = (result == JNI_OK);
    }

    if (result == JNI_OK)
    {
      m_env = env;
      m_vm = vm;
    }
  }

  ~ScopedEnv()
  {
    if (m_vm != nullptr && m_needToDetach)
      m_vm->DetachCurrentThread();
  }

  JNIEnv * operator->() { return m_env; }
  operator bool() const { return m_env != nullptr; }
  JNIEnv * get() { return m_env; }

private:
  bool m_needToDetach = false;
  JNIEnv * m_env = nullptr;
  JavaVM * m_vm = nullptr;
};
