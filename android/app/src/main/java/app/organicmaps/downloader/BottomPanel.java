package app.organicmaps.downloader;

import android.view.View;
import android.widget.Button;

import com.google.android.material.floatingactionbutton.FloatingActionButton;
import app.organicmaps.R;
import app.organicmaps.sdk.downloader.CountryItem;
import app.organicmaps.sdk.downloader.MapManager;
import app.organicmaps.sdk.downloader.UpdateInfo;
import app.organicmaps.util.StringUtils;
import app.organicmaps.util.UiUtils;
import static app.organicmaps.sdk.downloader.CountryItem.*;

class BottomPanel
{
  private final DownloaderFragment mFragment;
  private final FloatingActionButton mFab;
  private final Button mButton;

  private final View.OnClickListener mDownloadListener = new View.OnClickListener()
  {
    @Override
    public void onClick(View v)
    {
      MapManager.warn3gAndDownload(mFragment.requireActivity(), mFragment.getCurrentRoot(), null);
    }
  };

  private final View.OnClickListener mUpdateListener = new View.OnClickListener()
  {
    @Override
    public void onClick(View v)
    {
      final String country = mFragment.getCurrentRoot();
      MapManager.warnOn3gUpdate(mFragment.requireActivity(), country, () -> MapManager.nativeUpdate(country));
    }
  };

  private final View.OnClickListener mCancelListener = new View.OnClickListener()
  {
    @Override
    public void onClick(View v)
    {
        MapManager.nativeCancel(mFragment.getCurrentRoot());
    }
  };

  BottomPanel(DownloaderFragment fragment, View frame)
  {
    mFragment = fragment;

    mFab = frame.findViewById(R.id.fab);
    mFab.setOnClickListener(v -> {
      if (mFragment.getAdapter() != null )
        mFragment.getAdapter().setAvailableMapsMode();
      update();
    });

    mButton = frame.findViewById(R.id.action);
  }

  private void setUpdateAllState(UpdateInfo info)
  {
    mButton.setText(StringUtils.formatUsingUsLocale("%s (%s)", mFragment.getString(R.string.downloader_update_all_button),
                                  StringUtils.getFileSizeString(mFragment.requireContext(), info.totalSize)));
    mButton.setOnClickListener(mUpdateListener);
  }

  private void setDownloadAllState()
  {
    mButton.setText(R.string.downloader_download_all_button);
    mButton.setOnClickListener(mDownloadListener);
  }

  private void setCancelState()
  {
    mButton.setText(R.string.downloader_cancel_all);
    mButton.setOnClickListener(mCancelListener);
  }

  public void update()
  {
    DownloaderAdapter adapter = mFragment.getAdapter();
    boolean search = adapter.isSearchResultsMode();

    boolean show = !search;
    UiUtils.showIf(show && adapter.isMyMapsMode(), mFab);

    if (show)
    {
      String root = adapter.getCurrentRootId();
      int status = MapManager.nativeGetStatus(root);
      if (adapter.isMyMapsMode())
      {
        switch (status)
        {
          case STATUS_UPDATABLE ->
          {
            UpdateInfo info = MapManager.nativeGetUpdateInfo(root);
            setUpdateAllState(info);
          }  // Special case for "Countries" node when no maps currently downloaded.
          case STATUS_DOWNLOADABLE, STATUS_DONE, STATUS_PARTLY -> show = false;
          case STATUS_PROGRESS, STATUS_APPLYING, STATUS_ENQUEUED -> setCancelState();
          case STATUS_FAILED -> setDownloadAllState();
          default -> throw new IllegalArgumentException("Inappropriate status for \"" + root + "\": " + status);
        }
      }
      else
      {
        show = !CountryItem.isRoot(root);
        if (show)
        {
          switch (status)
          {
            case STATUS_UPDATABLE ->
            {
              UpdateInfo info = MapManager.nativeGetUpdateInfo(root);
              setUpdateAllState(info);
            }
            case STATUS_DONE -> show = false;
            case STATUS_PROGRESS, STATUS_APPLYING, STATUS_ENQUEUED -> setCancelState();
            default -> setDownloadAllState();
          }
        }
      }
    }

    UiUtils.showIf(show, mButton);
  }
}
