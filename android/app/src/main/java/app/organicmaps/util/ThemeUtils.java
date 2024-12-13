package app.organicmaps.util;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.util.TypedValue;

import androidx.annotation.AttrRes;
import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.StyleRes;
import androidx.appcompat.app.AppCompatDelegate;
import app.organicmaps.R;

public final class ThemeUtils
{
  private static final TypedValue VALUE_BUFFER = new TypedValue();

  private ThemeUtils() {}

  public static @ColorInt int getColor(@NonNull Context context, @AttrRes int attr)
  {
    if (!context.getTheme().resolveAttribute(attr, VALUE_BUFFER, true))
      throw new IllegalArgumentException("Failed to resolve color theme attribute");

    return VALUE_BUFFER.data;
  }

  public static int getResource(@NonNull Context context, @AttrRes int attr)
  {
    if (!context.getTheme().resolveAttribute(attr, VALUE_BUFFER, true))
      throw new IllegalArgumentException("Failed to resolve theme attribute");

    return VALUE_BUFFER.resourceId;
  }

  public static int getResource(@NonNull Context context, @AttrRes int style, @AttrRes int attr)
  {
    int styleRef = getResource(context, style);

    int[] attrs = new int[] { attr };
    TypedArray ta = context.getTheme().obtainStyledAttributes(styleRef, attrs);
    ta.getValue(0, VALUE_BUFFER);
    ta.recycle();

    return VALUE_BUFFER.resourceId;
  }

  public static String getAndroidTheme(@NonNull Context context)
  {
    String nightTheme = context.getString(R.string.theme_night);
    String defaultTheme = context.getString(R.string.theme_default);

    if (AppCompatDelegate.getDefaultNightMode() == AppCompatDelegate.MODE_NIGHT_YES)
      return nightTheme;

    if (AppCompatDelegate.getDefaultNightMode() == AppCompatDelegate.MODE_NIGHT_NO)
      return defaultTheme;

    int nightModeFlags = context.getResources()
                                .getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
    if (nightModeFlags == Configuration.UI_MODE_NIGHT_YES)
      return nightTheme;
    else
      return defaultTheme;
  }

  public static boolean isDefaultTheme(@NonNull Context context)
  {
    return isDefaultTheme(context, Config.getCurrentUiTheme(context));
  }

  public static boolean isDefaultTheme(@NonNull Context context, String theme)
  {
    String defaultTheme = context.getString(R.string.theme_default);
    return defaultTheme.equals(theme);
  }

  public static boolean isNightTheme(@NonNull Context context)
  {
    return isNightTheme(context, Config.getCurrentUiTheme(context));
  }

  public static boolean isNightTheme(@NonNull Context context, String theme)
  {
    String nightTheme = context.getString(R.string.theme_night);
    return nightTheme.equals(theme);
  }

  public static boolean isFollowSystemTheme(@NonNull Context context)
  {
    return isFollowSystemTheme(context, Config.getCurrentUiTheme(context));
  }

  public static boolean isFollowSystemTheme(@NonNull Context context, String theme)
  {
    String followSystemTheme = context.getString(R.string.theme_follow_system);
    return followSystemTheme.equals(theme);
  }

  public static boolean isNavAutoTheme(@NonNull Context context)
  {
    return isNavAutoTheme(context, Config.getUiThemeSettings(context));
  }

  public static boolean isNavAutoTheme(@NonNull Context context, String theme)
  {
    String navAutoTheme = context.getString(R.string.theme_nav_auto);
    return navAutoTheme.equals(theme);
}

  public static boolean isValidTheme(@NonNull Context context, String theme)
  {
    String defaultTheme = context.getString(R.string.theme_default);
    String nightTheme = context.getString(R.string.theme_night);
    return (defaultTheme.equals(theme) || nightTheme.equals(theme));
  }

  @StyleRes
  public static int getCardBgThemeResourceId(@NonNull Context context, @NonNull String theme)
  {
    if (isDefaultTheme(context, theme))
      return R.style.MwmTheme_CardBg;

    if (isNightTheme(context, theme))
      return R.style.MwmTheme_Night_CardBg;

    throw new IllegalArgumentException("Attempt to apply unsupported theme: " + theme);
  }

  @StyleRes
  public static int getWindowBgThemeResourceId(@NonNull Context context, @NonNull String theme)
  {
    if (isDefaultTheme(context, theme))
      return R.style.MwmTheme_WindowBg;

    if (isNightTheme(context, theme))
      return R.style.MwmTheme_Night_WindowBg;

    throw new IllegalArgumentException("Attempt to apply unsupported theme: " + theme);
  }
}
