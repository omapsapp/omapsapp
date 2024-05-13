package app.organicmaps.sdk;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;
import androidx.annotation.Keep;
import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Size;

import app.organicmaps.sdk.api.ParsedRoutingData;
import app.organicmaps.sdk.api.ParsedSearchRequest;
import app.organicmaps.sdk.api.RequestType;
import app.organicmaps.sdk.bookmarks.data.DistanceAndAzimut;
import app.organicmaps.sdk.bookmarks.data.FeatureId;
import app.organicmaps.sdk.bookmarks.data.MapObject;
import app.organicmaps.sdk.routing.JunctionInfo;
import app.organicmaps.sdk.routing.RouteMarkData;
import app.organicmaps.sdk.routing.RoutePointInfo;
import app.organicmaps.sdk.routing.RoutingInfo;
import app.organicmaps.sdk.routing.TransitRouteInfo;
import app.organicmaps.sdk.settings.SpeedCameraMode;
import app.organicmaps.widget.placepage.PlacePageData;
import app.organicmaps.sdk.util.Constants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.time.Instant;
import java.util.Date;
import java.util.Locale;

/**
 * This class wraps android::Framework.cpp class
 * via static methods
 */
public class Framework
{
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({MAP_STYLE_CLEAR, MAP_STYLE_DARK, MAP_STYLE_VEHICLE_CLEAR, MAP_STYLE_VEHICLE_DARK, MAP_STYLE_OUTDOORS_CLEAR, MAP_STYLE_OUTDOORS_DARK})

  public @interface MapStyle {}

