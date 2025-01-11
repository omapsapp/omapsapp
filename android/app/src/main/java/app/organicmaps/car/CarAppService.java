package app.organicmaps.car;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.car.app.CarContext;
import androidx.car.app.Session;
import androidx.car.app.SessionInfo;
import androidx.car.app.notification.CarAppExtender;
import androidx.car.app.notification.CarPendingIntent;
import androidx.car.app.validation.HostValidator;
import androidx.core.app.NotificationChannelCompat;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;

import app.organicmaps.BuildConfig;
import app.organicmaps.R;
import app.organicmaps.api.Const;

public final class CarAppService extends androidx.car.app.CarAppService
{
  private static final int NOTIFICATION_ID = CarAppService.class.getSimpleName().hashCode();
  public static final String ANDROID_AUTO_NOTIFICATION_CHANNEL_ID = "ANDROID_AUTO";

  public static final String API_CAR_HOST = Const.AUTHORITY + ".car";
  public static final String ACTION_SHOW_NAVIGATION_SCREEN = Const.ACTION_PREFIX + ".SHOW_NAVIGATION_SCREEN";

  @Nullable
  private static NotificationCompat.Extender mCarNotificationExtender;

  @NonNull
  @Override
  public HostValidator createHostValidator()
  {
    if (BuildConfig.DEBUG)
      return HostValidator.ALLOW_ALL_HOSTS_VALIDATOR;

    return new HostValidator.Builder(getApplicationContext())
        .addAllowedHosts(androidx.car.app.R.array.hosts_allowlist_sample)
        .build();
  }

  @NonNull
  @Override
  public Session onCreateSession(@Nullable SessionInfo sessionInfo)
  {
    createNotificationChannel();
    return new CarAppSession(sessionInfo);
  }

  @NonNull
  @Override
  public Session onCreateSession()
  {
    return onCreateSession(null);
  }

  @NonNull
  public static NotificationCompat.Extender getCarNotificationExtender(@NonNull CarContext context)
  {
    if (mCarNotificationExtender != null)
      return mCarNotificationExtender;

    final Intent intent = new Intent(Intent.ACTION_VIEW)
        .setComponent(new ComponentName(context, CarAppService.class))
        .setData(Uri.fromParts(Const.API_SCHEME, CarAppService.API_CAR_HOST, CarAppService.ACTION_SHOW_NAVIGATION_SCREEN));
    mCarNotificationExtender = new CarAppExtender.Builder()
        .setImportance(NotificationManagerCompat.IMPORTANCE_MIN)
        .setContentIntent(CarPendingIntent.getCarApp(context, intent.hashCode(), intent, 0))
        .build();

    return mCarNotificationExtender;
  }

  private void createNotificationChannel()
  {
    final NotificationManagerCompat notificationManager = NotificationManagerCompat.from(this);
    final NotificationChannelCompat notificationChannel =
        new NotificationChannelCompat.Builder(ANDROID_AUTO_NOTIFICATION_CHANNEL_ID, NotificationManagerCompat.IMPORTANCE_MIN)
            .setName(getString(R.string.car_notification_channel_name))
            .setLightsEnabled(false)    // less annoying
            .setVibrationEnabled(false) // less annoying
            .build();
    notificationManager.createNotificationChannel(notificationChannel);
  }
}
