#include "android/organicmaps/src/main/cpp/app/organicmaps/core/jni_helper.hpp"
#include "android/organicmaps/src/main/cpp/app/organicmaps/core/ScopedLocalRef.hpp"

#include "platform/locale.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <string>

/// This function is called from native c++ code
std::string GetAndroidSystemLanguage()
{
  static char const * DEFAULT_LANG = "en";

  JNIEnv * env = jni::GetEnv();
  if (!env)
  {
    LOG(LWARNING, ("Can't get JNIEnv"));
    return DEFAULT_LANG;
  }

  static jclass const languageClass = jni::GetGlobalClassRef(env, "app/organicmaps/util/Language");
  static jmethodID const getDefaultLocaleId = jni::GetStaticMethodID(env, languageClass, "getDefaultLocale", "()Ljava/lang/String;");

  jni::TScopedLocalRef localeRef(env, env->CallStaticObjectMethod(languageClass, getDefaultLocaleId));

  std::string res = jni::ToNativeString(env, (jstring) localeRef.get());
  if (res.empty())
    res = DEFAULT_LANG;

  return res;
}

namespace platform
{
Locale GetCurrentLocale()
{
  JNIEnv * env = jni::GetEnv();
  static jmethodID const getLanguageCodeId = jni::GetStaticMethodID(env, g_utilsClazz, "getLanguageCode",
                                                                    "()Ljava/lang/String;");
  jni::ScopedLocalRef languageCode(env, env->CallStaticObjectMethod(g_utilsClazz, getLanguageCodeId));

  static jmethodID const getCountryCodeId = jni::GetStaticMethodID(env, g_utilsClazz, "getCountryCode",
                                                                   "()Ljava/lang/String;");
  jni::ScopedLocalRef countryCode(env, env->CallStaticObjectMethod(g_utilsClazz, getCountryCodeId));

  static jmethodID const getCurrencyCodeId = jni::GetStaticMethodID(env, g_utilsClazz, "getCurrencyCode",
                                                                    "()Ljava/lang/String;");
  jni::ScopedLocalRef currencyCode(env, env->CallStaticObjectMethod(g_utilsClazz, getCurrencyCodeId));

  static jmethodID const getDecimalSeparatorId = jni::GetStaticMethodID(env, g_utilsClazz, "getDecimalSeparator",
                                                                        "()Ljava/lang/String;");
  jni::ScopedLocalRef decimalSeparatorChar(env,  env->CallStaticObjectMethod(g_utilsClazz, getDecimalSeparatorId));

  static jmethodID const getGroupingSeparatorId = jni::GetStaticMethodID(env, g_utilsClazz, "getGroupingSeparator",
                                                                         "()Ljava/lang/String;");
  jni::ScopedLocalRef groupingSeparatorChar(env, env->CallStaticObjectMethod(g_utilsClazz, getGroupingSeparatorId));

  return {jni::ToNativeString(env, static_cast<jstring>(languageCode.get())),
          jni::ToNativeString(env, static_cast<jstring>(countryCode.get())),
          currencyCode.get() ? jni::ToNativeString(env, static_cast<jstring>(currencyCode.get())) : "",
          jni::ToNativeString(env, static_cast<jstring>(decimalSeparatorChar.get())),
          jni::ToNativeString(env, static_cast<jstring>(groupingSeparatorChar.get()))};
}
}  // namespace platform
