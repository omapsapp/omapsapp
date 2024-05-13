#include "jni_helper.hpp"
#include "logging.hpp"
#include "ScopedLocalRef.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"

#include <vector>

static JavaVM * g_jvm = 0;
extern JavaVM * GetJVM()
{
  return g_jvm;
}

// Caching is necessary to create class from native threads.
jclass g_mapObjectClazz;
jclass g_featureIdClazz;
jclass g_bookmarkClazz;
jclass g_httpClientClazz;
jclass g_httpParamsClazz;
jclass g_platformSocketClazz;
jclass g_utilsClazz;
jclass g_loggerClazz;
jclass g_keyValueClazz;
jclass g_networkPolicyClazz;
jclass g_elevationInfoClazz;

extern "C"
{
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM * jvm, void *)
{
  g_jvm = jvm;
  jni::InitSystemLog();
  jni::InitAssertLog();

  JNIEnv * env = jni::GetEnv();
  g_mapObjectClazz = jni::GetGlobalClassRef(env, "app/organicmaps/bookmarks/data/MapObject");
  g_featureIdClazz = jni::GetGlobalClassRef(env, "app/organicmaps/bookmarks/data/FeatureId");
  g_bookmarkClazz = jni::GetGlobalClassRef(env, "app/organicmaps/bookmarks/data/Bookmark");
  g_httpClientClazz = jni::GetGlobalClassRef(env, "app/organicmaps/util/HttpClient");
  g_httpParamsClazz = jni::GetGlobalClassRef(env, "app/organicmaps/util/HttpClient$Params");
  g_platformSocketClazz = jni::GetGlobalClassRef(env, "app/organicmaps/location/PlatformSocket");
  g_utilsClazz = jni::GetGlobalClassRef(env, "app/organicmaps/util/Utils");
  g_loggerClazz = jni::GetGlobalClassRef(env, "app/organicmaps/util/log/Logger");
  g_keyValueClazz = jni::GetGlobalClassRef(env, "app/organicmaps/util/KeyValue");
  g_networkPolicyClazz = jni::GetGlobalClassRef(env, "app/organicmaps/util/NetworkPolicy");
  g_elevationInfoClazz = jni::GetGlobalClassRef(env, "app/organicmaps/bookmarks/data/ElevationInfo");

  return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
JNI_OnUnload(JavaVM *, void *)
{
  g_jvm = 0;
  JNIEnv * env = jni::GetEnv();
  env->DeleteGlobalRef(g_mapObjectClazz);
  env->DeleteGlobalRef(g_featureIdClazz);
  env->DeleteGlobalRef(g_bookmarkClazz);
  env->DeleteGlobalRef(g_httpClientClazz);
  env->DeleteGlobalRef(g_httpParamsClazz);
  env->DeleteGlobalRef(g_platformSocketClazz);
  env->DeleteGlobalRef(g_utilsClazz);
  env->DeleteGlobalRef(g_loggerClazz);
  env->DeleteGlobalRef(g_keyValueClazz);
  env->DeleteGlobalRef(g_networkPolicyClazz);
  env->DeleteGlobalRef(g_elevationInfoClazz);
}
} // extern "C"