  public static final int MAP_STYLE_CLEAR = 0;
  public static final int MAP_STYLE_DARK = 1;
  public static final int MAP_STYLE_VEHICLE_CLEAR = 3;
  public static final int MAP_STYLE_VEHICLE_DARK = 4;
  public static final int MAP_STYLE_OUTDOORS_CLEAR = 5;
  public static final int MAP_STYLE_OUTDOORS_DARK = 6;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ ROUTER_TYPE_VEHICLE, ROUTER_TYPE_PEDESTRIAN, ROUTER_TYPE_BICYCLE, ROUTER_TYPE_TRANSIT, ROUTER_TYPE_RULER })

  public @interface RouterType {}

  public static final int ROUTER_TYPE_VEHICLE = 0;
  public static final int ROUTER_TYPE_PEDESTRIAN = 1;
  public static final int ROUTER_TYPE_BICYCLE = 2;
  public static final int ROUTER_TYPE_TRANSIT = 3;
  public static final int ROUTER_TYPE_RULER = 4;

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ROUTE_REBUILD_AFTER_POINTS_LOADING})
  public @interface RouteRecommendationType {}

  public static final int ROUTE_REBUILD_AFTER_POINTS_LOADING = 0;

  public interface PlacePageActivationListener
  {
    // Called from JNI.
    @Keep
    @SuppressWarnings("unused")
    void onPlacePageActivated(@NonNull PlacePageData data);

    // Called from JNI
    @Keep
    @SuppressWarnings("unused")
    void onPlacePageDeactivated(boolean switchFullScreenMode);
  }

  public interface RoutingListener
  {
    // Called from JNI
    @Keep
    @SuppressWarnings("unused")
    @MainThread
    void onRoutingEvent(int resultCode, String[] missingMaps);
  }

  public interface RoutingProgressListener
  {
    // Called from JNI.
    @Keep
    @SuppressWarnings("unused")
    @MainThread
    void onRouteBuildingProgress(float progress);
  }

  public interface RoutingRecommendationListener
  {
    // Called from JNI.
    @Keep
    @SuppressWarnings("unused")
    void onRecommend(@RouteRecommendationType int recommendation);
  }

  public interface RoutingLoadPointsListener
  {
    // Called from JNI.
    @Keep
    @SuppressWarnings("unused")
    void onRoutePointsLoaded(boolean success);
  }

  // Used by JNI.
  @Keep
  @SuppressWarnings("unused")
  public static class Params3dMode
  {
    public boolean enabled;
    public boolean buildings;
  }

  // Used by JNI.
  @Keep
  @SuppressWarnings("unused")
  public static class RouteAltitudeLimits
  {
    public int totalAscent;
    public int totalDescent;
    public String totalAscentString;
    public String totalDescentString;
    public boolean isMetricUnits;
  }

  // this class is just bridge between Java and C++ worlds, we must not create it
  private Framework() {}

  public static String getHttpGe0Url(double lat, double lon, double zoomLevel, String name)
  {
    return nativeGetGe0Url(lat, lon, zoomLevel, name).replaceFirst(
            Constants.Url.SHORT_SHARE_PREFIX, Constants.Url.HTTP_SHARE_PREFIX);
  }

  /**
   * Generates Bitmap with route altitude image chart taking into account current map style.
   * @param width is width of the image.
   * @param height is height of the image.
   * @return Bitmap if there's pedestrian or bicycle route and null otherwise.
   */
  @Nullable
  public static Bitmap generateRouteAltitudeChart(int width, int height,
                                                  @NonNull RouteAltitudeLimits limits)
  {
    if (width <= 0 || height <= 0)
      return null;

    final int[] altitudeChartBits = Framework.nativeGenerateRouteAltitudeChartBits(width, height,
                                                                                   limits);
    if (altitudeChartBits == null)
      return null;

    return Bitmap.createBitmap(altitudeChartBits, width, height, Bitmap.Config.ARGB_8888);
  }

  public static void setSpeedCamerasMode(@NonNull SpeedCameraMode mode)
  {
    nativeSetSpeedCamManagerMode(mode.ordinal());
  }

  public static native void nativeShowTrackRect(long track);

  public static native int nativeGetDrawScale();

  public static native void nativePokeSearchInViewport();

  @Size(2)
  public static native double[] nativeGetScreenRectCenter();

  public static native DistanceAndAzimut nativeGetDistanceAndAzimuth(double dstMerX, double dstMerY, double srcLat, double srcLon, double north);

  public static native DistanceAndAzimut nativeGetDistanceAndAzimuthFromLatLon(double dstLat, double dstLon, double srcLat, double srcLon, double north);

  public static native String nativeFormatLatLon(double lat, double lon, int coordFormat);

  public static native String nativeFormatAltitude(double alt);

  public static native String nativeFormatSpeed(double speed);

  public static native String nativeGetGe0Url(double lat, double lon, double zoomLevel, String name);

  public static native String nativeGetAddress(double lat, double lon);

  public static native void nativePlacePageActivationListener(@NonNull PlacePageActivationListener listener);

  public static native void nativeRemovePlacePageActivationListener(@NonNull PlacePageActivationListener listener);

