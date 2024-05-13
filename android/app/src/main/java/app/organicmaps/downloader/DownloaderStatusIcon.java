package app.organicmaps.downloader;

import android.util.SparseIntArray;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.AttrRes;
import androidx.annotation.DrawableRes;

import app.organicmaps.R;
import app.organicmaps.sdk.downloader.CountryItem;
import app.organicmaps.widget.WheelProgressView;
import app.organicmaps.util.ThemeUtils;
import app.organicmaps.util.UiUtils;

public class DownloaderStatusIcon
{
  private final View mFrame;
  protected final ImageView mIcon;
  private final WheelProgressView mProgress;

  private static final SparseIntArray sIconsCache = new SparseIntArray();

  public DownloaderStatusIcon(View frame)
  {
    mFrame = frame;
    mIcon = mFrame.findViewById(R.id.downloader_status);
    mProgress = mFrame.findViewById(R.id.downloader_progress_wheel);
  }

  public DownloaderStatusIcon setOnIconClickListener(View.OnClickListener listener)
  {
    mIcon.setOnClickListener(listener);
    return this;
  }

  public DownloaderStatusIcon setOnCancelClickListener(View.OnClickListener listener)
  {
    mProgress.setOnClickListener(listener);
    return this;
  }

  protected @AttrRes int selectIcon(CountryItem country)
  {
    return switch (country.status)
    {
      case CountryItem.STATUS_DONE -> R.attr.status_done;
      case CountryItem.STATUS_DOWNLOADABLE, CountryItem.STATUS_PARTLY -> R.attr.status_downloadable;
      case CountryItem.STATUS_FAILED -> R.attr.status_failed;
      case CountryItem.STATUS_UPDATABLE -> R.attr.status_updatable;
      default -> throw new IllegalArgumentException("Inappropriate item status: " + country.status);
    };
  }

  private @DrawableRes int resolveIcon(@AttrRes int iconAttr)
  {
    int res = sIconsCache.get(iconAttr);
    if (res == 0)
    {
      res = ThemeUtils.getResource(mFrame.getContext(), R.attr.downloaderTheme, iconAttr);
      sIconsCache.put(iconAttr, res);
    }

    return res;
  }

  protected void updateIcon(CountryItem country)
  {
    @AttrRes int iconAttr = selectIcon(country);
    @DrawableRes int icon = resolveIcon(iconAttr);

    mIcon.setImageResource(icon);
  }

  public void update(CountryItem country)
  {
    boolean pending = (country.status == CountryItem.STATUS_ENQUEUED);
    boolean inProgress = (country.status == CountryItem.STATUS_PROGRESS ||
                          country.status == CountryItem.STATUS_APPLYING || pending);

    UiUtils.showIf(inProgress, mProgress);
    UiUtils.showIf(!inProgress, mIcon);
    mProgress.setPending(pending);

    if (inProgress)
    {
      if (!pending)
        mProgress.setProgress(Math.round(country.progress));
      return;
    }

    updateIcon(country);
  }

  public void show(boolean show)
  {
    UiUtils.showIf(show, mFrame);
  }

  public static void clearCache()
  {
    sIconsCache.clear();
  }
}
