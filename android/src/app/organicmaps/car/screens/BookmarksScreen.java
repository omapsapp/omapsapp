package app.organicmaps.car.screens;

import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.car.app.CarContext;
import androidx.car.app.constraints.ConstraintManager;
import androidx.car.app.model.Action;
import androidx.car.app.model.CarIcon;
import androidx.car.app.model.Header;
import androidx.car.app.model.ItemList;
import androidx.car.app.model.Row;
import androidx.car.app.model.Template;
import androidx.car.app.navigation.model.MapTemplate;
import androidx.core.graphics.drawable.IconCompat;

import app.organicmaps.R;
import app.organicmaps.bookmarks.data.BookmarkCategory;
import app.organicmaps.bookmarks.data.BookmarkInfo;
import app.organicmaps.bookmarks.data.BookmarkManager;
import app.organicmaps.car.OMController;
import app.organicmaps.util.Graphics;

import java.util.ArrayList;
import java.util.List;

public class BookmarksScreen extends MapScreen
{
  private final int MAX_CATEGORIES_SIZE;

  @Nullable
  private BookmarkCategory mBookmarkCategory;

  public BookmarksScreen(@NonNull CarContext carContext, @NonNull OMController mapController)
  {
    super(carContext, mapController);
    MAX_CATEGORIES_SIZE = getCarContext().getCarService(ConstraintManager.class).getContentLimit(ConstraintManager.CONTENT_LIMIT_TYPE_LIST);
  }

  private BookmarksScreen(@NonNull CarContext carContext, @NonNull OMController mapController, @NonNull BookmarkCategory bookmarkCategory)
  {
    this(carContext, mapController);
    mBookmarkCategory = bookmarkCategory;
  }

  @NonNull
  @Override
  public Template onGetTemplate()
  {

    MapTemplate.Builder builder = new MapTemplate.Builder();
    builder.setHeader(createHeader());
    builder.setMapController(getMapController());
    builder.setActionStrip(getActionStrip());
    if (mBookmarkCategory == null)
      builder.setItemList(createBookmarkCategoriesList());
    else
      builder.setItemList(createBookmarksList());
    return builder.build();
  }

  @NonNull
  private Header createHeader()
  {
    Header.Builder builder = new Header.Builder();
    builder.setStartHeaderAction(Action.BACK);
    if (mBookmarkCategory == null)
      builder.setTitle(getCarContext().getString(R.string.bookmarks));
    else
      builder.setTitle(mBookmarkCategory.getName());
    return builder.build();
  }

  @NonNull
  private ItemList createBookmarkCategoriesList()
  {
    final List<BookmarkCategory> bookmarkCategories = getBookmarks();
    final int categoriesSize = Math.min(bookmarkCategories.size(), MAX_CATEGORIES_SIZE);

    ItemList.Builder builder = new ItemList.Builder();
    for (int i = 0; i < categoriesSize; ++i)
    {
      final BookmarkCategory bookmarkCategory = bookmarkCategories.get(i);

      Row.Builder itemBuilder = new Row.Builder();
      itemBuilder.setTitle(bookmarkCategory.getName());
      itemBuilder.addText(bookmarkCategory.getDescription());
      itemBuilder.setOnClickListener(() -> getScreenManager().push(new BookmarksScreen(getCarContext(), getOMController(), bookmarkCategory)));
      itemBuilder.setBrowsable(true);
      builder.addItem(itemBuilder.build());
    }
    return builder.build();
  }

  @NonNull
  private ItemList createBookmarksList()
  {
    final long bookmarkCategoryId = mBookmarkCategory.getId();
    final int bookmarkCategoriesSize = Math.min(mBookmarkCategory.getBookmarksCount(), MAX_CATEGORIES_SIZE);

    ItemList.Builder builder = new ItemList.Builder();
    for (int i = 0; i < bookmarkCategoriesSize; ++i)
    {
      final long bookmarkId = BookmarkManager.INSTANCE.getBookmarkIdByPosition(bookmarkCategoryId, i);
      final BookmarkInfo bookmarkInfo = new BookmarkInfo(bookmarkCategoryId, bookmarkId);

      Row.Builder itemBuilder = new Row.Builder();
      itemBuilder.setTitle(bookmarkInfo.getName());
      if (!bookmarkInfo.getAddress().isEmpty())
        itemBuilder.addText(bookmarkInfo.getAddress());
      if (!bookmarkInfo.getFeatureType().isEmpty())
        itemBuilder.addText(bookmarkInfo.getFeatureType());
      final Drawable icon = Graphics.drawCircleAndImage(bookmarkInfo.getIcon().argb(),
          R.dimen.track_circle_size,
          bookmarkInfo.getIcon().getResId(),
          R.dimen.bookmark_icon_size,
          getCarContext());
      itemBuilder.setImage(new CarIcon.Builder(IconCompat.createWithBitmap(Graphics.drawableToBitmap(icon))).build());
      builder.addItem(itemBuilder.build());
    }
    return builder.build();
  }

  @NonNull
  private static List<BookmarkCategory> getBookmarks()
  {
    List<BookmarkCategory> bookmarkCategories = new ArrayList<>(BookmarkManager.INSTANCE.getCategories());

    List<BookmarkCategory> toRemove = new ArrayList<>();
    for (BookmarkCategory bookmarkCategory : bookmarkCategories)
    {
      if (bookmarkCategory.getBookmarksCount() == 0)
        toRemove.add(bookmarkCategory);
    }
    bookmarkCategories.removeAll(toRemove);

    return bookmarkCategories;
  }
}
