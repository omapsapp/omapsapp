#include "jni_java_methods.hpp"
#include "jni_helper.hpp"

namespace jni
{

PairBuilder::PairBuilder(JNIEnv * env)
{
  m_class = jni::GetGlobalClassRef(env, "android/util/Pair");
  m_ctor = jni::GetConstructorID(env, m_class, "(Ljava/lang/Object;Ljava/lang/Object;)V");
  ASSERT(m_ctor, ());
}

jobject PairBuilder::Create(JNIEnv * env, jobject o1, jobject o2) const
{
  return env->NewObject(m_class, m_ctor, o1, o2);
}

ListBuilder::ListBuilder(JNIEnv * env)
{
  m_arrayClass = jni::GetGlobalClassRef(env, "java/util/ArrayList");
  m_arrayCtor = jni::GetConstructorID(env, m_arrayClass, "(I)V");

  jclass clazz = env->FindClass("java/util/List");
  m_add = env->GetMethodID(clazz, "add", "(Ljava/lang/Object;)Z");
}

jobject ListBuilder::CreateArray(JNIEnv * env, size_t sz) const
{
  return env->NewObject(m_arrayClass, m_arrayCtor, sz);
}
} // namespace jni
