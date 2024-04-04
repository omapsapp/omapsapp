package app.organicmaps.settings;

import android.app.Activity;
import android.content.Intent;

import androidx.activity.result.ActivityResultLauncher;
import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import app.organicmaps.base.BaseMwmFragmentActivity;

public class DrivingOptionsActivity extends BaseMwmFragmentActivity
{
  @Override
  protected Class<? extends Fragment> getFragmentClass()
  {
    return DrivingOptionsFragment.class;
  }

  public static void start(@NonNull Activity activity, ActivityResultLauncher<Intent> startDrivingOptionsForResult)
  {
    Intent intent = new Intent(activity, DrivingOptionsActivity.class);
    startDrivingOptionsForResult.launch(intent);
  }
}
