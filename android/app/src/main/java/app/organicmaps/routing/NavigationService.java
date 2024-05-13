package app.organicmaps.routing;

import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;
import static android.Manifest.permission.POST_NOTIFICATIONS;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;

import android.annotation.SuppressLint;
import android.app.ForegroundServiceStartNotAllowedException;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.location.Location;
import android.os.Build;
import android.os.IBinder;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresPermission;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.app.ActivityCompat;
import androidx.core.app.NotificationChannelCompat;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;
import androidx.core.content.ContextCompat;

import app.organicmaps.sdk.Framework;
import app.organicmaps.MwmActivity;
import app.organicmaps.MwmApplication;
import app.organicmaps.R;
import app.organicmaps.sdk.routing.RoutingController;
import app.organicmaps.sdk.routing.RoutingInfo;
import app.organicmaps.sound.MediaPlayerWrapper;
import app.organicmaps.sdk.location.LocationHelper;
import app.organicmaps.sdk.location.LocationListener;
import app.organicmaps.sound.TtsPlayer;
import app.organicmaps.util.Graphics;
import app.organicmaps.sdk.util.LocationUtils;
import app.organicmaps.sdk.util.log.Logger;

public class NavigationService extends Service implements LocationListener
{
  private static final String TAG = NavigationService.class.getSimpleName();

  private static final String CHANNEL_ID = "NAVIGATION";
  private static final int NOTIFICATION_ID = 12345678;

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private MediaPlayerWrapper mPlayer;

  // Destroyed in onDestroy()
  @SuppressLint("StaticFieldLeak")
  @Nullable
  private static NotificationCompat.Builder mNotificationBuilder;

  @Nullable
  private static NotificationCompat.Extender mCarNotificationExtender;

  /**
   * Start the foreground service for turn-by-turn voice-guided navigation.
   *
   * @param context Context to start service from.
   */
  @RequiresPermission(value = ACCESS_FINE_LOCATION)
  public static void startForegroundService(@NonNull Context context)
  {
    Logger.i(TAG);
    ContextCompat.startForegroundService(context, new Intent(context, NavigationService.class));
  }

  /**
   * Start the foreground service for turn-by-turn voice-guided navigation.
   *
   * @param context                 Context to start service from.
   * @param carNotificationExtender Extender used for displaying notifications in the Android Auto
   */
  @RequiresPermission(value = ACCESS_FINE_LOCATION)
  public static void startForegroundService(@NonNull Context context, @NonNull NotificationCompat.Extender carNotificationExtender)
  {
    Logger.i(TAG);
    mCarNotificationExtender = carNotificationExtender;
    startForegroundService(context);
  }

  /**
   * Stop the foreground service for  turn-by-turn voice-guided navigation.
   *
   * @param context Context to stop service from.
   */
  public static void stopService(@NonNull Context context)
  {
    Logger.i(TAG);
    context.stopService(new Intent(context, NavigationService.class));
  }

  /**
   * Creates notification channel for navigation.
   *
   * @param context Context to create channel from.
   */
  public static void createNotificationChannel(@NonNull Context context)
  {
    Logger.i(TAG);

    final NotificationManagerCompat notificationManager = NotificationManagerCompat.from(context);
    final NotificationChannelCompat channel = new NotificationChannelCompat.Builder(CHANNEL_ID,
        NotificationManagerCompat.IMPORTANCE_LOW)
        .setName(context.getString(R.string.pref_navigation))
        .setLightsEnabled(false)    // less annoying
        .setVibrationEnabled(false) // less annoying
        .build();
    notificationManager.createNotificationChannel(channel);
  }

  /**
   * See {@link android.app.Notification.Builder#setColorized(boolean) }
   */
  private static boolean isColorizedSupported()
  {
    // Nice colorized notifications should be supported on API=26 and later.
    // Nonetheless, even on API=32, Xiaomi uses their own legacy implementation that displays white-on-white instead.
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O &&
        !"xiaomi".equalsIgnoreCase(Build.MANUFACTURER);
  }

  @NonNull
  public static NotificationCompat.Builder getNotificationBuilder(@NonNull Context context)
  {
    if (mNotificationBuilder != null)
      return mNotificationBuilder;

    final int FLAG_IMMUTABLE = Build.VERSION.SDK_INT < Build.VERSION_CODES.M ? 0 : PendingIntent.FLAG_IMMUTABLE;
    final Intent contentIntent = new Intent(context, MwmActivity.class);
    final PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, contentIntent,
        PendingIntent.FLAG_CANCEL_CURRENT | FLAG_IMMUTABLE);

    mNotificationBuilder = new NotificationCompat.Builder(context, CHANNEL_ID)
        .setCategory(NotificationCompat.CATEGORY_NAVIGATION)
        .setPriority(NotificationManager.IMPORTANCE_LOW)
        .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
        .setOngoing(true)
        .setShowWhen(false)
        .setOnlyAlertOnce(true)
        .setSmallIcon(R.drawable.ic_splash)
        .setContentIntent(pendingIntent)
        .setColorized(isColorizedSupported())
        .setColor(ContextCompat.getColor(context, R.color.notification));

