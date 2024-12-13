package app.organicmaps.car.screens.settings;

import androidx.annotation.NonNull;
import androidx.car.app.CarContext;
import androidx.car.app.model.Action;
import androidx.car.app.model.CarIcon;
import androidx.car.app.model.Header;
import androidx.car.app.model.ItemList;
import androidx.car.app.model.ListTemplate;
import androidx.car.app.model.Row;
import androidx.car.app.model.Template;
import androidx.car.app.navigation.model.MapWithContentTemplate;
import androidx.core.graphics.drawable.IconCompat;

import app.organicmaps.R;
import app.organicmaps.car.SurfaceRenderer;
import app.organicmaps.car.screens.base.BaseMapScreen;
import app.organicmaps.car.util.ThemeUtils;
import app.organicmaps.car.util.UiHelpers;

public class ThemeScreen extends BaseMapScreen
{
  @NonNull
  private final CarIcon mRadioButtonIcon;
  @NonNull
  private final CarIcon mRadioButtonSelectedIcon;

  public ThemeScreen(@NonNull CarContext carContext, @NonNull SurfaceRenderer surfaceRenderer)
  {
    super(carContext, surfaceRenderer);
    mRadioButtonIcon = new CarIcon.Builder(IconCompat.createWithResource(carContext, R.drawable.ic_radio_button_unchecked)).build();
    mRadioButtonSelectedIcon = new CarIcon.Builder(IconCompat.createWithResource(carContext, R.drawable.ic_radio_button_checked)).build();
  }

  @NonNull
  @Override
  public Template onGetTemplate()
  {
    final MapWithContentTemplate.Builder builder = new MapWithContentTemplate.Builder();
    builder.setMapController(UiHelpers.createMapController(getCarContext(), getSurfaceRenderer()));
    builder.setContentTemplate(createRadioButtonsTemplate());
    return builder.build();
  }

  @NonNull
  private Header createHeader()
  {
    final Header.Builder builder = new Header.Builder();
    builder.setStartHeaderAction(Action.BACK);
    builder.setTitle(getCarContext().getString(R.string.pref_appearance_title));
    return builder.build();
  }

  @NonNull
  private ListTemplate createRadioButtonsTemplate()
  {
    final ItemList.Builder builder = new ItemList.Builder();
    final ThemeUtils.ThemeMode currentThemeMode = ThemeUtils.getThemeMode(getCarContext());
    builder.addItem(createRadioButton(ThemeUtils.ThemeMode.FOLLOW_SYSTEM, currentThemeMode));
    builder.addItem(createRadioButton(ThemeUtils.ThemeMode.NIGHT, currentThemeMode));
    builder.addItem(createRadioButton(ThemeUtils.ThemeMode.LIGHT, currentThemeMode));
    return new ListTemplate.Builder().setHeader(createHeader()).setSingleList(builder.build()).build();
  }

  @NonNull
  private Row createRadioButton(@NonNull ThemeUtils.ThemeMode themeMode, @NonNull ThemeUtils.ThemeMode currentThemeMode)
  {
    final Row.Builder builder = new Row.Builder();
    builder.setTitle(getCarContext().getString(themeMode.getTitleId()));
    builder.setOnClickListener(() -> {
      if (themeMode == currentThemeMode)
        return;
      ThemeUtils.setThemeMode(getCarContext(), themeMode);
      invalidate();
    });
    if (themeMode == currentThemeMode)
      builder.setImage(mRadioButtonSelectedIcon);
    else
      builder.setImage(mRadioButtonIcon);
    return builder.build();
  }
}
