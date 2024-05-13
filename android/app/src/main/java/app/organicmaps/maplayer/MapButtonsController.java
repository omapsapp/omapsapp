package app.organicmaps.maplayer;

import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.core.view.ViewCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProvider;
import app.organicmaps.MwmActivity;
import app.organicmaps.R;
import app.organicmaps.sdk.downloader.MapManager;
import app.organicmaps.sdk.downloader.UpdateInfo;
import app.organicmaps.routing.RoutingController;
import app.organicmaps.sdk.util.Config;
import app.organicmaps.util.ThemeUtils;
import app.organicmaps.util.UiUtils;
import app.organicmaps.widget.menu.MyPositionButton;
import app.organicmaps.widget.placepage.PlacePageViewModel;
import com.google.android.material.badge.BadgeDrawable;
import com.google.android.material.badge.BadgeUtils;
import com.google.android.material.floatingactionbutton.FloatingActionButton;

import java.util.HashMap;
import java.util.Map;

public class MapButtonsController extends Fragment
{
  Map<MapButtons, View> mButtonsMap;
  private View mFrame;
  private View mInnerLeftButtonsFrame;
  private View mInnerRightButtonsFrame;
  @Nullable
  private View mBottomButtonsFrame;
  @Nullable
  private FloatingActionButton mToggleMapLayerButton;

  @Nullable
  private MyPositionButton mNavMyPosition;
  private SearchWheel mSearchWheel;
  private BadgeDrawable mBadgeDrawable;
  private float mContentHeight;
  private float mContentWidth;

  private MapButtonClickListener mMapButtonClickListener;
  private PlacePageViewModel mPlacePageViewModel;
  private MapButtonsViewModel mMapButtonsViewModel;

  private final Observer<Integer> mPlacePageDistanceToTopObserver = this::move;
  private final Observer<Boolean> mButtonHiddenObserver = this::setButtonsHidden;
  private final Observer<Integer> mMyPositionModeObserver = this::updateNavMyPositionButton;
  private final Observer<SearchWheel.SearchOption> mSearchOptionObserver = this::onSearchOptionChange;


  @Nullable
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    final FragmentActivity activity = requireActivity();
    mMapButtonClickListener = (MwmActivity) activity;
    mPlacePageViewModel = new ViewModelProvider(activity).get(PlacePageViewModel.class);
    mMapButtonsViewModel = new ViewModelProvider(activity).get(MapButtonsViewModel.class);
    final LayoutMode layoutMode = mMapButtonsViewModel.getLayoutMode().getValue();
    if (layoutMode == LayoutMode.navigation)
      mFrame = inflater.inflate(R.layout.map_buttons_layout_navigation, container, false);
    else if (layoutMode == LayoutMode.planning)
      mFrame = inflater.inflate(R.layout.map_buttons_layout_planning, container, false);
    else
      mFrame = inflater.inflate(R.layout.map_buttons_layout_regular, container, false);

    mInnerLeftButtonsFrame = mFrame.findViewById(R.id.map_buttons_inner_left);
    mInnerRightButtonsFrame = mFrame.findViewById(R.id.map_buttons_inner_right);
    mBottomButtonsFrame = mFrame.findViewById(R.id.map_buttons_bottom);

    final FloatingActionButton helpButton = mFrame.findViewById(R.id.help_button);
    if (helpButton != null)
    {
      if (Config.isNY() && !TextUtils.isEmpty(Config.getDonateUrl(requireContext())))
        helpButton.setImageResource(R.drawable.ic_christmas_tree);
      else
        helpButton.setImageResource(R.drawable.logo);
      // Keep this button colorful in normal theme.
      if (!ThemeUtils.isNightTheme(requireContext()))
        helpButton.getDrawable().setTintList(null);
    }

    final View zoomFrame = mFrame.findViewById(R.id.zoom_buttons_container);
    mFrame.findViewById(R.id.nav_zoom_in)
          .setOnClickListener((v) -> mMapButtonClickListener.onMapButtonClick(MapButtons.zoomIn));
    mFrame.findViewById(R.id.nav_zoom_out)
          .setOnClickListener((v) -> mMapButtonClickListener.onMapButtonClick(MapButtons.zoomOut));
    final View bookmarksButton = mFrame.findViewById(R.id.btn_bookmarks);
    bookmarksButton.setOnClickListener((v) -> mMapButtonClickListener.onMapButtonClick(MapButtons.bookmarks));
    final View myPosition = mFrame.findViewById(R.id.my_position);
    mNavMyPosition = new MyPositionButton(myPosition, (v) -> mMapButtonClickListener.onMapButtonClick(MapButtons.myPosition));

