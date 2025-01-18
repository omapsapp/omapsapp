package app.organicmaps.car.screens;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.car.app.CarContext;
import androidx.car.app.CarToast;
import androidx.car.app.model.Action;
import androidx.car.app.model.ActionStrip;
import androidx.car.app.model.CarIcon;
import androidx.car.app.model.Template;
import androidx.car.app.navigation.NavigationManager;
import androidx.car.app.navigation.NavigationManagerCallback;
import androidx.car.app.navigation.model.NavigationTemplate;
import androidx.car.app.navigation.model.Step;
import androidx.car.app.navigation.model.TravelEstimate;
import androidx.car.app.navigation.model.Trip;
import androidx.core.graphics.drawable.IconCompat;
import androidx.lifecycle.LifecycleOwner;

import app.organicmaps.Framework;
import app.organicmaps.R;
import app.organicmaps.car.CarAppService;
import app.organicmaps.car.SurfaceRenderer;
import app.organicmaps.car.screens.base.BaseMapScreen;
import app.organicmaps.car.screens.settings.DrivingOptionsScreen;
import app.organicmaps.car.util.Colors;
import app.organicmaps.car.util.RoutingUtils;
import app.organicmaps.car.util.ThemeUtils;
import app.organicmaps.car.util.UiHelpers;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.location.LocationListener;
import app.organicmaps.sdk.routing.JunctionInfo;
import app.organicmaps.routing.NavigationService;
import app.organicmaps.routing.RoutingController;
import app.organicmaps.sdk.routing.RoutingInfo;
import app.organicmaps.sound.TtsPlayer;
import app.organicmaps.util.LocationUtils;
import app.organicmaps.util.log.Logger;

import java.util.List;
import java.util.Objects;

public class NavigationScreen extends BaseMapScreen implements RoutingController.Container, NavigationManagerCallback
{
  private static final String TAG = NavigationScreen.class.getSimpleName();

  public static final String MARKER = NavigationScreen.class.getSimpleName();

  @NonNull
  private final RoutingController mRoutingController;
  @NonNull
  private final NavigationManager mNavigationManager;
  @NonNull
  private final LocationListener mLocationListener = (unused) -> updateTrip();

  @NonNull
  private Trip mTrip = new Trip.Builder().setLoading(true).build();

  // This value is used to decide whether to display the "trip finished" toast or not
  // False: trip is finished -> show toast
  // True: navigation is cancelled by the user or host -> don't show toast
  private boolean mNavigationCancelled = false;

  private NavigationScreen(@NonNull Builder builder)
  {
    super(builder.mCarContext, builder.mSurfaceRenderer);
    mNavigationManager = builder.mCarContext.getCarService(NavigationManager.class);
    mRoutingController = RoutingController.get();
  }

  @NonNull
  @Override
  public Template onGetTemplate()
  {
    final NavigationTemplate.Builder builder = new NavigationTemplate.Builder();
    builder.setBackgroundColor(ThemeUtils.isNightMode(getCarContext()) ? Colors.NAVIGATION_TEMPLATE_BACKGROUND_NIGHT : Colors.NAVIGATION_TEMPLATE_BACKGROUND_DAY);
    builder.setActionStrip(createActionStrip());
    builder.setMapActionStrip(UiHelpers.createMapActionStrip(getCarContext(), getSurfaceRenderer()));

    final TravelEstimate destinationTravelEstimate = getDestinationTravelEstimate();
    if (destinationTravelEstimate != null)
      builder.setDestinationTravelEstimate(destinationTravelEstimate);

    builder.setNavigationInfo(getNavigationInfo());
    return builder.build();
  }

  @Override
  public void onStopNavigation()
  {
    LocationHelper.from(getCarContext()).removeListener(mLocationListener);
    mNavigationCancelled = true;
    mRoutingController.cancel();
  }

  /**
   * To verify your app's navigation functionality when you submit it to the Google Play Store, your app must implement
   * the NavigationManagerCallback.onAutoDriveEnabled callback. When this callback is called, your app must simulate
   * navigation to the chosen destination when the user begins navigation. Your app can exit this mode whenever
   * the lifecycle of the current Session reaches the Lifecycle.Event.ON_DESTROY state.
   * <a href="https://developer.android.com/training/cars/apps/navigation#simulating-navigation">More info</a>
   */
  @Override
  public void onAutoDriveEnabled()
  {
    Logger.i(TAG);
    final JunctionInfo[] points = Framework.nativeGetRouteJunctionPoints();
    if (points == null)
    {
      Logger.e(TAG, "Navigation has not started yet");
      return;
    }

    final LocationHelper locationHelper = LocationHelper.from(getCarContext());
    locationHelper.startNavigationSimulation(points);
  }

  @Override
  public void onNavigationCancelled()
  {
    if (!mNavigationCancelled)
      CarToast.makeText(getCarContext(), getCarContext().getString(R.string.trip_finished), CarToast.LENGTH_LONG).show();
    finish();
    getScreenManager().popToRoot();
  }

