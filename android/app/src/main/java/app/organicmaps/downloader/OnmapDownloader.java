package app.organicmaps.downloader;

import android.location.Location;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.Nullable;

import androidx.core.view.ViewCompat;
import app.organicmaps.MwmActivity;
import app.organicmaps.R;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.routing.RoutingController;
import app.organicmaps.sdk.downloader.CountryItem;
import app.organicmaps.sdk.downloader.MapManager;
import app.organicmaps.widget.WheelProgressView;
import app.organicmaps.sdk.util.Config;
import app.organicmaps.util.ConnectionState;
import app.organicmaps.util.StringUtils;
import app.organicmaps.util.UiUtils;

import java.util.List;

public class OnmapDownloader implements MwmActivity.LeftAnimationTrackListener
{
  private static boolean sAutodownloadLocked;

  private final MwmActivity mActivity;
  private final View mFrame;
  private final TextView mParent;
  private final TextView mTitle;
  private final TextView mSize;
  private final WheelProgressView mProgress;
  private final Button mButton;

  private int mStorageSubscriptionSlot;

  @Nullable
  private CountryItem mCurrentCountry;

  private final MapManager.StorageCallback mStorageCallback = new MapManager.StorageCallback()
  {
    @Override
    public void onStatusChanged(List<MapManager.StorageCallbackData> data)
    {
      if (mCurrentCountry == null)
        return;

      for (MapManager.StorageCallbackData item : data)
      {
        if (!item.isLeafNode)
          continue;

        if (item.newStatus == CountryItem.STATUS_FAILED)
          MapManager.showError(mActivity, item, null);

        if (mCurrentCountry.id.equals(item.countryId))
        {
          mCurrentCountry.update();
          updateProgressState(false);

          return;
        }
      }
    }

    @Override
    public void onProgress(String countryId, long localSize, long remoteSize)
    {
      if (mCurrentCountry != null && mCurrentCountry.id.equals(countryId))
      {
        mCurrentCountry.update();
        updateProgressState(false);
      }
    }
  };

  private final MapManager.CurrentCountryChangedListener mCountryChangedListener = new MapManager.CurrentCountryChangedListener()
  {
    @Override
    public void onCurrentCountryChanged(String countryId)
    {
      mCurrentCountry = (TextUtils.isEmpty(countryId) ? null : CountryItem.fill(countryId));
      updateState(true);
    }
  };

  public void updateState(boolean shouldAutoDownload)
  {
    updateStateInternal(shouldAutoDownload);
  }

  private static boolean isMapDownloading(@Nullable CountryItem country)
  {
    if (country == null) return false;

    boolean enqueued = country.status == CountryItem.STATUS_ENQUEUED;
    boolean progress = country.status == CountryItem.STATUS_PROGRESS;
    boolean applying = country.status == CountryItem.STATUS_APPLYING;
    return enqueued || progress || applying;
  }

  private void updateProgressState(boolean shouldAutoDownload)
  {
    updateStateInternal(shouldAutoDownload);
  }

