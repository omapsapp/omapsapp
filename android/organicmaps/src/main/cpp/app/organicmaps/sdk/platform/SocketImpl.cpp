#include "app/organicmaps/core/jni_helper.hpp"

#include "platform/socket.hpp"

#include "base/logging.hpp"

#include <memory>

namespace platform
{
class SocketImpl : public Socket
{
public:
  SocketImpl()
  {
    JNIEnv * env = jni::GetEnv();
    static jmethodID const socketConstructor = jni::GetConstructorID(env, g_platformSocketClazz, "()V");
    jni::TScopedLocalRef localSelf(env, env->NewObject(g_platformSocketClazz, socketConstructor));
    m_self = env->NewGlobalRef(localSelf.get());
    ASSERT(m_self, ());
  }

  ~SocketImpl()
  {
    Close();
    JNIEnv * env = jni::GetEnv();
    env->DeleteGlobalRef(m_self);
  }

  bool Open(std::string const & host, uint16_t port)
  {
    JNIEnv * env = jni::GetEnv();
    static jmethodID const openMethod =
        jni::GetMethodID(env, m_self, "open", "(Ljava/lang/String;I)Z");
    jni::TScopedLocalRef hostRef(env, jni::ToJavaString(env, host));
    jboolean result = env->CallBooleanMethod(m_self, openMethod, hostRef.get(), static_cast<jint>(port));
    if (jni::HandleJavaException(env))
      return false;
    return result;
  }

  void Close()
  {
    JNIEnv * env = jni::GetEnv();
    static jmethodID const closeMethod = jni::GetMethodID(env, m_self, "close", "()V");
    env->CallVoidMethod(m_self, closeMethod);
    jni::HandleJavaException(env);
  }

  bool Read(uint8_t * data, uint32_t count)
  {
    JNIEnv * env = jni::GetEnv();
    jbyteArray array = env->NewByteArray(count);
    static jmethodID const readMethod = jni::GetMethodID(env, m_self, "read", "([BI)Z");
    jboolean result = env->CallBooleanMethod(m_self, readMethod, array, static_cast<jint>(count));
    if (jni::HandleJavaException(env))
      return false;
    //this call copies java byte array to native buffer
    env->GetByteArrayRegion(array, 0, count, reinterpret_cast<jbyte *>(data));
    if (jni::HandleJavaException(env))
      return false;
    return result;
  }

  bool Write(uint8_t const * data, uint32_t count)
  {
    JNIEnv * env = jni::GetEnv();
    jni::TScopedLocalByteArrayRef arrayRef(env, env->NewByteArray(count));
    //this call copies native buffer to java byte array
    env->SetByteArrayRegion(arrayRef.get(), 0, count, reinterpret_cast<const jbyte *>(data));
    static jmethodID const writeMethod = jni::GetMethodID(env, m_self, "write", "([BI)Z");
    jboolean result = env->CallBooleanMethod(m_self, writeMethod, arrayRef.get(), static_cast<jint>(count));
    if (jni::HandleJavaException(env))
      return false;
    return result;
  }

  void SetTimeout(uint32_t milliseconds)
  {
    JNIEnv * env = jni::GetEnv();
    static jmethodID const setTimeoutMethod = jni::GetMethodID(env, m_self, "setTimeout", "(I)V");
    env->CallVoidMethod(m_self, setTimeoutMethod, static_cast<jint>(milliseconds));
    jni::HandleJavaException(env);
  };

private:
  jobject m_self;
};

std::unique_ptr<Socket> CreateSocket() { return std::make_unique<SocketImpl>(); }
}
