package app.organicmaps.car.screens.settings;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.car.app.CarContext;
import androidx.car.app.model.Action;
import androidx.car.app.model.CarIcon;
import androidx.car.app.model.Header;
import androidx.car.app.model.Item;
import androidx.car.app.model.ItemList;
import androidx.car.app.model.Row;
import androidx.car.app.model.Template;
import androidx.car.app.navigation.model.MapTemplate;
import androidx.core.graphics.drawable.IconCompat;

import app.organicmaps.R;
import app.organicmaps.car.SurfaceRenderer;
import app.organicmaps.car.UiHelpers;
import app.organicmaps.car.screens.MapScreen;
import app.organicmaps.util.Config;

public class SettingsScreen extends MapScreen
{
  private interface PrefsGetter
  {
    boolean get();
  }

  private interface PrefsSetter
  {
    void set(boolean newValue);
  }

  @NonNull
  private final CarIcon mCheckboxIcon;
  @NonNull
  private final CarIcon mCheckboxSelectedIcon;

  public SettingsScreen(@NonNull CarContext carContext, @NonNull SurfaceRenderer surfaceRenderer)
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
    builder.setItemList(createSettingsList());
    return builder.build();
  }

  @NonNull
  private Header createHeader()
  {
    Header.Builder builder = new Header.Builder();
    builder.setStartHeaderAction(Action.BACK);
    builder.setTitle(getCarContext().getString(R.string.settings));
    return builder.build();
  }

  @NonNull
  private ItemList createSettingsList()
  {
    ItemList.Builder builder = new ItemList.Builder();
    builder.addItem(createRoutingOptionsItem());
    builder.addItem(createSharedPrefsCheckbox(R.string.big_font, Config::isLargeFontsSize, Config::setLargeFontsSize));
    builder.addItem(createSharedPrefsCheckbox(R.string.transliteration_title, Config::isTransliteration, Config::setTransliteration));
    builder.addItem(createHelpItem());
    return builder.build();
  }

  @NonNull
  private Item createRoutingOptionsItem()
  {
    Row.Builder builder = new Row.Builder();
    builder.setTitle(getCarContext().getString(R.string.driving_options_title));
    builder.setOnClickListener(() -> getScreenManager().push(new DrivingOptionsScreen(getCarContext(), getSurfaceRenderer())));
    builder.setBrowsable(true);
    return builder.build();
  }

  @NonNull
  private Item createHelpItem()
  {
    Row.Builder builder = new Row.Builder();
    builder.setTitle(getCarContext().getString(R.string.help));
    builder.setOnClickListener(() -> getScreenManager().push(new HelpScreen(getCarContext(), getSurfaceRenderer())));
    builder.setBrowsable(true);
    return builder.build();
  }

  @NonNull
  private Row createSharedPrefsCheckbox(@StringRes int titleRes, PrefsGetter getter, PrefsSetter setter)
  {
    final boolean getterValue = getter.get();

    Row.Builder builder = new Row.Builder();
    builder.setTitle(getCarContext().getString(titleRes));
    builder.setOnClickListener(() -> {
      setter.set(!getterValue);
      SettingsScreen.this.invalidate();
    });
    builder.setImage(getterValue ? mCheckboxSelectedIcon : mCheckboxIcon);
    return builder.build();
  }
}
