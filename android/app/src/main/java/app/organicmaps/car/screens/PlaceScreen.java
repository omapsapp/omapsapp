package app.organicmaps.car.screens;

import static android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE;
import static android.text.Spanned.SPAN_INCLUSIVE_INCLUSIVE;

import android.content.Intent;
import android.net.Uri;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.car.app.CarContext;
import androidx.car.app.CarToast;
import androidx.car.app.model.Action;
import androidx.car.app.model.CarIcon;
import androidx.car.app.model.DistanceSpan;
import androidx.car.app.model.DurationSpan;
import androidx.car.app.model.ForegroundCarColorSpan;
import androidx.car.app.model.Header;
import androidx.car.app.model.Pane;
import androidx.car.app.model.PaneTemplate;
import androidx.car.app.model.Row;
import androidx.car.app.model.Template;
import androidx.car.app.navigation.model.MapWithContentTemplate;
import androidx.core.graphics.drawable.IconCompat;
import androidx.lifecycle.LifecycleOwner;

import app.organicmaps.Framework;
import app.organicmaps.R;
import app.organicmaps.bookmarks.data.MapObject;
import app.organicmaps.bookmarks.data.Metadata;
import app.organicmaps.car.SurfaceRenderer;
import app.organicmaps.car.screens.base.BaseMapScreen;
import app.organicmaps.car.screens.download.DownloadMapsScreenBuilder;
import app.organicmaps.car.screens.settings.DrivingOptionsScreen;
import app.organicmaps.car.util.Colors;
import app.organicmaps.car.util.OnBackPressedCallback;
import app.organicmaps.car.util.RoutingHelpers;
import app.organicmaps.car.util.UiHelpers;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.routing.ResultCodesHelper;
import app.organicmaps.routing.RoutingController;
import app.organicmaps.sdk.routing.RoutingInfo;
import app.organicmaps.sdk.Router;
import app.organicmaps.util.Config;

import java.util.Objects;

public class PlaceScreen extends BaseMapScreen implements OnBackPressedCallback.Callback, RoutingController.Container
{
  private static final Router ROUTER = Router.Vehicle;

  @Nullable
  private MapObject mMapObject;
  private boolean mIsBuildError = false;

  @NonNull
  private final RoutingController mRoutingController;

  @NonNull
  private final OnBackPressedCallback mOnBackPressedCallback;

  private PlaceScreen(@NonNull Builder builder)
  {
    super(builder.mCarContext, builder.mSurfaceRenderer);
    mMapObject = builder.mMapObject;
    mRoutingController = RoutingController.get();
    mOnBackPressedCallback = new OnBackPressedCallback(getCarContext(), this);
  }

  @NonNull
  @Override
  public Template onGetTemplate()
  {
    final MapWithContentTemplate.Builder builder = new MapWithContentTemplate.Builder();
    builder.setMapController(UiHelpers.createMapController(getCarContext(), getSurfaceRenderer()));
    builder.setActionStrip(UiHelpers.createSettingsActionStrip(this, getSurfaceRenderer()));
    builder.setContentTemplate(createPaneTemplate());
    return builder.build();
  }

  @Override
  public void onCreate(@NonNull LifecycleOwner owner)
  {
    mRoutingController.restore();
    if (mRoutingController.isNavigating() && mRoutingController.getLastRouterType() == ROUTER)
    {
      showNavigation(true);
      return;
    }

    mRoutingController.attach(this);
    if (mMapObject == null)
      mRoutingController.restoreRoute();
    else
    {
      final boolean hasIncorrectEndPoint = mRoutingController.isPlanning() && (!MapObject.same(mMapObject, mRoutingController.getEndPoint()));
      final boolean hasIncorrectRouterType = mRoutingController.getLastRouterType() != ROUTER;
      final boolean isNotPlanningMode = !mRoutingController.isPlanning();
      if (hasIncorrectRouterType)
      {
        mRoutingController.setRouterType(ROUTER);
        mRoutingController.rebuildLastRoute();
      }
      else if (hasIncorrectEndPoint || isNotPlanningMode)
      {
        mRoutingController.prepare(LocationHelper.from(getCarContext()).getMyPosition(), mMapObject);
      }
    }
  }

