package app.organicmaps.sdk.util;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;

import androidx.annotation.IntDef;
import androidx.annotation.IntRange;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import app.organicmaps.MwmApplication;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public final class BatteryState
{
  public static final byte CHARGING_STATUS_UNKNOWN = 0;
  public static final byte CHARGING_STATUS_PLUGGED = 1;
  public static final byte CHARGING_STATUS_UNPLUGGED = 2;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ CHARGING_STATUS_UNKNOWN, CHARGING_STATUS_PLUGGED, CHARGING_STATUS_UNPLUGGED })
  public @interface ChargingStatus {}

  private BatteryState() {}

  @NonNull
  public static State getState(@NonNull Context context)
  {
    IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
    // Because it's a sticky intent, you don't need to register a BroadcastReceiver
    // by simply calling registerReceiver passing in null
    Intent batteryStatus = MwmApplication.from(context).registerReceiver(null, filter);
    if (batteryStatus == null)
      return new State(0, CHARGING_STATUS_UNKNOWN);

    return new State(getLevel(batteryStatus), getChargingStatus(batteryStatus));
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @IntRange(from=0, to=100)
  public static int getLevel(@NonNull Context context)
  {
    return getState(context).getLevel();
  }

  @IntRange(from=0, to=100)
  private static int getLevel(@NonNull Intent batteryStatus)
  {
    return batteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, 0);
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @ChargingStatus
  public static int getChargingStatus(@NonNull Context context)
  {
    return getState(context).getChargingStatus();
  }

  @ChargingStatus
  private static int getChargingStatus(@NonNull Intent batteryStatus)
  {
    // Extra for {@link android.content.Intent#ACTION_BATTERY_CHANGED}:
    // integer indicating whether the device is plugged in to a power
    // source; 0 means it is on battery, other constants are different
    // types of power sources.
    int chargePlug = batteryStatus.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
    if (chargePlug > 0)
      return CHARGING_STATUS_PLUGGED;
    else if (chargePlug < 0)
      return CHARGING_STATUS_UNKNOWN;

    return CHARGING_STATUS_UNPLUGGED;
  }

  public static final class State
  {
    @IntRange(from=0, to=100)
    private final int mLevel;

    @ChargingStatus
    private final int mChargingStatus;

    public State(@IntRange(from=0, to=100) int level, @ChargingStatus int chargingStatus)
    {
      mLevel = level;
      mChargingStatus = chargingStatus;
    }

    @IntRange(from=0, to=100)
    public int getLevel()
    {
      return mLevel;
    }

    @ChargingStatus
    public int getChargingStatus()
    {
      return mChargingStatus;
    }
  }
}
