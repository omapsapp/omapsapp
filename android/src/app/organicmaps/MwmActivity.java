package app.organicmaps;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.location.Location;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.TextView;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import app.organicmaps.Framework.PlacePageActivationListener;
import app.organicmaps.api.Const;
import app.organicmaps.background.Notifier;
import app.organicmaps.base.BaseMwmFragmentActivity;
import app.organicmaps.base.CustomNavigateUpListener;
import app.organicmaps.base.NoConnectionListener;
import app.organicmaps.base.OnBackPressListener;
import app.organicmaps.bookmarks.BookmarkCategoriesActivity;
import app.organicmaps.bookmarks.data.BookmarkInfo;
import app.organicmaps.bookmarks.data.BookmarkManager;
import app.organicmaps.bookmarks.data.MapObject;
import app.organicmaps.bookmarks.data.Track;
import app.organicmaps.downloader.DownloaderActivity;
import app.organicmaps.downloader.DownloaderFragment;
import app.organicmaps.downloader.MapManager;
import app.organicmaps.downloader.OnmapDownloader;
import app.organicmaps.downloader.UpdateInfo;
import app.organicmaps.editor.Editor;
import app.organicmaps.editor.EditorActivity;
import app.organicmaps.editor.EditorHostFragment;
import app.organicmaps.editor.FeatureCategoryActivity;
import app.organicmaps.editor.ReportFragment;
import app.organicmaps.help.HelpActivity;
import app.organicmaps.intent.Factory;
import app.organicmaps.intent.MapTask;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.location.LocationListener;
import app.organicmaps.location.LocationState;
import app.organicmaps.maplayer.MapButtonsController;
import app.organicmaps.maplayer.Mode;
import app.organicmaps.maplayer.ToggleMapLayerFragment;
import app.organicmaps.maplayer.isolines.IsolinesManager;
import app.organicmaps.maplayer.isolines.IsolinesState;
import app.organicmaps.maplayer.subway.SubwayManager;
import app.organicmaps.routing.NavigationController;
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
import app.organicmaps.sound.TtsPlayer;
import app.organicmaps.util.log.Logger;
import app.organicmaps.widget.menu.MainMenu;
import app.organicmaps.widget.placepage.PlacePageController;
import app.organicmaps.widget.placepage.PlacePageData;
import app.organicmaps.widget.placepage.PlacePageFactory;
import app.organicmaps.widget.placepage.RoutingModeListener;
import app.organicmaps.util.Config;
import app.organicmaps.util.Counters;
import app.organicmaps.util.LocationUtils;
import app.organicmaps.util.SharingUtils;
import app.organicmaps.util.ThemeSwitcher;
import app.organicmaps.util.ThemeUtils;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.util.bottomsheet.MenuBottomSheetFragment;
import app.organicmaps.util.bottomsheet.MenuBottomSheetItem;

import java.util.ArrayList;
import java.util.Objects;
import java.util.Stack;

import static app.organicmaps.util.concurrency.UiThread.runLater;
import static app.organicmaps.widget.placepage.PlacePageButtons.PLACEPAGE_MORE_MENU_ID;

