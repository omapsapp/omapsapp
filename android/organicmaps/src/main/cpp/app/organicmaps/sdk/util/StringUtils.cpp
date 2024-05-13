#include "app/organicmaps/core/jni_helper.hpp"
#include "app/organicmaps/core/jni_java_methods.hpp"
#include "app/organicmaps/util/Distance.hpp"

#include "indexer/search_string_utils.hpp"

#include "platform/localization.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/settings.hpp"

#include <string>

namespace
{
jobject MakeJavaPair(JNIEnv * env, std::string const & first, std::string const & second)
{
  return jni::PairBuilder::Instance(env).Create(env, jni::ToJavaString(env, first), jni::ToJavaString(env, second));
}
}  // namespace

extern "C"
{
JNIEXPORT jboolean JNICALL
Java_app_organicmaps_util_StringUtils_nativeIsHtml(JNIEnv * env, jclass thiz, jstring text)
{
  return strings::IsHTML(jni::ToNativeString(env, text));
}

JNIEXPORT jboolean JNICALL
Java_app_organicmaps_util_StringUtils_nativeContainsNormalized(JNIEnv * env, jclass thiz, jstring str, jstring substr)
{
  return search::ContainsNormalized(jni::ToNativeString(env, str), jni::ToNativeString(env, substr));
}

JNIEXPORT jobjectArray JNICALL
Java_app_organicmaps_util_StringUtils_nativeFilterContainsNormalized(JNIEnv * env, jclass thiz, jobjectArray src, jstring jSubstr)
{
  std::string const substr = jni::ToNativeString(env, jSubstr);
  int const length = env->GetArrayLength(src);
  std::vector<std::string> filtered;
  filtered.reserve(length);
  for (int i = 0; i < length; i++)
  {
    std::string const str = jni::ToNativeString(env, static_cast<jstring>(env->GetObjectArrayElement(src, i)));
    if (search::ContainsNormalized(str, substr))
      filtered.push_back(str);
  }

  return jni::ToJavaStringArray(env, filtered);
}

JNIEXPORT jobject JNICALL Java_app_organicmaps_util_StringUtils_nativeFormatSpeedAndUnits(
    JNIEnv * env, jclass thiz, jdouble metersPerSecond)
{
  auto const units = measurement_utils::GetMeasurementUnits();
  return MakeJavaPair(env, measurement_utils::FormatSpeedNumeric(metersPerSecond, units),
                      platform::GetLocalizedSpeedUnits(units));
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_util_StringUtils_nativeFormatDistance(JNIEnv * env, jclass, jdouble distanceInMeters)
{
  return ToJavaDistance(env, platform::Distance::CreateFormatted(distanceInMeters));
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_util_StringUtils_nativeGetLocalizedDistanceUnits(JNIEnv * env, jclass)
{
  auto const localizedUnits = platform::GetLocalizedDistanceUnits();
  return MakeJavaPair(env, localizedUnits.m_high, localizedUnits.m_low);
}

JNIEXPORT jobject JNICALL
Java_app_organicmaps_util_StringUtils_nativeGetLocalizedAltitudeUnits(JNIEnv * env, jclass)
{
  auto const localizedUnits = platform::GetLocalizedAltitudeUnits();
  return MakeJavaPair(env, localizedUnits.m_high, localizedUnits.m_low);
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_util_StringUtils_nativeGetLocalizedSpeedUnits(JNIEnv * env, jclass)
{
  return jni::ToJavaString(env, platform::GetLocalizedSpeedUnits());
}
} // extern "C"
