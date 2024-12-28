package app.organicmaps.util;

import static app.organicmaps.util.Config.KEY_PREF_STATISTICS;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceManager;

import app.organicmaps.MwmApplication;
import app.organicmaps.R;
import app.organicmaps.maplayer.Mode;

import java.io.IOException;
import java.util.Locale;

public final class SharedPropertiesUtils
{
  private static final String PREFS_SHOW_EMULATE_BAD_STORAGE_SETTING = "ShowEmulateBadStorageSetting";
  private static final String PREFS_SHOULD_SHOW_LAYER_MARKER_FOR = "ShouldShowGuidesLayerMarkerFor";

  //Utils class
  private SharedPropertiesUtils()
  {
    throw new IllegalStateException("Try instantiate utility class SharedPropertiesUtils");
  }

  public static boolean isStatisticsEnabled(@NonNull Context context)
  {
    return MwmApplication.prefs(context).getBoolean(KEY_PREF_STATISTICS, true);
  }

  public static void setShouldShowEmulateBadStorageSetting(@NonNull Context context, boolean show)
  {
    SharedPreferences prefs = PreferenceManager
        .getDefaultSharedPreferences(MwmApplication.from(context));
    prefs.edit().putBoolean(PREFS_SHOW_EMULATE_BAD_STORAGE_SETTING, show).apply();
  }

  public static boolean shouldShowEmulateBadStorageSetting(@NonNull Context context)
  {
    SharedPreferences prefs = PreferenceManager
        .getDefaultSharedPreferences(MwmApplication.from(context));
    return prefs.getBoolean(PREFS_SHOW_EMULATE_BAD_STORAGE_SETTING, false);
  }

  /**
   * @param context context
   * @throws IOException if "Emulate bad storage" is enabled in preferences
   */
  public static void emulateBadExternalStorage(@NonNull Context context) throws IOException
  {
    SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(MwmApplication.from(context));
    String key = MwmApplication.from(context).getString(R.string.pref_emulate_bad_external_storage);
    if (prefs.getBoolean(key, false))
    {
      // Emulate one time only -> reset setting to run normally next time.
      prefs.edit().putBoolean(key, false).apply();
      throw new IOException("Bad external storage error injection");
    }
  }

  public static boolean shouldShowNewMarkerForLayerMode(@NonNull Context context,
                                                        @NonNull Mode mode)
  {
    return switch (mode)
    {
      case SUBWAY, TRAFFIC, ISOLINES -> false;
      default -> getBoolean(context, PREFS_SHOULD_SHOW_LAYER_MARKER_FOR + mode.name().toLowerCase(Locale.ENGLISH), true);
    };
  }

  public static void setLayerMarkerShownForLayerMode(@NonNull Context context, @NonNull Mode mode)
  {
    putBoolean(context, PREFS_SHOULD_SHOW_LAYER_MARKER_FOR + mode.name()
                                                                 .toLowerCase(Locale.ENGLISH), false);
  }

  private static boolean getBoolean(@NonNull Context context,  @NonNull String key, boolean defValue)
  {
    return MwmApplication.prefs(context).getBoolean(key, defValue);
  }

  private static void putBoolean(@NonNull Context context,  @NonNull String key, boolean value)
  {
    MwmApplication.prefs(context)
                  .edit()
                  .putBoolean(key, value)
                  .apply();
  }
}
