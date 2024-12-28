package app.organicmaps.routing;

import android.location.Location;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import app.organicmaps.Framework;
import app.organicmaps.R;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.maplayer.traffic.TrafficManager;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.widget.LanesView;
import app.organicmaps.widget.SpeedLimitView;
import app.organicmaps.util.WindowInsetUtils;
import app.organicmaps.widget.menu.NavMenu;
import com.google.android.material.bottomsheet.BottomSheetBehavior;

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
  private final LanesView mLanesView;
  @NonNull
  private final SpeedLimitView mSpeedLimit;

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

    mLanesView = topFrame.findViewById(R.id.lanes);

    mSpeedLimit = topFrame.findViewById(R.id.nav_speed_limit);

    // Show a blank view below the navbar to hide the menu content
    final View navigationBarBackground = mFrame.findViewById(R.id.nav_bottom_sheet_nav_bar);
    final View nextTurnContainer = mFrame.findViewById(R.id.nav_next_turn_container);
    ViewCompat.setOnApplyWindowInsetsListener(mStreetFrame, (v, windowInsets) -> {
      UiUtils.setViewInsetsPaddingNoBottom(v, windowInsets);

      final Insets safeDrawingInsets = windowInsets.getInsets(WindowInsetUtils.TYPE_SAFE_DRAWING);
      nextTurnContainer.setPadding(
          safeDrawingInsets.left, nextTurnContainer.getPaddingTop(),
          nextTurnContainer.getPaddingEnd(), nextTurnContainer.getPaddingBottom());
      navigationBarBackground.getLayoutParams().height = safeDrawingInsets.bottom;
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

    UiUtils.visibleIf(info.nextCarDirection.containsNextTurn(), mNextNextTurnFrame);
    if (info.nextCarDirection.containsNextTurn())
      info.nextCarDirection.setNextTurnDrawable(mNextNextTurnImage);

    mLanesView.setLanes(info.lanes);

    updateSpeedLimit(info);
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

  private void updateSpeedLimit(@NonNull final RoutingInfo info)
  {
    final Location location = LocationHelper.from(mFrame.getContext()).getSavedLocation();
    if (location == null) {
      mSpeedLimit.setSpeedLimitMps(0);
      return;
    }
    mSpeedLimit.setCurrentSpeed(location.getSpeed());
    mSpeedLimit.setSpeedLimitMps(info.speedLimitMps);
  }
}
