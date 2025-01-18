package app.organicmaps.car.screens.settings;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.car.app.CarContext;
import androidx.car.app.model.Action;
import androidx.car.app.model.Header;
import androidx.car.app.model.ItemList;
import androidx.car.app.model.ListTemplate;
import androidx.car.app.model.OnClickListener;
import androidx.car.app.model.Row;
import androidx.car.app.model.Template;
import androidx.car.app.navigation.model.MapWithContentTemplate;
import androidx.lifecycle.LifecycleOwner;

import app.organicmaps.R;
import app.organicmaps.car.SurfaceRenderer;
import app.organicmaps.car.screens.base.BaseMapScreen;
import app.organicmaps.car.util.Toggle;
import app.organicmaps.car.util.UiHelpers;
import app.organicmaps.sdk.routing.RoutingOptions;
import app.organicmaps.settings.RoadType;

import java.util.HashMap;
import java.util.Map;

public class DrivingOptionsScreen extends BaseMapScreen
{
  public static final Object DRIVING_OPTIONS_RESULT_CHANGED = 0x1;

  private record DrivingOption(RoadType roadType, @StringRes int text)
  {
  }

  private final DrivingOption[] mDrivingOptions = {
      new DrivingOption(RoadType.Toll, R.string.avoid_tolls),
      new DrivingOption(RoadType.Dirty, R.string.avoid_unpaved),
      new DrivingOption(RoadType.Ferry, R.string.avoid_ferry),
      new DrivingOption(RoadType.Motorway, R.string.avoid_motorways)
  };

  @NonNull
  private final Map<RoadType, Boolean> mInitialDrivingOptionsState = new HashMap<>();

  public DrivingOptionsScreen(@NonNull CarContext carContext, @NonNull SurfaceRenderer surfaceRenderer)
  {
    super(carContext, surfaceRenderer);

    initDrivingOptionsState();
  }

  @NonNull
  @Override
  public Template onGetTemplate()
  {
    final MapWithContentTemplate.Builder builder = new MapWithContentTemplate.Builder();
    builder.setMapController(UiHelpers.createMapController(getCarContext(), getSurfaceRenderer()));
    builder.setContentTemplate(createDrivingOptionsListTemplate());
    return builder.build();
  }

  @Override
  public void onStop(@NonNull LifecycleOwner owner)
  {
    for (final DrivingOption drivingOption : mDrivingOptions)
    {
      if (Boolean.TRUE.equals(mInitialDrivingOptionsState.get(drivingOption.roadType)) != RoutingOptions.hasOption(drivingOption.roadType))
      {
        setResult(DRIVING_OPTIONS_RESULT_CHANGED);
        return;
      }
    }
  }

  @NonNull
  private Header createHeader()
  {
    final Header.Builder builder = new Header.Builder();
    builder.setStartHeaderAction(Action.BACK);
    builder.setTitle(getCarContext().getString(R.string.driving_options_title));
    return builder.build();
  }

  @NonNull
  private ListTemplate createDrivingOptionsListTemplate()
  {
    final ItemList.Builder builder = new ItemList.Builder();
    for (final DrivingOption drivingOption : mDrivingOptions)
      builder.addItem(createDrivingOptionsToggle(drivingOption.roadType, drivingOption.text));
    return new ListTemplate.Builder().setHeader(createHeader()).setSingleList(builder.build()).build();
  }

  @NonNull
  private Row createDrivingOptionsToggle(RoadType roadType, @StringRes int title)
  {
    final OnClickListener listener = () -> {
      if (RoutingOptions.hasOption(roadType))
        RoutingOptions.removeOption(roadType);
      else
        RoutingOptions.addOption(roadType);
      invalidate();
    };
    return Toggle.create(getCarContext(), title, listener, RoutingOptions.hasOption(roadType));
  }

  private void initDrivingOptionsState()
  {
    for (final DrivingOption drivingOption : mDrivingOptions)
      mInitialDrivingOptionsState.put(drivingOption.roadType, RoutingOptions.hasOption(drivingOption.roadType));
  }
}