public class MwmActivity extends BaseMwmFragmentActivity
    implements PlacePageActivationListener,
               View.OnTouchListener,
               MapRenderingListener,
               CustomNavigateUpListener,
               RoutingController.Container,
               LocationListener,
               LocationState.ModeChangeListener,
               RoutingPlanInplaceController.RoutingPlanListener,
               RoutingBottomMenuListener,
               BookmarkManager.BookmarksLoadingListener,
               FloatingSearchToolbarController.SearchToolbarListener,
               PlacePageController.SlideListener,
               RoutingModeListener,
               NoConnectionListener,
               MenuBottomSheetFragment.MenuBottomSheetInterfaceWithHeader,
               ToggleMapLayerFragment.LayerItemClickListener
{
  private static final String TAG = MwmActivity.class.getSimpleName();

  public static final String EXTRA_TASK = "map_task";
  public static final String EXTRA_LAUNCH_BY_DEEP_LINK = "launch_by_deep_link";
  public static final String EXTRA_BACK_URL = "backurl";
  private static final String EXTRA_CONSUMED = "mwm.extra.intent.processed";

  private static final String[] DOCKED_FRAGMENTS = { SearchFragment.class.getName(),
                                                     DownloaderFragment.class.getName(),
                                                     RoutingPlanFragment.class.getName(),
                                                     EditorHostFragment.class.getName(),
                                                     ReportFragment.class.getName() };

  private static final String EXTRA_CURRENT_LAYOUT_MODE = "CURRENT_LAYOUT_MODE";
  private static final String EXTRA_IS_FULLSCREEN = "IS_FULLSCREEN";
  public static final int REQ_CODE_ERROR_DRIVING_OPTIONS_DIALOG = 5;
  public static final int REQ_CODE_DRIVING_OPTIONS = 6;
  private static final int REQ_CODE_ISOLINES_ERROR = 8;

  public static final String ERROR_DRIVING_OPTIONS_DIALOG_TAG = "error_driving_options_dialog_tag";
  private static final String ISOLINES_ERROR_DIALOG_TAG = "isolines_dialog_tag";

  private static final String MAIN_MENU_ID = "MAIN_MENU_BOTTOM_SHEET";
  private static final String LAYERS_MENU_ID = "LAYERS_MENU_BOTTOM_SHEET";

  // Map tasks that we run AFTER rendering initialized
  private final Stack<MapTask> mTasks = new Stack<>();

  @Nullable
  private MapFragment mMapFragment;

  private View mPointChooser;
  private Toolbar mPointChooserToolbar;
  enum PointChooserMode
  {
    NONE,
    EDITOR,
    API
  }
  @NonNull
  private PointChooserMode mPointChooserMode = PointChooserMode.NONE;

  private RoutingPlanInplaceController mRoutingPlanInplaceController;

  private NavigationController mNavigationController;

  private MainMenu mMainMenu;

  private PanelAnimator mPanelAnimator;
  @Nullable
  private OnmapDownloader mOnmapDownloader;

  @Nullable
  private MapButtonsController mMapButtonsController;

  private boolean mIsTabletLayout;
  private boolean mIsFullscreen;
  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private FloatingSearchToolbarController mSearchController;

  private boolean mRestoreRoutingPlanFragmentNeeded;
  @Nullable
  private Bundle mSavedForTabletState;
  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private PlacePageController mPlacePageController;
  private MapButtonsController.LayoutMode mCurrentLayoutMode;

  private String mDonatesUrl;

  private int mNavBarHeight;

  private WindowInsets mCurrentWindowInsets;

  public interface LeftAnimationTrackListener
  {
    void onTrackStarted(boolean collapsed);

    void onTrackFinished(boolean collapsed);

    void onTrackLeftAnimation(float offset);
  }

  public static Intent createShowMapIntent(@NonNull Context context, @Nullable String countryId)
  {
    return new Intent(context, DownloadResourcesLegacyActivity.class)
        .putExtra(DownloadResourcesLegacyActivity.EXTRA_COUNTRY, countryId);
  }

  @Override
  public void onRenderingCreated()
  {
    checkMeasurementSystem();
  }

  @Override
  public void onRenderingRestored()
  {
    runTasks();
  }

  @Override
  public void onRenderingInitializationFinished()
  {
    runTasks();
  }

  private void runTasks()
  {
    while (!mTasks.isEmpty())
      mTasks.pop().run(this);
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

  public boolean containsFragment(@NonNull Class<? extends Fragment> fragmentClass)
  {
    return mIsTabletLayout && getFragment(fragmentClass) != null;
  }

  private void showBookmarks()
  {
    closeFloatingPanels();
    BookmarkCategoriesActivity.start(this);
  }

  public void showHelp()
  {
    Intent intent = new Intent(requireActivity(), HelpActivity.class);
    startActivity(intent);
  }

  public void showSearch(String query)
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
    final Location loc = LocationHelper.INSTANCE.getSavedLocation();
    if (loc != null)
    {
      SharingUtils.shareLocation(this, loc);
      return;
    }

    new AlertDialog.Builder(MwmActivity.this, R.style.MwmTheme_AlertDialog)
        .setMessage(R.string.unknown_current_position)
        .setCancelable(true)
        .setPositiveButton(R.string.ok, null)
        .show();
  }

  @Override
  public void showDownloader(boolean openDownloaded)
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
  public int getThemeResourceId(@NonNull String theme)
  {
    Context context = getApplicationContext();

    if (ThemeUtils.isDefaultTheme(context, theme))
      return R.style.MwmTheme_MainActivity;

    if (ThemeUtils.isNightTheme(context, theme))
      return R.style.MwmTheme_Night_MainActivity;

    return super.getThemeResourceId(theme);
  }

  @SuppressLint("InlinedApi")
  @CallSuper
  @Override
  protected void onSafeCreate(@Nullable Bundle savedInstanceState)
  {
    super.onSafeCreate(savedInstanceState);
    if (savedInstanceState != null)
    {
      mCurrentLayoutMode = MapButtonsController.LayoutMode.values()[savedInstanceState.getInt(EXTRA_CURRENT_LAYOUT_MODE)];
      mIsFullscreen = savedInstanceState.getBoolean(EXTRA_IS_FULLSCREEN);
    }
    else
    {
      mCurrentLayoutMode = MapButtonsController.LayoutMode.regular;
    }
    mIsTabletLayout = getResources().getBoolean(R.bool.tabletLayout);

    if (!mIsTabletLayout)
      getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);

    setContentView(R.layout.activity_map);
    UiUtils.setupTransparentStatusBar(this);

    mPlacePageController = PlacePageFactory.createCompositePlacePageController(
        this, this);
    mPlacePageController.initialize(this);
    mPlacePageController.onActivityCreated(this, savedInstanceState);

    mSearchController = new FloatingSearchToolbarController(this, this);
    mSearchController.getToolbar()
                     .getViewTreeObserver();

    boolean isLaunchByDeepLink = getIntent().getBooleanExtra(EXTRA_LAUNCH_BY_DEEP_LINK, false);
    initViews(isLaunchByDeepLink);
    updateViewsInsets();

    boolean isConsumed = savedInstanceState == null && processIntent(getIntent());
    boolean isFirstLaunch = Counters.isFirstLaunch(this);
    // If the map activity is launched by any incoming intent (deeplink, update maps event, etc)
    // or it's the first launch (onboarding) we haven't to try restoring the route,
    // showing the tips, etc.
    if (isConsumed || isFirstLaunch)
      return;

    if (savedInstanceState == null && RoutingController.get().hasSavedRoute())
      addTask(new Factory.RestoreRouteTask());
  }

  private void refreshLightStatusBar()
  {
    UiUtils.setLightStatusBar(this, !(
        ThemeUtils.isNightTheme(this)
        || RoutingController.get().isPlanning()
        || Framework.nativeIsInChoosePositionMode()
    ));
  }

  private void updateViewsInsets()
  {
    mPointChooser.setOnApplyWindowInsetsListener((view, windowInsets) -> {
      UiUtils.setViewInsetsPaddingBottom(mPointChooser, windowInsets);
      UiUtils.extendViewWithStatusBar(mPointChooserToolbar, windowInsets);

      mNavBarHeight = mIsFullscreen ? 0 : windowInsets.getSystemWindowInsetBottom();
      // For the first loading, set compass top margin to status bar size
      if (mCurrentWindowInsets == null)
        adjustCompass(windowInsets.getSystemWindowInsetTop(), windowInsets.getSystemWindowInsetRight());
      else
        adjustCompass(-1, windowInsets.getSystemWindowInsetRight());
      refreshLightStatusBar();
      adjustBottomWidgets(windowInsets.getSystemWindowInsetLeft());
      mCurrentWindowInsets = windowInsets;
      return windowInsets;
    });
  }

  private int getDownloadMapsCounter()
  {
    UpdateInfo info = MapManager.nativeGetUpdateInfo(null);
    return info == null ? 0 : info.filesCount;
  }

  @Override
  public void onNoConnectionError()
  {
    new AlertDialog.Builder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.common_check_internet_connection_dialog_title)
        .setMessage(R.string.common_check_internet_connection_dialog)
        .setPositiveButton(R.string.ok, null)
        .show();
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

    mNavigationController = new NavigationController(this, mMapButtonsController, v -> onSettingsOptionSelected());
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
    mPointChooserToolbar.setNavigationOnClickListener(v -> {
      closePositionChooser();
      if (mPointChooserMode == PointChooserMode.API)
        finish();
    });
    mPointChooser.findViewById(R.id.done).setOnClickListener(
        v ->
        {
          switch (mPointChooserMode)
          {
          case API:
            final Intent apiResult = new Intent();
            final double[] center = Framework.nativeGetScreenRectCenter();
            apiResult.putExtra(Const.EXTRA_POINT_LAT, center[0]);
            apiResult.putExtra(Const.EXTRA_POINT_LON, center[1]);
            apiResult.putExtra(Const.EXTRA_ZOOM_LEVEL, Framework.nativeGetDrawScale());
            setResult(Activity.RESULT_OK, apiResult);
            finish();
            break;
          case EDITOR:
            if (Framework.nativeIsDownloadedMapAtScreenCenter())
              startActivity(new Intent(MwmActivity.this, FeatureCategoryActivity.class));
            else
            {
                new AlertDialog.Builder(this, R.style.MwmTheme_AlertDialog)
                    .setTitle(R.string.message_invalid_feature_position)
                    .setPositiveButton(R.string.ok, null)
                    .show();
            }
            break;
          case NONE:
            throw new IllegalStateException("Unexpected mPositionChooserMode");
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

  public void showPositionChooserForAPI(String appName)
  {
    showPositionChooser(PointChooserMode.API, false, false);
    if (!TextUtils.isEmpty(appName))
    {
      setTitle(appName);
      ((TextView) mPointChooser.findViewById(R.id.title)).setText(appName);
    }
  }

  public void showPositionChooserForEditor(boolean isBusiness, boolean applyPosition)
  {
    showPositionChooser(PointChooserMode.EDITOR, isBusiness, applyPosition);
  }

  private void showPositionChooser(PointChooserMode mode, boolean isBusiness, boolean applyPosition)
  {
    mPointChooserMode = mode;
    closeFloatingToolbarsAndPanels(false);
    UiUtils.show(mPointChooser);
    mMapButtonsController.showMapButtons(false);
    Framework.nativeTurnOnChoosePositionMode(isBusiness, applyPosition);
    refreshLightStatusBar();
  }

  private void hidePositionChooser()
  {
    UiUtils.hide(mPointChooser);
    Framework.nativeTurnOffChoosePositionMode();
    mMapButtonsController.showMapButtons(true);
    if (mPointChooserMode == PointChooserMode.API)
      finish();
    mPointChooserMode = PointChooserMode.NONE;
    refreshLightStatusBar();
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

  public boolean isMapAttached()
  {
    return mMapFragment != null && mMapFragment.isAdded();
  }


  private void initNavigationButtons()
  {
    initNavigationButtons(mCurrentLayoutMode);
  }

  private void initNavigationButtons(MapButtonsController.LayoutMode layoutMode)
  {
    if (mMapButtonsController == null || mMapButtonsController.getLayoutMode() != layoutMode)
    {
      mCurrentLayoutMode = layoutMode;

      mMapButtonsController = new MapButtonsController();
      mMapButtonsController.init(
          layoutMode,
          LocationHelper.INSTANCE.getMyPositionMode(),
          this::onMapButtonClick,
          (v) -> closeSearchToolbar(true, true),
          mPlacePageController,
          this::adjustBottomWidgets);


      FragmentTransaction transaction = getSupportFragmentManager()
          .beginTransaction().replace(R.id.map_buttons, mMapButtonsController);
      transaction.commit();
    }
  }

  void onMapButtonClick(MapButtonsController.MapButtons button)
  {
    switch (button)
    {
      case zoomIn:
        // TODO: fix
        // Map.zoomIn();
        break;
      case zoomOut:
        // TODO: fix
        // Map.zoomOut();
        break;
      case myPosition:
        LocationState.nativeSwitchToNextMode();
        if (!LocationHelper.INSTANCE.isActive())
          LocationHelper.INSTANCE.start();
        break;
      case toggleMapLayer:
        toggleMapLayerBottomSheet();
        break;
      case bookmarks:
        showBookmarks();
        break;
      case search:
        showSearch();
        break;
      case menu:
        closeFloatingPanels();
        showBottomSheet(MAIN_MENU_ID);
        break;
      case help:
        showHelp();
        break;
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
    if (mPlacePageController.isClosed())
      return false;

    mPlacePageController.close(true);
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
        mMapButtonsController.resetSearch();
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
    if (!LocationHelper.INSTANCE.isActive())
      LocationHelper.INSTANCE.start();

    MapObject startPoint = LocationHelper.INSTANCE.getMyPosition();
    RoutingController.get().prepare(startPoint, endPoint);

    // TODO: check for tablet.
    closePlacePage();
  }

  private void initMainMenu()
  {
    mMainMenu = new MainMenu(findViewById(R.id.menu_frame), this::adjustBottomWidgets);

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
    mPlacePageController.onSave(outState);
    if (!mIsTabletLayout && RoutingController.get().isPlanning())
      mRoutingPlanInplaceController.onSaveState(outState);

    if (mIsTabletLayout)
    {
      RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
      if (fragment != null)
        fragment.saveRoutingPanelState(outState);
    }

    mNavigationController.onActivitySaveInstanceState(this, outState);

    RoutingController.get().onSaveState();
    outState.putInt(EXTRA_CURRENT_LAYOUT_MODE, mCurrentLayoutMode.ordinal());
    outState.putBoolean(EXTRA_IS_FULLSCREEN, mIsFullscreen);

    if (!isChangingConfigurations())
      RoutingController.get().saveRoute();
    else
      // We no longer need in a saved route if it's a configuration changing: theme switching,
      // orientation changing, etc. Otherwise, the saved route might be restored at undesirable moment.
      RoutingController.get().deleteSavedRoute();

    super.onSaveInstanceState(outState);
  }

  @Override
  protected void onRestoreInstanceState(@NonNull Bundle savedInstanceState)
  {
    super.onRestoreInstanceState(savedInstanceState);
    mPlacePageController.onRestore(savedInstanceState);
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

    mNavigationController.onRestoreState(savedInstanceState, this);
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    super.onActivityResult(requestCode, resultCode, data);

    if (resultCode != Activity.RESULT_OK)
      return;

    switch (requestCode)
    {
      case REQ_CODE_DRIVING_OPTIONS:
        rebuildLastRoute();
        break;
    }
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

  @Override
  public void toggleRouteSettings(@NonNull RoadType roadType)
  {
    closePlacePage();
    RoutingOptions.addOption(roadType);
    rebuildLastRouteInternal();
  }

  private void onIsolinesStateChanged(@NonNull IsolinesState type)
  {
    if (type != IsolinesState.EXPIREDDATA)
    {
      type.activate(this, findViewById(R.id.coordinator), findViewById(R.id.menu_frame));
      return;
    }

    new AlertDialog.Builder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.downloader_update_maps)
        .setMessage(R.string.isolines_activation_error_dialog)
        .setPositiveButton(R.string.ok, (dialog, which) -> startActivity(new Intent(this, DownloaderActivity.class)))
        .setNegativeButton(R.string.cancel, null)
        .show();
  }

  @Override
  protected void onNewIntent(Intent intent)
  {
    super.onNewIntent(intent);
    setIntent(intent);
    processIntent(intent);
  }

  private boolean processIntent(Intent intent)
  {
    if (intent == null)
      return false;

    final Notifier notifier = Notifier.from(getApplication());
    notifier.processNotificationExtras(intent);

    if (intent.hasExtra(EXTRA_TASK))
    {
      addTask(intent);
      return true;
    }

    return false;
  }

  private void addTask(Intent intent)
  {
    if (intent != null &&
        !intent.getBooleanExtra(EXTRA_CONSUMED, false) &&
        intent.hasExtra(EXTRA_TASK) &&
        ((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) == 0))
    {
      final MapTask mapTask = (MapTask) intent.getSerializableExtra(EXTRA_TASK);
      mTasks.add(mapTask);
      intent.removeExtra(EXTRA_TASK);

      if (isMapRendererActive())
        runTasks();

      // mark intent as consumed
      intent.putExtra(EXTRA_CONSUMED, true);
    }
  }

  private boolean isMapRendererActive()
  {
    return mMapFragment != null && Map.isEngineCreated()
           && mMapFragment.isContextCreated();
  }

  private void addTask(MapTask task)
  {
    mTasks.add(task);
    if (isMapRendererActive())
      runTasks();
  }

  @CallSuper
  @Override
  protected void onResume()
  {
    super.onResume();
    refreshSearchToolbar();
    setFullscreen(mIsFullscreen);
    if (Framework.nativeIsInChoosePositionMode())
    {
      UiUtils.show(mPointChooser);
      mMapButtonsController.showMapButtons(false);
    }
    if (mOnmapDownloader != null)
      mOnmapDownloader.onResume();

    mNavigationController.onActivityResumed(this);
    mPlacePageController.onActivityResumed(this);
    refreshLightStatusBar();
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
    if (!RoutingController.get().isNavigating())
      TtsPlayer.INSTANCE.stop();
    if (mOnmapDownloader != null)
      mOnmapDownloader.onPause();
    mPlacePageController.onActivityPaused(this);
    mNavigationController.onActivityPaused(this);
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
    LocationHelper.INSTANCE.attach(this);
    LocationState.nativeSetListener(this);
    LocationHelper.INSTANCE.addListener(this);
    onMyPositionModeChanged(LocationState.nativeGetMode());
    mPlacePageController.onActivityStarted(this);
    mSearchController.attach(this);
    if (!Config.isScreenSleepEnabled())
      Utils.keepScreenOn(true, getWindow());
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    Framework.nativeRemovePlacePageActivationListener();
    BookmarkManager.INSTANCE.removeLoadingListener(this);
    LocationHelper.INSTANCE.removeListener(this);
    LocationState.nativeRemoveListener();
    LocationHelper.INSTANCE.detach();
    RoutingController.get().detach();
    mPlacePageController.onActivityStopped(this);
    IsolinesManager.from(getApplicationContext()).detach();
    mSearchController.detach();
    Utils.keepScreenOn(false, getWindow());
  }

  @CallSuper
  @Override
  protected void onSafeDestroy()
  {
    super.onSafeDestroy();
    mNavigationController.destroy();
    //TrafficManager.INSTANCE.detachAll();
    mPlacePageController.destroy();
  }

  @Override
  public void onBackPressed()
  {
    RoutingController routingController = RoutingController.get();
    if (!closeBottomSheet(MAIN_MENU_ID) && !closeBottomSheet(LAYERS_MENU_ID) && !collapseNavMenu() &&
        !closePlacePage() &&!closeSearchToolbar(true, true) &&
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
  public void onPlacePageActivated(@NonNull PlacePageData data)
  {
    setFullscreen(false);

    mPlacePageController.openFor(data);
  }

  // Called from JNI.
  @Override
  public void onPlacePageDeactivated(boolean switchFullScreenMode)
  {
    if (switchFullScreenMode)
    {
      if ((mPanelAnimator != null && mPanelAnimator.isVisible()) ||
          UiUtils.isVisible(mSearchController.getToolbar()))
        return;

      setFullscreen(!mIsFullscreen);
    }
    else
    {
      closePlacePage();
    }
  }

  private void setFullscreen(boolean isFullscreen)
  {
    if (RoutingController.get().isNavigating()
        || RoutingController.get().isBuilding()
        || RoutingController.get().isPlanning())
      return;

    mIsFullscreen = isFullscreen;
    mMapButtonsController.showMapButtons(!isFullscreen);
    UiUtils.setFullscreen(this, isFullscreen);
  }

  @Override
  public void onPlacePageSlide(int top)
  {
    mMapButtonsController.move(top);
  }

  @Override
  public boolean onTouch(View view, MotionEvent event)
  {
    return mMapFragment != null && mMapFragment.onTouch(view, event);
  }

  @Override
  public void customOnNavigateUp()
  {
    if (removeCurrentFragment(true))
    {
      refreshSearchToolbar();
    }
  }

  void adjustCompass(int offsetY)
  {
    adjustCompass(offsetY, -1);
  }

  void adjustCompass(int offsetY, int offsetX)
  {
    if (mMapFragment == null || !mMapFragment.isAdded())
      return;

    mMapFragment.adjustCompass(offsetX, offsetY);

    final double north = LocationHelper.INSTANCE.getSavedNorth();
    if (!Double.isNaN(north))
      Map.onCompassUpdated(north, true);
  }

  public void adjustBottomWidgets()
  {
    adjustBottomWidgets(-1);
  }

  public void adjustBottomWidgets(int offsetX)
  {
    if (mMapFragment == null || !mMapFragment.isAdded())
      return;

    int mapButtonsHeight = 0;
    int mainMenuHeight = 0;
    if (mMapButtonsController != null)
      mapButtonsHeight = (int) mMapButtonsController.getBottomButtonsHeight() + mNavBarHeight;
    if (mMainMenu != null)
      mainMenuHeight = mMainMenu.getMenuHeight();

    final int y = Math.max(Math.max(mapButtonsHeight, mainMenuHeight), mNavBarHeight);

    mMapFragment.adjustBottomWidgets(offsetX, y);
  }

  @Override
  @NonNull
  public AppCompatActivity requireActivity()
  {
    return this;
  }

  @Override
  public void showSearch()
  {
    showSearch("");
  }

  @Override
  public void updateMenu()
  {
    boolean isVisible = adjustMenuLineFrameVisibility();
    if (!isVisible)
      return;

    if (RoutingController.get().isNavigating())
    {
      mNavigationController.show(true);
      closeSearchToolbar(false, false);
      mMainMenu.setState(MainMenu.State.NAVIGATION, mIsFullscreen);
      return;
    }

    if (RoutingController.get().isPlanning())
    {
      mMainMenu.setState(MainMenu.State.ROUTE_PREPARE, mIsFullscreen);
      return;
    }

    mMainMenu.setState(MainMenu.State.MENU, mIsFullscreen);
  }

  private boolean adjustMenuLineFrameVisibility()
  {
    final RoutingController controller = RoutingController.get();

    if (controller.isBuilt())
    {
      showMainMenu(true);
      return true;
    }

    if (controller.isPlanning() || controller.isBuilding() || controller.isErrorEncountered())
    {
      if (showAddStartOrFinishFrame(controller, true))
      {
        return true;
      }

      showMainMenu(false);
      return false;
    }

    hideRoutingActionFrame();
    showMainMenu(true);
    return true;
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

    MapObject myPosition = LocationHelper.INSTANCE.getMyPosition();

    if (myPosition != null && !controller.hasEndPoint())
    {
      showAddFinishFrame();
      if (showFrame)
        showMainMenu(true);
      return true;
    }
    if (!controller.hasStartPoint())
    {
      showAddStartFrame();
      if (showFrame)
        showMainMenu(true);
      return true;
    }
    if (!controller.hasEndPoint())
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

  private void hideRoutingActionFrame()
  {
    if (!mIsTabletLayout)
    {
      mRoutingPlanInplaceController.hideActionFrame();
      return;
    }

    RoutingPlanFragment fragment = (RoutingPlanFragment) getFragment(RoutingPlanFragment.class);
    if (fragment != null)
      fragment.hideActionFrame();
  }

  private void showMainMenu(boolean show)
  {
    mMainMenu.show(show);
  }

  @Override
  public void onRoutingPlanStartAnimate(boolean show)
  {
    int offset = mCurrentWindowInsets.getSystemWindowInsetTop();
    if (show && mRoutingPlanInplaceController != null)
    {
      final int height = mRoutingPlanInplaceController.calcHeight();
      if (height != 0)
        offset = height;
    }
    adjustCompassAndTraffic(offset);
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
      if (mIsTabletLayout)
      {
        adjustCompassAndTraffic(mCurrentWindowInsets.getSystemWindowInsetTop());
      }
      else
      {
        mRoutingPlanInplaceController.show(false);
      }

      closeAllFloatingPanelsTablet();

      if (completionListener != null)
        completionListener.run();
    }
  }

  private void adjustCompassAndTraffic(final int offsetY)
  {
    addTask(new MapTask()
    {
      private static final long serialVersionUID = 9177064181621376624L;

      @Override
      public boolean run(@NonNull MwmActivity target)
      {
        adjustCompass(offsetY);
        return true;
      }
    });
  }

  @Override
  public void showNavigation(boolean show)
  {
    // TODO:
//    mPlacePage.refreshViews();
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
    mNavigationController.stop(this);
    initNavigationButtons(MapButtonsController.LayoutMode.regular);
    refreshLightStatusBar();
  }

  @Override
  public void onNavigationStarted()
  {
    closeFloatingToolbarsAndPanels(true);
    ThemeSwitcher.INSTANCE.restart(isMapRendererActive());
    mNavigationController.start(this);
    initNavigationButtons(MapButtonsController.LayoutMode.navigation);
    refreshLightStatusBar();
  }

  @Override
  public void onPlanningCancelled()
  {
    closeFloatingToolbarsAndPanels(true);
    initNavigationButtons(MapButtonsController.LayoutMode.regular);
    refreshLightStatusBar();
  }

  @Override
  public void onPlanningStarted()
  {
    closeFloatingToolbarsAndPanels(true);
    initNavigationButtons(MapButtonsController.LayoutMode.planning);
    refreshLightStatusBar();
  }

  @Override
  public void onResetToPlanningState()
  {
    closeFloatingToolbarsAndPanels(true);
    ThemeSwitcher.INSTANCE.restart(isMapRendererActive());
    mNavigationController.stop(this);
    initNavigationButtons(MapButtonsController.LayoutMode.planning);
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
    new AlertDialog.Builder(this, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.unable_to_calc_alert_title)
        .setMessage(R.string.unable_to_calc_alert_subtitle)
        .setPositiveButton(R.string.settings, (dialog, which) -> DrivingOptionsActivity.start(this))
        .setNegativeButton(R.string.cancel, null)
        .show();
  }

  @Override
  public void onMyPositionModeChanged(int newMode)
  {
    Logger.d(TAG, "location newMode = " + newMode);
    mMapButtonsController.updateNavMyPositionButton(newMode);
    RoutingController controller = RoutingController.get();
    if (controller.isPlanning())
      showAddStartOrFinishFrame(controller, true);
  }

  @Override
  public void onLocationUpdated(@NonNull Location location)
  {
    final RoutingController routing = RoutingController.get();
    if (!routing.isNavigating())
      return;

    mNavigationController.update(Framework.nativeGetRouteFollowingInfo());

    TtsPlayer.INSTANCE.playTurnNotifications(getApplicationContext());

    // TODO: consider to create callback mechanism to transfer 'ROUTE_IS_FINISHED' event from
    // the core to the platform code (https://github.com/organicmaps/organicmaps/issues/3589),
    // because calling the native method 'nativeIsRouteFinished'
    // too often can result in poor UI performance.
    if (Framework.nativeIsRouteFinished())
    {
      routing.cancel();
      // Restart location with a new interval.
      LocationHelper.INSTANCE.restart();
    }
  }

  @Override
  public void onCompassUpdated(double north)
  {
    Map.onCompassUpdated(north, false);
    mNavigationController.updateNorth();
  }

  @Override
  public void onUseMyPositionAsStart()
  {
    RoutingController.get().setStartPoint(LocationHelper.INSTANCE.getMyPosition());
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
    closeFloatingPanels();
    RoutingController.get().start();
  }

  @Override
  public void onBookmarksLoadingStarted()
  {
    // Do nothing
  }

  @Override
  public void onBookmarksLoadingFinished()
  {
    // Do nothing
  }

  @Override
  public void onBookmarksFileLoaded(boolean success)
  {
    Utils.showSnackbar(this, findViewById(R.id.coordinator), findViewById(R.id.menu_frame),
                        success ? R.string.load_kmz_successful : R.string.load_kmz_failed);
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
        // TODO: fix
        // Map.zoomOut();
        return true;
      case KeyEvent.KEYCODE_DPAD_UP:
        // TODO: fix
        // Map.zoomIn();
        return true;
      case KeyEvent.KEYCODE_ESCAPE:
        Intent currIntent = getIntent();
        if (currIntent == null || !currIntent.hasExtra(EXTRA_BACK_URL))
          return super.onKeyUp(keyCode, event);

        String backUrl = currIntent.getStringExtra(EXTRA_BACK_URL);
        if (TextUtils.isEmpty(backUrl))
          return super.onKeyUp(keyCode, event);

        Uri back_uri = Uri.parse(backUrl);
        if (back_uri == null)
          return super.onKeyUp(keyCode, event);

        return Utils.openUri(this, back_uri);
      default:
        return super.onKeyUp(keyCode, event);
    }
  }

  public void showTrackOnMap(long trackId)
  {
    Track track = BookmarkManager.INSTANCE.getTrack(trackId);
    Objects.requireNonNull(track);
    Framework.nativeShowTrackRect(trackId);
  }

  public void showBookmarkOnMap(long bookmarkId)
  {
    BookmarkInfo info = BookmarkManager.INSTANCE.getBookmarkInfo(bookmarkId);
    Objects.requireNonNull(info);
    BookmarkManager.INSTANCE.showBookmarkOnMap(bookmarkId);
  }

  public void showBookmarkCategoryOnMap(long categoryId)
  {
    BookmarkManager.INSTANCE.showBookmarkCategoryOnMap(categoryId);
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
    Intent intent = new Intent(requireActivity(), SettingsActivity.class);
    closeFloatingPanels();
    requireActivity().startActivity(intent);
  }

  public void onShareLocationOptionSelected()
  {
    closeFloatingPanels();
    shareMyLocation();
  }

  @Override
  public void onLayerItemClick(@NonNull Mode mode)
  {
    closeFloatingPanels();
    if (mMapButtonsController != null)
      mMapButtonsController.toggleMapLayer(mode);
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
      mDonatesUrl = Config.getDonateUrl();
      if (!TextUtils.isEmpty(mDonatesUrl))
        items.add(new MenuBottomSheetItem(R.string.donate, R.drawable.ic_donate, this::onDonateOptionSelected));
      items.add(new MenuBottomSheetItem(R.string.settings, R.drawable.ic_settings, this::onSettingsOptionSelected));
      items.add(new MenuBottomSheetItem(R.string.share_my_location, R.drawable.ic_share, this::onShareLocationOptionSelected));
      return items;
    }
    else if (id.equals(PLACEPAGE_MORE_MENU_ID))
      return mPlacePageController.getMenuBottomSheetItems();
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
}
