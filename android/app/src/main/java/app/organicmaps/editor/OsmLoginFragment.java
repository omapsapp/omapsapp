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

import app.organicmaps.sdk.Framework;
import app.organicmaps.R;
import app.organicmaps.base.BaseMwmToolbarFragment;
import app.organicmaps.sdk.editor.OsmOAuth;
import app.organicmaps.sdk.util.Constants;
import app.organicmaps.sdk.util.DateUtils;
import app.organicmaps.util.InputUtils;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;
import app.organicmaps.sdk.util.concurrency.ThreadPool;
import app.organicmaps.sdk.util.concurrency.UiThread;
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
      final String oauthToken = OsmOAuth.nativeAuthWithPassword(username, password);
      final String username1 = (oauthToken == null) ? null : OsmOAuth.nativeGetOsmUsername(oauthToken);
      UiThread.run(() -> processAuth(oauthToken, username1));
    });
  }

  private void enableInput(boolean enable)
  {
    mPasswordInput.setEnabled(enable);
    mLoginInput.setEnabled(enable);
    mLoginButton.setEnabled(enable);
    mLostPasswordButton.setEnabled(enable);
  }

  private void processAuth(String oauthToken, String username)
  {
    if (!isAdded())
      return;

    enableInput(true);
    UiUtils.hide(mProgress);
    mLoginButton.setText(R.string.login_osm);
    if (oauthToken == null)
      onAuthFail();
    else
      onAuthSuccess(oauthToken, username);
  }

  private void onAuthFail()
  {
    new MaterialAlertDialogBuilder(requireActivity(), R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.editor_login_error_dialog)
        .setPositiveButton(R.string.ok, null)
        .show();
  }

  private void onAuthSuccess(String oauthToken, String username)
  {
    OsmOAuth.setAuthorization(requireContext(), oauthToken, username);
    final Bundle extras = requireActivity().getIntent().getExtras();
    if (extras != null && extras.getBoolean("redirectToProfile", false))
      startActivity(new Intent(requireContext(), ProfileActivity.class));
    requireActivity().finish();
  }
}
