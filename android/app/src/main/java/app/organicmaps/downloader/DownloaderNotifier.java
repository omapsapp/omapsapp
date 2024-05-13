package app.organicmaps.downloader;

import static android.Manifest.permission.POST_NOTIFICATIONS;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.app.NotificationChannelCompat;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;
import androidx.core.content.ContextCompat;

import app.organicmaps.MwmActivity;
import app.organicmaps.R;
import app.organicmaps.sdk.downloader.MapManager;
import app.organicmaps.util.StringUtils;
import app.organicmaps.sdk.util.log.Logger;

public abstract class DownloaderNotifier
{
  private static final String TAG = DownloaderNotifier.class.getSimpleName();

  private static final String CHANNEL_ID = "downloader";
  private static final int NOTIFICATION_ID = 1;

  public static void createNotificationChannel(@NonNull Context context)
  {
    final NotificationManagerCompat notificationManager = NotificationManagerCompat.from(context);
    final NotificationChannelCompat channel = new NotificationChannelCompat.Builder(CHANNEL_ID,
        NotificationManagerCompat.IMPORTANCE_DEFAULT)
        .setName(context.getString(R.string.notification_channel_downloader))
        .setShowBadge(true)
        .setVibrationEnabled(false)
        .setLightsEnabled(false)
        .build();
    notificationManager.createNotificationChannel(channel);
  }

  public static void notifyDownloadFailed(@NonNull Context context, @Nullable String countryId)
  {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
        ContextCompat.checkSelfPermission(context, POST_NOTIFICATIONS) != PERMISSION_GRANTED)
    {
      Logger.w(TAG, "Permission POST_NOTIFICATIONS is not granted, skipping notification");
      return;
    }

    final String title = context.getString(R.string.app_name);
    final String countryName = MapManager.nativeGetName(countryId);
    final String content = context.getString(R.string.download_country_failed, countryName);

    final int FLAG_IMMUTABLE = Build.VERSION.SDK_INT < Build.VERSION_CODES.M ? 0 : PendingIntent.FLAG_IMMUTABLE;
    final Intent contentIntent = MwmActivity.createShowMapIntent(context, countryId);
    contentIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    final PendingIntent contentPendingIntent = PendingIntent.getActivity(context, 0, contentIntent,
        PendingIntent.FLAG_UPDATE_CURRENT | FLAG_IMMUTABLE);

    final Notification notification = new NotificationCompat.Builder(context, CHANNEL_ID)
        .setAutoCancel(true)
        .setCategory(NotificationCompat.CATEGORY_ERROR)
        .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
        .setSmallIcon(R.drawable.ic_splash)
        .setColor(ContextCompat.getColor(context, R.color.notification))
        .setContentTitle(title)
        .setContentText(content)
        .setShowWhen(true)
        .setTicker(getTicker(context, title, content))
        .setContentIntent(contentPendingIntent)
        .setOnlyAlertOnce(true)
        .build();

    Logger.i(TAG, "Notifying about failed map download");
    final NotificationManagerCompat notificationManager = NotificationManagerCompat.from(context);
    notificationManager.notify(NOTIFICATION_ID, notification);
  }

  static void cancelNotification(@NonNull Context context)
  {
    Logger.i(TAG, "Cancelling notification about failed map download");
    final NotificationManagerCompat notificationManager = NotificationManagerCompat.from(context);
    notificationManager.cancel(NOTIFICATION_ID);
  }

  @NonNull
  private static CharSequence getTicker(@NonNull Context context, @NonNull String title, @NonNull String content)
  {
    @StringRes final int templateResId = StringUtils.isRtl() ? R.string.notification_ticker_rtl
        : R.string.notification_ticker_ltr;
    return context.getString(templateResId, title, content);
  }
}
