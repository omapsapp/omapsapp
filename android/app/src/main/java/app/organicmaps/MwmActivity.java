package app.organicmaps;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.app.PendingIntent;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.location.Location;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.IntentSenderRequest;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.CallSuper;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.annotation.UiThread;
import androidx.appcompat.widget.Toolbar;
import androidx.core.app.ActivityCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.lifecycle.ViewModelProvider;
import app.organicmaps.Framework.PlacePageActivationListener;
import app.organicmaps.api.Const;
import app.organicmaps.base.BaseMwmFragmentActivity;
import app.organicmaps.base.OnBackPressListener;
import app.organicmaps.bookmarks.BookmarkCategoriesActivity;
import app.organicmaps.bookmarks.data.BookmarkManager;
import app.organicmaps.bookmarks.data.MapObject;
import app.organicmaps.display.DisplayChangedListener;
import app.organicmaps.display.DisplayManager;
import app.organicmaps.display.DisplayType;
import app.organicmaps.downloader.DownloaderActivity;
import app.organicmaps.downloader.DownloaderFragment;
import app.organicmaps.downloader.MapManager;
import app.organicmaps.downloader.OnmapDownloader;
import app.organicmaps.downloader.UpdateInfo;
import app.organicmaps.editor.Editor;
import app.organicmaps.editor.EditorActivity;
import app.organicmaps.editor.EditorHostFragment;
import app.organicmaps.editor.FeatureCategoryActivity;
import app.organicmaps.editor.OsmLoginActivity;
import app.organicmaps.editor.OsmOAuth;
import app.organicmaps.editor.ReportFragment;
import app.organicmaps.help.HelpActivity;
import app.organicmaps.intent.Factory;
import app.organicmaps.intent.IntentProcessor;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.location.LocationListener;
import app.organicmaps.location.LocationState;
import app.organicmaps.location.SensorHelper;
import app.organicmaps.location.SensorListener;
import app.organicmaps.location.TrackRecorder;
import app.organicmaps.location.TrackRecordingService;
import app.organicmaps.maplayer.MapButtonsController;
import app.organicmaps.maplayer.MapButtonsViewModel;
import app.organicmaps.maplayer.ToggleMapLayerFragment;
import app.organicmaps.maplayer.isolines.IsolinesManager;
import app.organicmaps.maplayer.isolines.IsolinesState;
import app.organicmaps.maplayer.subway.SubwayManager;
import app.organicmaps.routing.NavigationController;
import app.organicmaps.routing.NavigationService;
import app.organicmaps.routing.RoutePointInfo;
import app.organicmaps.routing.RoutingBottomMenuListener;
import app.organicmaps.routing.RoutingController;
import app.organicmaps.routing.RoutingErrorDialogFragment;
import app.organicmaps.routing.RoutingOptions;
import app.organicmaps.routing.RoutingPlanFragment;
import app.organicmaps.routing.RoutingPlanInplaceController;
import app.organicmaps.search.FloatingSearchToolbarController;
import app.organicmaps.search.SearchActivity;
import app.organicmaps.search.SearchEngine;
import app.organicmaps.search.SearchFragment;
import app.organicmaps.settings.DrivingOptionsActivity;
import app.organicmaps.settings.RoadType;
import app.organicmaps.settings.SettingsActivity;
import app.organicmaps.settings.UnitLocale;
import app.organicmaps.util.Config;
import app.organicmaps.util.LocationUtils;
import app.organicmaps.util.PowerManagment;
import app.organicmaps.util.SharingUtils;
import app.organicmaps.util.ThemeSwitcher;
import app.organicmaps.util.ThemeUtils;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.util.bottomsheet.MenuBottomSheetFragment;
import app.organicmaps.util.bottomsheet.MenuBottomSheetItem;
import app.organicmaps.util.log.Logger;
import app.organicmaps.widget.StackedButtonsDialog;
import app.organicmaps.widget.menu.MainMenu;
import app.organicmaps.widget.placepage.PlacePageController;
import app.organicmaps.widget.placepage.PlacePageData;
import app.organicmaps.widget.placepage.PlacePageViewModel;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.util.ArrayList;
import java.util.Objects;

import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;
import static android.Manifest.permission.POST_NOTIFICATIONS;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;
import static app.organicmaps.location.LocationState.FOLLOW;
import static app.organicmaps.location.LocationState.FOLLOW_AND_ROTATE;
import static app.organicmaps.location.LocationState.LOCATION_TAG;
import static app.organicmaps.util.PowerManagment.POWER_MANAGEMENT_TAG;