  @Override
  public void onResume(@NonNull LifecycleOwner owner)
  {
    mRoutingController.attach(this);
  }

  @Override
  public void onDestroy(@NonNull LifecycleOwner owner)
  {
    if (mRoutingController.isPlanning())
      mRoutingController.onSaveState();
    if (!mRoutingController.isNavigating())
      mRoutingController.detach();
  }

  @NonNull
  private Header createHeader()
  {
    final Header.Builder builder = new Header.Builder();
    builder.setStartHeaderAction(Action.BACK);
    getCarContext().getOnBackPressedDispatcher().addCallback(this, mOnBackPressedCallback);
    builder.addEndHeaderAction(createDrivingOptionsAction());
    return builder.build();
  }

  @NonNull
  private PaneTemplate createPaneTemplate()
  {
    final PaneTemplate.Builder builder = new PaneTemplate.Builder(createPane());
    builder.setHeader(createHeader());
    return builder.build();
  }

  @NonNull
  private Pane createPane()
  {
    final Pane.Builder builder = new Pane.Builder();
    final RoutingInfo routingInfo = Framework.nativeGetRouteFollowingInfo();

    if (routingInfo == null && !mIsBuildError)
    {
      builder.setLoading(true);
      return builder.build();
    }

    builder.addRow(getPlaceDescription());
    if (routingInfo != null)
      builder.addRow(getPlaceRouteInfo(routingInfo));

    final Row placeOpeningHours = UiHelpers.getPlaceOpeningHoursRow(Objects.requireNonNull(mMapObject), getCarContext());
    if (placeOpeningHours != null)
      builder.addRow(placeOpeningHours);

    createPaneActions(builder);

    return builder.build();
  }

  @NonNull
  private Row getPlaceDescription()
  {
    Objects.requireNonNull(mMapObject);

    final Row.Builder builder = new Row.Builder();
    builder.setTitle(mMapObject.getTitle());
    if (!mMapObject.getSubtitle().isEmpty())
      builder.addText(mMapObject.getSubtitle());
    String address = mMapObject.getAddress();
    if (address.isEmpty())
      address = Framework.nativeGetAddress(mMapObject.getLat(), mMapObject.getLon());
    if (!address.isEmpty())
      builder.addText(address);
    return builder.build();
  }

  @NonNull
  private Row getPlaceRouteInfo(@NonNull RoutingInfo routingInfo)
  {
    final Row.Builder builder = new Row.Builder();

    final SpannableString time = new SpannableString(" ");
    time.setSpan(DurationSpan.create(routingInfo.totalTimeInSeconds), 0, 1, SPAN_INCLUSIVE_INCLUSIVE);
    builder.setTitle(time);

    final SpannableString distance = new SpannableString(" ");
    distance.setSpan(DistanceSpan.create(RoutingHelpers.createDistance(routingInfo.distToTarget)), 0, 1, SPAN_INCLUSIVE_INCLUSIVE);
    distance.setSpan(ForegroundCarColorSpan.create(Colors.DISTANCE), 0, 1, SPAN_EXCLUSIVE_EXCLUSIVE);
    builder.addText(distance);

    return builder.build();
  }

  private void createPaneActions(@NonNull Pane.Builder builder)
  {
    Objects.requireNonNull(mMapObject);

    final String phones = mMapObject.getMetadata(Metadata.MetadataType.FMD_PHONE_NUMBER);
    if (!TextUtils.isEmpty(phones))
    {
      final String phoneNumber = phones.split(";", 1)[0];
      final Action.Builder openDialBuilder = new Action.Builder();
      openDialBuilder.setIcon(new CarIcon.Builder(IconCompat.createWithResource(getCarContext(), R.drawable.ic_phone)).build());
      openDialBuilder.setOnClickListener(() -> getCarContext().startCarApp(new Intent(Intent.ACTION_DIAL, Uri.parse("tel:" + phoneNumber))));
      builder.addAction(openDialBuilder.build());
    }

    // Don't show `Start` button when build error.
    if (mIsBuildError)
      return;

    final Action.Builder startRouteBuilder = new Action.Builder();
    startRouteBuilder.setBackgroundColor(Colors.START_NAVIGATION);
    startRouteBuilder.setFlags(Action.FLAG_DEFAULT);
    startRouteBuilder.setTitle(getCarContext().getString(R.string.p2p_start));
    startRouteBuilder.setIcon(new CarIcon.Builder(IconCompat.createWithResource(getCarContext(), R.drawable.ic_follow_and_rotate)).build());
    startRouteBuilder.setOnClickListener(() -> {
      Config.acceptRoutingDisclaimer();
      mRoutingController.start();
    });

    builder.addAction(startRouteBuilder.build());
  }

