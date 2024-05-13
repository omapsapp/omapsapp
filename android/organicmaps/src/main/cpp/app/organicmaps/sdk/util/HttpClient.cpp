/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Alexander Borsuk <me@alex.bio> from Minsk, Belarus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/
#include <jni.h>

#include "app/organicmaps/core/jni_helper.hpp"
#include "app/organicmaps/core/ScopedEnv.hpp"
#include "app/organicmaps/core/ScopedLocalRef.hpp"

#include "platform/http_client.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"
#include "base/logging.hpp"

#include <string>
#include <iterator>
#include <unordered_map>

DECLARE_EXCEPTION(JniException, RootException);

namespace
{
void RethrowOnJniException(ScopedEnv & env)
{
  if (!env->ExceptionCheck())
    return;

  env->ExceptionDescribe();
  env->ExceptionClear();
  MYTHROW(JniException, ());
}

jfieldID GetHttpParamsFieldId(ScopedEnv & env, const char * name,
                              const char * signature = "Ljava/lang/String;")
{
  return env->GetFieldID(g_httpParamsClazz, name, signature);
}

// Set string value to HttpClient.Params object, throws JniException and
void SetString(ScopedEnv & env, jobject params, jfieldID const fieldId, std::string const & value)
{
  if (value.empty())
    return;

  jni::ScopedLocalRef<jstring> const wrappedValue(env.get(), jni::ToJavaString(env.get(), value));
  RethrowOnJniException(env);

  env->SetObjectField(params, fieldId, wrappedValue.get());
  RethrowOnJniException(env);
}

void SetBoolean(ScopedEnv & env, jobject params, jfieldID const fieldId, bool const value)
{
  env->SetBooleanField(params, fieldId, value);
  RethrowOnJniException(env);
}

void SetInt(ScopedEnv & env, jobject params, jfieldID const fieldId, int const value)
{
  env->SetIntField(params, fieldId, value);
  RethrowOnJniException(env);
}

// Get string value from HttpClient.Params object, throws JniException.
void GetString(ScopedEnv & env, jobject const params, jfieldID const fieldId, std::string & result)
{
  jni::ScopedLocalRef<jstring> const wrappedValue(
      env.get(), static_cast<jstring>(env->GetObjectField(params, fieldId)));
  RethrowOnJniException(env);
  if (wrappedValue)
    result = jni::ToNativeString(env.get(), wrappedValue.get());
}

void GetInt(ScopedEnv & env, jobject const params, jfieldID const fieldId, int & result)
{
  result = env->GetIntField(params, fieldId);
  RethrowOnJniException(env);
}

void SetHeaders(ScopedEnv & env, jobject const params, platform::HttpClient::Headers const & headers)
{
  if (headers.empty())
    return;

  static jmethodID const setHeaders = env->GetMethodID(
      g_httpParamsClazz, "setHeaders", "([Lapp/organicmaps/util/KeyValue;)V");

  RethrowOnJniException(env);

  jni::TScopedLocalObjectArrayRef jHeaders(env.get(), jni::ToKeyValueArray(env.get(), headers));
  env->CallVoidMethod(params, setHeaders, jHeaders.get());
  RethrowOnJniException(env);
}

void LoadHeaders(ScopedEnv & env, jobject const params, platform::HttpClient::Headers & headers)
{
  static jmethodID const getHeaders =
      env->GetMethodID(g_httpParamsClazz, "getHeaders", "()[Ljava/lang/Object;");

  jni::ScopedLocalRef<jobjectArray> const headersArray(
      env.get(), static_cast<jobjectArray>(env->CallObjectMethod(params, getHeaders)));

  RethrowOnJniException(env);

  headers.clear();
  jni::ToNativekeyValueContainer(env.get(), headersArray, std::inserter(headers, headers.end()));

  RethrowOnJniException(env);
}

class Ids
{
public:
  explicit Ids(ScopedEnv & env)
  {
    m_fieldIds =
    {{"httpMethod", GetHttpParamsFieldId(env, "httpMethod")},
    {"inputFilePath", GetHttpParamsFieldId(env, "inputFilePath")},
    {"outputFilePath", GetHttpParamsFieldId(env, "outputFilePath")},
    {"cookies", GetHttpParamsFieldId(env, "cookies")},
    {"receivedUrl", GetHttpParamsFieldId(env, "receivedUrl")},
    {"followRedirects", GetHttpParamsFieldId(env, "followRedirects", "Z")},
    {"loadHeaders", GetHttpParamsFieldId(env, "loadHeaders", "Z")},
    {"httpResponseCode", GetHttpParamsFieldId(env, "httpResponseCode", "I")},
    {"timeoutMillisec", GetHttpParamsFieldId(env, "timeoutMillisec", "I")}};
  }

