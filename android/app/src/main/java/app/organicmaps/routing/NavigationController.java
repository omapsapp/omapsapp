package app.organicmaps.routing;

import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.recyclerview.widget.RecyclerView;
import app.organicmaps.sdk.Framework;
import app.organicmaps.R;
import app.organicmaps.sdk.maplayer.traffic.TrafficManager;
import app.organicmaps.sdk.routing.RoutingController;
import app.organicmaps.sdk.routing.RoutingInfo;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.widget.menu.NavMenu;
import com.google.android.material.bottomsheet.BottomSheetBehavior;

import java.util.Arrays;

public class NavigationController implements TrafficManager.TrafficCallback,
                                             NavMenu.NavMenuListener
{
  private final View mFrame;

  private final ImageView mNextTurnImage;
  private final TextView mNextTurnDistance;
  private final TextView mCircleExit;

  private final View mNextNextTurnFrame;
  private final ImageView mNextNextTurnImage;

  private final View mStreetFrame;
  private final TextView mNextStreet;

  @NonNull
  private final View mLanesFrame;
  @NonNull
  private final RecyclerView mLanes;
  @NonNull
  private final LanesAdapter mLanesAdapter;

  private final NavMenu mNavMenu;
  View.OnClickListener mOnSettingsClickListener;

  private void addWindowsInsets(@NonNull View topFrame)
  {
    ViewCompat.setOnApplyWindowInsetsListener(topFrame.findViewById(R.id.nav_next_turn_container), (view, windowInsets) -> {
      view.setPadding(windowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).left, view.getPaddingTop(),
                      view.getPaddingEnd(), view.getPaddingBottom());
      return windowInsets;
    });
  }

  private void initLanesRecycler()
  {
    mLanes.setAdapter(mLanesAdapter);
    mLanes.setNestedScrollingEnabled(false);
  }

  public NavigationController(AppCompatActivity activity, View.OnClickListener onSettingsClickListener,
                              NavMenu.OnMenuSizeChangedListener onMenuSizeChangedListener)
  {
    mFrame = activity.findViewById(R.id.navigation_frame);
    mNavMenu = new NavMenu(activity, this, onMenuSizeChangedListener);
    mOnSettingsClickListener = onSettingsClickListener;

    // Top frame
    View topFrame = mFrame.findViewById(R.id.nav_top_frame);
    View turnFrame = topFrame.findViewById(R.id.nav_next_turn_frame);
    mNextTurnImage = turnFrame.findViewById(R.id.turn);
    mNextTurnDistance = turnFrame.findViewById(R.id.distance);
    mCircleExit = turnFrame.findViewById(R.id.circle_exit);

    addWindowsInsets(topFrame);

    mNextNextTurnFrame = topFrame.findViewById(R.id.nav_next_next_turn_frame);
    mNextNextTurnImage = mNextNextTurnFrame.findViewById(R.id.turn);

    mStreetFrame = topFrame.findViewById(R.id.street_frame);
    mNextStreet = mStreetFrame.findViewById(R.id.street);

    mLanesFrame = topFrame.findViewById(R.id.lanes_frame);
    mLanes = mLanesFrame.findViewById(R.id.lanes);
    mLanesAdapter = new LanesAdapter();
    initLanesRecycler();

    // Show a blank view below the navbar to hide the menu content
    final View navigationBarBackground = mFrame.findViewById(R.id.nav_bottom_sheet_nav_bar);
    final View nextTurnContainer = mFrame.findViewById(R.id.nav_next_turn_container);
    ViewCompat.setOnApplyWindowInsetsListener(mStreetFrame, (v, windowInsets) -> {
      UiUtils.setViewInsetsPaddingNoBottom(v, windowInsets);
      nextTurnContainer.setPadding(windowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).left, nextTurnContainer.getPaddingTop(),
                                   nextTurnContainer.getPaddingEnd(), nextTurnContainer.getPaddingBottom());
      navigationBarBackground.getLayoutParams().height = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars()).bottom;
      // The gesture navigation bar stays at the bottom in landscape
      // We need to add a background only above the nav menu
      navigationBarBackground.getLayoutParams().width = mFrame.findViewById(R.id.nav_bottom_sheet).getWidth();
      return windowInsets;
    });
  }

  private void updateVehicle(@NonNull RoutingInfo info)
  {
    mNextTurnDistance.setText(Utils.formatDistance(mFrame.getContext(), info.distToTurn));
    info.carDirection.setTurnDrawable(mNextTurnImage);

    if (RoutingInfo.CarDirection.isRoundAbout(info.carDirection))
      UiUtils.setTextAndShow(mCircleExit, String.valueOf(info.exitNum));
    else
      UiUtils.hide(mCircleExit);

    UiUtils.showIf(info.nextCarDirection.containsNextTurn(), mNextNextTurnFrame);
    if (info.nextCarDirection.containsNextTurn())
      info.nextCarDirection.setNextTurnDrawable(mNextNextTurnImage);

    if (info.lanes != null)
    {
      UiUtils.show(mLanesFrame);
      mLanesAdapter.setItems(Arrays.asList(info.lanes));
    }
    else
    {
      UiUtils.hide(mLanesFrame);
      mLanesAdapter.clearItems();
    }
  }

  private void updatePedestrian(@NonNull RoutingInfo info)
  {
    mNextTurnDistance.setText(Utils.formatDistance(mFrame.getContext(), info.distToTurn));

    info.pedestrianTurnDirection.setTurnDrawable(mNextTurnImage);
  }

  public void updateNorth()
  {
    if (!RoutingController.get().isNavigating())
      return;

    update(Framework.nativeGetRouteFollowingInfo());
  }

  public void update(@Nullable RoutingInfo info)
  {
    if (info == null)
      return;

    if (Framework.nativeGetRouter() == Framework.ROUTER_TYPE_PEDESTRIAN)
      updatePedestrian(info);
    else
      updateVehicle(info);

    updateStreetView(info);
    mNavMenu.update(info);
  }

  private void updateStreetView(@NonNull RoutingInfo info)
  {
    boolean hasStreet = !TextUtils.isEmpty(info.nextStreet);
    // Sic: don't use UiUtils.showIf() here because View.GONE breaks layout
    // https://github.com/organicmaps/organicmaps/issues/3732
    UiUtils.visibleIf(hasStreet, mStreetFrame);
    if (!TextUtils.isEmpty(info.nextStreet))
      mNextStreet.setText(info.nextStreet);
  }

  public void show(boolean show)
  {
    if (show && !UiUtils.isVisible(mFrame))
      collapseNavMenu();
    UiUtils.showIf(show, mFrame);
  }

  public boolean isNavMenuCollapsed()
  {
    return mNavMenu.getBottomSheetState() == BottomSheetBehavior.STATE_COLLAPSED;
  }

  public boolean isNavMenuHidden()
  {
    return mNavMenu.getBottomSheetState() == BottomSheetBehavior.STATE_HIDDEN;
  }

  public void collapseNavMenu()
  {
    mNavMenu.collapseNavBottomSheet();
  }

  public void refresh()
  {
    mNavMenu.refreshTts();
  }

  @Override
  public void onEnabled()
  {
    // mNavMenu.refreshTraffic();
  }

  @Override
  public void onDisabled()
  {
    // mNavMenu.refreshTraffic();
  }

  @Override
  public void onWaitingData()
  {
    // no op
  }

  @Override
  public void onOutdated()
  {
    // no op
  }

  @Override
  public void onNoData()
  {
    // no op
  }

  @Override
  public void onNetworkError()
  {
    // no op
  }

  @Override
  public void onExpiredData()
  {
    // no op
  }

  @Override
  public void onExpiredApp()
  {
    // no op
  }

  @Override
  public void onSettingsClicked()
  {
    mOnSettingsClickListener.onClick(null);
  }

  @Override
  public void onStopClicked()
  {
    RoutingController.get().cancel();
  }

}
