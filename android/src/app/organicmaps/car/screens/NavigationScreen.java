package app.organicmaps.car.screens;

import androidx.annotation.NonNull;
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

public class NavigationScreen extends MapScreen
{
  public NavigationScreen(@NonNull CarContext carContext, @NonNull SurfaceRenderer surfaceRenderer)
  {
    super(carContext, surfaceRenderer);
  }

  @NonNull
  @Override
  public Template onGetTemplate()
  {
    MapTemplate.Builder builder = new MapTemplate.Builder();
    builder.setHeader(createHeader());
    builder.setMapController(UiHelpers.createMapController(getCarContext(), getSurfaceRenderer()));
    builder.setActionStrip(UiHelpers.createSettingsActionStrip(getCarContext(), getSurfaceRenderer()));
    builder.setItemList(createList());
    return builder.build();
  }

  @NonNull
  private Header createHeader()
  {
    Header.Builder builder = new Header.Builder();
    builder.setStartHeaderAction(new Action.Builder(Action.APP_ICON).build());
    builder.setTitle(getCarContext().getString(R.string.app_name));
    return builder.build();
  }

  @NonNull
  private ItemList createList()
  {
    ItemList.Builder builder = new ItemList.Builder();
    builder.addItem(createSearchItem());
    builder.addItem(createCategoriesItem());
    builder.addItem(createBookmarksItem());
    return builder.build();
  }

  @NonNull
  private Item createSearchItem()
  {
    final CarIcon iconSearch = new CarIcon.Builder(IconCompat.createWithResource(getCarContext(), R.drawable.ic_search)).build();

    Row.Builder builder = new Row.Builder();
    builder.setTitle(getCarContext().getString(R.string.search));
    builder.setImage(iconSearch);
    builder.setBrowsable(true);
    builder.setOnClickListener(this::openSearch);
    return builder.build();
  }

  @NonNull
  private Item createCategoriesItem()
  {
    Row.Builder builder = new Row.Builder();
    builder.setTitle(getCarContext().getString(R.string.categories));
    builder.setBrowsable(true);
    builder.setOnClickListener(this::openCategories);
    return builder.build();
  }

  @NonNull
  private Item createBookmarksItem()
  {
    Row.Builder builder = new Row.Builder();
    builder.setTitle(getCarContext().getString(R.string.bookmarks));
    builder.setBrowsable(true);
    builder.setOnClickListener(this::openBookmarks);
    return builder.build();
  }

  private void openSearch()
  {
    getScreenManager().push(new SearchScreen(getCarContext()));
  }

  private void openCategories()
  {
    getScreenManager().push(new CategoriesScreen(getCarContext(), getSurfaceRenderer()));
  }

  private void openBookmarks()
  {
    getScreenManager().push(new BookmarksScreen(getCarContext(), getSurfaceRenderer()));
  }
}
