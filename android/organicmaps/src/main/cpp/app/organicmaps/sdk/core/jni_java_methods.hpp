#pragma once

#include <jni.h>

#define DECLARE_BUILDER_INSTANCE(BuilderType) static BuilderType const & Instance(JNIEnv * env) { \
                                              static BuilderType const inst(env); return inst; }

namespace jni
{

class PairBuilder
{
  jclass m_class;
  jmethodID m_ctor;

  explicit PairBuilder(JNIEnv * env);

public:
  DECLARE_BUILDER_INSTANCE(PairBuilder);
  jobject Create(JNIEnv * env, jobject o1, jobject o2) const;
};

class ListBuilder
{
  jclass m_arrayClass;
  jmethodID m_arrayCtor;

public:
  jmethodID m_add;

  explicit ListBuilder(JNIEnv * env);
  DECLARE_BUILDER_INSTANCE(ListBuilder);

  jobject CreateArray(JNIEnv * env, size_t sz) const;
};

} // namespace jni
