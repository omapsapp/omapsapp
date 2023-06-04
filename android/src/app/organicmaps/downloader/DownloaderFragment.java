package app.organicmaps.downloader;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.R;
import app.organicmaps.base.BaseMwmRecyclerFragment;
import app.organicmaps.base.OnBackPressListener;
import app.organicmaps.search.NativeMapSearchListener;
import app.organicmaps.search.SearchEngine;
import app.organicmaps.widget.PlaceholderView;
import app.organicmaps.util.bottomsheet.MenuBottomSheetFragment;
import app.organicmaps.util.bottomsheet.MenuBottomSheetItem;
import com.google.android.material.floatingactionbutton.FloatingActionButton;


import java.util.ArrayList;
import java.util.List;

public class DownloaderFragment extends BaseMwmRecyclerFragment<DownloaderAdapter>
                             implements OnBackPressListener,
                                        MenuBottomSheetFragment.MenuBottomSheetInterface
{
  private DownloaderToolbarController mToolbarController;

  private BottomPanel mBottomPanel;
  @Nullable
  private DownloaderAdapter mAdapter;

  private long mCurrentSearch;
  private boolean mSearchRunning;

  private int mSubscriberSlot;
  
  private FloatingActionButton mFab;

  private final RecyclerView.OnScrollListener mScrollListener = new RecyclerView.OnScrollListener() {
    @Override
    public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
      if (newState == RecyclerView.SCROLL_STATE_DRAGGING) {
        mToolbarController.deactivate();
        // User started scrolling, hide the FAB
        hideFab();
      } else if (newState == RecyclerView.SCROLL_STATE_IDLE) {
        // User stopped scrolling, show the FAB
        showFab();
      }
    }
  };
   private boolean shouldShowFab() {
    // Determine whether the FAB should be shown on the current screen
    return mAdapter != null && mAdapter.getItemCount() > 0;
  }
  private void showFab() {
    if (mFab != null && shouldShowFab()) {
      mFab.show();
    }
  }

  private void hideFab() {
    if (mFab != null) {
      mFab.hide();
    }
  }


  private final NativeMapSearchListener mSearchListener = new NativeMapSearchListener()
  {
    @Override
    public void onMapSearchResults(Result[] results, long timestamp, boolean isLast)
    {
      if (!mSearchRunning || timestamp != mCurrentSearch)
        return;

      List<CountryItem> rs = new ArrayList<>();
      for (Result result : results)
      {
        CountryItem item = CountryItem.fill(result.countryId);
        item.searchResultName = result.matchedString;
        rs.add(item);
      }

      if (mAdapter != null)
        mAdapter.setSearchResultsMode(rs, mToolbarController.getQuery());

      if (isLast)
        onSearchEnd();
    }
  };

  boolean shouldShowSearch()
  {
    return CountryItem.isRoot(getCurrentRoot());
  }

  void startSearch()
  {
    mSearchRunning = true;
    mCurrentSearch = System.nanoTime();
    SearchEngine.searchMaps(requireContext(), mToolbarController.getQuery(), mCurrentSearch);
    mToolbarController.showProgress(true);
  }

  void clearSearchQuery()
  {
    mToolbarController.clear();
  }

  void cancelSearch()
  {
    if (mAdapter == null || !mAdapter.isSearchResultsMode())
      return;

    mAdapter.resetSearchResultsMode();
    onSearchEnd();
  }

  private void onSearchEnd()
  {
    mSearchRunning = false;
    mToolbarController.showProgress(false);
    update();
  }

  void update()
  {
    mToolbarController.update();
    mBottomPanel.update();
    
    boolean showFab = shouldShowFab();
    if (showFab) {
      showFab();
    } else {
      hideFab();
    }
  }

  @CallSuper
  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    requireActivity().getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_HIDDEN);
  }

  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    mFab = view.findViewById(R.id.fab);
    
    mSubscriberSlot = MapManager.nativeSubscribe(new MapManager.StorageCallback()
    {
      @Override
      public void onStatusChanged(List<MapManager.StorageCallbackData> data)
      {
        if (isAdded())
          update();
      }

      @Override
      public void onProgress(String countryId, long localSize, long remoteSize) {}
    });

    SearchEngine.INSTANCE.addMapListener(mSearchListener);

    getRecyclerView().addOnScrollListener(mScrollListener);
    if (mAdapter != null)
    {
      mAdapter.refreshData();
      mAdapter.attach();
    }

    mBottomPanel = new BottomPanel(this, view);
    mToolbarController = new DownloaderToolbarController(view, requireActivity(), this);

    update();
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    if (mAdapter != null)
      mAdapter.detach();
    mAdapter = null;

    if (mSubscriberSlot != 0)
    {
      MapManager.nativeUnsubscribe(mSubscriberSlot);
      mSubscriberSlot = 0;
    }

    SearchEngine.INSTANCE.removeMapListener(mSearchListener);
    
    if (getRecyclerView() != null) {
      getRecyclerView().removeOnScrollListener(mScrollListener);
    }
    mFab = null;
  }

  @Override
  public void onDestroy()
  {
    super.onDestroy();
    if (getRecyclerView() != null)
      getRecyclerView().removeOnScrollListener(mScrollListener);
  }

  @Override
  public boolean onBackPressed()
  {
    if (mToolbarController.hasQuery())
    {
      mToolbarController.clear();
      return true;
    }

    return mAdapter != null && mAdapter.goUpwards();
  }

  @Override
  protected int getLayoutRes()
  {
    return R.layout.fragment_downloader;
  }

  @NonNull
  @Override
  protected DownloaderAdapter createAdapter()
  {
    if (mAdapter == null)
      mAdapter = new DownloaderAdapter(this);

    return mAdapter;
  }

  @Override
  @SuppressWarnings("deprecation") // https://github.com/organicmaps/organicmaps/issues/3630
  public void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    super.onActivityResult(requestCode, resultCode, data);
    mToolbarController.onActivityResult(requestCode, resultCode, data);
  }

  @NonNull
  @Override
  public DownloaderAdapter getAdapter()
  {
    return mAdapter;
  }

  @NonNull
  String getCurrentRoot()
  {
    return mAdapter != null ? mAdapter.getCurrentRootId() : "";
  }

  @Override
  protected void setupPlaceholder(@Nullable PlaceholderView placeholder)
  {
    if (placeholder == null)
      return;

    if (mAdapter != null && mAdapter.isSearchResultsMode()){
      placeholder.setContent(R.string.search_not_found, R.string.search_not_found_query);
      hideFab();
    }else{
      placeholder.setContent(R.string.downloader_no_downloaded_maps_title, R.string.downloader_no_downloaded_maps_message);
      showFab();
    }
  }

  @Override
  @Nullable
  public ArrayList<MenuBottomSheetItem> getMenuBottomSheetItems(String id)
  {
    return mAdapter != null ? mAdapter.getMenuItems() : null;
  }
}
Signed-off-by: Zhu Pengming <TomABdh@users.noreply.github.com>
