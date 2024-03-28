package app.organicmaps;

import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.splashscreen.SplashScreen;

import app.organicmaps.display.DisplayManager;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.util.Config;
import app.organicmaps.util.LocationUtils;
import app.organicmaps.util.ThemeUtils;
import app.organicmaps.util.concurrency.UiThread;
import app.organicmaps.util.log.Logger;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.io.IOException;
import java.util.Objects;

public class SplashActivity extends AppCompatActivity
{
  private static final String TAG = SplashActivity.class.getSimpleName();

  private static final long DELAY = 100;

  private boolean mCanceled = false;

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private ActivityResultLauncher<String[]> mPermissionRequest;

  @NonNull
  private final Runnable mInitCoreDelayedTask = this::init;

  @Override
  protected void onCreate(@Nullable Bundle savedInstanceState)
  {
    SplashScreen splashScreen = SplashScreen.installSplashScreen(this);
    super.onCreate(savedInstanceState);
    final Context context = getApplicationContext();

    final int theme = getSplashTheme(context);
    // use new splashscreen api for >= 31
    // maybe doesn't work for android auto?
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
      getSplashScreen().setSplashScreenTheme(theme);
      //always keep splash while in SplashActivity
      splashScreen.setKeepOnScreenCondition(() -> true );
    }
    // use old splashscreen method for below 31
    else
    {
      setTheme(theme);
      setContentView(R.layout.activity_splash);
    }

    UiThread.cancelDelayedTasks(mInitCoreDelayedTask);

    mPermissionRequest = registerForActivityResult(new ActivityResultContracts.RequestMultiplePermissions(),
        result -> Config.setLocationRequested());

    if (DisplayManager.from(this).isCarDisplayUsed())
    {
      startActivity(new Intent(this, MapPlaceholderActivity.class));
      finish();
    }
  }

  @Override
  protected void onResume()
  {
    super.onResume();
    if (mCanceled)
      return;
    if (!Config.isLocationRequested() && !LocationUtils.checkCoarseLocationPermission(this))
    {
      Logger.d(TAG, "Requesting location permissions");
      mPermissionRequest.launch(new String[]{
          ACCESS_COARSE_LOCATION,
          ACCESS_FINE_LOCATION
      });
      return;
    }

    UiThread.runLater(mInitCoreDelayedTask, DELAY);
  }

  @Override
  protected void onPause()
  {
    super.onPause();
    UiThread.cancelDelayedTasks(mInitCoreDelayedTask);
  }

  @Override
  protected void onDestroy()
  {
    super.onDestroy();
    mPermissionRequest.unregister();
    mPermissionRequest = null;
  }

  private void showFatalErrorDialog(@StringRes int titleId, @StringRes int messageId)
  {
    mCanceled = true;
    new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(titleId)
        .setMessage(messageId)
        .setNegativeButton(R.string.ok, (dialog, which) -> SplashActivity.this.finish())
        .setCancelable(false)
        .show();
  }

  private void init()
  {
    MwmApplication app = MwmApplication.from(this);
    boolean asyncContinue = false;
    try
    {
      asyncContinue = app.init(this::processNavigation);
    } catch (IOException e)
    {
      showFatalErrorDialog(R.string.dialog_error_storage_title, R.string.dialog_error_storage_message);
      return;
    }

    if (Config.isFirstLaunch(this) && LocationUtils.checkLocationPermission(this))
    {
      final LocationHelper locationHelper = app.getLocationHelper();
      locationHelper.onEnteredIntoFirstRun();
      if (!locationHelper.isActive())
        locationHelper.start();
    }

    if (!asyncContinue)
      processNavigation();
  }

  // Called from MwmApplication::nativeInitFramework like callback.
  @Keep
  @SuppressWarnings({"unused", "unchecked"})
  public void processNavigation()
  {
    if (isDestroyed())
    {
      Logger.w(TAG, "Ignore late callback from core because activity is already destroyed");
      return;
    }

    // Re-use original intent to retain all flags and payload.
    // https://github.com/organicmaps/organicmaps/issues/6944
    final Intent intent = Objects.requireNonNull(getIntent());
    intent.setComponent(new ComponentName(this, DownloadResourcesLegacyActivity.class));
    // Flags like FLAG_ACTIVITY_NEW_TASK and FLAG_ACTIVITY_RESET_TASK_IF_NEEDED will break the cold start of the app.
    // https://github.com/organicmaps/organicmaps/pull/7287
    intent.setFlags(intent.getFlags() & (Intent.FLAG_ACTIVITY_FORWARD_RESULT | Intent.FLAG_GRANT_READ_URI_PERMISSION));

    Config.setFirstStartDialogSeen(this);
    startActivity(intent);
    finish();
  }
  private int getSplashTheme(Context context)
  {
    final String theme = Config.getCurrentUiTheme(context);
    if (ThemeUtils.isDefaultTheme(context, theme))
        return R.style.MwmTheme_Splash;
      else if (ThemeUtils.isNightTheme(context, theme))
        return R.style.MwmTheme_Night_Splash;
      else
        throw new IllegalArgumentException("Attempt to apply unsupported theme: " + theme);
  }
}
