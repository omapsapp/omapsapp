package app.organicmaps.widget.placepage;

import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.widget.NestedScrollView;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.interpolator.view.animation.FastOutSlowInInterpolator;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProvider;
import app.organicmaps.Framework;
import app.organicmaps.MwmActivity;
import app.organicmaps.R;
import app.organicmaps.api.Const;
import app.organicmaps.bookmarks.data.BookmarkManager;
import app.organicmaps.bookmarks.data.MapObject;
import app.organicmaps.bookmarks.data.RoadWarningMarkType;
import app.organicmaps.intent.Factory;
import app.organicmaps.routing.RoutingController;
import app.organicmaps.settings.RoadType;
import app.organicmaps.util.ThemeUtils;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.bottomsheet.MenuBottomSheetFragment;
import app.organicmaps.util.bottomsheet.MenuBottomSheetItem;
import app.organicmaps.util.log.Logger;
import com.google.android.material.bottomsheet.BottomSheetBehavior;

import java.util.ArrayList;
import java.util.List;

public class PlacePageController extends Fragment implements
                                                  PlacePageView.PlacePageViewListener,
                                                  PlacePageButtons.PlacePageButtonClickListener,
                                                  MenuBottomSheetFragment.MenuBottomSheetInterface,
                                                  Observer<MapObject>
{
  private static final String TAG = PlacePageController.class.getSimpleName();
  private static final String PLACE_PAGE_BUTTONS_FRAGMENT_TAG = "PLACE_PAGE_BUTTONS";
  private static final String PLACE_PAGE_FRAGMENT_TAG = "PLACE_PAGE";

  private static final float PREVIEW_PLUS_RATIO = 0.45f;
  private BottomSheetBehavior<View> mPlacePageBehavior;
  private NestedScrollView mPlacePage;
  private ViewGroup mPlacePageContainer;
  private View mPlacePageStatusBarBackground;
  private ViewGroup mCoordinator;
  private int mViewportMinHeight;
  private int mButtonsHeight;
  private int mMaxButtons;
  private int mRoutingHeaderHeight;
  private PlacePageViewModel mViewModel;
  private int mPreviewHeight;
  private int mFrameHeight;
  @Nullable
  private MapObject mMapObject;
  @Nullable
  private MapObject mPreviousMapObject;
  private WindowInsetsCompat mCurrentWindowInsets;

  private boolean mShouldCollapse;
  private int mDistanceToTop;

  private ValueAnimator mCustomPeekHeightAnimator;
  private PlacePageRouteSettingsListener mPlacePageRouteSettingsListener;
  private final Observer<Integer> mPlacePageDistanceToTopObserver = this::updateStatusBarBackground;

  private final BottomSheetBehavior.BottomSheetCallback mDefaultBottomSheetCallback = new BottomSheetBehavior.BottomSheetCallback()
  {
    @Override
    public void onStateChanged(@NonNull View bottomSheet, int newState)
    {
      Logger.d(TAG, "State change, new = " + PlacePageUtils.toString(newState));
      if (PlacePageUtils.isSettlingState(newState) || PlacePageUtils.isDraggingState(newState))
        return;

      PlacePageUtils.updateMapViewport(mCoordinator, mDistanceToTop, mViewportMinHeight);

      if (PlacePageUtils.isHiddenState(newState))
        onHiddenInternal();
    }

    @Override
    public void onSlide(@NonNull View bottomSheet, float slideOffset)
    {
      stopCustomPeekHeightAnimation();
      mDistanceToTop = bottomSheet.getTop();
      mViewModel.setPlacePageDistanceToTop(mDistanceToTop);
    }
  };

  @Nullable
  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.place_page_container_fragment, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    final FragmentActivity activity = requireActivity();
    mPlacePageRouteSettingsListener = (MwmActivity) activity;

    final Resources res = activity.getResources();
    mViewportMinHeight = res.getDimensionPixelSize(R.dimen.viewport_min_height);
    mButtonsHeight = (int) res.getDimension(R.dimen.place_page_buttons_height);
    mMaxButtons = res.getInteger(R.integer.pp_buttons_max);
    mRoutingHeaderHeight = (int) res.getDimension(ThemeUtils.getResource(requireContext(), androidx.appcompat.R.attr.actionBarSize));

    mCoordinator = activity.findViewById(R.id.coordinator);
    mPlacePage = view.findViewById(R.id.placepage);
    mPlacePageContainer = view.findViewById(R.id.placepage_container);
    mPlacePageBehavior = BottomSheetBehavior.from(mPlacePage);
    mPlacePageStatusBarBackground = view.findViewById(R.id.place_page_status_bar_background);

    mShouldCollapse = true;

    mPlacePageBehavior.setHideable(true);
    mPlacePageBehavior.setState(BottomSheetBehavior.STATE_HIDDEN);
    mPlacePageBehavior.setFitToContents(true);
    mPlacePageBehavior.setSkipCollapsed(true);

    UiUtils.bringViewToFrontOf(view.findViewById(R.id.pp_buttons_fragment), mPlacePage);

    mViewModel = new ViewModelProvider(requireActivity()).get(PlacePageViewModel.class);

    ViewCompat.setOnApplyWindowInsetsListener(mPlacePage, (v, windowInsets) -> {
      mCurrentWindowInsets = windowInsets;
      final Insets insets = mCurrentWindowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
      final ViewGroup.MarginLayoutParams layoutParams = (ViewGroup.MarginLayoutParams) mPlacePageStatusBarBackground.getLayoutParams();
      // Layout calculations are heavy so we compute them once then move the view from behind the place page to the status bar
      layoutParams.height = insets.top;
      layoutParams.width = mPlacePage.getWidth();
      // Make sure the view is centered within the insets as is the place page
      layoutParams.setMargins(insets.left, 0, insets.right, 0);
      mPlacePageStatusBarBackground.setLayoutParams(layoutParams);
      return windowInsets;
    });

    ViewCompat.requestApplyInsets(mPlacePage);
  }

  @NonNull
  private static PlacePageButtons.ButtonType toPlacePageButton(@NonNull RoadWarningMarkType type)
  {
    return switch (type)
    {
      case DIRTY -> PlacePageButtons.ButtonType.ROUTE_AVOID_UNPAVED;
      case FERRY -> PlacePageButtons.ButtonType.ROUTE_AVOID_FERRY;
      case TOLL -> PlacePageButtons.ButtonType.ROUTE_AVOID_TOLL;
      default -> throw new AssertionError("Unsupported road warning type: " + type);
    };
  }

  private void stopCustomPeekHeightAnimation()
  {
    if (mCustomPeekHeightAnimator != null && mCustomPeekHeightAnimator.isStarted())
    {
      mCustomPeekHeightAnimator.end();
      setPlacePageHeightBounds();
    }
  }

  private void onHiddenInternal()
  {
    Framework.nativeDeactivatePopup();
    PlacePageUtils.updateMapViewport(mCoordinator, mDistanceToTop, mViewportMinHeight);
    resetPlacePageHeightBounds();
    removePlacePageFragments();
  }

  @Nullable
  public ArrayList<MenuBottomSheetItem> getMenuBottomSheetItems(String id)
  {
    final List<PlacePageButtons.ButtonType> currentItems = mViewModel.getCurrentButtons().getValue();
    if (currentItems == null || currentItems.size() <= mMaxButtons)
      return null;
    ArrayList<MenuBottomSheetItem> items = new ArrayList<>();
    for (int i = mMaxButtons - 1; i < currentItems.size(); i++)
    {
      final PlacePageButton bsItem = PlacePageButtonFactory.createButton(currentItems.get(i), requireActivity());
      items.add(new MenuBottomSheetItem(
          bsItem.getTitle(),
          bsItem.getIcon(),
          () -> onPlacePageButtonClick(bsItem.getType())
      ));
    }
    return items;
  }

  private void setPlacePageInteractions(boolean enabled)
  {
    // Prevent place page scrolling when playing the close animation
    mPlacePageBehavior.setDraggable(enabled);
    mPlacePage.setNestedScrollingEnabled(enabled);
    // Prevent user interaction with place page content when closing
    mPlacePageContainer.setEnabled(enabled);
  }

  private void close()
  {
    setPlacePageInteractions(false);
    mPlacePageBehavior.setState(BottomSheetBehavior.STATE_HIDDEN);
  }

  private void resetPlacePageHeightBounds()
  {
    mFrameHeight = 0;
    mPlacePageContainer.setMinimumHeight(0);
    final int parentHeight = ((View) mPlacePage.getParent()).getHeight();
    mPlacePageBehavior.setMaxHeight(parentHeight);
  }

  /**
   * Set the min and max height of the place page to prevent jumps when switching from one map object
   * to the other.
   */
  private void setPlacePageHeightBounds()
  {
    final int peekHeight = calculatePeekHeight();
    final Insets insets = mCurrentWindowInsets != null
            ? mCurrentWindowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            : Insets.NONE;
    // Make sure the place page can reach the peek height
    final int minHeight = Math.max(peekHeight, mFrameHeight);
    // Prevent the place page from showing under the status bar
    // If we are in planning mode, prevent going above the header
    final int topInsets = insets.top + (RoutingController.get().isPlanning() ? mRoutingHeaderHeight : 0);
    final int maxHeight = Math.min(minHeight + insets.bottom, mCoordinator.getHeight() - topInsets);
    // Set the minimum height of the place page to prevent jumps when new data results in SMALLER content
    // This cannot be set on the place page itself as it has the fitToContent property set
    mPlacePageContainer.setMinimumHeight(minHeight);
    // Set the maximum height of the place page to prevent jumps when new data results in BIGGER content
    // It does not take into account the navigation bar height so we need to add it manually
    mPlacePageBehavior.setMaxHeight(maxHeight);
  }

  /**
   * Make sure the place page can reach the peek height
   */
  private void preparePlacePageMinHeight(int peekHeight)
  {
    final int currentHeight = mPlacePageContainer.getHeight();
    if (currentHeight < peekHeight)
      mPlacePageContainer.setMinimumHeight(peekHeight);
  }

  private void setPeekHeight()
  {
    final int peekHeight = calculatePeekHeight();
    preparePlacePageMinHeight(peekHeight);

    final int state = mPlacePageBehavior.getState();
    // Do not animate the peek height if the place page should not be collapsed (eg: when returning from editor)
    final boolean shouldAnimate = !(PlacePageUtils.isExpandedState(state) && !mShouldCollapse) && !PlacePageUtils.isHiddenState(state);
    if (shouldAnimate)
      animatePeekHeight(peekHeight);
    else
    {
      mPlacePageBehavior.setPeekHeight(peekHeight);
      setPlacePageHeightBounds();
    }
  }

  /**
   * Using the animate param in setPeekHeight does not work when adding removing fragments
   * from inside the place page so we manually animate the peek height with ValueAnimator
   */
  private void animatePeekHeight(int peekHeight)
  {
    if (mCurrentWindowInsets == null) {
      return;
    }
    final int bottomInsets = mCurrentWindowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).bottom;
    // Make sure to start from the current height of the place page
    final int parentHeight = ((View) mPlacePage.getParent()).getHeight();
    // Make sure to remove the navbar height because the peek height already takes it into account
    int initialHeight = parentHeight - mDistanceToTop - bottomInsets;

    if (mCustomPeekHeightAnimator != null)
      mCustomPeekHeightAnimator.cancel();
    mCustomPeekHeightAnimator = ValueAnimator.ofInt(initialHeight, peekHeight);
    mCustomPeekHeightAnimator.setInterpolator(new FastOutSlowInInterpolator());
    mCustomPeekHeightAnimator.addUpdateListener(valueAnimator -> {
      int value = (Integer) valueAnimator.getAnimatedValue();
      // Make sure the place page can reach the animated peek height to prevent jumps
      // maxHeight does not take the navbar height into account so we manually add it
      mPlacePageBehavior.setMaxHeight(value + bottomInsets);
      mPlacePageBehavior.setPeekHeight(value);
      // The place page is not firing the slide callbacks when using this animation, so we must call them manually
      mDistanceToTop = parentHeight - value - bottomInsets;
      mViewModel.setPlacePageDistanceToTop(mDistanceToTop);
      if (value == peekHeight)
      {
        PlacePageUtils.updateMapViewport(mCoordinator, mDistanceToTop, mViewportMinHeight);
        setPlacePageHeightBounds();
      }
    });
    mCustomPeekHeightAnimator.setDuration(200);
    mCustomPeekHeightAnimator.start();
  }

  private int calculatePeekHeight()
  {
    if (mMapObject != null && mMapObject.getOpeningMode() == MapObject.OPENING_MODE_PREVIEW_PLUS)
      return (int) (mCoordinator.getHeight() * PREVIEW_PLUS_RATIO);
    return mPreviewHeight + mButtonsHeight;
  }

  @Override
  public void onPlacePageContentChanged(int previewHeight, int frameHeight)
  {
    mPreviewHeight = previewHeight;
    mFrameHeight = frameHeight;
    mViewModel.setPlacePageWidth(mPlacePage.getWidth());
    mPlacePageStatusBarBackground.getLayoutParams().width = mPlacePage.getWidth();
    // Make sure to update the peek height on the UI thread to prevent weird animation jumps
    mPlacePage.post(() -> {
      setPeekHeight();
      if (mShouldCollapse && !PlacePageUtils.isCollapsedState(mPlacePageBehavior.getState()))
      {
        mPlacePageBehavior.setState(BottomSheetBehavior.STATE_COLLAPSED);
        // Make sure to reset the scroll position when opening the place page
        if (mPlacePage.getScrollY() != 0)
          mPlacePage.setScrollY(0);
      }
      mShouldCollapse = false;
    });
  }

  @Override
  public void onPlacePageRequestToggleState()
  {
    @BottomSheetBehavior.State
    int state = mPlacePageBehavior.getState();
    stopCustomPeekHeightAnimation();
    if (PlacePageUtils.isExpandedState(state))
      mPlacePageBehavior.setState(BottomSheetBehavior.STATE_HIDDEN);
    else
      mPlacePageBehavior.setState(BottomSheetBehavior.STATE_EXPANDED);
  }

  @Override
  public void onPlacePageRequestClose()
  {
    mPlacePageBehavior.setState(BottomSheetBehavior.STATE_HIDDEN);
  }

  @Override
  public void onPlacePageButtonClick(PlacePageButtons.ButtonType item)
  {
    switch (item)
    {
      case BOOKMARK_SAVE, BOOKMARK_DELETE -> onBookmarkBtnClicked();
      case BACK -> onBackBtnClicked();
      case ROUTE_FROM -> onRouteFromBtnClicked();
      case ROUTE_TO -> onRouteToBtnClicked();
      case ROUTE_ADD -> onRouteAddBtnClicked();
      case ROUTE_REMOVE -> onRouteRemoveBtnClicked();
      case ROUTE_AVOID_TOLL -> onAvoidTollBtnClicked();
      case ROUTE_AVOID_UNPAVED -> onAvoidUnpavedBtnClicked();
      case ROUTE_AVOID_FERRY -> onAvoidFerryBtnClicked();
    }
  }

  private void onBookmarkBtnClicked()
  {
    // mMapObject is set to null when the place page closes
    // We don't want users to interact with the buttons when the PP is closing
    if (mMapObject == null)
      return;
    // No need to call setMapObject here as the native methods will reopen the place page
    if (mMapObject.isBookmark())
      Framework.nativeDeleteBookmarkFromMapObject();
    else
      BookmarkManager.INSTANCE.addNewBookmark(mMapObject.getLat(), mMapObject.getLon());
  }

  private void onBackBtnClicked()
  {
    if (mMapObject == null)
      return;
    final Intent result = new Intent();
    result.putExtra(Const.EXTRA_POINT_LAT, mMapObject.getLat())
          .putExtra(Const.EXTRA_POINT_LON, mMapObject.getLon())
          .putExtra(Const.EXTRA_POINT_NAME, mMapObject.getTitle())
          .putExtra(Const.EXTRA_POINT_ID, mMapObject.getApiId())
          .putExtra(Const.EXTRA_ZOOM_LEVEL, Framework.nativeGetDrawScale());
    requireActivity().setResult(Activity.RESULT_OK, result);
    requireActivity().finish();
  }

  private void onRouteFromBtnClicked()
  {
    if (mMapObject == null)
      return;
    RoutingController controller = RoutingController.get();
    if (!controller.isPlanning())
    {
      controller.prepare(mMapObject, null);
      close();
    }
    else if (controller.setStartPoint(mMapObject))
      close();
  }

  private void onRouteToBtnClicked()
  {
    if (mMapObject == null)
      return;
    if (RoutingController.get().isPlanning())
    {
      RoutingController.get().setEndPoint(mMapObject);
      close();
    }
    else
      ((MwmActivity) requireActivity()).startLocationToPoint(mMapObject);
  }

  private void onRouteAddBtnClicked()
  {
    if (mMapObject != null)
      RoutingController.get().addStop(mMapObject);
  }

  private void onRouteRemoveBtnClicked()
  {
    if (mMapObject != null)
      RoutingController.get().removeStop(mMapObject);
  }

  private void onAvoidUnpavedBtnClicked()
  {
    onAvoidBtnClicked(RoadType.Dirty);
  }

  private void onAvoidFerryBtnClicked()
  {
    onAvoidBtnClicked(RoadType.Ferry);
  }

  private void onAvoidTollBtnClicked()
  {
    onAvoidBtnClicked(RoadType.Toll);
  }

  private void onAvoidBtnClicked(@NonNull RoadType roadType)
  {
    if (mMapObject != null)
      mPlacePageRouteSettingsListener.onPlacePageRequestToggleRouteSettings(roadType);
  }

  private void removePlacePageFragments()
  {
    final FragmentManager fm = getChildFragmentManager();
    final Fragment placePageButtonsFragment = fm.findFragmentByTag(PLACE_PAGE_BUTTONS_FRAGMENT_TAG);
    final Fragment placePageFragment = fm.findFragmentByTag(PLACE_PAGE_FRAGMENT_TAG);

    if (placePageButtonsFragment != null)
    {
      fm.beginTransaction()
        .setReorderingAllowed(true)
        .remove(placePageButtonsFragment)
        .commit();
    }
    if (placePageFragment != null)
    {
      fm.beginTransaction()
        .setReorderingAllowed(true)
        .remove(placePageFragment)
        .commit();
    }
    mViewModel.setMapObject(null);
  }

  private void createPlacePageFragments()
  {
    final FragmentManager fm = getChildFragmentManager();
    if (fm.findFragmentByTag(PLACE_PAGE_FRAGMENT_TAG) == null)
    {
      fm.beginTransaction()
        .setReorderingAllowed(true)
        .add(R.id.placepage_fragment, PlacePageView.class, null, PLACE_PAGE_FRAGMENT_TAG)
        .commit();
    }
    if (fm.findFragmentByTag(PLACE_PAGE_BUTTONS_FRAGMENT_TAG) == null)
    {
      fm.beginTransaction()
        .setReorderingAllowed(true)
        .add(R.id.pp_buttons_fragment, PlacePageButtons.class, null, PLACE_PAGE_BUTTONS_FRAGMENT_TAG)
        .commit();
    }
  }

  private void updateButtons(MapObject mapObject, boolean showBackButton, boolean showRoutingButton)
  {
    List<PlacePageButtons.ButtonType> buttons = new ArrayList<>();
    if (mapObject.getRoadWarningMarkType() != RoadWarningMarkType.UNKNOWN)
    {
      RoadWarningMarkType markType = mapObject.getRoadWarningMarkType();
      PlacePageButtons.ButtonType roadType = toPlacePageButton(markType);
      buttons.add(roadType);
    }
    else if (RoutingController.get().isRoutePoint(mapObject))
    {
      buttons.add(PlacePageButtons.ButtonType.ROUTE_REMOVE);
    }
    else
    {
      if (showBackButton)
        buttons.add(PlacePageButtons.ButtonType.BACK);

      boolean needToShowRoutingButtons = RoutingController.get().isPlanning() || showRoutingButton;

      if (needToShowRoutingButtons)
        buttons.add(PlacePageButtons.ButtonType.ROUTE_FROM);

      // If we can show the add route button, put it in the place of the bookmark button
      // And move the bookmark button at the end
      if (needToShowRoutingButtons && RoutingController.get().isStopPointAllowed())
        buttons.add(PlacePageButtons.ButtonType.ROUTE_ADD);
      else
        buttons.add(mapObject.isBookmark()
                    ? PlacePageButtons.ButtonType.BOOKMARK_DELETE
                    : PlacePageButtons.ButtonType.BOOKMARK_SAVE);

      if (needToShowRoutingButtons)
      {
        buttons.add(PlacePageButtons.ButtonType.ROUTE_TO);
        if (RoutingController.get().isStopPointAllowed())
          buttons.add(mapObject.isBookmark()
                      ? PlacePageButtons.ButtonType.BOOKMARK_DELETE
                      : PlacePageButtons.ButtonType.BOOKMARK_SAVE);
      }
    }
    mViewModel.setCurrentButtons(buttons);
  }

  @Override
  public void onChanged(@Nullable MapObject mapObject)
  {
    final Activity activity = requireActivity();
    final Intent intent = activity.getIntent();
    final boolean showBackButton = (intent != null && (Factory.isStartedForApiResult(intent) ||
        !TextUtils.isEmpty(Framework.nativeGetParsedBackUrl())));
    mMapObject = mapObject;
    if (mapObject != null)
    {
      setPlacePageInteractions(true);
      // Only collapse the place page if the data is different from the one already available
      mShouldCollapse = PlacePageUtils.isHiddenState(mPlacePageBehavior.getState()) || !MapObject.same(mPreviousMapObject, mMapObject);
      mPreviousMapObject = mMapObject;
      // Place page will automatically open when the bottom sheet content is loaded so we can compute the peek height
      createPlacePageFragments();
      updateButtons(
          mapObject,
          showBackButton,
          !mMapObject.isMyPosition());
    }
    else
      close();
  }

  private void updateStatusBarBackground(int distanceToTop)
  {
    // This callback may be called before insets are updated when resuming the app
    if (mCurrentWindowInsets == null)
      return;
    final int topInset = mCurrentWindowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).top;
    // Only animate the status bar background if the place page can reach it
    if (mCoordinator.getHeight() - mPlacePageContainer.getHeight() < topInset)
    {
      final int animationStartHeight = topInset * 3;
      int newHeight = 0;
      if (distanceToTop < animationStartHeight)
        newHeight = Math.min(topInset * (animationStartHeight - distanceToTop) / 100, topInset);
      if (newHeight > 0)
      {
        mPlacePageStatusBarBackground.setTranslationY(distanceToTop - newHeight);
        if (!UiUtils.isVisible(mPlacePageStatusBarBackground))
          UiUtils.show(mPlacePageStatusBarBackground);
      }
      else if (UiUtils.isVisible(mPlacePageStatusBarBackground))
        UiUtils.hide(mPlacePageStatusBarBackground);
    }
  }

  @Override
  public void onStart()
  {
    super.onStart();
    mPlacePageBehavior.addBottomSheetCallback(mDefaultBottomSheetCallback);
    mViewModel.getMapObject().observe(requireActivity(), this);
    mViewModel.getPlacePageDistanceToTop().observe(requireActivity(), mPlacePageDistanceToTopObserver);
  }

  @Override
  public void onResume()
  {
    super.onResume();
    if (mPlacePageBehavior.getState() != BottomSheetBehavior.STATE_HIDDEN && !Framework.nativeHasPlacePageInfo())
      mViewModel.setMapObject(null);
  }

  @Override
  public void onStop()
  {
    super.onStop();
    mPlacePageBehavior.removeBottomSheetCallback(mDefaultBottomSheetCallback);
    mViewModel.getMapObject().removeObserver(this);
    mViewModel.getPlacePageDistanceToTop().removeObserver(mPlacePageDistanceToTopObserver);
  }

  public interface PlacePageRouteSettingsListener
  {
    void onPlacePageRequestToggleRouteSettings(@NonNull RoadType roadType);
  }
}
