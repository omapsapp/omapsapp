package app.organicmaps.car.screens.download;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import app.organicmaps.downloader.CountryItem;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

public final class DownloaderHelpers
{
  static final String[] WORLD_MAPS = new String[]{"World", "WorldCoasts"};

  public static boolean isWorldMapsDownloadNeeded()
  {
    return !CountryItem.fill(WORLD_MAPS[0]).present || !CountryItem.fill(WORLD_MAPS[1]).present;
  }

  @NonNull
  static List<CountryItem> getCountryItemsFromIds(@Nullable final String[] countryIds)
  {
    final List<CountryItem> countryItems = new ArrayList<>();
    if (countryIds != null)
    {
      for (final String countryId : countryIds)
        countryItems.add(CountryItem.fill(countryId));
    }

    return countryItems;
  }

  static long getMapsSize(@NonNull final Collection<CountryItem> countries)
  {
    long totalSize = 0;

    for (final CountryItem item : countries)
      totalSize += item.totalSize;

    return totalSize;
  }

  @NonNull
  static String getCountryName(@NonNull CountryItem country)
  {
    boolean hasParent = !CountryItem.isRoot(country.topmostParentId) && !TextUtils.isEmpty(country.topmostParentName);
    final StringBuilder sb = new StringBuilder();
    if (hasParent)
    {
      sb.append(country.topmostParentName);
      sb.append(" • ");
    }
    sb.append(country.name);
    return sb.toString();
  }

  private DownloaderHelpers() {}
}
