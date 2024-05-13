package app.organicmaps.bookmarks;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.R;
import app.organicmaps.adapter.OnItemClickListener;
import app.organicmaps.sdk.bookmarks.data.BookmarkCategory;
import app.organicmaps.sdk.bookmarks.data.BookmarkManager;
import app.organicmaps.util.UiUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

public class BookmarkCollectionAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder>
{
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ TYPE_HEADER_ITEM, TYPE_CATEGORY_ITEM })
  public @interface SectionType { }

  private final static int TYPE_CATEGORY_ITEM = BookmarkManager.CATEGORY;
  private final static int TYPE_HEADER_ITEM = 3;

  @NonNull
  private final BookmarkCategory mBookmarkCategory;

  @NonNull
  private List<BookmarkCategory> mItemsCategory;

  private int mSectionCount;
  private int mCategorySectionIndex = SectionPosition.INVALID_POSITION;

  private boolean mVisible;

  @Nullable
  private OnItemClickListener<BookmarkCategory> mClickListener;
  @NonNull
  private final MassOperationAction mMassOperationAction = new MassOperationAction();

  private class ToggleVisibilityClickListener implements View.OnClickListener
  {
    @NonNull
    private final Holders.CollectionViewHolder mHolder;

    ToggleVisibilityClickListener(@NonNull Holders.CollectionViewHolder holder)
    {
      mHolder = holder;
    }

    @Override
    public void onClick(View v)
    {
      BookmarkCategory category = mHolder.getEntity();
      BookmarkManager.INSTANCE.toggleCategoryVisibility(category);
      notifyItemChanged(0);
    }
  }

  BookmarkCollectionAdapter(@NonNull BookmarkCategory bookmarkCategory,
                            @NonNull List<BookmarkCategory> itemsCategories)
  {
    mBookmarkCategory = bookmarkCategory;
    //noinspection AssignmentOrReturnOfFieldWithMutableType
    mItemsCategory = itemsCategories;

    mSectionCount = 0;
    if (!mItemsCategory.isEmpty())
      mCategorySectionIndex = mSectionCount++;
  }

  public int getItemsCount(int sectionIndex)
  {
    if (sectionIndex == mCategorySectionIndex)
      return mItemsCategory.size();
    return 0;
  }

  @SectionType
  public int getItemsType(int sectionIndex)
  {
    if (sectionIndex == mCategorySectionIndex)
      return TYPE_CATEGORY_ITEM;
    throw new AssertionError("Invalid section index: " + sectionIndex);
  }

  @NonNull
  private List<BookmarkCategory> getItemsListByType(@SectionType int type)
  {
      return mItemsCategory;
  }

  @NonNull
  public BookmarkCategory getGroupByPosition(SectionPosition sp, @SectionType int type)
  {
    List<BookmarkCategory> categories = getItemsListByType(type);

    int itemIndex = sp.getItemIndex();
    if (sp.getItemIndex() > categories.size() - 1)
      throw new ArrayIndexOutOfBoundsException(itemIndex);
    return categories.get(itemIndex);
  }

  public void setOnClickListener(@Nullable OnItemClickListener<BookmarkCategory> listener)
  {
    mClickListener = listener;
  }

  private SectionPosition getSectionPosition(int position)
  {
    int startSectionRow = 0;
    for (int i = 0; i < mSectionCount; ++i)
    {
      int sectionRowsCount = getItemsCount(i) + /* header */ 1;
      if (startSectionRow == position)
        return new SectionPosition(i, SectionPosition.INVALID_POSITION);
      if (startSectionRow + sectionRowsCount > position)
        return new SectionPosition(i, position - startSectionRow - /* header */ 1);
      startSectionRow += sectionRowsCount;
    }
    return new SectionPosition(SectionPosition.INVALID_POSITION, SectionPosition.INVALID_POSITION);
  }

  @NonNull
  @Override
  public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent,
                                                    @SectionType int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
    RecyclerView.ViewHolder holder = null;

    if (viewType == TYPE_HEADER_ITEM)
      holder = new Holders.HeaderViewHolder(inflater.inflate(R.layout.item_bookmark_group_list_header,
                                                             parent, false));
    if (viewType == TYPE_CATEGORY_ITEM)
      holder = new Holders.CollectionViewHolder(inflater.inflate(R.layout.item_bookmark_collection,
                                                                 parent, false));

    if (holder == null)
      throw new AssertionError("Unsupported view type: " + viewType);

    return holder;
  }

  @Override
  public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position)
  {
    SectionPosition sectionPosition = getSectionPosition(position);
    if (sectionPosition.isTitlePosition())
      bindHeaderHolder(holder, sectionPosition.getSectionIndex());
    else
      bindCollectionHolder(holder, sectionPosition, getItemsType(sectionPosition.getSectionIndex()));
  }

  @Override
  @SectionType
  public int getItemViewType(int position)
  {
    SectionPosition sectionPosition = getSectionPosition(position);
    if (sectionPosition.isTitlePosition())
      return TYPE_HEADER_ITEM;
    if (sectionPosition.isItemPosition())
      return getItemsType(sectionPosition.getSectionIndex());
    throw new AssertionError("Position not found: " + position);
  }

  private void bindCollectionHolder(RecyclerView.ViewHolder holder, SectionPosition position,
                                    @SectionType int type)
  {
    final BookmarkCategory category = getGroupByPosition(position, type);
    Holders.CollectionViewHolder collectionViewHolder = (Holders.CollectionViewHolder) holder;
    collectionViewHolder.setEntity(category);
    collectionViewHolder.setName(category.getName());
    collectionViewHolder.setSize();
    collectionViewHolder.setVisibilityState(category.isVisible());
    collectionViewHolder.setOnClickListener(mClickListener);
    ToggleVisibilityClickListener listener = new ToggleVisibilityClickListener(collectionViewHolder);
    collectionViewHolder.setVisibilityListener(listener);
    updateVisibility(collectionViewHolder.itemView);
  }

  private void bindHeaderHolder(@NonNull RecyclerView.ViewHolder holder, int nextSectionPosition)
  {
    Holders.HeaderViewHolder headerViewHolder = (Holders.HeaderViewHolder) holder;
    headerViewHolder.getText()
                    .setText(holder.itemView.getResources().getString(R.string.bookmarks));
    final boolean visibility = !BookmarkManager.INSTANCE.areAllCategoriesVisible();
    headerViewHolder.setAction(mMassOperationAction, visibility);
    updateVisibility(headerViewHolder.itemView);
  }

  private void updateVisibility(@NonNull View itemView)
  {
    UiUtils.showRecyclerItemView(mVisible, itemView);
  }

  @Override
  public long getItemId(int position)
  {
    return position;
  }

  @Override
  public int getItemCount()
  {
    int itemCount = 0;

    for (int i = 0; i < mSectionCount; ++i)
    {
      int sectionItemsCount = getItemsCount(i);
      if (sectionItemsCount == 0)
        continue;
      itemCount += sectionItemsCount + /* header */ 1;
    }
    return itemCount;
  }

  private void updateAllItems()
  {
    mItemsCategory = BookmarkManager.INSTANCE.getChildrenCategories(mBookmarkCategory.getId());
  }

  void show(boolean visible)
  {
    mVisible = visible;
    notifyDataSetChanged();
  }

  class MassOperationAction implements Holders.HeaderViewHolder.HeaderActionChildCategories
  {
    @Override
    public void onHideAll()
    {
      BookmarkManager.INSTANCE.setChildCategoriesVisibility(mBookmarkCategory.getId(), false);
      updateAllItems();
      notifyDataSetChanged();
    }

    @Override
    public void onShowAll()
    {
      BookmarkManager.INSTANCE.setChildCategoriesVisibility(mBookmarkCategory.getId(), true);
      updateAllItems();
      notifyDataSetChanged();
    }
  }
}

