package app.organicmaps.editor;

import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import app.organicmaps.R;
import app.organicmaps.base.BaseMwmToolbarFragment;
import app.organicmaps.sdk.editor.OsmOAuth;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.sdk.util.concurrency.ThreadPool;
import app.organicmaps.sdk.util.concurrency.UiThread;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

public class ProfileFragment extends BaseMwmToolbarFragment
{
  private TextView mEditsSent;
  private ProgressBar mEditsSentProgress;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_osm_profile, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    getToolbarController().setTitle(R.string.profile);
    initViews(view);
    refreshViews();
  }

  private void initViews(View view)
  {
    View logoutButton = getToolbarController().getToolbar().findViewById(R.id.logout);
    logoutButton.setOnClickListener((v) -> logout());
    View editsBlock = view.findViewById(R.id.block_edits);
    UiUtils.show(editsBlock);
    View sentBlock = editsBlock.findViewById(R.id.sent_edits);
    mEditsSent = sentBlock.findViewById(R.id.edits_count);
    mEditsSentProgress = sentBlock.findViewById(R.id.edits_count_progress);
    view.findViewById(R.id.about_osm).setOnClickListener((v) -> Utils.openUrl(requireActivity(), getString(R.string.osm_wiki_about_url)));
    view.findViewById(R.id.osm_history).setOnClickListener((v) -> Utils.openUrl(requireActivity(), OsmOAuth.getHistoryUrl(requireContext())));
  }

  private void refreshViews()
  {
    if (OsmOAuth.isAuthorized(requireContext()))
    {
      // Update the number of uploaded changesets from OSM.
      ThreadPool.getWorker().execute(() -> {
        if (mEditsSent.getText().equals(""))
        {
          UiUtils.hide(mEditsSent);
          UiUtils.show(mEditsSentProgress);
        }
        final int count = OsmOAuth.getOsmChangesetsCount(requireContext(), getParentFragmentManager());
        UiThread.run(() -> {
          mEditsSent.setText(String.valueOf(count));
          UiUtils.show(mEditsSent);
          UiUtils.hide(mEditsSentProgress);
        });
      });
    }
    else
    {
      Intent intent = new Intent(requireContext(), OsmLoginActivity.class);
      intent.putExtra("redirectToProfile", true);
      startActivity(intent);
      requireActivity().finish();
    }
  }

  private void logout()
  {
    new MaterialAlertDialogBuilder(requireContext(), R.style.MwmTheme_AlertDialog)
        .setMessage(R.string.are_you_sure)
        .setPositiveButton(R.string.yes, (dialog, which) ->
        {
          OsmOAuth.clearAuthorization(requireContext());
          refreshViews();
        })
        .setNegativeButton(R.string.no, null)
        .show();
  }
}
