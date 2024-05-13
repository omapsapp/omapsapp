package app.organicmaps.intent;

import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.core.content.IntentCompat;

import app.organicmaps.sdk.Framework;
import app.organicmaps.sdk.Map;
import app.organicmaps.MwmActivity;
import app.organicmaps.MwmApplication;
import app.organicmaps.sdk.OrganicMaps;
import app.organicmaps.sdk.api.ParsedRoutingData;
import app.organicmaps.sdk.api.ParsedSearchRequest;
import app.organicmaps.sdk.api.RequestType;
import app.organicmaps.sdk.api.RoutePoint;
import app.organicmaps.sdk.bookmarks.data.BookmarkManager;
import app.organicmaps.sdk.bookmarks.data.FeatureId;
import app.organicmaps.sdk.bookmarks.data.MapObject;
import app.organicmaps.routing.RoutingController;
import app.organicmaps.search.SearchActivity;
import app.organicmaps.search.SearchEngine;
import app.organicmaps.sdk.util.StorageUtils;
import app.organicmaps.sdk.util.concurrency.ThreadPool;

import java.io.File;
import java.util.Collections;
import java.util.List;

public class Factory
{
  public static boolean isStartedForApiResult(@NonNull Intent intent)
  {
    return (intent.getFlags() & Intent.FLAG_ACTIVITY_FORWARD_RESULT) != 0;
  }

  public static class KmzKmlProcessor implements IntentProcessor
  {
    @Override
    public boolean process(@NonNull Intent intent, @NonNull MwmActivity activity)
    {
      // See KML/KMZ/KMB intent filters in manifest.
      final List<Uri> uris;
      if (Intent.ACTION_VIEW.equals(intent.getAction()))
        uris = Collections.singletonList(intent.getData());
      else if (Intent.ACTION_SEND.equals(intent.getAction()))
        uris = Collections.singletonList(IntentCompat.getParcelableExtra(intent, Intent.EXTRA_STREAM, Uri.class));
      else if (Intent.ACTION_SEND_MULTIPLE.equals(intent.getAction()))
        uris = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
      else
        uris = null;
      if (uris == null)
        return false;

      final OrganicMaps om = OrganicMaps.from(activity);
      final File tempDir = new File(StorageUtils.getTempPath(om));
      final ContentResolver resolver = activity.getContentResolver();
      ThreadPool.getStorage().execute(() -> BookmarkManager.INSTANCE.importBookmarksFiles(resolver, uris, tempDir));
      return false;
    }
  }

  public static class UrlProcessor implements IntentProcessor
  {
    private static final int SEARCH_IN_VIEWPORT_ZOOM = 16;

    @Override
    public boolean process(@NonNull Intent intent, @NonNull MwmActivity target)
    {
      final Uri uri = intent.getData();
      if (uri == null)
        return false;

      switch (Framework.nativeParseAndSetApiUrl(uri.toString()))
      {
        case RequestType.INCORRECT:
          return false;

        case RequestType.MAP:
          SearchEngine.INSTANCE.cancelInteractiveSearch();
          Map.executeMapApiRequest();
          return true;

        case RequestType.ROUTE:
          SearchEngine.INSTANCE.cancelInteractiveSearch();
          final ParsedRoutingData data = Framework.nativeGetParsedRoutingData();
          RoutingController.get().setRouterType(data.mRouterType);
          final RoutePoint from = data.mPoints[0];
          final RoutePoint to = data.mPoints[1];
          RoutingController.get().prepare(MapObject.createMapObject(FeatureId.EMPTY, MapObject.API_POINT,
                                                                    from.mName, "", from.mLat, from.mLon),
                                          MapObject.createMapObject(FeatureId.EMPTY, MapObject.API_POINT,
                                                                    to.mName, "", to.mLat, to.mLon), true);
          return true;
        case RequestType.SEARCH:
        {
          SearchEngine.INSTANCE.cancelInteractiveSearch();
          final ParsedSearchRequest request = Framework.nativeGetParsedSearchRequest();
          final double[] latlon = Framework.nativeGetParsedCenterLatLon();
          if (latlon != null)
          {
            Framework.nativeStopLocationFollow();
            Framework.nativeSetViewportCenter(latlon[0], latlon[1], SEARCH_IN_VIEWPORT_ZOOM);
            // We need to update viewport for search api manually because of drape engine
            // will not notify subscribers when search activity is shown.
            if (!request.mIsSearchOnMap)
              Framework.nativeSetSearchViewport(latlon[0], latlon[1], SEARCH_IN_VIEWPORT_ZOOM);
          }
          SearchActivity.start(target, request.mQuery, request.mLocale, request.mIsSearchOnMap);
          return true;
        }
        case RequestType.CROSSHAIR:
        {
          SearchEngine.INSTANCE.cancelInteractiveSearch();
          target.showPositionChooserForAPI(Framework.nativeGetParsedAppName());

          final double[] latlon = Framework.nativeGetParsedCenterLatLon();
          if (latlon != null)
          {
            Framework.nativeStopLocationFollow();
            Framework.nativeSetViewportCenter(latlon[0], latlon[1], SEARCH_IN_VIEWPORT_ZOOM);
          }

          return true;
        }
      }

      return false;
    }
  }
}
