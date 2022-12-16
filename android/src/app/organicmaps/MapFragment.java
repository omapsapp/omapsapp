package app.organicmaps;

import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.core.content.res.ConfigurationHelper;

import app.organicmaps.base.BaseMwmFragment;
import app.organicmaps.util.log.Logger;

public class MapFragment extends BaseMwmFragment implements View.OnTouchListener, SurfaceHolder.Callback
{
  private static final String TAG = MapFragment.class.getSimpleName();
  private final Map mMap = new Map();

  public void adjustCompass(int offsetX, int offsetY)
  {
    mMap.setupCompass(requireContext(), offsetX, offsetY, true);
  }

  public void adjustBottomWidgets(int offsetX, int offsetY)
  {
    mMap.setupBottomWidgetsOffset(requireContext(), offsetX, offsetY);
  }

  public void destroySurface()
  {
    mMap.onSurfaceDestroyed(requireActivity().isChangingConfigurations(), isAdded());
  }

  public boolean isContextCreated()
  {
    return mMap.isContextCreated();
  }

  @Override
  public void surfaceCreated(SurfaceHolder surfaceHolder)
  {
    int densityDpi;

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
      densityDpi = ConfigurationHelper.getDensityDpi(requireContext().getResources());
    else
      densityDpi = getDensityDpiOld();

    mMap.onSurfaceCreated(requireContext(), surfaceHolder.getSurface(), surfaceHolder.getSurfaceFrame(), densityDpi);
  }

  @Override
  public void surfaceChanged(SurfaceHolder surfaceHolder, int format, int width, int height)
  {
    mMap.onSurfaceChanged(requireContext(), surfaceHolder.getSurface(), surfaceHolder.getSurfaceFrame(), surfaceHolder.isCreating());
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder surfaceHolder)
  {
    Logger.d(TAG, "surfaceDestroyed");
    destroySurface();
  }

  @Override
  public void onAttach(Context context)
  {
    super.onAttach(context);
    mMap.setMapRenderingListener((MapRenderingListener) context);
    mMap.setCallbackUnsupported(this::reportUnsupported);
  }

  @Override
  public void onDetach()
  {
    super.onDetach();
    mMap.setMapRenderingListener(null);
    mMap.setCallbackUnsupported(null);
  }

  @Override
  public void onCreate(Bundle b)
  {
    super.onCreate(b);
    setRetainInstance(true);
    boolean launchByDeepLink = false;
    Bundle args = getArguments();
    if (args != null)
      launchByDeepLink = args.getBoolean(Map.ARG_LAUNCH_BY_DEEP_LINK);
    mMap.onCreate(launchByDeepLink);
  }

  @Override
  public void onStart()
  {
    super.onStart();
    mMap.onStart();
    Logger.d(TAG, "onStart");
  }

  public void onStop()
  {
    super.onStop();
    mMap.onStop();
    Logger.d(TAG, "onStop");
  }

  @Override
  public void onPause()
  {
    super.onPause();
    mMap.onPause(requireContext());
  }

  @Override
  public void onResume()
  {
    super.onResume();
    mMap.onResume();
  }

  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    View view = inflater.inflate(R.layout.fragment_map, container, false);
    final SurfaceView mSurfaceView = view.findViewById(R.id.map_surfaceview);
    mSurfaceView.getHolder().addCallback(this);
    return view;
  }

  @Override
  public boolean onTouch(View view, MotionEvent event)
  {
    int action = event.getActionMasked();
    int pointerIndex = event.getActionIndex();
    switch (action)
    {
    case MotionEvent.ACTION_POINTER_UP:
      action = Map.NATIVE_ACTION_UP;
      break;
    case MotionEvent.ACTION_UP:
      action = Map.NATIVE_ACTION_UP;
      pointerIndex = 0;
      break;
    case MotionEvent.ACTION_POINTER_DOWN:
      action = Map.NATIVE_ACTION_DOWN;
      break;
    case MotionEvent.ACTION_DOWN:
      action = Map.NATIVE_ACTION_DOWN;
      pointerIndex = 0;
      break;
    case MotionEvent.ACTION_MOVE:
      action = Map.NATIVE_ACTION_MOVE;
      pointerIndex = Map.INVALID_POINTER_MASK;
      break;
    case MotionEvent.ACTION_CANCEL:
      action = Map.NATIVE_ACTION_CANCEL;
      break;
    }
    mMap.onTouch(action, event, pointerIndex);
    return true;
  }

  private void reportUnsupported()
  {
    new AlertDialog.Builder(requireContext(), R.style.MwmTheme_AlertDialog)
        .setMessage(R.string.unsupported_phone)
        .setCancelable(false)
        .setPositiveButton(R.string.close, (dlg, which) -> requireActivity().moveTaskToBack(true))
        .show();
  }

  @SuppressWarnings("deprecation")
  private int getDensityDpiOld()
  {
    final DisplayMetrics metrics = new DisplayMetrics();
    requireActivity().getWindowManager().getDefaultDisplay().getMetrics(metrics);
    return metrics.densityDpi;
  }
}
