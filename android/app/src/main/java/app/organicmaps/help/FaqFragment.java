package app.organicmaps.help;

import android.content.DialogInterface;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.activity.result.ActivityResultLauncher;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import app.organicmaps.R;
import app.organicmaps.WebContainerDelegate;
import app.organicmaps.base.BaseMwmFragment;
import app.organicmaps.util.Constants;
import app.organicmaps.util.SharingUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.util.WindowInsetUtils;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.floatingactionbutton.FloatingActionButton;

public class FaqFragment extends BaseMwmFragment
{
  private ActivityResultLauncher<SharingUtils.SharingIntent> shareLauncher;

  @NonNull
  private final DialogInterface.OnClickListener mDialogClickListener = new DialogInterface.OnClickListener()
  {
    private void sendGeneralFeedback()
    {
      Utils.sendFeedback(shareLauncher, requireActivity());
    }

    private void reportBug()
    {
      Utils.sendBugReport(shareLauncher, requireActivity(), "", "");
    }

    @Override
    public void onClick(DialogInterface dialog, int which)
    {
      switch (which)
      {
        case 0 -> sendGeneralFeedback();
        case 1 -> reportBug();
      }
    }
  };

  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.fragment_prefs_faq, container, false);

    ViewCompat.setOnApplyWindowInsetsListener(root, WindowInsetUtils.PaddingInsetsListener.excludeTop());

    new WebContainerDelegate(root, Constants.Url.FAQ)
    {
      @Override
      protected void doStartActivity(Intent intent)
      {
        startActivity(intent);
      }
    };

    FloatingActionButton feedbackFab = root.findViewById(R.id.feedback_fab);
    feedbackFab.setOnClickListener(v -> new MaterialAlertDialogBuilder(requireActivity(), R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.feedback)
        .setNegativeButton(R.string.cancel, null)
        .setItems(new CharSequence[] { getString(R.string.feedback_general), getString(R.string.report_a_bug) },
                  mDialogClickListener)
        .show());

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
    {
      root.findViewById(R.id.webview).setOnScrollChangeListener((v, scrollX, scrollY, oldScrollX, oldScrollY) -> {
        if (scrollY > oldScrollY)
          feedbackFab.hide();
        else
          feedbackFab.show();
      });
    }

    shareLauncher = SharingUtils.RegisterLauncher(this);

    return root;
  }
}