  @NonNull
  private Action createDrivingOptionsAction()
  {
    return new Action.Builder()
        .setIcon(new CarIcon.Builder(IconCompat.createWithResource(getCarContext(), R.drawable.ic_settings)).build())
        .setOnClickListener(() -> getScreenManager().pushForResult(new DrivingOptionsScreen(getCarContext(), getSurfaceRenderer()), this::onDrivingOptionsResult))
        .build();
  }

  private void onDrivingOptionsResult(@Nullable Object result)
  {
    if (result == null || result != DrivingOptionsScreen.DRIVING_OPTIONS_RESULT_CHANGED)
      return;

    // Driving Options changed. Let's rebuild the route
    mRoutingController.rebuildLastRoute();
  }

  @Override
  public void onBackPressed()
  {
    mRoutingController.cancel();
  }

  @Override
  public void showRoutePlan(boolean show, @Nullable Runnable completionListener)
  {
    if (!show)
      return;

    if (completionListener != null)
      completionListener.run();
  }

  @Override
  public void showNavigation(boolean show)
  {
    if (show)
    {
      getScreenManager().popToRoot();
      getScreenManager().push(new NavigationScreen.Builder(getCarContext(), getSurfaceRenderer()).build());
    }
  }

  @Override
  public void onBuiltRoute()
  {
    Framework.nativeDeactivateMapSelectionCircle();
    mMapObject = mRoutingController.getEndPoint();
    invalidate();
  }

  @Override
  public void onPlanningCancelled()
  {
    Framework.nativeDeactivateMapSelectionCircle();
  }

  @Override
  public void onCommonBuildError(int lastResultCode, @NonNull String[] lastMissingMaps)
  {
    if (ResultCodesHelper.isDownloadable(lastResultCode, lastMissingMaps.length))
      getScreenManager().pushForResult(
          new DownloadMapsScreenBuilder(getCarContext())
              .setDownloaderType(DownloadMapsScreenBuilder.DownloaderType.BuildRoute)
              .setMissingMaps(lastMissingMaps)
              .setResultCode(lastResultCode)
              .build(),
          (result) -> {
            if (Boolean.FALSE.equals(result))
            {
              CarToast.makeText(getCarContext(), R.string.unable_to_calc_alert_title, CarToast.LENGTH_LONG).show();
              mIsBuildError = true;
            }
            else
              mRoutingController.checkAndBuildRoute();
            invalidate();
          }
      );
    else
    {
      CarToast.makeText(getCarContext(), R.string.unable_to_calc_alert_title, CarToast.LENGTH_LONG).show();
      mIsBuildError = true;
      invalidate();
    }
  }

  @Override
  public void onDrivingOptionsBuildError()
  {
    onCommonBuildError(-1, new String[0]);
  }

  /**
   * A builder of {@link PlaceScreen}.
   */
  public static final class Builder
  {
    @NonNull
    private final CarContext mCarContext;
    @NonNull
    private final SurfaceRenderer mSurfaceRenderer;
    @Nullable
    private MapObject mMapObject;

    public Builder(@NonNull final CarContext carContext, @NonNull final SurfaceRenderer surfaceRenderer)
    {
      mCarContext = carContext;
      mSurfaceRenderer = surfaceRenderer;
    }

    public Builder setMapObject(@Nullable MapObject mapObject)
    {
      mMapObject = mapObject;
      return this;
    }

    @NonNull
    public PlaceScreen build()
    {
      return new PlaceScreen(this);
    }
  }
}
