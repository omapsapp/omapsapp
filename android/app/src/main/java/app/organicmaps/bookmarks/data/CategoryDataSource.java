package app.organicmaps.bookmarks.data;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.content.DataSource;

import java.util.List;

public class CategoryDataSource extends RecyclerView.AdapterDataObserver implements
                                                                         DataSource<BookmarkCategory>
{
  @NonNull
  private BookmarkCategory mCategory;

  public CategoryDataSource(@NonNull BookmarkCategory category)
  {
    mCategory = category;
  }

  @NonNull
  @Override
  public BookmarkCategory getData()
  {
    return mCategory;
  }

  @Override
  public void onChanged()
  {
    super.onChanged();
    List<BookmarkCategory> categories = BookmarkManager.INSTANCE.getCategories();
    int index = categories.indexOf(mCategory);
    if (index >= 0)
      mCategory = categories.get(index);
  }

  @Override
  public void invalidate()
  {
    onChanged();
  }
}
