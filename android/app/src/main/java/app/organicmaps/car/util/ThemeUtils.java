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
import app.organicmaps.sdk.MapStyle;

public final class ThemeUtils
{
  public enum ThemeMode
  {
    AUTO(R.string.auto, R.string.theme_auto),
    LIGHT(R.string.off, R.string.theme_default),
    NIGHT(R.string.on, R.string.theme_night);

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
    final ThemeMode newThemeMode = oldThemeMode == ThemeMode.AUTO ? (context.isDarkMode() ? ThemeMode.NIGHT : ThemeMode.LIGHT) : oldThemeMode;

    MapStyle newMapStyle;
    if (newThemeMode == ThemeMode.NIGHT)
      newMapStyle = RoutingController.get().isVehicleNavigation() ? MapStyle.VehicleDark : MapStyle.Dark;
    else
      newMapStyle = RoutingController.get().isVehicleNavigation() ? MapStyle.VehicleClear : MapStyle.Clear;

    if (MapStyle.get() != newMapStyle)
      MapStyle.set(newMapStyle);
  }

  public static boolean isNightMode(@NonNull CarContext context)
  {
    final ThemeMode themeMode = getThemeMode(context);
    return themeMode == ThemeMode.NIGHT || (themeMode == ThemeMode.AUTO && context.isDarkMode());
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
    final String autoTheme = context.getString(R.string.theme_auto);
    final String lightTheme = context.getString(R.string.theme_default);
    final String nightTheme = context.getString(R.string.theme_night);
    final String themeMode = getSharedPreferences(context).getString(THEME_KEY, autoTheme);

    if (themeMode.equals(autoTheme))
      return ThemeMode.AUTO;
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
