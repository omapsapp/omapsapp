package app.organicmaps.car;

import androidx.annotation.NonNull;
import androidx.car.app.CarContext;
import androidx.car.app.CarToast;
import androidx.car.app.ScreenManager;
import androidx.car.app.model.Action;
import androidx.car.app.model.ActionStrip;
import androidx.car.app.model.CarIcon;
import androidx.car.app.navigation.model.MapController;
import androidx.core.graphics.drawable.IconCompat;

import app.organicmaps.R;
import app.organicmaps.car.screens.settings.SettingsScreen;

public final class UiHelpers
{
  public static ActionStrip createSettingsActionStrip(@NonNull CarContext context, @NonNull SurfaceRenderer surfaceRenderer)
  {
    final CarIcon iconSettings = new CarIcon.Builder(IconCompat.createWithResource(context, R.drawable.ic_settings)).build();
    final Action settings = new Action.Builder().setIcon(iconSettings).setOnClickListener(
        () -> context.getCarService(ScreenManager.class).push(new SettingsScreen(context, surfaceRenderer))
    ).build();
    return new ActionStrip.Builder().addAction(settings).build();
  }

  public static MapController createMapController(@NonNull CarContext context, @NonNull SurfaceRenderer surfaceRenderer)
  {
    final CarIcon iconPlus = new CarIcon.Builder(IconCompat.createWithResource(context, R.drawable.ic_plus)).build();
    final CarIcon iconMinus = new CarIcon.Builder(IconCompat.createWithResource(context, R.drawable.ic_minus)).build();
    final CarIcon iconLocation = new CarIcon.Builder(IconCompat.createWithResource(context, R.drawable.ic_not_follow)).build();

    final Action panAction = new Action.Builder(Action.PAN).build();
    final Action location = new Action.Builder().setIcon(iconLocation).setOnClickListener(
        () -> CarToast.makeText(context, "Location", CarToast.LENGTH_LONG).show()
    ).build();
    final Action zoomIn = new Action.Builder().setIcon(iconPlus).setOnClickListener(surfaceRenderer::onZoomIn).build();
    final Action zoomOut = new Action.Builder().setIcon(iconMinus).setOnClickListener(surfaceRenderer::onZoomOut).build();
    final ActionStrip mapActionStrip = new ActionStrip.Builder()
        .addAction(location)
        .addAction(zoomIn)
        .addAction(zoomOut)
        .addAction(panAction)
        .build();
    return new MapController.Builder().setMapActionStrip(mapActionStrip).build();
  }
}
