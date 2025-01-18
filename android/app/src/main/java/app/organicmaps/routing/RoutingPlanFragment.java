package app.organicmaps.routing;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;

import app.organicmaps.R;
import app.organicmaps.base.BaseMwmFragment;
import app.organicmaps.sdk.Router;

public class RoutingPlanFragment extends BaseMwmFragment
{

  private RoutingPlanController mPlanController;

  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    final FragmentActivity activity = requireActivity();
    View res = inflater.inflate(R.layout.fragment_routing, container, false);
    RoutingBottomMenuListener listener = null;
    if (activity instanceof RoutingBottomMenuListener)
      listener = (RoutingBottomMenuListener) activity;

    RoutingPlanInplaceController.RoutingPlanListener planListener =
        (RoutingPlanInplaceController.RoutingPlanListener) activity;
    mPlanController = new RoutingPlanController(res, activity, planListener, listener);
    return res;
  }

  public void updateBuildProgress(int progress, Router router)
  {
    mPlanController.updateBuildProgress(progress, router);
  }

  @Override
  public boolean onBackPressed()
  {
    return RoutingController.get().cancel();
  }

  public void restoreRoutingPanelState(@NonNull Bundle state)
  {
    mPlanController.restoreRoutingPanelState(state);
  }

  public void saveRoutingPanelState(@NonNull Bundle outState)
  {
    mPlanController.saveRoutingPanelState(outState);
  }

  public void showAddStartFrame()
  {
    mPlanController.showAddStartFrame();
  }

  public void showAddFinishFrame()
  {
    mPlanController.showAddFinishFrame();
  }

}
