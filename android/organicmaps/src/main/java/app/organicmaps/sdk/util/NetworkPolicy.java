package app.organicmaps.sdk.util;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.fragment.app.FragmentManager;

import app.organicmaps.widget.StackedButtonDialogFragment;

import java.util.concurrent.TimeUnit;

@Keep
public final class NetworkPolicy
{
  public enum Type
  {
    ASK,

    ALWAYS()
        {
          @Override
          public void check(@NonNull FragmentManager fragmentManager,
                            @NonNull NetworkPolicyListener listener, boolean isDialogAllowed)
          {
            boolean nowInRoaming = ConnectionState.INSTANCE.isInRoaming();
            boolean acceptedInRoaming = Config.getMobileDataRoaming();

            if (nowInRoaming && !acceptedInRoaming)
              showDialog(fragmentManager, listener);
            else
              listener.onResult(new NetworkPolicy(true));
          }
        },

    NEVER()
        {
          @Override
          public void check(@NonNull FragmentManager fragmentManager,
                            @NonNull NetworkPolicyListener listener, boolean isDialogAllowed)
          {
            if (isDialogAllowed)
              showDialog(fragmentManager, listener);
            else
              listener.onResult(new NetworkPolicy(false));
          }
        },


    NOT_TODAY()
        {
          @Override
          public void check(@NonNull FragmentManager fragmentManager,
                            @NonNull NetworkPolicyListener listener, boolean isDialogAllowed)
          {
            if (isDialogAllowed)
              showDialog(fragmentManager, listener);
            else
              showDialogIfNeeded(fragmentManager, listener, new NetworkPolicy(false));
          }
        },

    TODAY()
        {
          @Override
          public void check(@NonNull FragmentManager fragmentManager,
                            @NonNull NetworkPolicyListener listener, boolean isDialogAllowed)
          {
            boolean nowInRoaming = ConnectionState.INSTANCE.isInRoaming();
            boolean acceptedInRoaming = Config.getMobileDataRoaming();

            if (nowInRoaming && !acceptedInRoaming)
              showDialog(fragmentManager, listener);
            else
              showDialogIfNeeded(fragmentManager, listener, new NetworkPolicy(true));
          }
        };

    public void check(@NonNull FragmentManager fragmentManager,
                      @NonNull final NetworkPolicyListener listener,
                      boolean isDialogAllowed)
    {
      showDialog(fragmentManager, listener);
    }
  }


  public static final int NONE = -1;

  private static final String TAG_NETWORK_POLICY = "network_policy";

  public static void checkNetworkPolicy(@NonNull FragmentManager fragmentManager,
                                        @NonNull final NetworkPolicyListener listener,
                                        boolean isDialogAllowed)
  {
    if (ConnectionState.INSTANCE.isWifiConnected())
    {
      listener.onResult(new NetworkPolicy(true));
      return;
    }

    if (!ConnectionState.INSTANCE.isMobileConnected())
    {
      listener.onResult(new NetworkPolicy(false));
      return;
    }

    Type type = Config.getUseMobileDataSettings();
    type.check(fragmentManager, listener, isDialogAllowed);
  }

  public static void checkNetworkPolicy(@NonNull FragmentManager fragmentManager,
                                        @NonNull final NetworkPolicyListener listener)
  {
    checkNetworkPolicy(fragmentManager, listener, false);
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  public static boolean getCurrentNetworkUsageStatus()
  {
    if (ConnectionState.INSTANCE.isWifiConnected())
      return true;

    if (!ConnectionState.INSTANCE.isMobileConnected())
      return false;

    boolean nowInRoaming = ConnectionState.INSTANCE.isInRoaming();
    boolean acceptedInRoaming = Config.getMobileDataRoaming();
    if (nowInRoaming && !acceptedInRoaming)
      return false;

    Type type = Config.getUseMobileDataSettings();
    return type == Type.ALWAYS || (type == Type.TODAY && isToday());
  }

  private static boolean isToday()
  {
    long timestamp = Config.getMobileDataTimeStamp();
    return TimeUnit.MILLISECONDS.toDays(System.currentTimeMillis() - timestamp) < 1;
  }

  private static void showDialogIfNeeded(@NonNull FragmentManager fragmentManager,
                                         @NonNull NetworkPolicyListener listener,
                                         @NonNull NetworkPolicy policy)
  {
    if (isToday())
    {
      listener.onResult(policy);
      return;
    }
    showDialog(fragmentManager, listener);
  }

  private static void showDialog(@NonNull FragmentManager fragmentManager,
                                 @NonNull NetworkPolicyListener listener)
  {
    StackedButtonDialogFragment dialog = (StackedButtonDialogFragment) fragmentManager
        .findFragmentByTag(TAG_NETWORK_POLICY);
    if (dialog != null)
      dialog.dismiss();

    dialog = new StackedButtonDialogFragment();
    dialog.setListener(listener);
    dialog.show(fragmentManager, TAG_NETWORK_POLICY);
  }

  public static NetworkPolicy newInstance(boolean canUse)
  {
    return new NetworkPolicy(canUse);
  }

  private final boolean mCanUseNetwork;

  private NetworkPolicy(boolean canUse)
  {
    mCanUseNetwork = canUse;
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  public boolean canUseNetwork()
  {
    return mCanUseNetwork;
  }

  public interface NetworkPolicyListener
  {
    void onResult(@NonNull NetworkPolicy policy);
  }
}