  private void updateStateInternal(boolean shouldAutoDownload)
  {
    boolean showFrame = (mCurrentCountry != null &&
                         !mCurrentCountry.present &&
                         !RoutingController.get().isNavigating());
    if (showFrame)
    {
      boolean enqueued = (mCurrentCountry.status == CountryItem.STATUS_ENQUEUED);
      boolean progress = (mCurrentCountry.status == CountryItem.STATUS_PROGRESS ||
                          mCurrentCountry.status == CountryItem.STATUS_APPLYING);
      boolean failed = (mCurrentCountry.status == CountryItem.STATUS_FAILED);

      showFrame = (enqueued || progress || failed ||
                   mCurrentCountry.status == CountryItem.STATUS_DOWNLOADABLE);

      if (showFrame)
      {
        boolean hasParent = !CountryItem.isRoot(mCurrentCountry.topmostParentId);

        UiUtils.showIf(progress || enqueued, mProgress);
        UiUtils.showIf(!progress && !enqueued, mButton);
        UiUtils.showIf(hasParent, mParent);

        if (hasParent)
          mParent.setText(mCurrentCountry.topmostParentName);

        mTitle.setText(mCurrentCountry.name);

        String sizeText;

        if (progress)
        {
          mProgress.setPending(false);
          mProgress.setProgress(Math.round(mCurrentCountry.progress));
          sizeText = StringUtils.formatUsingSystemLocale("%1$s %2$.2f%%",
              mActivity.getString(R.string.downloader_downloading), mCurrentCountry.progress);
        }
        else
        {
          if (enqueued)
          {
            sizeText = mActivity.getString(R.string.downloader_queued);
            mProgress.setPending(true);
          }
          else
          {
            sizeText = StringUtils.getFileSizeString(mActivity.getApplicationContext(), mCurrentCountry.totalSize);

            if (shouldAutoDownload &&
                Config.isAutodownloadEnabled() &&
                !sAutodownloadLocked &&
                !failed &&
                ConnectionState.INSTANCE.isWifiConnected())
            {
              Location loc = LocationHelper.from(mActivity).getSavedLocation();
              if (loc != null)
              {
                String country = MapManager.nativeFindCountry(loc.getLatitude(), loc.getLongitude());
                if (TextUtils.equals(mCurrentCountry.id, country) &&
                    MapManager.nativeHasSpaceToDownloadCountry(country))
                {
                  MapManager.nativeDownload(mCurrentCountry.id);
                }
              }
            }

            mButton.setText(failed ? R.string.downloader_retry
                                   : R.string.download);
          }
        }

        mSize.setText(sizeText);
      }
    }

    UiUtils.showIf(showFrame, mFrame);
  }

  public OnmapDownloader(MwmActivity activity)
  {
    mActivity = activity;
    mFrame = activity.findViewById(R.id.onmap_downloader);
    mParent = mFrame.findViewById(R.id.downloader_parent);
    mTitle = mFrame.findViewById(R.id.downloader_title);
    mSize = mFrame.findViewById(R.id.downloader_size);

    View controls = mFrame.findViewById(R.id.downloader_controls_frame);
    mProgress = controls.findViewById(R.id.wheel_downloader_progress);
    mButton = controls.findViewById(R.id.downloader_button);

    mProgress.setOnClickListener(v -> {
      if (mCurrentCountry == null)
        return;

      MapManager.nativeCancel(mCurrentCountry.id);
      setAutodownloadLocked(true);
    });
      mButton.setOnClickListener(v -> MapManager.warnOn3g(mActivity, mCurrentCountry == null ? null :
      mCurrentCountry.id, () -> {
      if (mCurrentCountry == null)
        return;

      boolean retry = (mCurrentCountry.status == CountryItem.STATUS_FAILED);
      if (retry)
      {
        DownloaderNotifier.cancelNotification(mActivity.getApplicationContext());
        MapManager.nativeRetry(mCurrentCountry.id);
      }
      else
      {
        MapManager.nativeDownload(mCurrentCountry.id);
        mActivity.requestPostNotificationsPermission();
      }
    }));

    ViewCompat.setOnApplyWindowInsetsListener(mFrame, (view, windowInsets) -> {
      UiUtils.setViewInsetsPadding(view, windowInsets);
      return windowInsets;
    });
  }

  @Override
  public void onTrackStarted(boolean collapsed) {}

  @Override
  public void onTrackFinished(boolean collapsed) {}

  @Override
  public void onTrackLeftAnimation(float offset)
  {
    ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams)mFrame.getLayoutParams();
    lp.leftMargin = (int)offset;
    mFrame.setLayoutParams(lp);
  }

  public void onPause()
  {
    if (mStorageSubscriptionSlot > 0)
    {
      MapManager.nativeUnsubscribe(mStorageSubscriptionSlot);
      mStorageSubscriptionSlot = 0;

      MapManager.nativeUnsubscribeOnCountryChanged();
    }
  }

  public void onResume()
  {
    if (mStorageSubscriptionSlot == 0)
    {
      mStorageSubscriptionSlot = MapManager.nativeSubscribe(mStorageCallback);

      MapManager.nativeSubscribeOnCountryChanged(mCountryChangedListener);
    }
  }

  public static void setAutodownloadLocked(boolean locked)
  {
    sAutodownloadLocked = locked;
  }
}
