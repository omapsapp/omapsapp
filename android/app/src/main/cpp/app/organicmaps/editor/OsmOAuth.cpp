#include <jni.h>

#include "app/organicmaps/core/jni_helper.hpp"
#include "app/organicmaps/Framework.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include "editor/osm_auth.hpp"
#include "editor/server_api.hpp"

namespace
{
using namespace osm;
using namespace jni;

bool LoadOsmUserPreferences(std::string const & oauthToken, UserPreferences & outPrefs)
{
  try
  {
    ServerApi06 const api(OsmOAuth::ServerAuth(oauthToken));
    outPrefs = api.GetUserPreferences();
    return true;
  }
  catch (std::exception const & ex)
  {
    LOG(LWARNING, ("Can't load user preferences from server: ", ex.what()));
  }
  return false;
}
}  // namespace

extern "C"
{

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_OsmOAuth_nativeGetOAuth2Url(JNIEnv * env, jclass)
{
  auto const auth = OsmOAuth::ServerAuth();
  return ToJavaString(env, auth.BuildOAuth2Url());
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_OsmOAuth_nativeAuthWithPassword(JNIEnv * env, jclass clazz,
                                                                jstring login, jstring password)
{
  OsmOAuth auth = OsmOAuth::ServerAuth();
  try
  {
    if (auth.AuthorizePassword(ToNativeString(env, login), ToNativeString(env, password)))
      return ToJavaString(env, auth.GetAuthToken());
    LOG(LWARNING, ("nativeAuthWithPassword: invalid login or password."));
  }
  catch (std::exception const & ex)
  {
    LOG(LWARNING, ("nativeAuthWithPassword error ", ex.what()));
  }
  return nullptr;
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_OsmOAuth_nativeAuthWithOAuth2Code(JNIEnv * env, jclass, jstring oauth2code)
{
  OsmOAuth auth = OsmOAuth::ServerAuth();
  try
  {
    auto token = auth.FinishAuthorization(ToNativeString(env, oauth2code));
    if (!token.empty())
    {
        auth.SetAuthToken(token);
        return ToJavaString(env, token);
    }
    LOG(LWARNING, ("nativeAuthWithOAuth2Code: invalid OAuth2 code", oauth2code));
  }
  catch (std::exception const & ex)
  {
    LOG(LWARNING, ("nativeAuthWithOAuth2Code error ", ex.what()));
  }
  return nullptr;
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_OsmOAuth_nativeGetOsmUsername(JNIEnv * env, jclass, jstring oauthToken)
{
  UserPreferences prefs;
  if (LoadOsmUserPreferences(jni::ToNativeString(env, oauthToken), prefs))
    return jni::ToJavaString(env, prefs.m_displayName);
  return nullptr;
}

JNIEXPORT jint JNICALL
Java_app_organicmaps_editor_OsmOAuth_nativeGetOsmChangesetsCount(JNIEnv * env, jclass, jstring oauthToken)
{
  UserPreferences prefs;
  if (LoadOsmUserPreferences(jni::ToNativeString(env, oauthToken), prefs))
    return prefs.m_changesets;
  return -1;
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_OsmOAuth_nativeGetOsmProfilePictureUrl(JNIEnv * env, jclass, jstring oauthToken)
{
  UserPreferences prefs;
  if (LoadOsmUserPreferences(jni::ToNativeString(env, oauthToken), prefs))
    return jni::ToJavaString(env, prefs.m_imageUrl);
  return nullptr;
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_OsmOAuth_nativeGetHistoryUrl(JNIEnv * env, jclass, jstring user)
{
  return jni::ToJavaString(env, OsmOAuth::ServerAuth().GetHistoryURL(jni::ToNativeString(env, user)));
}

JNIEXPORT jstring JNICALL
Java_app_organicmaps_editor_OsmOAuth_nativeGetNotesUrl(JNIEnv * env, jclass, jstring user)
{
  return jni::ToJavaString(env, OsmOAuth::ServerAuth().GetNotesURL(jni::ToNativeString(env, user)));
}
} // extern "C"