//  @UiThread
//  public static native String nativeGetOutdatedCountriesString();
//
//  @UiThread
//  @NonNull
//  public static native String[] nativeGetOutdatedCountries();
//
//  @UiThread
//  @DoAfterUpdate
//  public static native int nativeToDoAfterUpdate();
//
//  public static native boolean nativeIsDataVersionChanged();
//
//  public static native void nativeUpdateSavedDataVersion();

  private static native long nativeGetDataVersion();

  public static Date getDataVersion()
  {
    long dataVersion = nativeGetDataVersion();
    final SimpleDateFormat format = new SimpleDateFormat("yyMMdd", Locale.ENGLISH);
    try
    {
      return format.parse(String.valueOf(dataVersion));
    }
    catch (ParseException e)
    {
      throw new AssertionError("Invalid data version code: " + dataVersion);
    }
  }

  public static native void nativeClearApiPoints();

  @NonNull
  public static native @RequestType int nativeParseAndSetApiUrl(String url);
  public static native ParsedRoutingData nativeGetParsedRoutingData();
  public static native ParsedSearchRequest nativeGetParsedSearchRequest();
  public static native @Nullable String nativeGetParsedAppName();
  @Nullable @Size(2)
  public static native double[] nativeGetParsedCenterLatLon();
  public static native @Nullable String nativeGetParsedBackUrl();

  public static native void nativeDeactivatePopup();

  public static native String nativeGetDataFileExt();

  public static native String[] nativeGetMovableFilesExts();

  public static native String[] nativeGetBookmarksFilesExts();

  public static native String nativeGetBookmarkDir();

  public static native String nativeGetSettingsDir();

  public static native String nativeGetWritableDir();

  public static native void nativeChangeWritableDir(String newPath);

  // Routing.
  public static native boolean nativeIsRoutingActive();

  public static native boolean nativeIsRouteBuilt();

  public static native boolean nativeIsRouteBuilding();

  public static native void nativeCloseRouting();

  public static native void nativeBuildRoute();

  public static native void nativeRemoveRoute();

  public static native void nativeFollowRoute();

  public static native void nativeDisableFollowing();

  @Nullable
  public static native RoutingInfo nativeGetRouteFollowingInfo();

  @Nullable
  public static native JunctionInfo[] nativeGetRouteJunctionPoints();

  @Nullable
  public static native final int[] nativeGenerateRouteAltitudeChartBits(int width, int height, RouteAltitudeLimits routeAltitudeLimits);

  // When an end user is going to a turn he gets sound turn instructions.
  // If C++ part wants the client to pronounce an instruction nativeGenerateTurnNotifications returns
  // an array of one of more strings. C++ part assumes that all these strings shall be pronounced by the client's TTS.
  // For example if C++ part wants the client to pronounce "Make a right turn." this method returns
  // an array with one string "Make a right turn.". The next call of the method returns nothing.
  // nativeGenerateTurnNotifications shall be called by the client when a new position is available.
  @Nullable
  public static native String[] nativeGenerateNotifications();

  private static native void nativeSetSpeedCamManagerMode(int mode);

  public static native void nativeSetRoutingListener(RoutingListener listener);

  public static native void nativeSetRouteProgressListener(RoutingProgressListener listener);

  public static native void nativeSetRoutingRecommendationListener(RoutingRecommendationListener listener);

  public static native void nativeSetRoutingLoadPointsListener(
      @Nullable RoutingLoadPointsListener listener);

  public static native void nativeShowCountry(String countryId, boolean zoomToDownloadButton);

  public static native void nativeSetMapStyle(int mapStyle);

  @MapStyle
  public static native int nativeGetMapStyle();

  /**
   * This method allows to set new map style without immediate applying. It can be used before
   * engine recreation instead of nativeSetMapStyle to avoid huge flow of OpenGL invocations.
   * @param mapStyle style index
   */
  public static native void nativeMarkMapStyle(int mapStyle);

  public static native void nativeSetRouter(@RouterType int routerType);
  @RouterType
  public static native int nativeGetRouter();
  @RouterType
  public static native int nativeGetLastUsedRouter();
  @RouterType
  public static native int nativeGetBestRouter(double srcLat, double srcLon,
                                               double dstLat, double dstLon);

  public static native void nativeAddRoutePoint(String title, String subtitle,
                                                @RoutePointInfo.RouteMarkType int markType,
                                                int intermediateIndex, boolean isMyPosition,
                                                double lat, double lon);

  public static native void nativeRemoveRoutePoint(@RoutePointInfo.RouteMarkType int markType,
                                                   int intermediateIndex);

  public static native void nativeRemoveIntermediateRoutePoints();

  public static native boolean nativeCouldAddIntermediatePoint();
  @NonNull
  public static native RouteMarkData[] nativeGetRoutePoints();
  @NonNull
  public static native TransitRouteInfo nativeGetTransitRouteInfo();
  /**
   * Registers all maps(.mwms). Adds them to the models, generates indexes and does all necessary stuff.
   */
  public static native void nativeReloadWorldMaps();

  /**
   * Determines if currently is day or night at the given location. Used to switch day/night styles.
   * @param utcTimeSeconds Unix time in seconds.
   * @param lat latitude of the current location.
   * @param lon longitude of the current location.
   * @return {@code true} if it is day now or {@code false} otherwise.
   */
  public static native boolean nativeIsDayTime(long utcTimeSeconds, double lat, double lon);

  public static native void nativeGet3dMode(Params3dMode result);

  public static native void nativeSet3dMode(boolean allow3d, boolean allow3dBuildings);

  public static native boolean nativeGetAutoZoomEnabled();

  public static native void nativeSetAutoZoomEnabled(boolean enabled);

  public static native void nativeSetTransitSchemeEnabled(boolean enabled);

  public static native void nativeSaveSettingSchemeEnabled(boolean enabled);

  public static native boolean nativeIsTransitSchemeEnabled();

  public static native void nativeSetIsolinesLayerEnabled(boolean enabled);

  public static native boolean nativeIsIsolinesLayerEnabled();

  public static native void nativeSetOutdoorsLayerEnabled(boolean enabled);

  public static native boolean nativeIsOutdoorsLayerEnabled();

  @NonNull
  public static native MapObject nativeDeleteBookmarkFromMapObject();

  @NonNull
  public static native String nativeGetPoiContactUrl(int metadataType);

  public static native void nativeZoomToPoint(double lat, double lon, int zoom, boolean animate);

  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ChoosePositionMode.NONE, ChoosePositionMode.EDITOR, ChoosePositionMode.API})
  public @interface ChoosePositionMode
  {
    // Keep in sync with `enum ChoosePositionMode` in Framework.hpp.
    public static final int NONE = 0;
    public static final int EDITOR = 1;
    public static final int API = 2;
  }
  /**
   * @param mode - see ChoosePositionMode values.
   * @param isBusiness selection area will be bounded by building borders, if its true(eg. true for businesses in buildings).
   * @param applyPosition if true, map'll be animated to currently selected object.
   */
  public static native void nativeSetChoosePositionMode(@ChoosePositionMode int mode, boolean isBusiness,
                                                        boolean applyPosition);
  public static native @ChoosePositionMode int nativeGetChoosePositionMode();
  public static native boolean nativeIsDownloadedMapAtScreenCenter();

  public static native String nativeGetActiveObjectFormattedCuisine();

  public static native void nativeSetVisibleRect(int left, int top, int right, int bottom);

  // Navigation.
  public static native boolean nativeIsRouteFinished();

  public static native void nativeRunFirstLaunchAnimation();

  public static native int nativeOpenRoutePointsTransaction();
  public static native void nativeApplyRoutePointsTransaction(int transactionId);
  public static native void nativeCancelRoutePointsTransaction(int transactionId);
  public static native int nativeInvalidRoutePointsTransactionId();

  public static native boolean nativeHasSavedRoutePoints();
  public static native void nativeLoadRoutePoints();
  public static native void nativeSaveRoutePoints();
  public static native void nativeDeleteSavedRoutePoints();

  public static native void nativeShowFeature(@NonNull FeatureId featureId);

  public static native void nativeMakeCrash();

    public static native void nativeSetPowerManagerFacility(int facilityType, boolean state);
  public static native int nativeGetPowerManagerScheme();
  public static native void nativeSetPowerManagerScheme(int schemeType);
  public static native void nativeSetViewportCenter(double lat, double lon, int zoom);
  public static native void nativeStopLocationFollow();

  public static native void nativeSetSearchViewport(double lat, double lon, int zoom);

  /**
   * In case of the app was dumped by system to the hard drive, Java map object can be
   * restored from parcelable, but c++ framework is created from scratch and internal
   * place page object is not initialized. So, do not restore place page in this case.
   *
   * @return true if c++ framework has initialized internal place page object, otherwise - false.
   */
  public static native boolean nativeHasPlacePageInfo();

  public static native void nativeMemoryWarning();

  /**
   * @param countryIsoCode Two-letter ISO country code to use country-specific Kayak.com domain.
   * @param uri `$HOTEL_NAME,-c$CITY_ID-h$HOTEL_ID` URI.
   * @param firstDaySec the epoch seconds of the first day of planned stay.
   * @param lastDaySec the epoch seconds of the last day of planned stay.
   * @param isReferral enable referral code to help the project.
   * @return a URL to Kayak's hotel page.
   */
  @Nullable
  public static native String nativeGetKayakHotelLink(@NonNull String countryIsoCode, @NonNull String uri,
                                                      long firstDaySec, long lastDaySec, boolean isReferral);
}
