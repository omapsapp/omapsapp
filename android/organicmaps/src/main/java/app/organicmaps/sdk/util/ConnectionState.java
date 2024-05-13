package app.organicmaps.sdk.util;

import static app.organicmaps.sdk.util.ConnectionState.Type.NONE;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public enum ConnectionState
{
  INSTANCE;

  // values should correspond to ones from enum class EConnectionType (in platform/platform.hpp)
  private static final byte CONNECTION_NONE = 0;
  private static final byte CONNECTION_WIFI = 1;
  private static final byte CONNECTION_WWAN = 2;
  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private Context mContext;

  public enum Type
  {
    NONE(CONNECTION_NONE, -1),
    WIFI(CONNECTION_WIFI, ConnectivityManager.TYPE_WIFI),
    WWAN(CONNECTION_WWAN, ConnectivityManager.TYPE_MOBILE);

    private final byte mNativeRepresentation;
    private final int mPlatformRepresentation;

    Type(byte nativeRepresentation, int platformRepresentation)
    {
      mNativeRepresentation = nativeRepresentation;
      mPlatformRepresentation = platformRepresentation;
    }

    public byte getNativeRepresentation()
    {
      return mNativeRepresentation;
    }

    public int getPlatformRepresentation()
    {
      return mPlatformRepresentation;
    }
  }

  public void initialize(@NonNull Context context)
  {
    mContext = context;
  }

  private boolean isNetworkConnected(int networkType)
  {
    final NetworkInfo info = getActiveNetwork();
    return info != null && info.getType() == networkType && info.isConnected();
  }

  @Nullable
  public NetworkInfo getActiveNetwork()
  {
    ConnectivityManager manager =
        ((ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE));
    if (manager == null)
      return null;

    return manager.getActiveNetworkInfo();
  }

  public boolean isMobileConnected()
  {
    return isNetworkConnected(ConnectivityManager.TYPE_MOBILE);
  }

  public boolean isWifiConnected()
  {
    return isNetworkConnected(ConnectivityManager.TYPE_WIFI);
  }

  public boolean isConnected()
  {
    return isNetworkConnected(ConnectivityManager.TYPE_WIFI) || isNetworkConnected(ConnectivityManager.TYPE_MOBILE);
  }

  public boolean isInRoaming()
  {
    NetworkInfo info = getActiveNetwork();
    return info != null && info.isRoaming();
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  public static byte getConnectionState()
  {
    return INSTANCE.requestCurrentType().getNativeRepresentation();
  }

  @NonNull
  public Type requestCurrentType()
  {
    for (ConnectionState.Type each : ConnectionState.Type.values())
    {
      if (isNetworkConnected(each.getPlatformRepresentation()))
        return each;
    }
    return NONE;
  }
}