  jfieldID GetId(std::string const & fieldName) const
  {
    auto const it = m_fieldIds.find(fieldName);
    CHECK(it != m_fieldIds.end(), ("Incorrect field name:", fieldName));
    return it->second;
  }

private:
  std::unordered_map<std::string, jfieldID> m_fieldIds;
};
}  // namespace

//***********************************************************************
// Exported functions implementation
//***********************************************************************
namespace platform
{
bool HttpClient::RunHttpRequest()
{
  ScopedEnv env(jni::GetJVM());

  if (!env)
    return false;

  static Ids ids(env);

  // Create and fill request params.
  jni::ScopedLocalRef<jstring> const jniUrl(env.get(),
                                            jni::ToJavaString(env.get(), m_urlRequested));
  if (jni::HandleJavaException(env.get()))
    return false;

  static jmethodID const httpParamsConstructor =
      jni::GetConstructorID(env.get(), g_httpParamsClazz, "(Ljava/lang/String;)V");

  jni::ScopedLocalRef<jobject> const httpParamsObject(
      env.get(), env->NewObject(g_httpParamsClazz, httpParamsConstructor, jniUrl.get()));
  if (jni::HandleJavaException(env.get()))
    return false;

  // Cache it on the first call.
  static jfieldID const dataField = env->GetFieldID(g_httpParamsClazz, "data", "[B");
  if (!m_bodyData.empty())
  {
    jni::ScopedLocalRef<jbyteArray> const jniPostData(
        env.get(), env->NewByteArray(static_cast<jsize>(m_bodyData.size())));

    if (jni::HandleJavaException(env.get()))
      return false;

    env->SetByteArrayRegion(jniPostData.get(), 0, static_cast<jsize>(m_bodyData.size()),
                            reinterpret_cast<const jbyte *>(m_bodyData.data()));
    if (jni::HandleJavaException(env.get()))
      return false;

    env->SetObjectField(httpParamsObject.get(), dataField, jniPostData.get());
    if (jni::HandleJavaException(env.get()))
      return false;
  }

  ASSERT(!m_httpMethod.empty(), ("Http method type can not be empty."));

  try
  {
    SetString(env, httpParamsObject.get(), ids.GetId("httpMethod"), m_httpMethod);
    SetString(env, httpParamsObject.get(), ids.GetId("inputFilePath"), m_inputFile);
    SetString(env, httpParamsObject.get(), ids.GetId("outputFilePath"), m_outputFile);
    SetString(env, httpParamsObject.get(), ids.GetId("cookies"), m_cookies);
    SetBoolean(env, httpParamsObject.get(), ids.GetId("followRedirects"), m_followRedirects);
    SetBoolean(env, httpParamsObject.get(), ids.GetId("loadHeaders"), m_loadHeaders);
    SetInt(env, httpParamsObject.get(), ids.GetId("timeoutMillisec"),
           static_cast<int>(m_timeoutSec * 1000));

    SetHeaders(env, httpParamsObject.get(), m_headers);
  }
  catch (JniException const & ex)
  {
    return false;
  }

  static jmethodID const httpClientClassRun =
    env->GetStaticMethodID(g_httpClientClazz, "run",
        "(Lapp/organicmaps/util/HttpClient$Params;)Lapp/organicmaps/util/HttpClient$Params;");

  jni::ScopedLocalRef<jobject> const response(env.get(), env->CallStaticObjectMethod(g_httpClientClazz,
                                              httpClientClassRun, httpParamsObject.get()));
  if (jni::HandleJavaException(env.get()))
    return false;

  try
  {
    GetInt(env, response.get(), ids.GetId("httpResponseCode"), m_errorCode);
    GetString(env, response.get(), ids.GetId("receivedUrl"), m_urlReceived);
    ::LoadHeaders(env, httpParamsObject.get(), m_headers);
  }
  catch (JniException const & ex)
  {
    return false;
  }

  // dataField is already cached above.
  jni::ScopedLocalRef<jbyteArray> const jniData(
      env.get(), static_cast<jbyteArray>(env->GetObjectField(response, dataField)));
  if (jni::HandleJavaException(env.get()))
    return false;
  if (jniData)
  {
    jbyte * buffer = env->GetByteArrayElements(jniData.get(), nullptr);
    if (buffer)
    {
      m_serverResponse.assign(reinterpret_cast<const char *>(buffer), env->GetArrayLength(jniData.get()));
      env->ReleaseByteArrayElements(jniData.get(), buffer, JNI_ABORT);
    }
  }
  return true;
}
}  // namespace platform