  @Override
  public void onCreate(@NonNull LifecycleOwner owner)
  {
    Logger.d(TAG);
    mRoutingController.attach(this);
    ThemeUtils.update(getCarContext());
    mNavigationManager.setNavigationManagerCallback(this);
    mNavigationManager.navigationStarted();

    LocationHelper.from(getCarContext()).addListener(mLocationListener);
    if (LocationUtils.checkFineLocationPermission(getCarContext()))
      NavigationService.startForegroundService(getCarContext(), CarAppService.getCarNotificationExtender(getCarContext()));
    updateTrip();
  }

  @Override
  public void onResume(@NonNull LifecycleOwner owner)
  {
    Logger.d(TAG);
    mRoutingController.attach(this);
  }

  @Override
  public void onDestroy(@NonNull LifecycleOwner owner)
  {
    NavigationService.stopService(getCarContext());
    LocationHelper.from(getCarContext()).removeListener(mLocationListener);

    if (mRoutingController.isNavigating())
      mRoutingController.onSaveState();
    mRoutingController.detach();
    ThemeUtils.update(getCarContext());
    mNavigationManager.navigationEnded();
    mNavigationManager.clearNavigationManagerCallback();
  }

  @NonNull
  private ActionStrip createActionStrip()
  {
    final Action.Builder stopActionBuilder = new Action.Builder();
    stopActionBuilder.setIcon(new CarIcon.Builder(IconCompat.createWithResource(getCarContext(), R.drawable.ic_close)).build());
    stopActionBuilder.setOnClickListener(() -> {
      mNavigationCancelled = true;
      mRoutingController.cancel();
    });

    final ActionStrip.Builder builder = new ActionStrip.Builder();
    builder.addAction(createTtsAction());
    builder.addAction(UiHelpers.createSettingsActionForResult(this, getSurfaceRenderer(), this::onSettingsResult));
    builder.addAction(stopActionBuilder.build());
    return builder.build();
  }

  @Nullable
  private TravelEstimate getDestinationTravelEstimate()
  {
    if (mTrip.isLoading())
      return null;

    List<TravelEstimate> travelEstimates = mTrip.getDestinationTravelEstimates();
    if (travelEstimates.size() != 1)
      throw new RuntimeException("TravelEstimates size must be 1");

    return travelEstimates.get(0);
  }

  @NonNull
  private NavigationTemplate.NavigationInfo getNavigationInfo()
  {
    final androidx.car.app.navigation.model.RoutingInfo.Builder builder = new androidx.car.app.navigation.model.RoutingInfo.Builder();

    if (mTrip.isLoading())
    {
      builder.setLoading(true);
      return builder.build();
    }

    final List<Step> steps = mTrip.getSteps();
    final List<TravelEstimate> stepsEstimates = mTrip.getStepTravelEstimates();
    if (steps.isEmpty())
      throw new RuntimeException("Steps size must be at least 1");

    builder.setCurrentStep(steps.get(0), Objects.requireNonNull(stepsEstimates.get(0).getRemainingDistance()));
    if (steps.size() > 1)
      builder.setNextStep(steps.get(1));

    return builder.build();
  }

  private void onSettingsResult(@Nullable Object result)
  {
    if (result == null || result != DrivingOptionsScreen.DRIVING_OPTIONS_RESULT_CHANGED)
      return;

    // TODO (AndrewShkrob): Need to rebuild the route with updated driving options
    Logger.d(TAG, "Driving options changed");
  }

  @NonNull
  private Action createTtsAction()
  {
    final Action.Builder ttsActionBuilder = new Action.Builder();
    @DrawableRes final int imgRes = TtsPlayer.isEnabled() ? R.drawable.ic_voice_on : R.drawable.ic_voice_off;
    ttsActionBuilder.setIcon(new CarIcon.Builder(IconCompat.createWithResource(getCarContext(), imgRes)).build());
    ttsActionBuilder.setOnClickListener(() -> {
      TtsPlayer.setEnabled(!TtsPlayer.isEnabled());
      invalidate();
    });

    return ttsActionBuilder.build();
  }

  private void updateTrip()
  {
    final RoutingInfo info = Framework.nativeGetRouteFollowingInfo();
    mTrip = RoutingUtils.createTrip(getCarContext(), info, RoutingController.get().getEndPoint());
    mNavigationManager.updateTrip(mTrip);
    invalidate();
  }

  /**
   * A builder of {@link NavigationScreen}.
   */
  public static final class Builder
  {
    @NonNull
    private final CarContext mCarContext;
    @NonNull
    private final SurfaceRenderer mSurfaceRenderer;

    public Builder(@NonNull final CarContext carContext, @NonNull final SurfaceRenderer surfaceRenderer)
    {
      mCarContext = carContext;
      mSurfaceRenderer = surfaceRenderer;
    }

    @NonNull
    public NavigationScreen build()
    {
      final NavigationScreen navigationScreen = new NavigationScreen(this);
      navigationScreen.setMarker(MARKER);
      return navigationScreen;
    }
  }
}
