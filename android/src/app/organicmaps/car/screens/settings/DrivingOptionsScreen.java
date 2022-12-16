package app.organicmaps.car.screens.settings;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.car.app.CarContext;
import androidx.car.app.model.Action;
import androidx.car.app.model.CarIcon;
import androidx.car.app.model.Header;
import androidx.car.app.model.ItemList;
import androidx.car.app.model.Row;
import androidx.car.app.model.Template;
import androidx.car.app.navigation.model.MapTemplate;
import androidx.core.graphics.drawable.IconCompat;

import app.organicmaps.R;
import app.organicmaps.car.SurfaceRenderer;
import app.organicmaps.car.UiHelpers;
import app.organicmaps.car.screens.MapScreen;
import app.organicmaps.routing.RoutingOptions;
import app.organicmaps.settings.RoadType;

public class DrivingOptionsScreen extends MapScreen
{
  @NonNull
  private final CarIcon mCheckboxIcon;
  @NonNull
  private final CarIcon mCheckboxSelectedIcon;

  public DrivingOptionsScreen(@NonNull CarContext carContext, @NonNull SurfaceRenderer surfaceRenderer)
  {
    super(carContext, surfaceRenderer);
    mCheckboxIcon = new CarIcon.Builder(IconCompat.createWithResource(carContext, R.drawable.ic_check_box)).build();
    mCheckboxSelectedIcon = new CarIcon.Builder(IconCompat.createWithResource(carContext, R.drawable.ic_check_box_checked)).build();
  }

  @NonNull
  @Override
  public Template onGetTemplate()
  {
    MapTemplate.Builder builder = new MapTemplate.Builder();
    builder.setHeader(createHeader());
    builder.setMapController(UiHelpers.createMapController(getCarContext(), getSurfaceRenderer()));
    builder.setItemList(createDrivingOptionsList());
    return builder.build();
  }

  @NonNull
  private Header createHeader()
  {
    Header.Builder builder = new Header.Builder();
    builder.setStartHeaderAction(Action.BACK);
    builder.setTitle(getCarContext().getString(R.string.driving_options_subheader));
    return builder.build();
  }

  @NonNull
  private ItemList createDrivingOptionsList()
  {
    ItemList.Builder builder = new ItemList.Builder();
    builder.addItem(createDrivingOptionCheckbox(RoadType.Toll, R.string.avoid_tolls));
    builder.addItem(createDrivingOptionCheckbox(RoadType.Dirty, R.string.avoid_unpaved));
    builder.addItem(createDrivingOptionCheckbox(RoadType.Ferry, R.string.avoid_ferry));
    builder.addItem(createDrivingOptionCheckbox(RoadType.Motorway, R.string.avoid_motorways));
    return builder.build();
  }

  @NonNull
  private Row createDrivingOptionCheckbox(RoadType roadType, @StringRes int titleRes)
  {
    Row.Builder builder = new Row.Builder();
    builder.setTitle(getCarContext().getString(titleRes));
    builder.setOnClickListener(() -> {
      if (RoutingOptions.hasOption(roadType))
        RoutingOptions.removeOption(roadType);
      else
        RoutingOptions.addOption(roadType);
      DrivingOptionsScreen.this.invalidate();
    });
    builder.setImage(RoutingOptions.hasOption(roadType) ? mCheckboxSelectedIcon : mCheckboxIcon);
    return builder.build();
  }
}
