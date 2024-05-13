package app.organicmaps.sdk.util;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import app.organicmaps.util.log.Logger;

public final class SecureStorage
{
  private static final String TAG = SecureStorage.class.getSimpleName();

  private SecureStorage() {}

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  public static void save(@NonNull Context context, @NonNull String key, @NonNull String value)
  {
    Logger.d(TAG, "save: key = " + key);
    SharedPreferences prefs = context.getSharedPreferences("secure", Context.MODE_PRIVATE);
    prefs.edit().putString(key, value).apply();
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @Nullable
  public static String load(@NonNull Context context, @NonNull String key)
  {
    Logger.d(TAG, "load: key = " + key);
    SharedPreferences prefs = context.getSharedPreferences("secure", Context.MODE_PRIVATE);
    return prefs.getString(key, null);
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  public static void remove(@NonNull Context context, @NonNull String key)
  {
    Logger.d(TAG, "remove: key = " + key);
    SharedPreferences prefs = context.getSharedPreferences("secure", Context.MODE_PRIVATE);
    prefs.edit().remove(key).apply();
  }
}
