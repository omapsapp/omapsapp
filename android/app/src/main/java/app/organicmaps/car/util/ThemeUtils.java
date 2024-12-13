package app.organicmaps.car.util;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.annotation.UiThread;
import androidx.car.app.CarContext;

import app.organicmaps.Framework;
import app.organicmaps.R;
import app.organicmaps.routing.RoutingController;

public final class ThemeUtils
{
  public enum ThemeMode
  {
    LIGHT(R.string.light, R.string.theme_default),
    NIGHT(R.string.dark, R.string.theme_night),
    FOLLOW_SYSTEM(R.string.follow_system, R.string.theme_follow_system);

    ThemeMode(@StringRes int titleId, @StringRes int prefsKeyId)
    {
      mTitleId = titleId;
      mPrefsKeyId = prefsKeyId;
    }

    @StringRes
    public int getTitleId()
    {
      return mTitleId;
    }

    @StringRes
    public int getPrefsKeyId()
    {
      return mPrefsKeyId;
    }

    @StringRes
    private final int mTitleId;
    @StringRes
    private final int mPrefsKeyId;
  }

  private static final String ANDROID_AUTO_PREFERENCES_FILE_KEY = "ANDROID_AUTO_PREFERENCES_FILE_KEY";
  private static final String THEME_KEY = "ANDROID_AUTO_THEME_MODE";

  @UiThread
  public static void update(@NonNull CarContext context)
  {
    final ThemeMode oldThemeMode = getThemeMode(context);
    update(context, oldThemeMode);
  }

  @UiThread
  public static void update(@NonNull CarContext context, @NonNull ThemeMode oldThemeMode)
  {
    final ThemeMode newThemeMode = oldThemeMode == ThemeMode.FOLLOW_SYSTEM ? (context.isDarkMode() ? ThemeMode.NIGHT : ThemeMode.LIGHT) : oldThemeMode;

    @Framework.MapStyle
    int newMapStyle;
    if (newThemeMode == ThemeMode.NIGHT)
      newMapStyle = RoutingController.get().isVehicleNavigation() ? Framework.MAP_STYLE_VEHICLE_DARK : Framework.MAP_STYLE_DARK;
    else
      newMapStyle = RoutingController.get().isVehicleNavigation() ? Framework.MAP_STYLE_VEHICLE_CLEAR : Framework.MAP_STYLE_CLEAR;

    if (Framework.nativeGetMapStyle() != newMapStyle)
      Framework.nativeSetMapStyle(newMapStyle);
  }

  public static boolean isNightMode(@NonNull CarContext context)
  {
    final ThemeMode themeMode = getThemeMode(context);
    return themeMode == ThemeMode.NIGHT || (themeMode == ThemeMode.FOLLOW_SYSTEM && context.isDarkMode());
  }

  @SuppressLint("ApplySharedPref")
  @UiThread
  public static void setThemeMode(@NonNull CarContext context, @NonNull ThemeMode themeMode)
  {
    getSharedPreferences(context).edit().putString(THEME_KEY, context.getString(themeMode.getPrefsKeyId())).commit();
    update(context, themeMode);
  }

  @NonNull
  public static ThemeMode getThemeMode(@NonNull CarContext context)
  {
    final String followSystemTheme = context.getString(R.string.theme_follow_system);
    final String lightTheme = context.getString(R.string.theme_default);
    final String nightTheme = context.getString(R.string.theme_night);
    final String themeMode = getSharedPreferences(context).getString(THEME_KEY, followSystemTheme);

    if (themeMode.equals(followSystemTheme))
      return ThemeMode.FOLLOW_SYSTEM;
    else if (themeMode.equals(lightTheme))
      return ThemeMode.LIGHT;
    else if (themeMode.equals(nightTheme))
      return ThemeMode.NIGHT;
    else
      throw new IllegalArgumentException("Unsupported value");
  }

  @NonNull
  private static SharedPreferences getSharedPreferences(@NonNull CarContext context)
  {
    return context.getSharedPreferences(ANDROID_AUTO_PREFERENCES_FILE_KEY, Context.MODE_PRIVATE);
  }

  private ThemeUtils() {}
}