    return mNotificationBuilder;
  }

  @Override
  public void onCreate()
  {
    Logger.i(TAG);

    mPlayer = new MediaPlayerWrapper(getApplicationContext());
  }

  @Override
  public void onDestroy()
  {
    Logger.i(TAG);

    mNotificationBuilder = null;
    mCarNotificationExtender = null;
    LocationHelper.from(this).removeListener(this);
    TtsPlayer.INSTANCE.stop();

    // The notification is cancelled automatically by the system.

    mPlayer.release();
  }

  @Override
  public void onLowMemory()
  {
    Logger.d(TAG);
  }

  @Override
  public int onStartCommand(@NonNull Intent intent, int flags, int startId)
  {
    if (!MwmApplication.from(this).arePlatformAndCoreInitialized())
    {
      // The system restarts the service if the app's process has crashed or been stopped. It would be nice to
      // automatically restore the last route and resume navigation. Unfortunately, the current implementation of
      // the routing state machine (RoutingController and underlying NDK part) requires a complete re-planning of
      // the route. Such operation can fail for some reason. We have no UI (i.e. RoutePlanFragment) started to
      // handle any route planning errors. Starting any new Activities from Services is not allowed also.
      // https://github.com/organicmaps/organicmaps/issues/6233
      Logger.w(TAG, "Application is not initialized");
      stopSelf();
      return START_NOT_STICKY; // The service will be stopped by stopSelf().
    }

    if (!LocationUtils.checkFineLocationPermission(this))
    {
      // In a hypothetical scenario, the user could revoke location permissions after the app's process crashed,
      // but before the service with START_STICKY was restarted by the system.
      Logger.w(TAG, "Permission ACCESS_FINE_LOCATION is not granted, skipping NavigationService");
      stopSelf();
      return START_NOT_STICKY; // The service will be stopped by stopSelf().
    }

    Logger.i(TAG, "Starting foreground");
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
    {
      try
      {
        startForeground(NavigationService.NOTIFICATION_ID, getNotificationBuilder(this).build());
      } catch (ForegroundServiceStartNotAllowedException e)
      {
        Logger.e(TAG, "Oops! ForegroundService is not allowed", e);
      }
    }
    else
    {
      startForeground(NavigationService.NOTIFICATION_ID, getNotificationBuilder(this).build());
    }

    final LocationHelper locationHelper = LocationHelper.from(this);

    // Subscribe to location updates. This call is idempotent.
    locationHelper.addListener(this);

    // Restart the location with more frequent refresh interval for navigation.
    locationHelper.restartWithNewMode();

    // Please make this service START_STICKY after fixing the issues at the beginning of the function.
    return START_NOT_STICKY;
  }

  @Nullable
  @Override
  public IBinder onBind(Intent intent)
  {
    return null;
  }

  @Override
  @RequiresPermission(anyOf = {ACCESS_COARSE_LOCATION, ACCESS_FINE_LOCATION})
  public void onLocationUpdated(@NonNull Location location)
  {
    // Ignore any pending notifications when service is being stopping.
    final RoutingController routingController = RoutingController.get();
    if (!routingController.isNavigating())
      return;

    final String[] turnNotifications = Framework.nativeGenerateNotifications();
    if (turnNotifications != null)
      TtsPlayer.INSTANCE.playTurnNotifications(turnNotifications);

    // TODO: consider to create callback mechanism to transfer 'ROUTE_IS_FINISHED' event from
    // the core to the platform code (https://github.com/organicmaps/organicmaps/issues/3589),
    // because calling the native method 'nativeIsRouteFinished'
    // too often can result in poor UI performance.
    // This check should be done after playTurnNotifications() to play the last turn notification.
    if (Framework.nativeIsRouteFinished())
    {
      routingController.cancel();
      LocationHelper.from(this).restartWithNewMode();
      stopSelf();
      return;
    }

    final RoutingInfo routingInfo = Framework.nativeGetRouteFollowingInfo();
    if (routingInfo == null)
      return;

    if (routingInfo.shouldPlayWarningSignal())
      mPlayer.playback(R.raw.speed_cams_beep);

    // Don't spend time on updating RemoteView if notifications are not allowed.
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
        ActivityCompat.checkSelfPermission(this, POST_NOTIFICATIONS) != PERMISSION_GRANTED)
      return;

    final NotificationCompat.Builder notificationBuilder = getNotificationBuilder(this)
        .setContentTitle(routingInfo.distToTurn.toString(this))
        .setContentText(routingInfo.nextStreet);

    final Drawable drawable = AppCompatResources.getDrawable(this, routingInfo.carDirection.getTurnRes());
    if (drawable != null)
    {
      final Bitmap bitmap = isColorizedSupported() ?
          Graphics.drawableToBitmap(drawable) :
          Graphics.drawableToBitmapWithTint(drawable, ContextCompat.getColor(this, R.color.base_accent));
      notificationBuilder.setLargeIcon(bitmap);
    }

    if (mCarNotificationExtender != null)
      notificationBuilder.extend(mCarNotificationExtender);

    // The notification object must be re-created for every update.
    NotificationManagerCompat.from(this).notify(NOTIFICATION_ID, notificationBuilder.build());
  }
}
