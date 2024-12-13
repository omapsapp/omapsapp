package app.organicmaps.car.screens.settings;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.car.app.CarContext;
import androidx.car.app.model.Action;
import androidx.car.app.model.Header;
import androidx.car.app.model.Item;
import androidx.car.app.model.ItemList;
import androidx.car.app.model.ListTemplate;
import androidx.car.app.model.OnClickListener;
import androidx.car.app.model.Row;
import androidx.car.app.model.Template;
import androidx.car.app.navigation.model.MapWithContentTemplate;

import app.organicmaps.Framework;
import app.organicmaps.R;
import app.organicmaps.car.SurfaceRenderer;
import app.organicmaps.car.screens.base.BaseMapScreen;
import app.organicmaps.car.util.ThemeUtils;
import app.organicmaps.car.util.Toggle;
import app.organicmaps.car.util.UiHelpers;
import app.organicmaps.util.Config;

public class SettingsScreen extends BaseMapScreen
{
  private interface PrefsGetter
  {
    boolean get();
  }

  private interface PrefsSetter
  {
    void set(boolean newValue);
  }

  public SettingsScreen(@NonNull CarContext carContext, @NonNull SurfaceRenderer surfaceRenderer)
  {
    super(carContext, surfaceRenderer);
  }

  @NonNull
  @Override
  public Template onGetTemplate()
  {
    final MapWithContentTemplate.Builder builder = new MapWithContentTemplate.Builder();
    builder.setMapController(UiHelpers.createMapController(getCarContext(), getSurfaceRenderer()));
    builder.setContentTemplate(createSettingsListTemplate());
    return builder.build();
  }

  @NonNull
  private Header createHeader()
  {
    final Header.Builder builder = new Header.Builder();
    builder.setStartHeaderAction(Action.BACK);
    builder.setTitle(getCarContext().getString(R.string.settings));
    return builder.build();
  }

  @NonNull
  private ListTemplate createSettingsListTemplate()
  {
    final ItemList.Builder builder = new ItemList.Builder();
    builder.addItem(createThemeItem());
    builder.addItem(createRoutingOptionsItem());
    builder.addItem(create3dBuildingsItem());
    builder.addItem(createSharedPrefsToggle(R.string.big_font, Config::isLargeFontsSize, Config::setLargeFontsSize));
    builder.addItem(createSharedPrefsToggle(R.string.transliteration_title, Config::isTransliteration, Config::setTransliteration));
    builder.addItem(createHelpItem());
    return new ListTemplate.Builder().setHeader(createHeader()).setSingleList(builder.build()).build();
  }

  @NonNull
  private Item createThemeItem()
  {
    final Row.Builder builder = new Row.Builder();
    builder.setTitle(getCarContext().getString(R.string.pref_appearance_title));
    builder.addText(getCarContext().getString(ThemeUtils.getThemeMode(getCarContext()).getTitleId()));
    builder.setOnClickListener(() -> getScreenManager().push(new ThemeScreen(getCarContext(), getSurfaceRenderer())));
    builder.setBrowsable(true);
    return builder.build();
  }

  @NonNull
  private Item createRoutingOptionsItem()
  {
    final Row.Builder builder = new Row.Builder();
    builder.setTitle(getCarContext().getString(R.string.driving_options_title));
    builder.setOnClickListener(() -> getScreenManager().pushForResult(new DrivingOptionsScreen(getCarContext(), getSurfaceRenderer()), this::setResult));
    builder.setBrowsable(true);
    return builder.build();
  }

  @NonNull
  private Item create3dBuildingsItem()
  {
    final Framework.Params3dMode _3d = new Framework.Params3dMode();
    Framework.nativeGet3dMode(_3d);

    final OnClickListener listener = () -> {
      Framework.nativeSet3dMode(_3d.enabled, !_3d.buildings);
      invalidate();
    };
    return Toggle.create(getCarContext(), R.string.pref_map_3d_buildings_title, listener, _3d.buildings);
  }

  @NonNull
  private Item createHelpItem()
  {
    final Row.Builder builder = new Row.Builder();
    builder.setTitle(getCarContext().getString(R.string.help));
    builder.setOnClickListener(() -> getScreenManager().push(new HelpScreen(getCarContext(), getSurfaceRenderer())));
    builder.setBrowsable(true);
    return builder.build();
  }

  @NonNull
  private Row createSharedPrefsToggle(@StringRes int titleRes, @NonNull PrefsGetter getter, @NonNull PrefsSetter setter)
  {
    final boolean enabled = getter.get();
    final OnClickListener listener = () -> {
      setter.set(!enabled);
      invalidate();
    };
    return Toggle.create(getCarContext(), titleRes, listener, enabled);
  }
}
