package app.organicmaps.editor;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Size;
import androidx.annotation.WorkerThread;
import androidx.fragment.app.FragmentManager;

import java.util.Map;

import app.organicmaps.MwmApplication;
import app.organicmaps.util.NetworkPolicy;

public final class OsmOAuth
{
  private OsmOAuth() {}

  public enum AuthType
  {
    OSM("OSM"),
    GOOGLE("Google");

    public final String name;

    AuthType(String name)
    {
      this.name = name;
    }
  }

  public static final int OK = 0;

  private static final String PREF_OSM_TOKEN = "OsmToken";   // Unused after migration from OAuth1 to OAuth2
  private static final String PREF_OSM_SECRET = "OsmSecret"; // Unused after migration from OAuth1 to OAuth2
  private static final String PREF_OSM_USERNAME = "OsmUsername";
  private static final String PREF_OSM_CHANGESETS_COUNT = "OsmChangesetsCount";
  private static final String PREF_OSM_OAUTH2_TOKEN = "OsmOAuth2Token";

  public static final String URL_PARAM_VERIFIER = "oauth_verifier";

  public static boolean isAuthorized(@NonNull Context context)
  {
    return MwmApplication.prefs(context).contains(PREF_OSM_OAUTH2_TOKEN);
  }

  public static boolean containsOAuth1Credentials(@NonNull Context context)
  {
    SharedPreferences prefs = MwmApplication.prefs(context);
    return prefs.contains(PREF_OSM_TOKEN) && prefs.contains(PREF_OSM_SECRET);
  }

  public static void clearOAuth1Credentials(@NonNull Context context)
  {
    MwmApplication.prefs(context).edit()
            .remove(PREF_OSM_TOKEN)
            .remove(PREF_OSM_SECRET)
            .apply();
  }

  public static String getAuthToken(@NonNull Context context)
  {
    return MwmApplication.prefs(context).getString(PREF_OSM_OAUTH2_TOKEN, "");
  }

  public static String getUsername(@NonNull Context context)
  {
    return MwmApplication.prefs(context).getString(PREF_OSM_USERNAME, "");
  }

  public static Bitmap getProfilePicture(@NonNull Context context)
  {
    //TODO(HB): load and store image in cache here
    return null;
  }

  public static void setAuthorization(@NonNull Context context, String oauthToken, String username)
  {
    MwmApplication.prefs(context).edit()
                  .putString(PREF_OSM_OAUTH2_TOKEN, oauthToken)
                  .putString(PREF_OSM_USERNAME, username)
                  .apply();
  }

  public static void clearAuthorization(@NonNull Context context)
  {
    MwmApplication.prefs(context).edit()
                  .remove(PREF_OSM_TOKEN)
                  .remove(PREF_OSM_SECRET)
                  .remove(PREF_OSM_USERNAME)
                  .remove(PREF_OSM_OAUTH2_TOKEN)
                  .apply();
  }

  public static String getHistoryUrl(@NonNull Context context)
  {
    return nativeGetHistoryUrl(getUsername(context));
  }

  public static String getNotesUrl(@NonNull Context context)
  {
    return nativeGetNotesUrl(getUsername(context));
  }

  /*
   Returns 5 strings: ServerURL, ClientId, ClientSecret, Scope, RedirectUri
   */
  @NonNull
  public static native String nativeGetOAuth2Url();

  /**
   * @return string with OAuth2 token
   */
  @WorkerThread
  @Size(2)
  @Nullable
  public static native String nativeAuthWithPassword(String login, String password);

  /**
   * @return string with OAuth2 token
   */
  @WorkerThread
  @Nullable
  public static native String nativeAuthWithOAuth2Code(String oauth2code);

  @WorkerThread
  @Nullable
  public static native String nativeGetOsmUsername(String oauthToken);

  @WorkerThread
  @Nullable
  public static native String nativeGetOsmProfilePictureUrl(String oauthToken);

  @WorkerThread
  @NonNull
  public static native String nativeGetHistoryUrl(String user);

  @WorkerThread
  @NonNull
  public static native String nativeGetNotesUrl(String user);

  /**
   * @return < 0 if failed to get changesets count.
   */
  @WorkerThread
  private static native int nativeGetOsmChangesetsCount(String oauthToken);

  @WorkerThread
  public static int getOsmChangesetsCount(@NonNull Context context, @NonNull FragmentManager fm) {
    final int[] editsCount = {-1};
    NetworkPolicy.checkNetworkPolicy(fm, policy -> {
      if (!policy.canUseNetwork())
        return;

      final String token = getAuthToken(context);
      editsCount[0] = OsmOAuth.nativeGetOsmChangesetsCount(token);
    });
    final SharedPreferences prefs = MwmApplication.prefs(context);
    if (editsCount[0] < 0)
      return prefs.getInt(PREF_OSM_CHANGESETS_COUNT, 0);

    prefs.edit().putInt(PREF_OSM_CHANGESETS_COUNT, editsCount[0]).apply();
    return editsCount[0];
  }
}
