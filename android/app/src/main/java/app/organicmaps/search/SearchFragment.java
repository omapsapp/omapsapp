package app.organicmaps.search;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.location.Location;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.viewpager.widget.ViewPager;

import com.google.android.material.appbar.AppBarLayout;
import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton;
import com.google.android.material.tabs.TabLayout;
import app.organicmaps.sdk.Framework;
import app.organicmaps.MwmActivity;
import app.organicmaps.R;
import app.organicmaps.base.BaseMwmFragment;
import app.organicmaps.sdk.bookmarks.data.FeatureId;
import app.organicmaps.sdk.bookmarks.data.MapObject;
import app.organicmaps.downloader.CountrySuggestFragment;
import app.organicmaps.sdk.downloader.MapManager;
import app.organicmaps.sdk.location.LocationHelper;
import app.organicmaps.sdk.location.LocationListener;
import app.organicmaps.sdk.routing.RoutingController;
import app.organicmaps.sdk.search.NativeSearchListener;
import app.organicmaps.sdk.search.SearchEngine;
import app.organicmaps.sdk.search.SearchRecents;
import app.organicmaps.sdk.search.SearchResult;
import app.organicmaps.widget.PlaceholderView;
import app.organicmaps.widget.SearchToolbarController;
import app.organicmaps.util.SharedPropertiesUtils;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.sdk.util.Config;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class SearchFragment extends BaseMwmFragment
                         implements NativeSearchListener,
                                    CategoriesAdapter.CategoriesUiListener
{
  private long mLastQueryTimestamp;
  @NonNull
  private final List<HiddenCommand> mHiddenCommands = new ArrayList<>();

  private static class LastPosition
  {
    double lat;
    double lon;
    boolean valid;

    public void set(double lat, double lon)
    {
      this.lat = lat;
      this.lon = lon;
      valid = true;
    }
  }

  private class ToolbarController extends SearchToolbarController
  {
    public ToolbarController(View root)
    {
      super(root, SearchFragment.this.requireActivity());
    }

    @Override
    protected void onTextChanged(String query)
    {
      if (!isAdded())
        return;

      if (TextUtils.isEmpty(query))
      {
        mSearchAdapter.clear();
        stopSearch();
        return;
      }

      if (tryRecognizeHiddenCommand(query))
      {
        mSearchAdapter.clear();
        stopSearch();
        closeSearch();
        return;
      }

      runSearch();
    }

    @Override
    protected boolean onStartSearchClick()
    {
      deactivate();
      return true;
    }

    @Override
    protected int getVoiceInputPrompt()
    {
      return R.string.search_map;
    }

    @Override
    @SuppressWarnings("deprecation") // https://github.com/organicmaps/organicmaps/issues/3630
    protected void startVoiceRecognition(Intent intent, int code)
    {
      startActivityForResult(intent, code);
    }

    @Override
    protected boolean supportsVoiceSearch()
    {
      return true;
    }

    @Override
    public void onUpClick()
    {
      if (!onBackPressed())
        super.onUpClick();
    }

    @Override
    public void clear()
    {
      super.clear();
    }
  }

  private View mResultsFrame;
  private PlaceholderView mResultsPlaceholder;
  private ExtendedFloatingActionButton mShowOnMapFab;

  @NonNull
  private SearchToolbarController mToolbarController;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private SearchAdapter mSearchAdapter;

  private final List<RecyclerView> mAttachedRecyclers = new ArrayList<>();
  private final RecyclerView.OnScrollListener mRecyclerListener = new RecyclerView.OnScrollListener()
  {
    @Override
    public void onScrollStateChanged(@NonNull RecyclerView recyclerView, int newState)
    {
      if (newState == RecyclerView.SCROLL_STATE_DRAGGING)
        mToolbarController.deactivate();
    }
  };

  private final LastPosition mLastPosition = new LastPosition();
  private boolean mSearchRunning;
  private String mInitialQuery;
  @Nullable
  private String mInitialLocale;
  private boolean mInitialSearchOnMap = false;

  private final LocationListener mLocationListener = new LocationListener()
  {
    @Override
    public void onLocationUpdated(Location location)
    {
      mLastPosition.set(location.getLatitude(), location.getLongitude());

      if (!TextUtils.isEmpty(getQuery()))
        mSearchAdapter.notifyDataSetChanged();
    }
  };

  private static boolean doShowDownloadSuggest()
  {
    return (MapManager.nativeGetDownloadedCount() == 0 && !MapManager.nativeIsDownloading());
  }

  private void showDownloadSuggest()
  {
    final FragmentManager fm = getChildFragmentManager();
    final String fragmentName = CountrySuggestFragment.class.getName();
    Fragment fragment = fm.findFragmentByTag(fragmentName);

    if (fragment == null || fragment.isDetached() || fragment.isRemoving())
    {
      fragment = fm.getFragmentFactory().instantiate(requireActivity().getClassLoader(), fragmentName);
      fm.beginTransaction()
        .add(R.id.download_suggest_frame, fragment, fragmentName)
        .commit();
    }
  }

  private void hideDownloadSuggest()
  {
    if (!isAdded())
      return;

    final FragmentManager manager = getChildFragmentManager();
    final Fragment fragment = manager.findFragmentByTag(CountrySuggestFragment.class.getName());
    if (fragment != null && !fragment.isDetached() && !fragment.isRemoving())
      manager.beginTransaction()
             .remove(fragment)
             .commitAllowingStateLoss();
  }

  private void updateFrames()
  {
    final boolean hasQuery = mToolbarController.hasQuery();
    Toolbar toolbar = mToolbarController.getToolbar();
    AppBarLayout.LayoutParams lp = (AppBarLayout.LayoutParams) toolbar.getLayoutParams();
    lp.setScrollFlags(hasQuery ? AppBarLayout.LayoutParams.SCROLL_FLAG_ENTER_ALWAYS
                                 | AppBarLayout.LayoutParams.SCROLL_FLAG_SCROLL : 0);
    toolbar.setLayoutParams(lp);

    UiUtils.showIf(hasQuery, mResultsFrame);
    UiUtils.showIf(hasQuery, mShowOnMapFab);
    if (hasQuery)
      hideDownloadSuggest();
    else if (doShowDownloadSuggest())
      showDownloadSuggest();
    else
      hideDownloadSuggest();
  }

  private void updateResultsPlaceholder()
  {
    final boolean show = !mSearchRunning
                         && mSearchAdapter.getItemCount() == 0
                         && mToolbarController.hasQuery();

    UiUtils.showIf(show, mResultsPlaceholder);
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_search, container, false);
  }

  @CallSuper
  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    mSearchAdapter = new SearchAdapter(this);
    readArguments();

    ViewGroup root = (ViewGroup) view;
    View mTabFrame = root.findViewById(R.id.tab_frame);
    ViewPager pager = mTabFrame.findViewById(R.id.pages);

    mToolbarController = new ToolbarController(view);
    TabLayout tabLayout = root.findViewById(R.id.tabs);

    if (Config.isSearchHistoryEnabled())
      tabLayout.setVisibility(View.VISIBLE);
    else
      tabLayout.setVisibility(View.GONE);

    final TabAdapter tabAdapter = new TabAdapter(getChildFragmentManager(), pager, tabLayout);

    mResultsFrame = root.findViewById(R.id.results_frame);
    RecyclerView mResults = mResultsFrame.findViewById(R.id.recycler);
    setRecyclerScrollListener(mResults);
    mResultsPlaceholder = mResultsFrame.findViewById(R.id.placeholder);
    mResultsPlaceholder.setContent(R.string.search_not_found, R.string.search_not_found_query);
    mSearchAdapter.registerAdapterDataObserver(new RecyclerView.AdapterDataObserver()

  {
      @Override
      public void onChanged()
      {
        updateResultsPlaceholder();
      }
    });
    mShowOnMapFab = root.findViewById(R.id.show_on_map_fab);
    mShowOnMapFab.setOnClickListener(v -> showAllResultsOnMap());

    mResults.setLayoutManager(new LinearLayoutManager(view.getContext()));
    mResults.setAdapter(mSearchAdapter);

    updateFrames();
    updateResultsPlaceholder();

    mToolbarController.activate();

    SearchEngine.INSTANCE.addListener(this);

    if (SearchRecents.getSize() == 0 && Config.isSearchHistoryEnabled())
      pager.setCurrentItem(TabAdapter.Tab.CATEGORIES.ordinal());

    tabAdapter.setTabSelectedListener(tab -> mToolbarController.deactivate());

    if (mInitialSearchOnMap)
      showAllResultsOnMap();
  }

  @Override
  public void onStart()
  {
    super.onStart();
    mToolbarController.attach(requireActivity());
  }

  public void onResume()
  {
    super.onResume();
    LocationHelper.from(requireContext()).addListener(mLocationListener);
    if (mInitialQuery != null) {
      setQuery(mInitialQuery, false);
      mInitialQuery = null;
    }
  }

  @Override
  public void onPause()
  {
    LocationHelper.from(requireContext()).removeListener(mLocationListener);
    super.onPause();
  }

  @Override
  public void onStop()
  {
    super.onStop();
    mToolbarController.detach();
  }

  @Override
  public void onDestroy()
  {
    for (RecyclerView v : mAttachedRecyclers)
      v.removeOnScrollListener(mRecyclerListener);

    mAttachedRecyclers.clear();
    SearchEngine.INSTANCE.removeListener(this);
    super.onDestroy();
  }

  private String getQuery() { return mToolbarController.getQuery(); }
  private boolean isCategory() { return mToolbarController.isCategory(); }
  void setQuery(String text, boolean isCategory) { mToolbarController.setQuery(text, isCategory); }

  private void readArguments()
  {
    final Bundle arguments = getArguments();
    if (arguments == null)
      return;

    mInitialQuery = arguments.getString(SearchActivity.EXTRA_QUERY);
    mInitialLocale = arguments.getString(SearchActivity.EXTRA_LOCALE);
    mInitialSearchOnMap = arguments.getBoolean(SearchActivity.EXTRA_SEARCH_ON_MAP);
  }

  private boolean tryRecognizeHiddenCommand(@NonNull String query)
  {
    for(HiddenCommand command: getHiddenCommands())
    {
      if (command.execute(query))
        return true;
    }

    return false;
  }

  @NonNull
  private List<HiddenCommand> getHiddenCommands()
  {
    if (mHiddenCommands.isEmpty())
    {
      mHiddenCommands.addAll(
          Arrays.asList(new BadStorageCommand("?emulateBadStorage", requireContext()),
                        new JavaCrashCommand("?emulateJavaCrash"),
                        new NativeCrashCommand("?emulateNativeCrash"),
                        new PushTokenCommand("?pushToken")));
    }

    return mHiddenCommands;
  }

  void showSingleResultOnMap(@NonNull SearchResult result, int resultIndex)
  {
    final String query = getQuery();
    if (Config.isSearchHistoryEnabled())
      SearchRecents.add(query, requireContext());
    SearchEngine.INSTANCE.cancel();
    SearchEngine.INSTANCE.setQuery(query);

    if (RoutingController.get().isWaitingPoiPick())
    {
      final String subtitle = (result.description != null) ? result.description.localizedFeatureType : "";
      final String title = TextUtils.isEmpty(result.name) ? subtitle : result.name;

      final MapObject point = MapObject.createMapObject(FeatureId.EMPTY, MapObject.SEARCH,
          title, subtitle, result.lat, result.lon);
      RoutingController.get().onPoiSelected(point);
    }
    else
    {
      SearchEngine.INSTANCE.showResult(resultIndex);
    }

    mToolbarController.deactivate();

    if (requireActivity() instanceof SearchActivity)
      Utils.navigateToParent(requireActivity());
  }

  void showAllResultsOnMap()
  {
    // The previous search should be cancelled before the new one is started, since previous search
    // results are no longer needed.
    SearchEngine.INSTANCE.cancel();

    final String query = getQuery();
    if (Config.isSearchHistoryEnabled())
      SearchRecents.add(query, requireContext());
    mLastQueryTimestamp = System.nanoTime();

    SearchEngine.INSTANCE.searchInteractive(
        query, isCategory(), !TextUtils.isEmpty(mInitialLocale)
               ? mInitialLocale : app.organicmaps.sdk.util.Language.getKeyboardLocale(requireContext()),
        mLastQueryTimestamp, false /* isMapAndTable */);

    SearchEngine.INSTANCE.setQuery(query);
    Utils.navigateToParent(requireActivity());
  }

  private void onSearchEnd()
  {
    if (mSearchRunning && isAdded())
      updateSearchView();
  }

  private void updateSearchView()
  {
    mSearchRunning = false;
    mToolbarController.showProgress(false);
    updateFrames();
    updateResultsPlaceholder();
  }

  private void stopSearch()
  {
    SearchEngine.INSTANCE.cancel();
    updateSearchView();
  }

  private boolean isTabletSearch()
  {
    // TODO @yunitsky Implement more elegant solution.
    return requireActivity() instanceof MwmActivity;
  }

  private void runSearch()
  {
    // The previous search should be cancelled before the new one is started, since previous search
    // results are no longer needed.
    SearchEngine.INSTANCE.cancel();

    mLastQueryTimestamp = System.nanoTime();
    if (isTabletSearch())
    {
      SearchEngine.INSTANCE.searchInteractive(requireContext(), getQuery(), isCategory(),
              mLastQueryTimestamp, true /* isMapAndTable */);
    }
    else
    {
      if (!SearchEngine.INSTANCE.search(requireContext(), getQuery(), isCategory(),
              mLastQueryTimestamp, mLastPosition.valid, mLastPosition.lat, mLastPosition.lon))
      {
        return;
      }
    }

    mSearchRunning = true;
    mToolbarController.showProgress(true);

    updateFrames();
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @Override
  public void onResultsUpdate(@NonNull SearchResult[] results, long timestamp)
  {
    if (!isAdded() || !mToolbarController.hasQuery())
      return;

    refreshSearchResults(results);
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @Override
  public void onResultsEnd(long timestamp)
  {
    onSearchEnd();
  }

  @Override
  public void onSearchCategorySelected(@Nullable String category)
  {
    mToolbarController.setQuery(category, true);
  }

  private void refreshSearchResults(@NonNull SearchResult[] results)
  {
    mSearchRunning = true;
    updateFrames();
    mSearchAdapter.refreshData(results);
    mToolbarController.showProgress(true);
  }

  @Override
  @SuppressWarnings("deprecation") // https://github.com/organicmaps/organicmaps/issues/3630
  public void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    super.onActivityResult(requestCode, resultCode, data);
    mToolbarController.onActivityResult(requestCode, resultCode, data);
  }

  @Override
  public boolean onBackPressed()
  {
    if (mToolbarController.hasQuery())
    {
      mToolbarController.clear();
      return true;
    }

    boolean isSearchActivity = requireActivity() instanceof SearchActivity;
    mToolbarController.deactivate();
    if (RoutingController.get().isWaitingPoiPick())
    {
      RoutingController.get().onPoiSelected(null);
      if (isSearchActivity)
        closeSearch();
      return !isSearchActivity;
    }

    if (isSearchActivity)
      closeSearch();
    return isSearchActivity;
  }

  private void closeSearch()
  {
    final Activity activity  = requireActivity();
    activity.finish();
  }

  public void setRecyclerScrollListener(RecyclerView recycler)
  {
    recycler.addOnScrollListener(mRecyclerListener);
    mAttachedRecyclers.add(recycler);
  }

  @NonNull
  public SearchToolbarController requireController()
  {
    return mToolbarController;
  }

  private static class BadStorageCommand extends HiddenCommand.BaseHiddenCommand
  {
    @NonNull
    Context mContext;

    BadStorageCommand(@NonNull String command, @NonNull Context context)
    {
      super(command);
      mContext = context;
    }

    @Override
    void executeInternal()
    {
      SharedPropertiesUtils.setShouldShowEmulateBadStorageSetting(mContext, true);
    }
  }

  private static class JavaCrashCommand extends HiddenCommand.BaseHiddenCommand
  {
    JavaCrashCommand(@NonNull String command)
    {
      super(command);
    }

    @Override
    void executeInternal()
    {
      throw new RuntimeException("Diagnostic java crash!");
    }
  }

  private static class NativeCrashCommand extends HiddenCommand.BaseHiddenCommand
  {
    NativeCrashCommand(@NonNull String command)
    {
      super(command);
    }

    @Override
    void executeInternal()
    {
      Framework.nativeMakeCrash();
    }
  }

  private static class PushTokenCommand extends HiddenCommand.BaseHiddenCommand
  {
    PushTokenCommand(@NonNull String command)
    {
      super(command);
    }

    @Override
    void executeInternal()
    {
    }
  }
}
