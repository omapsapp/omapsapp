package app.organicmaps.editor;

import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Size;

import app.organicmaps.Framework;
import app.organicmaps.R;
import app.organicmaps.base.BaseMwmToolbarFragment;
import app.organicmaps.util.Config;
import app.organicmaps.util.Constants;
import app.organicmaps.util.DateUtils;
import app.organicmaps.util.InputUtils;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.util.concurrency.ThreadPool;
import app.organicmaps.util.concurrency.UiThread;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.textfield.TextInputEditText;

public class OsmLoginFragment extends BaseMwmToolbarFragment
{
  private ProgressBar mProgress;
  private Button mLoginButton;
  private Button mLostPasswordButton;
  private TextInputEditText mLoginInput;
  private TextInputEditText mPasswordInput;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_osm_login, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    getToolbarController().setTitle(R.string.login);
    mLoginInput = view.findViewById(R.id.osm_username);
    mPasswordInput = view.findViewById(R.id.osm_password);
    mLoginButton = view.findViewById(R.id.login);
    mLoginButton.setOnClickListener((v) -> login());
    mLostPasswordButton = view.findViewById(R.id.lost_password);
    mLostPasswordButton.setOnClickListener((v) -> Utils.openUrl(requireActivity(), Constants.Url.OSM_RECOVER_PASSWORD));
    Button registerButton = view.findViewById(R.id.register);
    registerButton.setOnClickListener((v) -> Utils.openUrl(requireActivity(), Constants.Url.OSM_REGISTER));
    mProgress = view.findViewById(R.id.osm_login_progress);
    final String dataVersion = DateUtils.getShortDateFormatter().format(Framework.getDataVersion());
    ((TextView) view.findViewById(R.id.osm_presentation))
        .setText(getString(R.string.osm_presentation, dataVersion));

    if (!Config.isOsmLoginEnabled(requireContext()))
    {
      new MaterialAlertDialogBuilder(requireActivity(), R.style.MwmTheme_AlertDialog)
          .setMessage(R.string.osm_login_not_available)
          .setCancelable(true)
          .setNegativeButton(R.string.details, (dialog, which) ->
              Utils.openUrl(requireContext(), "https://github.com/organicmaps/organicmaps/issues/7000"))
          .setOnDismissListener(dialog -> requireActivity().finish())
          .show();
    }
  }

  private void login()
  {
    InputUtils.hideKeyboard(mLoginInput);
    final String username = mLoginInput.getText().toString();
    final String password = mPasswordInput.getText().toString();
    enableInput(false);
    UiUtils.show(mProgress);
    mLoginButton.setText("");

    ThreadPool.getWorker().execute(() -> {
      final String[] auth = OsmOAuth.nativeAuthWithPassword(username, password);
      final String username1 = auth == null ? null : OsmOAuth.nativeGetOsmUsername(auth[0], auth[1]);
      UiThread.run(() -> processAuth(auth, username1));
    });
  }

  private void enableInput(boolean enable)
  {
    mPasswordInput.setEnabled(enable);
    mLoginInput.setEnabled(enable);
    mLoginButton.setEnabled(enable);
    mLostPasswordButton.setEnabled(enable);
  }

  private void processAuth(@Size(2) String[] auth, String username)
  {
    if (!isAdded())
      return;

    enableInput(true);
    UiUtils.hide(mProgress);
    mLoginButton.setText(R.string.login_osm);
    if (auth == null)
      onAuthFail();
    else
      onAuthSuccess(auth, username);
  }

  private void onAuthFail()
  {
    new MaterialAlertDialogBuilder(requireActivity(), R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.editor_login_error_dialog)
        .setPositiveButton(R.string.ok, null)
        .show();
  }

  private void onAuthSuccess(@Size(2) String[] auth, String username)
  {
    OsmOAuth.setAuthorization(requireContext(), auth[0], auth[1], username);
    final Bundle extras = requireActivity().getIntent().getExtras();
    if (extras != null && extras.getBoolean("redirectToProfile", false))
      startActivity(new Intent(requireContext(), ProfileActivity.class));
    requireActivity().finish();
  }
}
