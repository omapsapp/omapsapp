package app.organicmaps.downloader;

import android.location.Location;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;

import app.organicmaps.R;
import app.organicmaps.base.BaseMwmFragment;
import app.organicmaps.base.BaseMwmFragmentActivity;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.sdk.downloader.CountryItem;
import app.organicmaps.sdk.downloader.MapManager;
import app.organicmaps.widget.WheelProgressView;
import app.organicmaps.util.StringUtils;
import app.organicmaps.util.UiUtils;

import java.util.List;

public class CountrySuggestFragment extends BaseMwmFragment implements View.OnClickListener
{
  private LinearLayout mLlWithLocation;
  private LinearLayout mLlNoLocation;
  private LinearLayout mLlSelectDownload;
  private LinearLayout mLlActiveDownload;
  private WheelProgressView mWpvDownloadProgress;
  private TextView mTvCountry;
  private TextView mTvActiveCountry;
  private TextView mTvProgress;
  private Button mBtnDownloadMap;

  private CountryItem mCurrentCountry;
  private CountryItem mDownloadingCountry;
  private int mListenerSlot;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_suggest_country_download, container, false);
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    initViews(view);
    mListenerSlot = MapManager.nativeSubscribe(new MapManager.StorageCallback()
    {
      @Override
      public void onStatusChanged(List<MapManager.StorageCallbackData> data)
      {
        if (!isAdded())
          return;

        for (MapManager.StorageCallbackData item : data)
        {
          if (!item.isLeafNode)
            continue;

          if (mDownloadingCountry == null)
            mDownloadingCountry = CountryItem.fill(item.countryId);
          else if (!item.countryId.equals(mDownloadingCountry.id))
            continue;

          switch (item.newStatus)
          {
          case CountryItem.STATUS_FAILED:
            updateViews();
            return;

          case CountryItem.STATUS_DONE:
            exitFragment();
            return;
          }

          break;
        }

        updateViews();
      }

      @Override
      public void onProgress(String countryId, long localSize, long remoteSize)
      {
        if (!isAdded())
          return;

        if (mDownloadingCountry == null)
          mDownloadingCountry = CountryItem.fill(countryId);
        else
          mDownloadingCountry.update();

        updateProgress();
      }
    });
  }

  private void exitFragment()
  {
    // TODO find more elegant way
    getParentFragment().getChildFragmentManager().beginTransaction().remove(this).commitAllowingStateLoss();
  }

  @Override
  public void onResume()
  {
    super.onResume();

    Location loc = LocationHelper.from(requireContext()).getSavedLocation();
    if (loc != null)
    {
      String id = MapManager.nativeFindCountry(loc.getLatitude(), loc.getLongitude());
      if (!TextUtils.isEmpty(id))
        mCurrentCountry = CountryItem.fill(id);
    }

    updateViews();
  }

  @Override
  public void onDestroy()
  {
    super.onDestroy();
    MapManager.nativeUnsubscribe(mListenerSlot);
  }

  private void refreshDownloadButton()
  {
    if (mCurrentCountry == null || !isAdded())
      return;

    mBtnDownloadMap.setText(StringUtils.formatUsingUsLocale("%1$s (%2$s)",
                                          getString(R.string.downloader_download_map),
                                          StringUtils.getFileSizeString(requireContext(), mCurrentCountry.totalSize)));
  }

  private void initViews(View view)
  {
    mLlSelectDownload = view.findViewById(R.id.ll__select_download);
    mLlActiveDownload = view.findViewById(R.id.ll__active_download);
    mLlWithLocation = view.findViewById(R.id.ll__location_determined);
    mLlNoLocation = view.findViewById(R.id.ll__location_unknown);
    mBtnDownloadMap = view.findViewById(R.id.btn__download_map);
    mBtnDownloadMap.setOnClickListener(this);
    Button selectMap = view.findViewById(R.id.btn__select_map);
    selectMap.setOnClickListener(this);
    mWpvDownloadProgress = view.findViewById(R.id.wpv__download_progress);
    mWpvDownloadProgress.setOnClickListener(this);
    mTvCountry = view.findViewById(R.id.tv__country_name);
    mTvActiveCountry = view.findViewById(R.id.tv__active_country_name);
    mTvProgress = view.findViewById(R.id.downloader_progress);
  }

  private void updateViews()
  {
    if (!isAdded() || MapManager.nativeGetDownloadedCount() > 0)
      return;

    boolean downloading = MapManager.nativeIsDownloading();
    UiUtils.showIf(downloading, mLlActiveDownload);
    UiUtils.showIf(!downloading, mLlSelectDownload);

    if (!downloading)
    {
      boolean hasLocation = (mCurrentCountry != null);
      UiUtils.showIf(hasLocation, mLlWithLocation);
      UiUtils.showIf(!hasLocation, mLlNoLocation);
      refreshDownloadButton();

      if (hasLocation)
        mTvCountry.setText(mCurrentCountry.name);

      if (mDownloadingCountry != null)
      {
        mDownloadingCountry.progress = 0;
        updateProgress();
      }
      return;
    }

    mTvActiveCountry.setText(mDownloadingCountry.name);
    updateProgress();
  }

  private void updateProgress()
  {
    String text = StringUtils.formatUsingSystemLocale("%1$s %2$.2f%%", getString(R.string.downloader_downloading),
      mDownloadingCountry.progress);
    mTvProgress.setText(text);
    mWpvDownloadProgress.setProgress(Math.round(mDownloadingCountry.progress));
  }

  @Override
  public void onClick(View v)
  {
    final int id = v.getId();
    if (id == R.id.btn__download_map)
    {
      MapManager.warn3gAndDownload(requireActivity(), mCurrentCountry.id, () -> mDownloadingCountry = mCurrentCountry);
    }
    else if (id == R.id.btn__select_map)
    {
      final FragmentActivity fragmentActivity = requireActivity();
      if (!(fragmentActivity instanceof BaseMwmFragmentActivity))
        throw new IllegalStateException("Activity is not instance of BaseMwmFragmentActivity");
      ((BaseMwmFragmentActivity) fragmentActivity).replaceFragment(DownloaderFragment.class, null, null);
    }
    else if (id == R.id.wpv__download_progress)
      MapManager.nativeCancel(mDownloadingCountry.id);
  }
}
