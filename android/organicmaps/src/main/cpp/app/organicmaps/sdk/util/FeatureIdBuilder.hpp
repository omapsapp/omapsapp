#pragma once

#include "app/organicmaps/core/jni_helper.hpp"

#include "indexer/feature_decl.hpp"

class FeatureIdBuilder
{
public:
  explicit FeatureIdBuilder(JNIEnv * env)
  {
    jclass clazz = env->FindClass("app/organicmaps/bookmarks/data/FeatureId");
    m_countryName = env->GetFieldID(clazz, "mMwmName", "Ljava/lang/String;");
    ASSERT(m_countryName, ());
    m_index = env->GetFieldID(clazz, "mFeatureIndex", "I");
    ASSERT(m_index, ());
  }

  FeatureID Build(JNIEnv * env, jobject obj) const
  {
    jstring jcountryName = static_cast<jstring>(env->GetObjectField(obj, m_countryName));
    jint jindex = env->GetIntField(obj, m_index);

    auto const & ds = g_framework->GetDataSource();
    auto const id = ds.GetMwmIdByCountryFile(platform::CountryFile(jni::ToNativeString(env, jcountryName)));
    return FeatureID(id, static_cast<uint32_t>(jindex));
  }

private:
  jfieldID m_countryName;
  jfieldID m_index;
};