    // Some buttons do not exist in navigation mode
    mToggleMapLayerButton = mFrame.findViewById(R.id.layers_button);
    if (mToggleMapLayerButton != null)
    {
      mToggleMapLayerButton.setOnClickListener(view -> mMapButtonClickListener.onMapButtonClick(MapButtons.toggleMapLayer));
      mToggleMapLayerButton.setVisibility(View.VISIBLE);
    }
    final View menuButton = mFrame.findViewById(R.id.menu_button);
    if (menuButton != null)
    {
      menuButton.setOnClickListener((v) -> mMapButtonClickListener.onMapButtonClick(MapButtons.menu));
      // This hack is needed to show the badge on the initial startup. For some reason, updateMenuBadge does not work from onResume() there.
      menuButton.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
        @Override
        public void onGlobalLayout()
        {
          updateMenuBadge();
          menuButton.getViewTreeObserver().removeOnGlobalLayoutListener(this);
        }
      });
    }
    if (helpButton != null)
      helpButton.setOnClickListener((v) -> mMapButtonClickListener.onMapButtonClick(MapButtons.help));

    mSearchWheel = new SearchWheel(mFrame,
                                   (v) -> mMapButtonClickListener.onMapButtonClick(MapButtons.search),
                                   (v) -> mMapButtonClickListener.onSearchCanceled(),
                                   mMapButtonsViewModel);
    final View searchButton = mFrame.findViewById(R.id.btn_search);

    // Used to get the maximum height the buttons will evolve in
    mFrame.addOnLayoutChangeListener(new MapButtonsController.ContentViewLayoutChangeListener(mFrame));

    mButtonsMap = new HashMap<>();
    mButtonsMap.put(MapButtons.zoom, zoomFrame);
    mButtonsMap.put(MapButtons.myPosition, myPosition);
    mButtonsMap.put(MapButtons.bookmarks, bookmarksButton);
    mButtonsMap.put(MapButtons.search, searchButton);

    if (mToggleMapLayerButton != null)
      mButtonsMap.put(MapButtons.toggleMapLayer, mToggleMapLayerButton);
    if (menuButton != null)
      mButtonsMap.put(MapButtons.menu, menuButton);
    if (helpButton != null)
      mButtonsMap.put(MapButtons.help, helpButton);

    ViewCompat.setOnApplyWindowInsetsListener(mFrame, (view, windowInsets) -> {
      UiUtils.setViewInsetsPadding(view, windowInsets);
      return windowInsets;
    });
    return mFrame;
  }

  public void showButton(boolean show, MapButtonsController.MapButtons button)
  {
    // TODO(AB): Why do we need this check? Isn't it better to crash and fix the wrong logic ASAP?
    final View buttonView = mButtonsMap.get(button);
    if (buttonView == null)
      return;
    switch (button)
    {
      case zoom:
        UiUtils.showIf(show && Config.showZoomButtons(), buttonView);
        break;
      case toggleMapLayer:
        if (mToggleMapLayerButton != null)
          UiUtils.showIf(show && !isInNavigationMode(), mToggleMapLayerButton);
        break;
      case myPosition:
        if (mNavMyPosition != null)
          mNavMyPosition.showButton(show);
        break;
      case search:
        mSearchWheel.show(show);
      case bookmarks:
      case menu:
        UiUtils.showIf(show, buttonView);
    }
  }

  private static int dpToPx(float dp, Context context)
  {
    return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, dp, context.getResources().getDisplayMetrics());
  }

  @OptIn(markerClass = com.google.android.material.badge.ExperimentalBadgeUtils.class)
  public void updateMenuBadge()
  {
    final View menuButton = mButtonsMap.get(MapButtons.menu);
    final Context context = getContext();
    // Sometimes the global layout listener fires when the fragment is not attached to a context
    if (menuButton == null || context == null)
      return;
    final UpdateInfo info = MapManager.nativeGetUpdateInfo(null);
    final int count = (info == null ? 0 : info.filesCount);
    final int verticalOffset = dpToPx(8, context) + dpToPx(Integer.toString(count).length() * 5, context);
    BadgeUtils.detachBadgeDrawable(mBadgeDrawable, menuButton);
    mBadgeDrawable = BadgeDrawable.create(context);
    mBadgeDrawable.setMaxCharacterCount(3);
    mBadgeDrawable.setHorizontalOffset(verticalOffset);
    mBadgeDrawable.setVerticalOffset(dpToPx(9, context));
    mBadgeDrawable.setNumber(count);
    mBadgeDrawable.setVisible(count > 0);
    BadgeUtils.attachBadgeDrawable(mBadgeDrawable, menuButton);
  }

  private boolean isBehindPlacePage(View v)
  {
    if (mPlacePageViewModel == null)
      return false;
    final Integer placePageWidth = mPlacePageViewModel.getPlacePageWidth().getValue();
    if (placePageWidth != null)
      return !(mContentWidth / 2 > (placePageWidth.floatValue() / 2.0) + v.getWidth());
    return true;
  }

  private boolean isMoving(View v)
  {
    return v.getTranslationY() < 0;
  }

  public void move(float translationY)
  {
    if (mContentHeight == 0)
      return;

    // Move the buttons containers to follow the place page
    if (mInnerRightButtonsFrame != null &&
        (isBehindPlacePage(mInnerRightButtonsFrame) || isMoving(mInnerRightButtonsFrame)))
      applyMove(mInnerRightButtonsFrame, translationY);
    if (mInnerLeftButtonsFrame != null &&
        (isBehindPlacePage(mInnerLeftButtonsFrame) || isMoving(mInnerLeftButtonsFrame)))
      applyMove(mInnerLeftButtonsFrame, translationY);
  }

  private void applyMove(View frame, float translationY)
  {
    final float rightTranslation = translationY - frame.getBottom();
    final float appliedTranslation = rightTranslation <= 0 ? rightTranslation : 0;
    frame.setTranslationY(appliedTranslation);
    updateButtonsVisibility(appliedTranslation, frame);
  }

  public void updateButtonsVisibility()
  {
    if (mInnerLeftButtonsFrame != null)
      updateButtonsVisibility(mInnerLeftButtonsFrame.getTranslationY(), mInnerLeftButtonsFrame);
    if (mInnerRightButtonsFrame != null)
      updateButtonsVisibility(mInnerRightButtonsFrame.getTranslationY(), mInnerRightButtonsFrame);
  }

  private void updateButtonsVisibility(final float translation, @Nullable View parent)
  {
    if (parent == null)
      return;
    for (Map.Entry<MapButtons, View> entry : mButtonsMap.entrySet())
    {
      final View button = entry.getValue();
      if (button.getParent() == parent)
        showButton(getViewTopOffset(translation, button) >= 0, entry.getKey());
    }
  }

  private float getBottomButtonsHeight()
  {
    if (mBottomButtonsFrame != null && mFrame != null && UiUtils.isVisible(mFrame))
      return mBottomButtonsFrame.getMeasuredHeight();
    else
      return 0;
  }

  public void setButtonsHidden(boolean buttonHidden)
  {
    UiUtils.showIf(!buttonHidden, mFrame);
    if (!buttonHidden)
      updateButtonsVisibility();
    mMapButtonsViewModel.setBottomButtonsHeight(getBottomButtonsHeight());
  }

  private boolean isInNavigationMode()
  {
    return RoutingController.get().isPlanning() || RoutingController.get().isNavigating();
  }

  public void updateNavMyPositionButton(int newMode)
  {
    if (mNavMyPosition != null)
      mNavMyPosition.update(newMode);
  }

  private int getViewTopOffset(float translation, View v)
  {
    return (int) (translation + v.getTop());
  }

  @Override
  public void onStart()
  {
    super.onStart();
    final FragmentActivity activity = requireActivity();
    mPlacePageViewModel.getPlacePageDistanceToTop().observe(activity, mPlacePageDistanceToTopObserver);
    mMapButtonsViewModel.getButtonsHidden().observe(activity, mButtonHiddenObserver);
    mMapButtonsViewModel.getMyPositionMode().observe(activity, mMyPositionModeObserver);
    mMapButtonsViewModel.getSearchOption().observe(activity, mSearchOptionObserver);
  }

  public void onResume()
  {
    super.onResume();
    mSearchWheel.onResume();
    updateMenuBadge();
  }

  @Override
  public void onStop()
  {
    super.onStop();
    mPlacePageViewModel.getPlacePageDistanceToTop().removeObserver(mPlacePageDistanceToTopObserver);
    mMapButtonsViewModel.getButtonsHidden().removeObserver(mButtonHiddenObserver);
    mMapButtonsViewModel.getMyPositionMode().removeObserver(mMyPositionModeObserver);
    mMapButtonsViewModel.getSearchOption().removeObserver(mSearchOptionObserver);
  }

  public void onSearchOptionChange(@Nullable SearchWheel.SearchOption searchOption)
  {
    if (searchOption == null)
      mSearchWheel.reset();
  }

  public enum LayoutMode
  {
    regular,
    planning,
    navigation
  }

  public enum MapButtons
  {
    myPosition,
    toggleMapLayer,
    zoomIn,
    zoomOut,
    zoom,
    search,
    bookmarks,
    menu,
    help
  }

  public interface MapButtonClickListener
  {
    void onMapButtonClick(MapButtons button);

    void onSearchCanceled();
  }

  private class ContentViewLayoutChangeListener implements View.OnLayoutChangeListener
  {
    @NonNull
    private final View mContentView;

    public ContentViewLayoutChangeListener(@NonNull View contentView)
    {
      mContentView = contentView;
    }

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
                               int oldTop, int oldRight, int oldBottom)
    {
      mContentHeight = bottom - top;
      mContentWidth = right - left;
      mMapButtonsViewModel.setBottomButtonsHeight(getBottomButtonsHeight());
      mContentView.removeOnLayoutChangeListener(this);
    }
  }
}
