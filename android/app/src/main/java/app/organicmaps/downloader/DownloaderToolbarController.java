package app.organicmaps.downloader;

import android.app.Activity;
import android.content.Intent;
import android.text.TextUtils;
import android.view.View;

import app.organicmaps.R;
import app.organicmaps.widget.SearchToolbarController;
import app.organicmaps.util.UiUtils;

class DownloaderToolbarController extends SearchToolbarController
{
  private final DownloaderFragment mFragment;

  DownloaderToolbarController(View root, Activity activity, DownloaderFragment fragment)
  {
    super(root, activity);
    mFragment = fragment;
    setHint(R.string.downloader_search_field_hint);
  }

  @Override
  public void onUpClick()
  {
    requireActivity().onBackPressed();
  }

  @Override
  protected void onTextChanged(String query)
  {
    if (!mFragment.isAdded() || !mFragment.shouldShowSearch())
      return;

    if (TextUtils.isEmpty(query))
      mFragment.cancelSearch();
    else
      mFragment.startSearch();
  }

  public void update()
  {
    boolean showSearch = mFragment.shouldShowSearch();
    String title = (showSearch ? "" : mFragment.getAdapter().getCurrentRootName());

    showSearchControls(showSearch);
    if (!showSearch)
      UiUtils.setupHomeUpButtonAsNavigationIcon(getToolbar(), mNavigationClickListener);
    else
      UiUtils.clearHomeUpButton(getToolbar());
    setTitle(title);
  }

  @Override
  protected int getVoiceInputPrompt()
  {
    return R.string.downloader_search_field_hint;
  }

  @Override
  protected void startVoiceRecognition(Intent intent)
  {
    mFragment.startVoiceRecognitionForResult.launch(intent);
  }

  @Override
  protected boolean supportsVoiceSearch()
  {
    return true;
  }
}
