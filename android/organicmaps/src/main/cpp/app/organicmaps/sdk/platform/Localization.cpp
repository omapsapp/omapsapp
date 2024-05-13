#include <jni.h>

#include "app/organicmaps/core/jni_helper.hpp"
#include "app/organicmaps/core/ScopedLocalRef.hpp"
#include "app/organicmaps/platform/AndroidPlatform.hpp"

#include "platform/localization.hpp"

#include <string>

namespace
{
jmethodID GetMethodId(std::string const & methodName)
{
  JNIEnv * env = jni::GetEnv();
  return jni::GetStaticMethodID(env, g_utilsClazz, methodName.c_str(),
                                "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;");
}

std::string GetLocalizedStringByUtil(jmethodID const & methodId, std::string const & str)
{
  JNIEnv * env = jni::GetEnv();

  jni::TScopedLocalRef strRef(env, jni::ToJavaString(env, str));
  jobject context = android::Platform::Instance().GetContext();
  jni::TScopedLocalRef localizedStrRef(env, env->CallStaticObjectMethod(g_utilsClazz, methodId,
                                                                        context, strRef.get()));
  return jni::ToNativeString(env, static_cast<jstring>(localizedStrRef.get()));
}
}  // namespace

namespace platform
{
std::string GetLocalizedTypeName(std::string const & type)
{
  static auto const methodId = GetMethodId("getLocalizedFeatureType");
  return GetLocalizedStringByUtil(methodId, type);
}

std::string GetLocalizedBrandName(std::string const & brand)
{
  static auto const methodId = GetMethodId("getLocalizedBrand");
  return GetLocalizedStringByUtil(methodId, brand);
}

std::string GetLocalizedString(std::string const & key)
{
  static auto const methodId = GetMethodId("getStringValueByKey");
  return GetLocalizedStringByUtil(methodId, key);
}

std::string GetCurrencySymbol(std::string const & currencyCode)
{
  JNIEnv * env = jni::GetEnv();
  static auto const methodId = jni::GetStaticMethodID(env, g_utilsClazz, "getCurrencySymbol",
                                                      "(Ljava/lang/String;)Ljava/lang/String;");

  jni::TScopedLocalRef currencyCodeRef(env, jni::ToJavaString(env, currencyCode));
  jni::TScopedLocalRef localizedStrRef(
      env, env->CallStaticObjectMethod(g_utilsClazz, methodId, currencyCodeRef.get()));
  return jni::ToNativeString(env, static_cast<jstring>(localizedStrRef.get()));
}

std::string GetLocalizedMyPositionBookmarkName()
{
  JNIEnv * env = jni::GetEnv();
  static auto const methodId = jni::GetStaticMethodID(env, g_utilsClazz, "getMyPositionBookmarkName",
                                                      "(Landroid/content/Context;)Ljava/lang/String;");

  jobject context = android::Platform::Instance().GetContext();
  jni::TScopedLocalRef localizedStrRef(env, env->CallStaticObjectMethod(g_utilsClazz, methodId, context));
  return jni::ToNativeString(env, static_cast<jstring>(localizedStrRef.get()));
}
}  // namespace platform
