package app.organicmaps.editor;

import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import app.organicmaps.R;
import app.organicmaps.base.BaseMwmToolbarFragment;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.util.concurrency.ThreadPool;
import app.organicmaps.util.concurrency.UiThread;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import java.text.NumberFormat;

public class ProfileFragment extends BaseMwmToolbarFragment
{
  private View mUserInfoBlock;
  private TextView mEditsSent;
  private TextView mProfileName;
  private ImageView mProfileImage;
  private ProgressBar mProfileInfoLoading;

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
    mUserInfoBlock = view.findViewById(R.id.block_user_info);
    mProfileInfoLoading = view.findViewById(R.id.user_profile_loading);
    mEditsSent = mUserInfoBlock.findViewById(R.id.user_sent_edits);
    mProfileName = mUserInfoBlock.findViewById(R.id.user_profile_name);
    mProfileImage = mUserInfoBlock.findViewById(R.id.user_profile_image);
    view.findViewById(R.id.about_osm).setOnClickListener((v) -> Utils.openUrl(requireActivity(), getString(R.string.osm_wiki_about_url)));
    view.findViewById(R.id.osm_history).setOnClickListener((v) -> Utils.openUrl(requireActivity(), OsmOAuth.getHistoryUrl(requireContext())));
    view.findViewById(R.id.osm_notes).setOnClickListener((v) -> Utils.openUrl(requireActivity(), OsmOAuth.getNotesUrl(requireContext())));

  }

  private void refreshViews()
  {
    if (OsmOAuth.isAuthorized(requireContext()))
    {
      // Update the number of uploaded changesets from OSM.
      ThreadPool.getWorker().execute(() -> {
        if (mEditsSent.getText().equals(""))
        {
          UiUtils.show(mProfileInfoLoading);
          UiUtils.hide(mUserInfoBlock);
        }
        final int profileEditCount = OsmOAuth.getOsmChangesetsCount(requireContext(), getParentFragmentManager());
        final String profileUsername = OsmOAuth.getUsername(requireContext());
        final Bitmap profilePicture = OsmOAuth.getProfilePicture(requireContext());

        UiThread.run(() -> {
          mEditsSent.setText(NumberFormat.getInstance().format(profileEditCount));
          mProfileName.setText(profileUsername);

          // Use generic image if user has no profile picture or it failed to load.
          if (profilePicture != null)
            mProfileImage.setImageBitmap(profilePicture);
          else
            mProfileImage.setImageResource(R.drawable.profile_generic);

          UiUtils.show(mUserInfoBlock);
          UiUtils.hide(mProfileInfoLoading);
        });
      });
    }
    else
    {
      Intent intent = new Intent(requireContext(), OsmLoginActivity.class);
      intent.putExtra(ProfileActivity.EXTRA_REDIRECT_TO_PROFILE, true);
      startActivity(intent);
      requireActivity().finish();
    }
  }

  private void logout()
  {
    new MaterialAlertDialogBuilder(requireContext(), R.style.MwmTheme_AlertDialog)
        .setMessage(R.string.osm_log_out_confirmation)
        .setPositiveButton(R.string.yes, (dialog, which) ->
        {
          OsmOAuth.clearAuthorization(requireContext());
          refreshViews();
        })
        .setNegativeButton(R.string.no, null)
        .show();
  }
}