namespace jni
{
JNIEnv * GetEnv()
{
  JNIEnv * env;
  if (JNI_OK != g_jvm->GetEnv((void **)&env, JNI_VERSION_1_6))
  {
    LOG(LERROR, ("Can't get JNIEnv. Is the thread attached to JVM?"));
    MYTHROW(RootException, ("Can't get JNIEnv. Is the thread attached to JVM?"));
  }
  return env;
}

JavaVM * GetJVM()
{
  ASSERT(g_jvm, ("JVM is not initialized"));
  return g_jvm;
}

jmethodID GetMethodID(JNIEnv * env, jobject obj, char const * name, char const * signature)
{
  // GetObjectClass may hang in WaitHoldingLocks.
  TScopedLocalClassRef clazz(env, env->GetObjectClass(obj));
  ASSERT(clazz.get(), ("Can't get class: ", DescribeException()));

  jmethodID mid = env->GetMethodID(clazz.get(), name, signature);
  ASSERT(mid, ("Can't get method ID", name, signature, DescribeException()));
  return mid;
}

jmethodID GetStaticMethodID(JNIEnv * env, jclass clazz, char const * name, char const * signature)
{
  jmethodID mid = env->GetStaticMethodID(clazz, name, signature);
  ASSERT(mid, ("Can't get static method ID", name, signature, DescribeException()));
  return mid;
}

jfieldID GetStaticFieldID(JNIEnv * env, jclass clazz, char const * name, char const * signature)
{
  jfieldID fid = env->GetStaticFieldID(clazz, name, signature);
  ASSERT(fid, ("Can't get static field ID", name, signature, DescribeException()));
  return fid;
}

jmethodID GetConstructorID(JNIEnv * env, jclass clazz, char const * signature)
{
  jmethodID const ctorID = env->GetMethodID(clazz, "<init>", signature);
  ASSERT(ctorID, (DescribeException()));
  return ctorID;
}

jclass GetGlobalClassRef(JNIEnv * env, char const * sig)
{
  jclass klass = env->FindClass(sig);
  ASSERT(klass, ("Can't get class : ", DescribeException()));
  return static_cast<jclass>(env->NewGlobalRef(klass));
}

std::string ToNativeString(JNIEnv * env, jstring str)
{
  std::string result;
  char const * utfBuffer = env->GetStringUTFChars(str, 0);
  if (utfBuffer)
  {
    result = utfBuffer;
    env->ReleaseStringUTFChars(str, utfBuffer);
  }
  return result;
}

std::string ToNativeString(JNIEnv * env, jbyteArray const & bytes)
{
  int const len = env->GetArrayLength(bytes);
  std::vector<char> buffer(len);
  env->GetByteArrayRegion(bytes, 0, len, reinterpret_cast<jbyte *>(buffer.data()));
  return std::string(buffer.data(), len);
}

jstring ToJavaString(JNIEnv * env, char const * s)
{
  return env->NewStringUTF(s);
}

jclass GetStringClass(JNIEnv * env)
{
  return env->FindClass(GetStringClassName());
}

char const * GetStringClassName()
{
  return "java/lang/String";
}

struct global_ref_deleter
{
  void operator()(jobject * ref)
  {
    GetEnv()->DeleteGlobalRef(*ref);
    delete ref;
  }
};

std::shared_ptr<jobject> make_global_ref(jobject obj)
{
  jobject * ref = new jobject;
  *ref = GetEnv()->NewGlobalRef(obj);
  return std::shared_ptr<jobject>(ref, global_ref_deleter());
}

std::string ToNativeString(JNIEnv * env, const jthrowable & e)
{
  jni::TScopedLocalClassRef logClassRef(env, env->FindClass("android/util/Log"));
  ASSERT(logClassRef.get(), ());
  static jmethodID const getStacktraceMethod =
    jni::GetStaticMethodID(env, logClassRef.get(), "getStackTraceString",
                           "(Ljava/lang/Throwable;)Ljava/lang/String;");
  ASSERT(getStacktraceMethod, ());
  TScopedLocalRef resultRef(env, env->CallStaticObjectMethod(logClassRef.get(), getStacktraceMethod, e));
  return ToNativeString(env, (jstring) resultRef.get());
}


bool HandleJavaException(JNIEnv * env)
{
  if (env->ExceptionCheck())
   {
     jni::ScopedLocalRef<jthrowable> const e(env, env->ExceptionOccurred());
     env->ExceptionDescribe();
     env->ExceptionClear();
     base::LogLevel level = GetLogLevelForException(env, e.get());
     LOG(level, (ToNativeString(env, e.get())));
     return true;
   }
   return false;
}

base::LogLevel GetLogLevelForException(JNIEnv * env, const jthrowable & e)
{
  static jclass const errorClass = jni::GetGlobalClassRef(env, "java/lang/Error");
  ASSERT(errorClass, (jni::DescribeException()));
  static jclass const runtimeExceptionClass =
    jni::GetGlobalClassRef(env, "java/lang/RuntimeException");
  ASSERT(runtimeExceptionClass, (jni::DescribeException()));
  // If Unchecked Exception or Error is occurred during Java call the app should fail immediately.
  // In other cases, just a warning message about exception (Checked Exception)
  // will be written into LogCat.
  if (env->IsInstanceOf(e, errorClass) || env->IsInstanceOf(e, runtimeExceptionClass))
    return LERROR;

  return LWARNING;
}

std::string DescribeException()
{
  JNIEnv * env = GetEnv();

  if (env->ExceptionCheck())
  {
    jni::ScopedLocalRef<jthrowable> const e(env, env->ExceptionOccurred());

    // have to clear the exception before JNI will work again.
    env->ExceptionClear();

    return ToNativeString(env, e.get());
  }
  return {};
}

jobject GetNewParcelablePointD(JNIEnv * env, m2::PointD const & point)
{
  jclass klass = env->FindClass("app/organicmaps/bookmarks/data/ParcelablePointD");
  ASSERT ( klass, () );
  jmethodID methodID = GetConstructorID(env, klass, "(DD)V");

  return env->NewObject(klass, methodID,
                        static_cast<jdouble>(point.x),
                        static_cast<jdouble>(point.y));
}

jobject GetNewPoint(JNIEnv * env, m2::PointD const & point)
{
  return GetNewPoint(env, m2::PointI(static_cast<int>(point.x), static_cast<int>(point.y)));
}

jobject GetNewPoint(JNIEnv * env, m2::PointI const & point)
{
  jclass klass = env->FindClass("android/graphics/Point");
  ASSERT ( klass, () );
  jmethodID methodID = GetConstructorID(env, klass, "(II)V");

  return env->NewObject(klass, methodID,
                        static_cast<jint>(point.x),
                        static_cast<jint>(point.y));
}

// This util method dumps content of local and global reference jni tables to logcat for debug and testing purposes
void DumpDalvikReferenceTables()
{
  JNIEnv * env = GetEnv();
  jclass vm_class = env->FindClass("dalvik/system/VMDebug");
  jmethodID dump_mid = env->GetStaticMethodID(vm_class, "dumpReferenceTables", "()V");
  env->CallStaticVoidMethod(vm_class, dump_mid);
  env->DeleteLocalRef(vm_class);
}

jobject ToKeyValue(JNIEnv * env, std::pair<std::string, std::string> src)
{
  static jmethodID const keyValueInit = jni::GetConstructorID(
    env, g_keyValueClazz, "(Ljava/lang/String;Ljava/lang/String;)V");

  jni::TScopedLocalRef key(env, jni::ToJavaString(env, src.first));
  jni::TScopedLocalRef value(env, jni::ToJavaString(env, src.second));

  return env->NewObject(g_keyValueClazz, keyValueInit, key.get(), value.get());
}

std::pair<std::string, std::string> ToNativeKeyValue(JNIEnv * env, jobject pairOfStrings)
{
  static jfieldID const keyId = env->GetFieldID(g_keyValueClazz, "mKey",
                                                  "Ljava/lang/String;");
  static jfieldID const valueId = env->GetFieldID(g_keyValueClazz, "mValue",
                                                   "Ljava/lang/String;");

  jni::ScopedLocalRef<jstring> const key(
    env, static_cast<jstring>(env->GetObjectField(pairOfStrings, keyId)));
  jni::ScopedLocalRef<jstring> const value(
    env, static_cast<jstring>(env->GetObjectField(pairOfStrings, valueId)));

  return { jni::ToNativeString(env, key.get()), jni::ToNativeString(env, value.get()) };
}
}  // namespace jni
