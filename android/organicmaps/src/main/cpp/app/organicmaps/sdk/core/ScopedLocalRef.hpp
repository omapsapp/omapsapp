#pragma once

#include <jni.h>

namespace jni
{
// A smart pointer that deletes a JNI local reference when it goes out of scope.
template <typename T>
class ScopedLocalRef {
public:
  ScopedLocalRef(JNIEnv * env, T localRef) : m_env(env), m_localRef(localRef) {}

  ~ScopedLocalRef() { reset(); }

  void reset(T ptr = nullptr)
  {
    if (ptr == m_localRef)
      return;

    if (m_localRef != nullptr)
      m_env->DeleteLocalRef(m_localRef);
    m_localRef = ptr;
  }

  T get() const { return m_localRef; }

  operator T() const { return m_localRef; }

private:
  JNIEnv * m_env;
  T m_localRef;

  // Disallow copy and assignment.
  ScopedLocalRef(ScopedLocalRef const &) = delete;
  void operator=(ScopedLocalRef const &) = delete;
};

}  // namespace jni
