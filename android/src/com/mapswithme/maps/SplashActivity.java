package com.mapswithme.maps;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.FragmentManager;
import com.mapswithme.maps.base.BaseActivity;
import com.mapswithme.maps.base.BaseActivityDelegate;
import com.mapswithme.maps.location.LocationHelper;
import com.mapswithme.util.Config;
import com.mapswithme.util.Counters;
import com.mapswithme.util.PermissionsUtils;
import com.mapswithme.util.ThemeUtils;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.concurrency.UiThread;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

public class SplashActivity extends AppCompatActivity implements BaseActivity
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.MISC);
  private static final String TAG = SplashActivity.class.getSimpleName();
  private static final String EXTRA_ACTIVITY_TO_START = "extra_activity_to_start";
  public static final String EXTRA_INITIAL_INTENT = "extra_initial_intent";
  private static final int REQUEST_PERMISSIONS = 1;
  private static final long DELAY = 100;

  private View mIvLogo;
  private View mAppName;

  private boolean mPermissionsGranted;
  private boolean mCanceled;

  @NonNull
  private final Runnable mPermissionsDelayedTask = new Runnable()
  {
    @Override
    public void run()
    {
      PermissionsUtils.requestLocationPermission(SplashActivity.this, REQUEST_PERMISSIONS);
    }
  };

  @NonNull
  private final Runnable mInitCoreDelayedTask = new Runnable()
  {
    @Override
    public void run()
    {
      MwmApplication app = (MwmApplication) getApplication();
      if (app.arePlatformAndCoreInitialized())
      {
        UiThread.runLater(mFinalDelayedTask);
        return;
      }

      init();

      LOGGER.i(TAG, "Core initialized: " + app.arePlatformAndCoreInitialized());

//    Run delayed task because resumeDialogs() must see the actual value of mCanceled flag,
//    since onPause() callback can be blocked because of UI thread is busy with framework
//    initialization.
      UiThread.runLater(mFinalDelayedTask);
    }
  };

  @NonNull
  private final Runnable mFinalDelayedTask = new Runnable()
  {
    @Override
    public void run()
    {
      resumeDialogs();
    }
  };

  @NonNull
  private final BaseActivityDelegate mBaseDelegate = new BaseActivityDelegate(this);

  public static void start(@NonNull Context context,
                           @Nullable Class<? extends Activity> activityToStart,
                           @Nullable Intent initialIntent)
  {
    Intent intent = new Intent(context, SplashActivity.class);
    if (activityToStart != null)
      intent.putExtra(EXTRA_ACTIVITY_TO_START, activityToStart);
    if (initialIntent != null)
      intent.putExtra(EXTRA_INITIAL_INTENT, initialIntent);
    context.startActivity(intent);
  }

  @Override
  protected void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    mBaseDelegate.onCreate();
    UiThread.cancelDelayedTasks(mPermissionsDelayedTask);
    UiThread.cancelDelayedTasks(mInitCoreDelayedTask);
    UiThread.cancelDelayedTasks(mFinalDelayedTask);
    Counters.initCounters(this);
    initView();
  }

  @Override
  protected void onNewIntent(Intent intent)
  {
    super.onNewIntent(intent);
    mBaseDelegate.onNewIntent(intent);
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    mBaseDelegate.onStart();
  }

  @Override
  protected void onResume()
  {
    super.onResume();
    mBaseDelegate.onResume();
    mCanceled = false;

    Context context = getApplicationContext();
    if (Counters.isMigrationNeeded(context))
    {
      Config.migrateCountersToSharedPrefs(context);
      Counters.setMigrationExecuted(context);
    }
    
    final boolean isFirstLaunch = isFirstLaunch();
    if (isFirstLaunch)
      MwmApplication.from(this).setFirstLaunch(true);

    if (processPermissionGranting())
      runInitCoreTask();
  }

  private boolean processPermissionGranting()
  {
    mPermissionsGranted = PermissionsUtils.isLocationGranted(this);
    if (!mPermissionsGranted)
    {
      UiThread.runLater(mPermissionsDelayedTask, DELAY);
      return false;
    }

    return true;
  }

  private void runInitCoreTask()
  {
    UiThread.runLater(mInitCoreDelayedTask, DELAY);
  }

  @Override
  protected void onPause()
  {
    super.onPause();
    mBaseDelegate.onPause();
    mCanceled = true;
    UiThread.cancelDelayedTasks(mPermissionsDelayedTask);
    UiThread.cancelDelayedTasks(mInitCoreDelayedTask);
    UiThread.cancelDelayedTasks(mFinalDelayedTask);
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    mBaseDelegate.onStop();
  }

  @Override
  protected void onDestroy()
  {
    super.onDestroy();
    mBaseDelegate.onDestroy();
  }

  private void resumeDialogs()
  {
    if (mCanceled)
      return;

    MwmApplication app = (MwmApplication) getApplication();
    if (!app.arePlatformAndCoreInitialized())
    {
      showExternalStorageErrorDialog();
      return;
    }

    processNavigation();
  }

  private void showExternalStorageErrorDialog()
  {
    AlertDialog dialog = new AlertDialog.Builder(this)
        .setTitle(R.string.dialog_error_storage_title)
        .setMessage(R.string.dialog_error_storage_message)
        .setPositiveButton(R.string.ok, new DialogInterface.OnClickListener()
        {
          @Override
          public void onClick(DialogInterface dialog, int which)
          {
            SplashActivity.this.finish();
          }
        })
        .setCancelable(false)
        .create();
    dialog.show();
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                         @NonNull int[] grantResults)
  {
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    if (grantResults.length == 0)
      return;

    mPermissionsGranted = PermissionsUtils.computePermissionsResult(permissions, grantResults)
                                          .isLocationGranted();
  }

  private void initView()
  {
    UiUtils.setupStatusBar(this);
    setContentView(R.layout.activity_splash);
    mIvLogo = findViewById(R.id.iv__logo);
    mAppName = findViewById(R.id.tv__app_name);
  }

  private void init()
  {
    MwmApplication app = MwmApplication.from(this);
    boolean success = app.initCore();
    if (!success || !app.isFirstLaunch())
      return;

    LocationHelper.INSTANCE.onEnteredIntoFirstRun();
    if (!LocationHelper.INSTANCE.isActive())
      LocationHelper.INSTANCE.start();
  }

  @SuppressWarnings("unchecked")
  private void processNavigation()
  {
    Intent input = getIntent();
    Intent result = new Intent(this, DownloadResourcesLegacyActivity.class);
    if (input != null)
    {
      if (input.hasExtra(EXTRA_ACTIVITY_TO_START))
      {
        result = new Intent(this,
                            (Class<? extends Activity>) input.getSerializableExtra(EXTRA_ACTIVITY_TO_START));
      }

      Intent initialIntent = input.hasExtra(EXTRA_INITIAL_INTENT) ?
                           input.getParcelableExtra(EXTRA_INITIAL_INTENT) :
                           input;
      result.putExtra(EXTRA_INITIAL_INTENT, initialIntent);
    }
    startActivity(result);
    finish();
  }

  @Override
  @NonNull
  public Activity get()
  {
    return this;
  }

  @Override
  public int getThemeResourceId(@NonNull String theme)
  {
    Context context = getApplicationContext();
    if (ThemeUtils.isDefaultTheme(context, theme))
      return R.style.MwmTheme;

    if (ThemeUtils.isNightTheme(context, theme))
      return R.style.MwmTheme_Night;

    throw new IllegalArgumentException("Attempt to apply unsupported theme: " + theme);
  }

  boolean isFirstLaunch()
  {
    if (Counters.getFirstInstallVersion(getApplicationContext()) < BuildConfig.VERSION_CODE)
      return false;

    FragmentManager fm = getSupportFragmentManager();
    if (fm.isDestroyed())
      return false;

    return !Counters.isFirstStartDialogSeen(this);
  }
}