public class MwmActivity extends BaseMwmFragmentActivity
    implements PlacePageActivationListener,
               View.OnTouchListener,
               MapRenderingListener,
               RoutingController.Container,
               LocationListener,
               SensorListener,
               LocationState.ModeChangeListener,
               RoutingPlanInplaceController.RoutingPlanListener,
               RoutingBottomMenuListener,
               BookmarkManager.BookmarksLoadingListener,
               FloatingSearchToolbarController.SearchToolbarListener,
               MenuBottomSheetFragment.MenuBottomSheetInterfaceWithHeader,
               PlacePageController.PlacePageRouteSettingsListener,
               MapButtonsController.MapButtonClickListener,
               DisplayChangedListener
{
  private static final String TAG = MwmActivity.class.getSimpleName();

  public static final String EXTRA_COUNTRY_ID = "country_id";
  public static final String EXTRA_CATEGORY_ID = "category_id";
  public static final String EXTRA_BOOKMARK_ID = "bookmark_id";
  public static final String EXTRA_TRACK_ID = "track_id";
  public static final String EXTRA_UPDATE_THEME = "update_theme";
  private static final String EXTRA_CONSUMED = "mwm.extra.intent.processed";

  private static final String[] DOCKED_FRAGMENTS = { SearchFragment.class.getName(),
                                                     DownloaderFragment.class.getName(),
                                                     RoutingPlanFragment.class.getName(),
                                                     EditorHostFragment.class.getName(),
                                                     ReportFragment.class.getName() };

  public static final int REQ_CODE_DRIVING_OPTIONS = 6;

  private static final String MAIN_MENU_ID = "MAIN_MENU_BOTTOM_SHEET";
  private static final String LAYERS_MENU_ID = "LAYERS_MENU_BOTTOM_SHEET";

  private static final String POWER_SAVE_DISCLAIMER_SHOWN = "POWER_SAVE_DISCLAIMER_SHOWN";

  @Nullable
  private MapFragment mMapFragment;

  private View mPointChooser;
  private Toolbar mPointChooserToolbar;

  private RoutingPlanInplaceController mRoutingPlanInplaceController;

  private NavigationController mNavigationController;

  private MainMenu mMainMenu;

  private PanelAnimator mPanelAnimator;
  @Nullable
  private OnmapDownloader mOnmapDownloader;
  private boolean mIsTabletLayout;
  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private FloatingSearchToolbarController mSearchController;

  private boolean mRestoreRoutingPlanFragmentNeeded;
  @Nullable
  private Bundle mSavedForTabletState;
  private String mDonatesUrl;

  private int mNavBarHeight;

  private PlacePageViewModel mPlacePageViewModel;
  private MapButtonsViewModel mMapButtonsViewModel;
  private MapButtonsController.LayoutMode mPreviousMapLayoutMode;

  @Nullable
  private WindowInsetsCompat mCurrentWindowInsets;

  @Nullable
  private Dialog mLocationErrorDialog;

  @Nullable
  private Dialog mAlertDialog;

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private ActivityResultLauncher<String[]> mLocationPermissionRequest;
  private boolean mLocationPermissionRequestedForRecording = false;

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private ActivityResultLauncher<String> mPostNotificationPermissionRequest;

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private ActivityResultLauncher<IntentSenderRequest> mLocationResolutionRequest;
  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private ActivityResultLauncher<SharingUtils.SharingIntent> mShareLauncher;
  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private ActivityResultLauncher<Intent> mPowerSaveSettings;
  @NonNull
  private boolean mPowerSaveDisclaimerShown = false;

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private DisplayManager mDisplayManager;

  private boolean mRemoveDisplayListener = true;
  private int mLastUiMode = Configuration.UI_MODE_TYPE_UNDEFINED;

  public interface LeftAnimationTrackListener
  {
    void onTrackStarted(boolean collapsed);

    void onTrackFinished(boolean collapsed);

    void onTrackLeftAnimation(float offset);
  }

  public static Intent createShowMapIntent(@NonNull Context context, @Nullable String countryId)
  {
    return new Intent(context, DownloadResourcesLegacyActivity.class)
        .putExtra(EXTRA_COUNTRY_ID, countryId);
  }

  @Override
  public void onRenderingCreated()
  {
    checkMeasurementSystem();
  }

  // Called from JNI.
  @Override
  @Keep
  @SuppressWarnings("unused")
  public void onRenderingInitializationFinished()
  {
    ThemeSwitcher.INSTANCE.restart(true);

    if (RoutingController.get().isPlanning())
      onPlanningStarted();
    else if (RoutingController.get().isNavigating())
      onNavigationStarted();
    else if (RoutingController.get().hasSavedRoute())
      RoutingController.get().restoreRoute();

    if (TrackRecorder.nativeIsTrackRecordingEnabled() && !startTrackRecording())
    {
      // The user has revoked location permissions in the system settings, causing the app to
      // restart while recording was active. Save the recorded data and stop the recording.
      saveAndStopTrackRecording();
    }

    processIntent();
    migrateOAuthCredentials();
  }

  /**
   * Process intents AFTER rendering is initialized.
   */
  private void processIntent()
  {
    if (!Map.isEngineCreated())
      throw new AssertionError("Must be called with initialized Drape");

    final Intent intent = getIntent();
    if (intent == null || intent.getBooleanExtra(EXTRA_CONSUMED, false))
      return;
    intent.putExtra(EXTRA_CONSUMED, true);

    final long categoryId = intent.getLongExtra(EXTRA_CATEGORY_ID, -1);
    final long bookmarkId = intent.getLongExtra(EXTRA_BOOKMARK_ID, -1);
    final long trackId = intent.getLongExtra(EXTRA_TRACK_ID, -1);
    if (bookmarkId != -1)
    {
      Objects.requireNonNull(BookmarkManager.INSTANCE.getBookmarkInfo(bookmarkId));
      BookmarkManager.INSTANCE.showBookmarkOnMap(bookmarkId);
      return;
    }
    else if (trackId != -1)
    {
      Objects.requireNonNull(BookmarkManager.INSTANCE.getTrack(trackId));
      Framework.nativeShowTrackRect(trackId);
      return;
    }
    else if (categoryId != -1)
    {
      BookmarkManager.INSTANCE.showBookmarkCategoryOnMap(categoryId);
      return;
    }

    final String countryId = intent.getStringExtra(EXTRA_COUNTRY_ID);
    if (countryId != null)
    {
      Framework.nativeShowCountry(countryId, false);
      return;
    }

    final IntentProcessor[] mIntentProcessors = {
        new Factory.UrlProcessor(),
        new Factory.KmzKmlProcessor(),
    };
    for (IntentProcessor ip : mIntentProcessors)
    {
      if (ip.process(intent, this))
        break;
    }
  }

  private void migrateOAuthCredentials()
  {
    if (OsmOAuth.containsOAuth1Credentials(this))
    {
      // Remove old OAuth v1 secrets
      OsmOAuth.clearOAuth1Credentials(this);

      // Notify user to re-login
      dismissAlertDialog();
      final DialogInterface.OnClickListener navigateToLoginHandler = (dialog, which) -> startActivity(new Intent(MwmActivity.this, OsmLoginActivity.class));

      final int marginBase = getResources().getDimensionPixelSize(R.dimen.margin_base);
      final float textSize = getResources().getDimension(R.dimen.line_spacing_extra_1);
      final TextView text = new TextView(this);
      text.setText(getText(R.string.alert_reauth_message));
      text.setPadding(marginBase, marginBase, marginBase, marginBase);
      text.setTextSize(textSize);
      text.setMovementMethod(LinkMovementMethod.getInstance());

      mAlertDialog = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
              .setTitle(R.string.login_osm)
              .setView(text)
              .setPositiveButton(R.string.login, navigateToLoginHandler)
              .setNegativeButton(R.string.cancel, null)
              .setOnDismissListener(dialog -> mAlertDialog = null)
              .show();
    }
  }

  private static void checkMeasurementSystem()
  {
    UnitLocale.initializeCurrentUnits();
  }

  @Override
  protected int getFragmentContentResId()
  {
    return (mIsTabletLayout ? R.id.fragment_container
                            : super.getFragmentContentResId());
  }

  @Nullable
  Fragment getFragment(Class<? extends Fragment> clazz)
  {
    if (!mIsTabletLayout)
      throw new IllegalStateException("Must be called for tablets only!");

    return getSupportFragmentManager().findFragmentByTag(clazz.getName());
  }

  void replaceFragmentInternal(Class<? extends Fragment> fragmentClass, Bundle args)
  {
    super.replaceFragment(fragmentClass, args, null);
  }

  @Override
  public void replaceFragment(@NonNull Class<? extends Fragment> fragmentClass, @Nullable Bundle args, @Nullable Runnable completionListener)
  {
    if (mPanelAnimator.isVisible() && getFragment(fragmentClass) != null)
    {
      if (completionListener != null)
        completionListener.run();
      return;
    }

    mPanelAnimator.show(fragmentClass, args, completionListener);
  }

  private void showBookmarks()
  {
    BookmarkCategoriesActivity.start(this);
  }

  private void showHelp()
  {
    Intent intent = new Intent(this, HelpActivity.class);
    startActivity(intent);
  }

  private void showSearch(String query)
  {
    closeSearchToolbar(false, true);
    if (mIsTabletLayout)
    {
      final Bundle args = new Bundle();
      args.putString(SearchActivity.EXTRA_QUERY, query);
      replaceFragment(SearchFragment.class, args, null);
    }
    else
    {
      SearchActivity.start(this, query);
    }
  }

  public void showEditor()
  {
    // TODO(yunikkk) think about refactoring. It probably should be called in editor.
    Editor.nativeStartEdit();
    if (mIsTabletLayout)
      replaceFragment(EditorHostFragment.class, null, null);
    else
      EditorActivity.start(this);
  }

  private void shareMyLocation()
  {
    final Location loc = LocationHelper.from(this).getSavedLocation();
    if (loc != null)
    {
      SharingUtils.shareLocation(this, loc);
      return;
    }

    dismissLocationErrorDialog();
    mLocationErrorDialog = new MaterialAlertDialogBuilder(MwmActivity.this, R.style.MwmTheme_AlertDialog)
        .setMessage(R.string.unknown_current_position)
        .setCancelable(true)
        .setPositiveButton(R.string.ok, null)
        .setOnDismissListener(dialog -> mLocationErrorDialog = null)
        .show();
  }

  private void showDownloader(boolean openDownloaded)
  {
    final Bundle args = new Bundle();
    args.putBoolean(DownloaderActivity.EXTRA_OPEN_DOWNLOADED, openDownloaded);
    if (mIsTabletLayout)
    {
      closeSearchToolbar(false, true);
      replaceFragment(DownloaderFragment.class, args, null);
    }
    else
    {
      startActivity(new Intent(this, DownloaderActivity.class).putExtras(args));
    }
  }

  @Override
  @StyleRes
  protected int getThemeResourceId(@NonNull String theme)
  {
    Context context = getApplicationContext();

    if (ThemeUtils.isDefaultTheme(context, theme))
      return R.style.MwmTheme_MainActivity;

    if (ThemeUtils.isNightTheme(context, theme))
      return R.style.MwmTheme_Night_MainActivity;

    return super.getThemeResourceId(theme);
  }

  @Override
  public void onDisplayChangedToCar(@NonNull Runnable onTaskFinishedCallback)
  {
    mRemoveDisplayListener = false;
    startActivity(new Intent(this, MapPlaceholderActivity.class));
    Objects.requireNonNull(mMapFragment).notifyOnSurfaceDestroyed(onTaskFinishedCallback);
    finish();
  }

  @Override
  public void onConfigurationChanged(@NonNull Configuration newConfig)
  {
    super.onConfigurationChanged(newConfig);

    final int newUiMode = newConfig.uiMode & Configuration.UI_MODE_TYPE_MASK;
    final boolean newUiModeIsCarConnected = newUiMode == Configuration.UI_MODE_TYPE_CAR;
    final boolean newUiModeIsCarDisconnected = mLastUiMode == Configuration.UI_MODE_TYPE_CAR && newUiMode == Configuration.UI_MODE_TYPE_NORMAL;
    mLastUiMode = newUiMode;

    if (newUiModeIsCarConnected || newUiModeIsCarDisconnected)
      return;
    recreate();
  }

  @SuppressLint("InlinedApi")
  @CallSuper
  @Override
  protected void onSafeCreate(@Nullable Bundle savedInstanceState)
  {
    super.onSafeCreate(savedInstanceState);

    mIsTabletLayout = getResources().getBoolean(R.bool.tabletLayout);

    if (!mIsTabletLayout)
      getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);

    setContentView(R.layout.activity_map);

    mPlacePageViewModel = new ViewModelProvider(this).get(PlacePageViewModel.class);
    mMapButtonsViewModel = new ViewModelProvider(this).get(MapButtonsViewModel.class);
    // We don't need to manually handle removing the observers it follows the activity lifecycle
    mMapButtonsViewModel.getBottomButtonsHeight().observe(this, this::onMapBottomButtonsHeightChange);
    mMapButtonsViewModel.getLayoutMode().observe(this, this::initNavigationButtons);

    mSearchController = new FloatingSearchToolbarController(this, this);
    mSearchController.getToolbar()
                     .getViewTreeObserver();

    // Note: You must call registerForActivityResult() before the fragment or activity is created.
    mLocationPermissionRequest = registerForActivityResult(new ActivityResultContracts.RequestMultiplePermissions(),
        this::onLocationPermissionsResult);
    mLocationResolutionRequest = registerForActivityResult(new ActivityResultContracts.StartIntentSenderForResult(),
        this::onLocationResolutionResult);
    mPostNotificationPermissionRequest = registerForActivityResult(new ActivityResultContracts.RequestPermission(),
        this::onPostNotificationPermissionResult);
    mPowerSaveSettings = registerForActivityResult(new ActivityResultContracts.StartActivityForResult(),
        this::onPowerSaveResult);

    mShareLauncher = SharingUtils.RegisterLauncher(this);

    mDisplayManager = DisplayManager.from(this);
    if (mDisplayManager.isCarDisplayUsed())
    {
      mRemoveDisplayListener = false;
      startActivity(new Intent(this, MapPlaceholderActivity.class));
      finish();
      return;
    }
    mDisplayManager.addListener(DisplayType.Device, this);

    final Intent intent = getIntent();
    final boolean isLaunchByDeepLink = intent != null && !intent.hasCategory(Intent.CATEGORY_LAUNCHER);
    initViews(isLaunchByDeepLink);
    updateViewsInsets();

    if (getIntent().getBooleanExtra(EXTRA_UPDATE_THEME, false))
      ThemeSwitcher.INSTANCE.restart(isMapRendererActive());

    /*
     * onRenderingInitializationFinished() hook is not called when MwmActivity is recreated with the already
     * initialized Drape engine. This can happen when the activity is swiped away from the most recent app lists
     * during navigation and then restarted from the launcher. Call this hook explicitly here to run operations
     * that require initialized Drape, such as restoring navigation and processing incoming intents.
     * https://github.com/organicmaps/organicmaps/issues/6712
     */
    if (Map.isEngineCreated())
      onRenderingInitializationFinished();
  }

  private void refreshLightStatusBar()
  {
    UiUtils.setLightStatusBar(this, !(
        ThemeUtils.isNightTheme(this)
        || RoutingController.get().isPlanning()
        || Framework.nativeGetChoosePositionMode() != Framework.ChoosePositionMode.NONE
    ));
  }

  private void updateViewsInsets()
  {
    ViewCompat.setOnApplyWindowInsetsListener(mPointChooser, (view, windowInsets) -> {
      UiUtils.setViewInsetsPaddingBottom(mPointChooser, windowInsets);
      UiUtils.setViewInsetsPaddingNoBottom(mPointChooserToolbar, windowInsets);

      mNavBarHeight = isFullscreen() ? 0 : windowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).bottom;
      // For the first loading, set compass top margin to status bar size
      // The top inset will be then be updated by the routing controller
      if (mCurrentWindowInsets == null)
        updateCompassOffset(windowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).top, windowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).right);
      else
        updateCompassOffset(-1, windowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).right);
      refreshLightStatusBar();
      updateBottomWidgetsOffset(windowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).left);
      mCurrentWindowInsets = windowInsets;
      return windowInsets;
    });
  }

  private int getDownloadMapsCounter()
  {
    UpdateInfo info = MapManager.nativeGetUpdateInfo(null);
    return info == null ? 0 : info.filesCount;
  }

  private void initViews(boolean isLaunchByDeeplink)
  {
    initMap(isLaunchByDeeplink);
    initNavigationButtons();

    if (!mIsTabletLayout)
    {
      mRoutingPlanInplaceController = new RoutingPlanInplaceController(this, this, this);
      removeCurrentFragment(false);
    }

    mNavigationController = new NavigationController(this, v -> onSettingsOptionSelected(), this::updateBottomWidgetsOffset);
    //TrafficManager.INSTANCE.attach(mNavigationController);

    initMainMenu();
    initOnmapDownloader();
    initPositionChooser();
  }

  private void initPositionChooser()
  {
    mPointChooser = findViewById(R.id.position_chooser);
    if (mPointChooser == null)
      return;

    mPointChooserToolbar = mPointChooser.findViewById(R.id.toolbar_point_chooser);
    UiUtils.showHomeUpButton(mPointChooserToolbar);
    mPointChooserToolbar.setNavigationOnClickListener(v -> closePositionChooser());
    mPointChooser.findViewById(R.id.done).setOnClickListener(
        v ->
        {
          switch (Framework.nativeGetChoosePositionMode())
          {
          case Framework.ChoosePositionMode.API:
            final Intent apiResult = new Intent();
            final double[] center = Framework.nativeGetScreenRectCenter();
            apiResult.putExtra(Const.EXTRA_POINT_LAT, center[0]);
            apiResult.putExtra(Const.EXTRA_POINT_LON, center[1]);
            apiResult.putExtra(Const.EXTRA_ZOOM_LEVEL, Framework.nativeGetDrawScale());
            setResult(Activity.RESULT_OK, apiResult);
            finish();
            break;
          case Framework.ChoosePositionMode.EDITOR:
            if (Framework.nativeIsDownloadedMapAtScreenCenter())
              startActivity(new Intent(MwmActivity.this, FeatureCategoryActivity.class));
            else
            {
                dismissAlertDialog();
                mAlertDialog = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
                    .setTitle(R.string.message_invalid_feature_position)
                    .setPositiveButton(R.string.ok, null)
                    .setOnDismissListener(dialog -> mAlertDialog = null)
                    .show();
            }
            break;
          case Framework.ChoosePositionMode.NONE:
            throw new IllegalStateException("Unexpected Framework.nativeGetChoosePositionMode()");
          }
          closePositionChooser();
        });
    UiUtils.hide(mPointChooser);
  }

  private void refreshSearchToolbar()
  {
    mSearchController.showProgress(false);
    final CharSequence query = SearchEngine.INSTANCE.getQuery();
    if (!TextUtils.isEmpty(query))
    {
      mSearchController.setQuery(query);
      // Close all panels and tool bars (including search) but do not stop search backend
      closeFloatingToolbars(false, false);
      // Do not show the search tool bar if we are planning or navigating
      if (!RoutingController.get().isNavigating() && !RoutingController.get().isPlanning())
      {
        showSearchToolbar();
      }
    }
    else
    {
      closeSearchToolbar(true, true);
    }
  }

  private void showSearchToolbar()
  {
    mSearchController.show();
  }

  public void showPositionChooserForAPI(@Nullable String appName)
  {
    showPositionChooser(Framework.ChoosePositionMode.API, false, false);
    if (!TextUtils.isEmpty(appName))
    {
      setTitle(appName);
      ((TextView) mPointChooser.findViewById(R.id.title)).setText(appName);
    }
  }

  public void showPositionChooserForEditor(boolean isBusiness, boolean applyPosition)
  {
    showPositionChooser(Framework.ChoosePositionMode.EDITOR, isBusiness, applyPosition);
  }

  private void showPositionChooser(@Framework.ChoosePositionMode int mode, boolean isBusiness, boolean applyPosition)
  {
    closeFloatingToolbarsAndPanels(false);
    UiUtils.show(mPointChooser);
    mMapButtonsViewModel.setButtonsHidden(true);
    Framework.nativeSetChoosePositionMode(mode, isBusiness, applyPosition);
    refreshLightStatusBar();
  }

  private void hidePositionChooser()
  {
    UiUtils.hide(mPointChooser);
    @Framework.ChoosePositionMode int mode = Framework.nativeGetChoosePositionMode();
    Framework.nativeSetChoosePositionMode(Framework.ChoosePositionMode.NONE, false, false);
    mMapButtonsViewModel.setButtonsHidden(false);
    refreshLightStatusBar();
    if (mode == Framework.ChoosePositionMode.API)
      finish();
  }

  private void initMap(boolean isLaunchByDeepLink)
  {
    final FragmentManager manager = getSupportFragmentManager();
    mMapFragment = (MapFragment) manager.findFragmentByTag(MapFragment.class.getName());
    if (mMapFragment == null)
    {
      Bundle args = new Bundle();
      args.putBoolean(Map.ARG_LAUNCH_BY_DEEP_LINK, isLaunchByDeepLink);
      final FragmentFactory factory = manager.getFragmentFactory();
      mMapFragment = (MapFragment) factory.instantiate(getClassLoader(), MapFragment.class.getName());
      mMapFragment.setArguments(args);
      manager
          .beginTransaction()
          .replace(R.id.map_fragment_container, mMapFragment, MapFragment.class.getName())
          .commit();
    }

    View container = findViewById(R.id.map_fragment_container);
    if (container != null)
    {
      container.setOnTouchListener(this);
    }
  }

  private void initNavigationButtons()
  {
    initNavigationButtons(mMapButtonsViewModel.getLayoutMode().getValue());
  }

  private void initNavigationButtons(MapButtonsController.LayoutMode layoutMode)
  {
    // Recreate the navigation buttons with the correct layout when it changes
    if (mPreviousMapLayoutMode != layoutMode)
    {
      FragmentTransaction transaction = getSupportFragmentManager()
          .beginTransaction().replace(R.id.map_buttons, new MapButtonsController());
      transaction.commit();
      mPreviousMapLayoutMode = layoutMode;
    }
  }

  @Override
  public void onSearchCanceled()
  {
    closeSearchToolbar(true, true);
  }

  @Override
  public void onMapButtonClick(MapButtonsController.MapButtons button)
  {
    switch (button)
    {
      case zoomIn -> Map.zoomIn();
      case zoomOut -> Map.zoomOut();
      case myPosition ->
      {
        Logger.i(LOCATION_TAG, "The location button pressed");
        // Calls onMyPositionModeChanged(mode + 1).
        LocationState.nativeSwitchToNextMode();
      }
      case toggleMapLayer -> toggleMapLayerBottomSheet();
      case bookmarks -> showBookmarks();
      case search -> showSearch("");
      case menu ->
      {
        closeFloatingPanels();
        showBottomSheet(MAIN_MENU_ID);
      }
      case help -> showHelp();
    }
  }


  private boolean closeBottomSheet(String id)
  {
    MenuBottomSheetFragment bottomSheet =
        (MenuBottomSheetFragment) getSupportFragmentManager().findFragmentByTag(id);
    if (bottomSheet == null || !bottomSheet.isAdded())
      return false;
    bottomSheet.dismiss();
    return true;
  }

  private void showBottomSheet(String id)
  {
    MenuBottomSheetFragment.newInstance(id).show(getSupportFragmentManager(), id);
  }

  private void toggleMapLayerBottomSheet()
  {
    if (!closeBottomSheet(LAYERS_MENU_ID))
      showBottomSheet(LAYERS_MENU_ID);
  }

  /**
   * @return False if the place page was already closed, true otherwise
   */
  public boolean closePlacePage()
  {
    if (mPlacePageViewModel.getMapObject().getValue() == null)
      return false;

    mPlacePageViewModel.setMapObject(null);
    return true;
  }

  /**
   * @return False if the navigation menu was already collapsed or hidden, true otherwise
   */
  public boolean collapseNavMenu()
  {
    if (mNavigationController.isNavMenuCollapsed() || mNavigationController.isNavMenuHidden())
      return false;
    mNavigationController.collapseNavMenu();
    return true;
  }

  /**
   * @return False if the side panel was already closed, true otherwise
   */
  public boolean closeSidePanel()
  {
    if (interceptBackPress())
      return true;

    return removeCurrentFragment(true);
  }

  private void closeAllFloatingPanelsTablet()
  {
    if (!mIsTabletLayout)
      return;

    closePlacePage();
    removeCurrentFragment(true);
  }

  /**
   * @return False if the position chooser was already closed, true otherwise
   */
  private boolean closePositionChooser()
  {
    if (UiUtils.isVisible(mPointChooser))
    {
      hidePositionChooser();
      return true;
    }
    return false;
  }

  /**
   * @param clearText True to clear the search query
   * @param stopSearch True to stop the search engine
   * @return False if the search toolbar was already closed and the search query was empty, true otherwise
   */
  private boolean closeSearchToolbar(boolean clearText, boolean stopSearch)
  {
    if (UiUtils.isVisible(mSearchController.getToolbar()) || !TextUtils.isEmpty(SearchEngine.INSTANCE.getQuery()))
    {
      if (stopSearch)
      {
        mSearchController.cancelSearchApiAndHide(clearText);
        mMapButtonsViewModel.setSearchOption(null);
      }
      else
      {
        mSearchController.hide();
        if (clearText)
        {
          mSearchController.clear();
        }
      }
      return true;
    }
    return false;
  }

  private void closeFloatingToolbarsAndPanels(boolean clearSearchText)
  {
    closeFloatingPanels();
    closeFloatingToolbars(clearSearchText, true);
  }

  public void closeFloatingPanels()
  {
    closeBottomSheet(LAYERS_MENU_ID);
    closeBottomSheet(MAIN_MENU_ID);
    closePlacePage();
  }

  private void closeFloatingToolbars(boolean clearSearchText, boolean stopSearch)
  {
    closePositionChooser();
    closeSearchToolbar(clearSearchText, stopSearch);
  }

  public void startLocationToPoint(final @Nullable MapObject endPoint)
  {
    closeFloatingPanels();
    if (isFullscreen())
      setFullscreen(false);

    if (LocationState.getMode() == LocationState.NOT_FOLLOW_NO_POSITION)
    {
      // Calls onMyPositionModeChanged(PENDING_POSITION).
      LocationState.nativeSwitchToNextMode();
    }

    MapObject startPoint = LocationHelper.from(this).getMyPosition();
    RoutingController.get().prepare(startPoint, endPoint);

    // TODO: check for tablet.
    closePlacePage();
  }

  private void initMainMenu()
  {
    final View menuFrame = findViewById(R.id.menu_frame);
    mMainMenu = new MainMenu(menuFrame, (visible) -> {
      this.updateBottomWidgetsOffset();
      if (visible)
        mPlacePageViewModel.setPlacePageDistanceToTop(menuFrame.getTop());
    });

    if (mIsTabletLayout)
    {
      mPanelAnimator = new PanelAnimator(this);
    }
  }

  private void initOnmapDownloader()
  {
    mOnmapDownloader = new OnmapDownloader(this);
    if (mIsTabletLayout)
      mPanelAnimator.registerListener(mOnmapDownloader);
  }

  @Override
  protected void onSaveInstanceState(@NonNull Bundle outState)
  {
    if (!mIsTabletLayout && RoutingController.get().isPlanning())
      mRoutingPlanInplaceController.onSaveState(outState);

    if (mIsTabletLayout)
    {
      RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
      if (fragment != null)
        fragment.saveRoutingPanelState(outState);
    }

    RoutingController.get().onSaveState();

    if (!isChangingConfigurations())
      RoutingController.get().saveRoute();
    else
      // We no longer need in a saved route if it's a configuration changing: theme switching,
      // orientation changing, etc. Otherwise, the saved route might be restored at undesirable moment.
      RoutingController.get().deleteSavedRoute();

    outState.putBoolean(POWER_SAVE_DISCLAIMER_SHOWN, mPowerSaveDisclaimerShown);
    super.onSaveInstanceState(outState);
  }

  @Override
  protected void onRestoreInstanceState(@NonNull Bundle savedInstanceState)
  {
    super.onRestoreInstanceState(savedInstanceState);
    if (mIsTabletLayout)
    {
      RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
      if (fragment != null)
      {
        fragment.restoreRoutingPanelState(savedInstanceState);
      }
      else if (RoutingController.get().isPlanning())
      {
        mRestoreRoutingPlanFragmentNeeded = true;
        mSavedForTabletState = savedInstanceState;
      }
    }

    if (!mIsTabletLayout && RoutingController.get().isPlanning())
      mRoutingPlanInplaceController.restoreState(savedInstanceState);

    mPowerSaveDisclaimerShown = savedInstanceState.getBoolean(POWER_SAVE_DISCLAIMER_SHOWN, false);
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    super.onActivityResult(requestCode, resultCode, data);

    if (resultCode != Activity.RESULT_OK)
      return;

    if (requestCode == REQ_CODE_DRIVING_OPTIONS)
      rebuildLastRoute();
  }

  private void rebuildLastRoute()
  {
    RoutingController.get().attach(this);
    rebuildLastRouteInternal();
  }

  private void rebuildLastRouteInternal()
  {
    if (mRoutingPlanInplaceController == null)
      return;

    mRoutingPlanInplaceController.hideDrivingOptionsView();
    RoutingController.get().rebuildLastRoute();
  }

  private void onIsolinesStateChanged(@NonNull IsolinesState type)
  {
    if (type != IsolinesState.EXPIREDDATA)
    {
      type.activate(this, findViewById(R.id.coordinator), findViewById(R.id.menu_frame));
      return;
    }

    dismissAlertDialog();
    mAlertDialog = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.downloader_update_maps)
        .setMessage(R.string.isolines_activation_error_dialog)
        .setPositiveButton(R.string.ok, (dialog, which) -> startActivity(new Intent(this, DownloaderActivity.class)))
        .setNegativeButton(R.string.cancel, null)
        .setOnDismissListener(dialog -> mAlertDialog = null)
        .show();
  }

  @Override
  protected void onNewIntent(Intent intent)
  {
    setIntent(intent);
    super.onNewIntent(intent);
    if (isMapRendererActive())
      processIntent();
    if (intent.getAction() != null && intent.getAction()
                                            .equals(TrackRecordingService.STOP_TRACK_RECORDING))
    {
      //closes the bottom sheet in case it is opened to deal with updation of track recording status in bottom sheet.
      closeBottomSheet(MAIN_MENU_ID);
      showTrackSaveDialog();
    }
  }


  private boolean isMapRendererActive()
  {
    return mMapFragment != null && Map.isEngineCreated()
           && mMapFragment.isContextCreated();
  }

  @CallSuper
  @Override
  protected void onResume()
  {
    super.onResume();
    ThemeSwitcher.INSTANCE.restart(isMapRendererActive());
    refreshSearchToolbar();
    setFullscreen(isFullscreen());
    if (Framework.nativeGetChoosePositionMode() != Framework.ChoosePositionMode.NONE)
    {
      UiUtils.show(mPointChooser);
      mMapButtonsViewModel.setButtonsHidden(true);
    }
    if (mOnmapDownloader != null)
      mOnmapDownloader.onResume();

    mNavigationController.refresh();
    refreshLightStatusBar();

    SensorHelper.from(this).addListener(this);
  }

  @Override
  public void recreate()
  {
    // Explicitly destroy surface before activity recreation.
    if (mMapFragment != null)
      mMapFragment.destroySurface();
    super.recreate();
  }

  @Override
  protected void onResumeFragments()
  {
    super.onResumeFragments();
    RoutingController.get().restore();
  }

  @Override
  protected void onPause()
  {
    if (mOnmapDownloader != null)
      mOnmapDownloader.onPause();
    SensorHelper.from(this).removeListener(this);
    dismissLocationErrorDialog();
    dismissAlertDialog();
    super.onPause();
  }

  @Override
  protected void onStart()
  {
    super.onStart();
    Framework.nativePlacePageActivationListener(this);
    BookmarkManager.INSTANCE.addLoadingListener(this);
    RoutingController.get().attach(this);
    IsolinesManager.from(getApplicationContext()).attach(this::onIsolinesStateChanged);
    LocationState.nativeSetListener(this);
    LocationHelper.from(this).addListener(this);
    mSearchController.attach(this);
    Utils.keepScreenOn(Config.isKeepScreenOnEnabled() || RoutingController.get().isNavigating(), getWindow());
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    Framework.nativeRemovePlacePageActivationListener(this);
    BookmarkManager.INSTANCE.removeLoadingListener(this);
    LocationHelper.from(this).removeListener(this);
    if (mDisplayManager.isDeviceDisplayUsed() && !RoutingController.get().isNavigating())
    {
      LocationState.nativeRemoveListener();
      RoutingController.get().detach();
    }
    IsolinesManager.from(getApplicationContext()).detach();
    mSearchController.detach();
    Utils.keepScreenOn(false, getWindow());

    final String backUrl = Framework.nativeGetParsedBackUrl();
    if (!TextUtils.isEmpty(backUrl))
      Utils.openUri(this, Uri.parse(backUrl), null);
  }

  @CallSuper
  @Override
  protected void onSafeDestroy()
  {
    super.onSafeDestroy();
    mLocationPermissionRequest.unregister();
    mLocationPermissionRequest = null;
    mLocationResolutionRequest.unregister();
    mLocationResolutionRequest = null;
    mPostNotificationPermissionRequest.unregister();
    mPostNotificationPermissionRequest = null;
    mPowerSaveSettings.unregister();
    mPowerSaveSettings = null;
    if (mRemoveDisplayListener && !isChangingConfigurations())
      mDisplayManager.removeListener(DisplayType.Device);
  }

  @Override
  public void onBackPressed()
  {
    final RoutingController routingController = RoutingController.get();
    if (!closeBottomSheet(MAIN_MENU_ID) && !closeBottomSheet(LAYERS_MENU_ID) &&
        !collapseNavMenu() && !closePlacePage() && !closeSearchToolbar(true, true) &&
        !closeSidePanel() && !closePositionChooser() &&
        !routingController.resetToPlanningStateIfNavigating() && !routingController.cancel())
    {
      try
      {
        super.onBackPressed();
      }
      catch (IllegalStateException e)
      {
        // Sometimes this can be called after onSaveState() for unknown reason.
      }
    }
  }

  private boolean interceptBackPress()
  {
    final FragmentManager manager = getSupportFragmentManager();
    for (String tag : DOCKED_FRAGMENTS)
    {
      final Fragment fragment = manager.findFragmentByTag(tag);
      if (fragment != null && fragment.isResumed() && fragment instanceof OnBackPressListener)
        return ((OnBackPressListener) fragment).onBackPressed();
    }

    return false;
  }

  private void removeFragmentImmediate(Fragment fragment)
  {
    FragmentManager fm = getSupportFragmentManager();
    if (fm.isDestroyed())
      return;

    fm.beginTransaction()
      .remove(fragment)
      .commitAllowingStateLoss();
    fm.executePendingTransactions();
  }

  private boolean removeCurrentFragment(boolean animate)
  {
    for (String tag : DOCKED_FRAGMENTS)
      if (removeFragment(tag, animate))
        return true;

    return false;
  }

  private boolean removeFragment(String className, boolean animate)
  {
    if (animate && mPanelAnimator == null)
      animate = false;

    final Fragment fragment = getSupportFragmentManager().findFragmentByTag(className);
    if (fragment == null)
      return false;

    if (animate)
      mPanelAnimator.hide(() -> removeFragmentImmediate(fragment));
    else
      removeFragmentImmediate(fragment);

    return true;
  }

  // Called from JNI.
  @Override
  @SuppressWarnings("unused")
  public void onPlacePageActivated(@NonNull PlacePageData data)
  {
    // This will open the place page
    mPlacePageViewModel.setMapObject((MapObject) data);
  }

  // Called from JNI.
  @Override
  @SuppressWarnings("unused")
  public void onPlacePageDeactivated()
  {
    closePlacePage();
  }

  // Called from JNI.
  @Override
  @SuppressWarnings("unused")
  public void onSwitchFullScreenMode()
  {
    if ((mPanelAnimator != null && mPanelAnimator.isVisible()) || UiUtils.isVisible(mSearchController.getToolbar()))
      return;

    setFullscreen(!isFullscreen());
    if (isFullscreen())
    {
      closePlacePage();
      // Show the toast every time so that users don't forget and don't get trapped in the FS mode.
      // TODO(pastk): there are better solutions, see https://github.com/organicmaps/organicmaps/issues/9344
      Toast.makeText(this, R.string.long_tap_toast, Toast.LENGTH_LONG).show();
    }
  }

  private void setFullscreen(boolean isFullscreen)
  {
    if (RoutingController.get().isNavigating()
            || RoutingController.get().isBuilding()
            || RoutingController.get().isPlanning())
      return;

    mMapButtonsViewModel.setButtonsHidden(isFullscreen);
    UiUtils.setFullscreen(this, isFullscreen);
  }

  private boolean isFullscreen()
  {
    // Buttons are hidden in position chooser mode but we are not in fullscreen
    return Boolean.TRUE.equals(mMapButtonsViewModel.getButtonsHidden().getValue()) &&
        Framework.nativeGetChoosePositionMode() == Framework.ChoosePositionMode.NONE;
  }

  @Override
  public boolean onTouch(View view, MotionEvent event)
  {
    return mMapFragment != null && mMapFragment.onTouch(view, event);
  }

  public void customOnNavigateUp()
  {
    if (removeCurrentFragment(true))
    {
      refreshSearchToolbar();
    }
  }

  void updateCompassOffset(int offsetY)
  {
    updateCompassOffset(offsetY, -1);
  }

  void updateCompassOffset(int offsetY, int offsetX)
  {
    if (mMapFragment == null || !mMapFragment.isAdded())
      return;

    mMapFragment.updateCompassOffset(offsetX, offsetY);

    final double north = SensorHelper.from(this).getSavedNorth();
    if (!Double.isNaN(north))
      Map.onCompassUpdated(north, true);
  }

  public void onMapBottomButtonsHeightChange(float height) {
    updateBottomWidgetsOffset();
  }

  public void updateBottomWidgetsOffset()
  {
    updateBottomWidgetsOffset(-1);
  }

  public void updateBottomWidgetsOffset(int offsetX)
  {
    if (mMapFragment == null || !mMapFragment.isAdded())
      return;

    int offsetY = mNavBarHeight;
    final Float bottomButtonHeight = mMapButtonsViewModel.getBottomButtonsHeight().getValue();
    if (bottomButtonHeight != null)
      offsetY = Math.max(offsetY, bottomButtonHeight.intValue() + mNavBarHeight);
    if (mMainMenu != null)
      offsetY = Math.max(offsetY, mMainMenu.getMenuHeight());

    final View navBottomSheetLineFrame = findViewById(R.id.line_frame);
    final View navBottomSheetNavBar = findViewById(R.id.nav_bottom_sheet_nav_bar);
    if (navBottomSheetLineFrame != null)
      offsetY = Math.max(offsetY, navBottomSheetLineFrame.getHeight() + navBottomSheetNavBar.getHeight());

    if (mDisplayManager.isDeviceDisplayUsed())
    {
      mMapFragment.updateBottomWidgetsOffset(offsetX, offsetY);
      mMapFragment.updateMyPositionRoutingOffset(offsetY);
    }
  }

  @Override
  public void updateMenu()
  {
    final RoutingController controller = RoutingController.get();

    if (controller.isNavigating())
    {
      mNavigationController.show(true);
      closeSearchToolbar(false, false);
      mMainMenu.setState(MainMenu.State.NAVIGATION, isFullscreen());
      return;
    }

    if (controller.isBuilt())
    {
      showMainMenu(true);
      return;
    }

    if (controller.isPlanning() || controller.isBuilding() || controller.isErrorEncountered())
    {
      if (showAddStartOrFinishFrame(controller, true))
        return;

      if (controller.isPlanning())
      {
        mMainMenu.setState(MainMenu.State.ROUTE_PREPARE, isFullscreen());
        return;
      }
    }

    mMainMenu.setState(MainMenu.State.MENU, isFullscreen());
  }

  private boolean showAddStartOrFinishFrame(@NonNull RoutingController controller,
                                            boolean showFrame)
  {
    // S - start, F - finish, L - my position
    // -S-F-L -> Start
    // -S-F+L -> Finish
    // -S+F-L -> Start
    // -S+F+L -> Start + Use
    // +S-F-L -> Finish
    // +S-F+L -> Finish
    // +S+F-L -> Hide
    // +S+F+L -> Hide

    MapObject myPosition = LocationHelper.from(this).getMyPosition();

    if (myPosition != null && controller.getEndPoint() == null)
    {
      showAddFinishFrame();
      if (showFrame)
        showMainMenu(true);
      return true;
    }
    if (controller.getStartPoint() == null)
    {
      showAddStartFrame();
      if (showFrame)
        showMainMenu(true);
      return true;
    }
    if (controller.getEndPoint() == null)
    {
      showAddFinishFrame();
      if (showFrame)
        showMainMenu(true);
      return true;
    }

    return false;
  }

  private void showAddStartFrame()
  {
    if (!mIsTabletLayout)
    {
      mRoutingPlanInplaceController.showAddStartFrame();
      return;
    }

    RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
    if (fragment != null)
      fragment.showAddStartFrame();
  }

  private void showAddFinishFrame()
  {
    if (!mIsTabletLayout)
    {
      mRoutingPlanInplaceController.showAddFinishFrame();
      return;
    }

    RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
    if (fragment != null)
      fragment.showAddFinishFrame();
  }

  private void showMainMenu(boolean show)
  {
    mMainMenu.show(show);
  }

  @Override
  public void onRoutingPlanStartAnimate(boolean show)
  {
    // TODO This code section may be called when insets are not yet initialized
    // This is only a workaround to prevent crashes but a proper fix should be implemented
    if (mCurrentWindowInsets == null) {
      return;
    }
    int offset = mCurrentWindowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).top;
    if (show && mRoutingPlanInplaceController != null)
    {
      final int height = mRoutingPlanInplaceController.calcHeight();
      if (height != 0)
        offset = height;
    }
    updateCompassOffset(offset);
  }

  @Override
  public void showRoutePlan(boolean show, @Nullable Runnable completionListener)
  {
    if (show)
    {
      if (mIsTabletLayout)
      {
        replaceFragment(RoutingPlanFragment.class, null, completionListener);
        if (mRestoreRoutingPlanFragmentNeeded && mSavedForTabletState != null)
        {
          RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
          if (fragment != null)
            fragment.restoreRoutingPanelState(mSavedForTabletState);
        }
        showAddStartOrFinishFrame(RoutingController.get(), false);
      }
      else
      {
        mRoutingPlanInplaceController.show(true);
        if (completionListener != null)
          completionListener.run();
      }
    }
    else
    {
      if (mIsTabletLayout && mCurrentWindowInsets != null)
        updateCompassOffset(mCurrentWindowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).top);
      else if (!mIsTabletLayout)
        mRoutingPlanInplaceController.show(false);

      closeAllFloatingPanelsTablet();

      if (completionListener != null)
        completionListener.run();
    }
  }

  @Override
  public void showNavigation(boolean show)
  {
    // TODO:
    // mPlacePage.refreshViews();
    mNavigationController.show(show);
    if (mOnmapDownloader != null)
      mOnmapDownloader.updateState(false);
  }

  @Override
  public void updateBuildProgress(int progress, @Framework.RouterType int router)
  {
    if (mIsTabletLayout)
    {
      RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
      if (fragment != null)
        fragment.updateBuildProgress(progress, router);
    }
    else
    {
      mRoutingPlanInplaceController.updateBuildProgress(progress, router);
    }
  }

  @Override
  public void onStartRouteBuilding()
  {
    if (mRoutingPlanInplaceController == null)
      return;

    mRoutingPlanInplaceController.hideDrivingOptionsView();
  }

  @Override
  public void onNavigationCancelled()
  {
    closeFloatingToolbarsAndPanels(true);
    ThemeSwitcher.INSTANCE.restart(isMapRendererActive());
    if (mRoutingPlanInplaceController == null)
      return;

    mRoutingPlanInplaceController.hideDrivingOptionsView();
    NavigationService.stopService(this);
    mMapButtonsViewModel.setSearchOption(null);
    mMapButtonsViewModel.setLayoutMode(MapButtonsController.LayoutMode.regular);
    refreshLightStatusBar();
    Utils.keepScreenOn(Config.isKeepScreenOnEnabled(), getWindow());
  }

  @Override
  public void onNavigationStarted()
  {
    closeFloatingToolbarsAndPanels(true);
    ThemeSwitcher.INSTANCE.restart(isMapRendererActive());
    mMapButtonsViewModel.setLayoutMode(MapButtonsController.LayoutMode.navigation);
    refreshLightStatusBar();

    // Don't start the background navigation service without fine location.
    if (!LocationUtils.checkFineLocationPermission(this))
    {
      Logger.w(LOCATION_TAG, "Permission ACCESS_FINE_LOCATION is not granted, skipping NavigationService");
      return;
    }

    requestPostNotificationsPermission();
    NavigationService.startForegroundService(this);
    Utils.keepScreenOn(true, getWindow());
  }

  @Override
  public void onPlanningCancelled()
  {
    closeFloatingToolbarsAndPanels(true);
    mMapButtonsViewModel.setLayoutMode(MapButtonsController.LayoutMode.regular);
    refreshLightStatusBar();
  }

  @Override
  public void onPlanningStarted()
  {
    closeFloatingToolbarsAndPanels(true);
    mMapButtonsViewModel.setLayoutMode(MapButtonsController.LayoutMode.planning);
    refreshLightStatusBar();
  }

  @Override
  public void onResetToPlanningState()
  {
    closeFloatingToolbarsAndPanels(true);
    ThemeSwitcher.INSTANCE.restart(isMapRendererActive());
    NavigationService.stopService(this);
    mMapButtonsViewModel.setSearchOption(null);
    mMapButtonsViewModel.setLayoutMode(MapButtonsController.LayoutMode.planning);
    refreshLightStatusBar();
  }

  @Override
  public void onAddedStop()
  {
    closePlacePage();
  }

  @Override
  public void onRemovedStop()
  {
    closePlacePage();
  }

  @Override
  public void onBuiltRoute()
  {
    if (!RoutingController.get().isPlanning())
      return;

    closeSearchToolbar(true, true);
  }

  @Override
  public void onDrivingOptionsWarning()
  {
    if (mRoutingPlanInplaceController == null)
      return;

    mRoutingPlanInplaceController.showDrivingOptionView();
  }

  @Override
  public boolean isSubwayEnabled()
  {
    return SubwayManager.from(this).isEnabled();
  }

  @Override
  public void onCommonBuildError(int lastResultCode, @NonNull String[] lastMissingMaps)
  {
    RoutingErrorDialogFragment fragment = RoutingErrorDialogFragment.create(getSupportFragmentManager().getFragmentFactory(),
                                                                            getApplicationContext(), lastResultCode, lastMissingMaps);
    fragment.show(getSupportFragmentManager(), RoutingErrorDialogFragment.class.getSimpleName());
  }

  @Override
  public void onDrivingOptionsBuildError()
  {
    dismissAlertDialog();
    mAlertDialog = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.unable_to_calc_alert_title)
        .setMessage(R.string.unable_to_calc_alert_subtitle)
        .setPositiveButton(R.string.settings, (dialog, which) -> DrivingOptionsActivity.start(this))
        .setNegativeButton(R.string.cancel, null)
        .setOnDismissListener(dialog -> mAlertDialog = null)
        .show();
  }

  private boolean showRoutingDisclaimer()
  {
    if (Config.isRoutingDisclaimerAccepted())
      return true;

    final StringBuilder builder = new StringBuilder();
    for (int resId : new int[]{R.string.dialog_routing_disclaimer_priority, R.string.dialog_routing_disclaimer_precision,
        R.string.dialog_routing_disclaimer_recommendations, R.string.dialog_routing_disclaimer_borders,
        R.string.dialog_routing_disclaimer_beware})
      builder.append(getString(resId)).append("\n\n");

    dismissAlertDialog();
    mAlertDialog = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.dialog_routing_disclaimer_title)
        .setMessage(builder.toString())
        .setCancelable(false)
        .setNegativeButton(R.string.decline, null)
        .setPositiveButton(R.string.accept, (dlg, which) -> {
          Config.acceptRoutingDisclaimer();
          onRoutingStart();
        })
        .setOnDismissListener(dialog -> mAlertDialog = null)
        .show();

    return false;
  }

  public void openKayakLink(@NonNull String url)
  {
    // The disclaimer is not needed if a user had explicitly opted-in via the setting.
    if (Config.isKayakDisclaimerAccepted() || Config.isKayakDisplayEnabled())
    {
      Utils.openUrl(this, url);
      return;
    }

    dismissAlertDialog();
    mAlertDialog = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.how_to_support_us)
        .setMessage(R.string.dialog_kayak_disclaimer)
        .setCancelable(true)
        .setPositiveButton(R.string.dialog_kayak_button, (dlg, which) -> {
          Config.acceptKayakDisclaimer();
          Utils.openUrl(this, url);
        })
        .setNegativeButton(R.string.cancel, null)
        .setNeutralButton(R.string.dialog_kayak_disable_button, (dlg, which) -> {
          Config.setKayakDisplay(false);
          UiUtils.hide(findViewById(R.id.ll__place_kayak));
        })
        .setOnDismissListener(dialog -> mAlertDialog = null)
        .show();
  }

  private boolean showStartPointNotice()
  {
    final RoutingController controller = RoutingController.get();

    if (showAddStartOrFinishFrame(controller, true))
      return false;

    // Starting and ending points must be non-null, see {@link #showAddStartOrFinishFrame() }.
    final MapObject startPoint = Objects.requireNonNull(controller.getStartPoint());
    if (startPoint.isMyPosition())
      return true;

    final MapObject endPoint = Objects.requireNonNull(controller.getEndPoint());
    final MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.p2p_only_from_current)
        .setMessage(R.string.p2p_reroute_from_current)
        .setCancelable(false)
        .setNegativeButton(R.string.cancel, null)
        .setPositiveButton(R.string.ok, endPoint.isMyPosition() ?
            (dialog, which) -> controller.swapPoints() :
            (dialog, which) -> {
              // The current location may change while this dialog is still shown on the screen.
              final MapObject myPosition = LocationHelper.from(this).getMyPosition();
              controller.setStartPoint(myPosition);
            }
        )
        .setOnDismissListener(dialog -> mAlertDialog = null);
    dismissAlertDialog();
    mAlertDialog = builder.show();
    return false;
  }

  @Override
  public void onMyPositionModeChanged(int newMode)
  {
    Logger.d(LOCATION_TAG, "newMode = " + LocationState.nameOf(newMode));
    mMapButtonsViewModel.setMyPositionMode(newMode);
    RoutingController controller = RoutingController.get();
    if (controller.isPlanning() || controller.isBuilding() || controller.isErrorEncountered())
      showAddStartOrFinishFrame(controller, true);

    final LocationHelper locationHelper = LocationHelper.from(this);

    // Check if location was disabled by the user.
    if (LocationState.getMode() == LocationState.NOT_FOLLOW_NO_POSITION)
    {
      Logger.i(LOCATION_TAG, "Location updates are stopped by the user manually.");
      if (locationHelper.isActive())
        locationHelper.stop();
      return;
    }

    // Check for any location permissions.
    if (!LocationUtils.checkCoarseLocationPermission(this))
    {
      Logger.w(LOCATION_TAG, "Permissions ACCESS_COARSE_LOCATION and ACCESS_FINE_LOCATION are not granted");
      // Calls onMyPositionModeChanged(NOT_FOLLOW_NO_POSITION).
      LocationState.nativeOnLocationError(LocationState.ERROR_DENIED);

      Logger.i(LOCATION_TAG, "Requesting ACCESS_FINE_LOCATION + ACCESS_FINE_LOCATION permissions");
      dismissLocationErrorDialog();
      mLocationPermissionRequest.launch(new String[]{
          ACCESS_COARSE_LOCATION,
          ACCESS_FINE_LOCATION
      });
      return;
    }

    locationHelper.restartWithNewMode();

    if ((newMode == FOLLOW || newMode == FOLLOW_AND_ROTATE) && !LocationUtils.checkFineLocationPermission(this))
    {
      // Try to optimistically request FINE permission for FOLLOW and FOLLOW_AND_ROTATE modes.
      Logger.i(LOCATION_TAG, "Requesting ACCESS_FINE_LOCATION permission for " + LocationState.nameOf(newMode));
      dismissLocationErrorDialog();
      mLocationPermissionRequest.launch(new String[]{
          ACCESS_COARSE_LOCATION,
          ACCESS_FINE_LOCATION
      });
    }
  }

  /**
   * Dismiss the active modal dialog from the screen, if any.
   */
  private void dismissAlertDialog()
  {
    if (mAlertDialog != null && mAlertDialog.isShowing())
      mAlertDialog.dismiss();
    mAlertDialog = null;
  }

  /**
   * Dismiss location error dialog from the screen, if any.
   */
  private void dismissLocationErrorDialog()
  {
    if (mLocationErrorDialog != null && mLocationErrorDialog.isShowing())
      mLocationErrorDialog.dismiss();
    mLocationErrorDialog = null;
  }

  /**
   * Called when location is updated.
   * @param location new location
   */
  @Override
  @UiThread
  public void onLocationUpdated(@NonNull Location location)
  {
    dismissLocationErrorDialog();

    final RoutingController routing = RoutingController.get();
    if (!routing.isNavigating())
      return;

    mNavigationController.update(Framework.nativeGetRouteFollowingInfo());
  }

  @Override
  @UiThread
  public void onLocationUpdateTimeout()
  {
    requestBatterySaverPermission();
  }

  /**
   * Called when compass data is updated.
   * @param north offset from the north
   */
  @Override
  @UiThread
  public void onCompassUpdated(double north)
  {
    Map.onCompassUpdated(north, false);
    mNavigationController.updateNorth();
  }

  @Override
  @UiThread
  public void onCompassCalibrationRecommended()
  {
    Toast.makeText(this, getString(R.string.compass_calibration_recommended),
        Toast.LENGTH_LONG).show();
  }

  @Override
  @UiThread
  public void onCompassCalibrationRequired()
  {
    Toast.makeText(this, getString(R.string.compass_calibration_required),
        Toast.LENGTH_LONG).show();
  }

  /**
   * Request POST_NOTIFICATIONS permission.
   */
  public void requestPostNotificationsPermission()
  {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU ||
        ActivityCompat.checkSelfPermission(this, POST_NOTIFICATIONS) == PERMISSION_GRANTED)
    {
      Logger.i(TAG, "Permissions POST_NOTIFICATIONS is granted");
      return;
    }

    Logger.i(TAG, "Requesting POST_NOTIFICATIONS permission");
    mPostNotificationPermissionRequest.launch(POST_NOTIFICATIONS);
  }

  /**
   * Called on the result of the system location dialog.
   * @param permissions permissions granted or refused.
   */
  @UiThread
  private void onLocationPermissionsResult(java.util.Map<String, Boolean> permissions)
  {
    // Print permissions that have been granted or refused.
    for (java.util.Map.Entry<String, Boolean> entry : permissions.entrySet())
    {
      final String permission = entry.getKey().substring(entry.getKey().lastIndexOf('.') + 1);
      if (entry.getValue())
        Logger.i(LOCATION_TAG, "Permission " + permission + " has been granted");
      else
        Logger.w(LOCATION_TAG, "Permission " + permission + " has been refused");
    }

    boolean requestedForRecording = mLocationPermissionRequestedForRecording;
    mLocationPermissionRequestedForRecording = false;
    if (LocationUtils.checkLocationPermission(this))
    {
      if (LocationState.getMode() == LocationState.NOT_FOLLOW_NO_POSITION)
        LocationState.nativeSwitchToNextMode();

      if (requestedForRecording && LocationUtils.checkFineLocationPermission(this))
        startTrackRecording();

      return;
    }

    Logger.w(LOCATION_TAG, "Permissions ACCESS_COARSE_LOCATION and ACCESS_FINE_LOCATION have been refused");
    // Calls onMyPositionModeChanged(NOT_FOLLOW_NO_POSITION).
    LocationState.nativeOnLocationError(LocationState.ERROR_DENIED);

    if (mLocationErrorDialog != null && mLocationErrorDialog.isShowing())
    {
      Logger.w(LOCATION_TAG, "Don't show 'location denied' error dialog because another dialog is in progress");
      return;
    }

    mLocationErrorDialog = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.enable_location_services)
        .setMessage(R.string.location_is_disabled_long_text)
        .setOnDismissListener(dialog -> mLocationErrorDialog = null)
        .setNegativeButton(R.string.close, null)
        .show();
  }

  /**
   * Called on the result of the POST_NOTIFICATIONS request.
   * @param granted true if permission has been granted.
   */
  @UiThread
  private void onPostNotificationPermissionResult(boolean granted)
  {
    if (granted)
      Logger.i(TAG, "Permission POST_NOTIFICATIONS has been granted");
    else
      Logger.w(TAG, "Permission POST_NOTIFICATIONS has been refused");
  }

  @UiThread
  private void onPowerSaveResult(@NonNull ActivityResult result)
  {
    if (!PowerManagment.isSystemPowerSaveMode(this))
      Logger.i(POWER_MANAGEMENT_TAG, "Power Save mode has been disabled on the device");
    else
      Logger.w(POWER_MANAGEMENT_TAG, "Power Save mode wasn't disabled on the device");
  }

  /**
   * Called by GoogleFusedLocationProvider to request to GPS and/or Wi-Fi.
   * @param pendingIntent an intent to launch.
   */
  @Override
  @UiThread
  public void onLocationResolutionRequired(@NonNull PendingIntent pendingIntent)
  {
    Logger.d(LOCATION_TAG);

    // Cancel our dialog in favor of system dialog.
    dismissLocationErrorDialog();

    // Launch system permission resolution dialog.
    Logger.i(LOCATION_TAG, "Starting location resolution dialog");
    IntentSenderRequest intentSenderRequest = new IntentSenderRequest.Builder(pendingIntent.getIntentSender()).build();
    mLocationResolutionRequest.launch(intentSenderRequest);
  }

  /**
   * Triggered by onLocationResolutionRequired().
   * @param result invocation result.
   */
  @UiThread
  private void onLocationResolutionResult(@NonNull ActivityResult result)
  {
    final int resultCode = result.getResultCode();
    Logger.d(LOCATION_TAG, "resultCode = " + resultCode);

    if (resultCode != Activity.RESULT_OK)
    {
      Logger.w(LOCATION_TAG, "Location resolution has been refused");
      // Calls onMyPositionModeChanged(NOT_FOLLOW_NO_POSITION).
      LocationState.nativeOnLocationError(LocationState.ERROR_GPS_OFF);
      return;
    }

    Logger.i(LOCATION_TAG, "Location resolution has been granted, restarting location");
    if (LocationState.getMode() == LocationState.NOT_FOLLOW_NO_POSITION)
    {
      // Calls onMyPositionModeChanged(PENDING_POSITION).
      LocationState.nativeSwitchToNextMode();
    }
  }

  /**
   * Called by AndroidNativeLocationProvider when no suitable location methods are available.
   */
  @Override
  @UiThread
  public void onLocationDisabled()
  {
    Logger.d(LOCATION_TAG, "settings = " + LocationUtils.areLocationServicesTurnedOn(this));

    // Calls onMyPositionModeChanged(NOT_FOLLOW_NO_POSITION).
    LocationState.nativeOnLocationError(LocationState.ERROR_GPS_OFF);

    if (mLocationErrorDialog != null && mLocationErrorDialog.isShowing())
    {
      Logger.d(LOCATION_TAG, "Don't show 'location disabled' error dialog because another dialog is in progress");
      return;
    }

    final MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.enable_location_services)
        .setMessage(R.string.location_is_disabled_long_text)
        .setOnDismissListener(dialog -> mLocationErrorDialog = null)
        .setNegativeButton(R.string.close, null);
    final Intent intent = Utils.makeSystemLocationSettingIntent(this);
    if (intent != null)
    {
      intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
      intent.addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY);
      intent.addFlags(Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
      builder.setPositiveButton(R.string.location_settings, (dialog, which) -> startActivity(intent));
    }
    mLocationErrorDialog = builder.show();
  }

  @Override
  public void onUseMyPositionAsStart()
  {
    RoutingController.get().setStartPoint(LocationHelper.from(this).getMyPosition());
  }

  @Override
  public void onSearchRoutePoint(@RoutePointInfo.RouteMarkType int pointType)
  {
    RoutingController.get().waitForPoiPick(pointType);
    closeSearchToolbar(true, true);
    showSearch("");
  }

  @Override
  public void onRoutingStart()
  {
    if (!showStartPointNotice())
      return;

    if (!showRoutingDisclaimer())
      return;

    closeFloatingPanels();
    setFullscreen(false);
    RoutingController.get().start();
  }

  private boolean requestBatterySaverPermission()
  {
    if (!PowerManagment.isSystemPowerSaveMode(this))
    {
      Logger.i(POWER_MANAGEMENT_TAG, "Power Save mode is disabled on the device");
      return true;
    }
    Logger.w(POWER_MANAGEMENT_TAG, "Power Save mode is enabled on the device");

    if (mPowerSaveDisclaimerShown)
    {
      Logger.i(POWER_MANAGEMENT_TAG, "The Power Save disclaimer has been already shown in this session");
      return true;
    }

    // TODO (rtsisyk): re-enable this new dialog for all cases after testing on the track recorder.
    if (!TrackRecorder.nativeIsTrackRecordingEnabled())
      return true;

    final Intent intent = PowerManagment.makeSystemPowerSaveSettingIntent(this);
    if (intent == null)
    {
      Logger.w(POWER_MANAGEMENT_TAG, "No known way to launch the system Power Save settings");
      return true;
    }

    dismissAlertDialog();
    final MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.current_location_unknown_error_title)
        .setCancelable(true)
        .setMessage(R.string.power_save_dialog_summary)
        .setNegativeButton(R.string.not_now, (dialog, which) -> {
          Logger.d(POWER_MANAGEMENT_TAG, "The Power Save disclaimer was ignored");
          mPowerSaveDisclaimerShown = true;
        })
        .setOnDismissListener(dialog -> mAlertDialog = null)
        .setPositiveButton(R.string.settings, (dlg, which) -> {
          Logger.d(POWER_MANAGEMENT_TAG, "Launching the system Power Save settings");
          mPowerSaveDisclaimerShown = true;
          mPowerSaveSettings.launch(intent);
        });
    Logger.d(POWER_MANAGEMENT_TAG, "Displaying the Power Save disclaimer");
    mAlertDialog = builder.show();
    return false;
  }

  @Override
  public void onBookmarksFileUnsupported(@NonNull Uri uri)
  {
    dismissAlertDialog();
    mAlertDialog = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.load_kmz_title)
        .setMessage(getString(R.string.unknown_file_type, uri))
        .setPositiveButton(R.string.ok, null)
        .setNegativeButton(R.string.report_a_bug, (dialog, which) -> Utils.sendBugReport(mShareLauncher, this,
            getString(R.string.load_kmz_title), getString(R.string.unknown_file_type, uri)))
        .setOnDismissListener(dialog -> mAlertDialog = null)
        .show();
  }

  @Override
  public void onBookmarksFileDownloadFailed(@NonNull Uri uri, @NonNull String error)
  {
    dismissAlertDialog();
    mAlertDialog = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.load_kmz_title)
        .setMessage(getString(R.string.failed_to_open_file, uri, error))
        .setPositiveButton(R.string.ok, null)
        .setNegativeButton(R.string.report_a_bug, (dialog, which) -> Utils.sendBugReport(mShareLauncher, this,
            getString(R.string.load_kmz_title), getString(R.string.failed_to_open_file, uri, error)))
        .setOnDismissListener(dialog -> mAlertDialog = null)
        .show();
  }

  @Override
  public void onBookmarksFileImportSuccessful()
  {
    Utils.showSnackbar(this, findViewById(R.id.coordinator), R.string.load_kmz_successful);
  }

  @Override
  public void onBookmarksFileImportFailed()
  {
    dismissAlertDialog();
    mAlertDialog = new MaterialAlertDialogBuilder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.load_kmz_title)
        .setMessage(R.string.load_kmz_failed)
        .setPositiveButton(R.string.ok, null)
        .setOnDismissListener(dialog -> mAlertDialog = null)
        .show();
  }

  @Override
  public void onSearchClearClick()
  {
    closeSearchToolbar(true, true);
  }

  @Override
  public void onSearchUpClick(@Nullable String query)
  {
    closeFloatingToolbarsAndPanels(true);
    showSearch(query);
  }

  @Override
  public void onSearchQueryClick(@Nullable String query)
  {
    closeFloatingToolbarsAndPanels(true);
    showSearch(query);
  }

  @Override
  public boolean onKeyUp(int keyCode, KeyEvent event)
  {
    switch (keyCode)
    {
      case KeyEvent.KEYCODE_DPAD_DOWN:
        Map.zoomOut();
        return true;
      case KeyEvent.KEYCODE_DPAD_UP:
        Map.zoomIn();
        return true;
      case KeyEvent.KEYCODE_ESCAPE:
        final Intent currIntent = getIntent();
        final String backUrl = Framework.nativeGetParsedBackUrl();
        if (TextUtils.isEmpty(backUrl) || (currIntent != null && Factory.isStartedForApiResult(currIntent)))
        {
          finish();
          return true;
        }
        return super.onKeyUp(keyCode, event);
      default:
        return super.onKeyUp(keyCode, event);
    }
  }

  public void onAddPlaceOptionSelected()
  {
    closeFloatingPanels();
    showPositionChooserForEditor(false, false);
  }

  public void onDownloadMapsOptionSelected()
  {
    RoutingController.get().cancel();
    closeFloatingPanels();
    showDownloader(false);
  }

  public void onDonateOptionSelected()
  {
    Utils.openUrl(this, mDonatesUrl);
  }

  public void onSettingsOptionSelected()
  {
    Intent intent = new Intent(this, SettingsActivity.class);
    closeFloatingPanels();
    startActivity(intent);
  }

  private boolean startTrackRecording()
  {
    if (!LocationUtils.checkFineLocationPermission(this))
    {
      Logger.i(TAG, "Location permission not granted");
      // This variable is a simple hack to re initiate the flow
      // according to action of user. Calling it hack because we are avoiding
      // creation of new methods by using this variable.
      mLocationPermissionRequestedForRecording = true;
      mLocationPermissionRequest.launch(new String[] { ACCESS_COARSE_LOCATION, ACCESS_FINE_LOCATION });
      return false;
    }

    requestPostNotificationsPermission();

    Toast.makeText(this, R.string.track_recording, Toast.LENGTH_SHORT).show();
    TrackRecordingService.startForegroundService(getApplicationContext());
    mMapButtonsViewModel.setTrackRecorderState(true);
    return true;
  }

  private void stopTrackRecording()
  {
    TrackRecordingService.stopService(getApplicationContext());
    mMapButtonsViewModel.setTrackRecorderState(false);
  }

  private void saveAndStopTrackRecording()
  {
    if (!TrackRecorder.nativeIsTrackRecordingEmpty())
      TrackRecorder.nativeSaveTrackRecordingWithName("");
    TrackRecorder.nativeStopTrackRecording();
    stopTrackRecording();
  }

  private void onTrackRecordingOptionSelected()
  {
    if (TrackRecorder.nativeIsTrackRecordingEnabled())
      showTrackSaveDialog();
    else
      startTrackRecording();
  }

  private void showTrackSaveDialog()
  {
    if (TrackRecorder.nativeIsTrackRecordingEmpty())
    {
      Toast.makeText(this, R.string.track_recording_toast_nothing_to_save, Toast.LENGTH_SHORT)
           .show();
      stopTrackRecording();
      return;
    }

    dismissAlertDialog();
    mAlertDialog = new StackedButtonsDialog.Builder(this)
        .setTitle(R.string.track_recording_alert_title)
        .setCancelable(false)
        // Negative/Positive/Neutral doesn't do not have the usual meaning here.
        .setPositiveButton(R.string.continue_recording, (dialog, which) -> {
          mAlertDialog = null;
        })
        .setNeutralButton(R.string.stop_without_saving, (dialog, which) -> {
          stopTrackRecording();
          mAlertDialog = null;
        })
        .setNegativeButton(R.string.save, (dialog, which) -> {
          saveAndStopTrackRecording();
          mAlertDialog = null;
        })
        .build();
    mAlertDialog.show();
  }

  public void onShareLocationOptionSelected()
  {
    closeFloatingPanels();
    shareMyLocation();
  }

  @Override
  @Nullable
  public ArrayList<MenuBottomSheetItem> getMenuBottomSheetItems(String id)
  {
    if (id.equals(MAIN_MENU_ID))
    {
      ArrayList<MenuBottomSheetItem> items = new ArrayList<>();
      items.add(new MenuBottomSheetItem(R.string.placepage_add_place_button, R.drawable.ic_plus, this::onAddPlaceOptionSelected));
      items.add(new MenuBottomSheetItem(
          R.string.download_maps,
          R.drawable.ic_download,
          getDownloadMapsCounter(),
          this::onDownloadMapsOptionSelected
      ));
      mDonatesUrl = Config.getDonateUrl(getApplicationContext());
      if (!TextUtils.isEmpty(mDonatesUrl))
        items.add(new MenuBottomSheetItem(R.string.donate, R.drawable.ic_donate, this::onDonateOptionSelected));
      items.add(new MenuBottomSheetItem(R.string.settings, R.drawable.ic_settings, this::onSettingsOptionSelected));
      items.add(new MenuBottomSheetItem(R.string.start_track_recording, R.drawable.ic_track_recording_off, -1, this::onTrackRecordingOptionSelected));
      items.add(new MenuBottomSheetItem(R.string.share_my_location, R.drawable.ic_share, this::onShareLocationOptionSelected));
      return items;
    }
    return null;
  }

  @Override
  @Nullable
  public Fragment getMenuBottomSheetFragment(String id)
  {
    if (id.equals(LAYERS_MENU_ID))
      return new ToggleMapLayerFragment();
    return null;
  }

  @Override
  public void onPlacePageRequestToggleRouteSettings(@NonNull RoadType roadType)
  {
    closePlacePage();
    RoutingOptions.addOption(roadType);
    rebuildLastRouteInternal();
  }

  @Override
  public void onTrimMemory(int level)
  {
    super.onTrimMemory(level);
    Logger.d(TAG, "trim memory, level = " + level);
    if (level >= TRIM_MEMORY_RUNNING_LOW)
      Framework.nativeMemoryWarning();
  }
}
