package app.organicmaps.sdk.util;

import androidx.annotation.IntDef;

import app.organicmaps.Framework;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public final class PowerManagment
{
  // It should consider to power_managment::Scheme from
  // map/power_management/power_management_schemas.hpp
  public static final int NONE = 0;
  public static final int NORMAL = 1;
  public static final int MEDIUM = 2;
  public static final int HIGH = 3;
  public static final int AUTO = 4;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ NONE, NORMAL, MEDIUM, HIGH, AUTO })
  public @interface SchemeType
  {
  }

  @SchemeType
  public static int getScheme()
  {
    return Framework.nativeGetPowerManagerScheme();
  }

  public static void setScheme(@SchemeType int value)
  {
    Framework.nativeSetPowerManagerScheme(value);
  }
}
