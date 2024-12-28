package app.organicmaps.bookmarks;

import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.R;
import app.organicmaps.bookmarks.data.BookmarkCategory;
import app.organicmaps.bookmarks.data.BookmarkInfo;
import app.organicmaps.bookmarks.data.BookmarkManager;
import app.organicmaps.bookmarks.data.IconClickListener;
import app.organicmaps.bookmarks.data.SortedBlock;
import app.organicmaps.content.DataSource;
import app.organicmaps.widget.recycler.RecyclerClickListener;
import app.organicmaps.widget.recycler.RecyclerLongClickListener;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class BookmarkListAdapter extends RecyclerView.Adapter<Holders.BaseBookmarkHolder>
{
  // view types
  static final int TYPE_TRACK = 0;
  static final int TYPE_BOOKMARK = 1;
  static final int TYPE_SECTION = 2;
  static final int TYPE_DESC = 3;
  static final int MAX_VISIBLE_LINES = 2;

  @NonNull
  private final DataSource<BookmarkCategory> mDataSource;
  @Nullable
  private List<Long> mSearchResults;
  @Nullable
  private List<SortedBlock> mSortedResults;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private SectionsDataSource mSectionsDataSource;

  @Nullable
  private RecyclerClickListener mClickListener;
  @Nullable
  private RecyclerLongClickListener mLongClickListener;
  private RecyclerClickListener mMoreClickListener;
  private IconClickListener mIconClickListener;

  public static abstract class SectionsDataSource
  {
    @NonNull
    private final DataSource<BookmarkCategory> mDataSource;

    SectionsDataSource(@NonNull DataSource<BookmarkCategory> dataSource)
    {
      mDataSource = dataSource;
    }

    public BookmarkCategory getCategory() { return mDataSource.getData(); }

    boolean hasDescription()
    {
      return (!mDataSource.getData().getAnnotation().isEmpty() ||
              !mDataSource.getData().getDescription().isEmpty());
    }

    void invalidate()
    {
      mDataSource.invalidate();
    }

    public abstract int getSectionsCount();
    public abstract boolean isEditable(int sectionIndex);
    public abstract boolean hasTitle(int sectionIndex);
    @Nullable
    public abstract String getTitle(int sectionIndex, @NonNull Resources rs);
    public abstract int getItemsCount(int sectionIndex);
    public abstract int getItemsType(int sectionIndex);
    public abstract long getBookmarkId(@NonNull SectionPosition pos);
    public abstract long getTrackId(@NonNull SectionPosition pos);
    public abstract void onDelete(@NonNull SectionPosition pos);
  }

  private static class CategorySectionsDataSource extends SectionsDataSource
  {
    private int mSectionsCount;
    private int mBookmarksSectionIndex;
    private int mTracksSectionIndex;
    private int mDescriptionSectionIndex;

    CategorySectionsDataSource(@NonNull DataSource<BookmarkCategory> dataSource)
    {
      super(dataSource);
      calculateSections();
    }

    private void calculateSections()
    {
      mBookmarksSectionIndex = SectionPosition.INVALID_POSITION;
      mTracksSectionIndex = SectionPosition.INVALID_POSITION;

      mSectionsCount = 0;
      // We must always show the description, even if it's blank.
      mDescriptionSectionIndex = mSectionsCount++;
      if (getCategory().getTracksCount() > 0)
        mTracksSectionIndex = mSectionsCount++;
      if (getCategory().getBookmarksCount() > 0)
        mBookmarksSectionIndex = mSectionsCount++;
    }

    @Override
    public int getSectionsCount() { return mSectionsCount; }

    @Override
    public boolean isEditable(int sectionIndex)
    {
      return sectionIndex != mDescriptionSectionIndex;
    }

    @Override
    public boolean hasTitle(int sectionIndex) { return true; }

    @Nullable
    public String getTitle(int sectionIndex, @NonNull Resources rs)
    {
      if (sectionIndex == mDescriptionSectionIndex)
        return rs.getString(R.string.description);
      if (sectionIndex == mTracksSectionIndex)
        return rs.getString(R.string.tracks_title);
      return rs.getString(R.string.bookmarks);
    }

    @Override
    public int getItemsCount(int sectionIndex)
    {
      if (sectionIndex == mDescriptionSectionIndex)
        return 1;
      if (sectionIndex == mTracksSectionIndex)
        return getCategory().getTracksCount();
      if (sectionIndex == mBookmarksSectionIndex)
        return getCategory().getBookmarksCount();
      return 0;
    }

    @Override
    public int getItemsType(int sectionIndex)
    {
      if (sectionIndex == mDescriptionSectionIndex)
        return TYPE_DESC;
      if (sectionIndex == mTracksSectionIndex)
        return TYPE_TRACK;
      if (sectionIndex == mBookmarksSectionIndex)
        return TYPE_BOOKMARK;
      throw new AssertionError("Invalid section index: " + sectionIndex);
    }

    @Override
    public void onDelete(@NonNull SectionPosition pos)
    {
      // we must invalidate datasource before calculate sections
      invalidate();

      calculateSections();
    }

    @Override
    public long getBookmarkId(@NonNull SectionPosition pos)
    {
      return BookmarkManager.INSTANCE.getBookmarkIdByPosition(getCategory().getId(),
                                                              pos.getItemIndex());
    }

    @Override
    public long getTrackId(@NonNull SectionPosition pos)
    {
      return BookmarkManager.INSTANCE.getTrackIdByPosition(getCategory().getId(),
                                                           pos.getItemIndex());
    }
  }

  private static class SearchResultsSectionsDataSource extends SectionsDataSource
  {
    @NonNull
    private final List<Long> mSearchResults;

    SearchResultsSectionsDataSource(@NonNull DataSource<BookmarkCategory> dataSource,
                                    @NonNull List<Long> searchResults)
    {
      super(dataSource);
      mSearchResults = searchResults;
    }

    @Override
    public int getSectionsCount() { return 1; }

    @Override
    public boolean isEditable(int sectionIndex) { return true; }

    @Override
    public boolean hasTitle(int sectionIndex) { return false; }

    @Nullable
    public String getTitle(int sectionIndex, @NonNull Resources rs) { return null; }

    @Override
    public int getItemsCount(int sectionIndex) { return mSearchResults.size(); }

    @Override
    public int getItemsType(int sectionIndex) { return TYPE_BOOKMARK; }

    @Override
    public void onDelete(@NonNull SectionPosition pos)
    {
      mSearchResults.remove(pos.getItemIndex());
    }

    @Override
    public long getBookmarkId(@NonNull SectionPosition pos)
    {
      return mSearchResults.get(pos.getItemIndex());
    }

    @Override
    public long getTrackId(@NonNull SectionPosition pos)
    {
      throw new AssertionError("Tracks unsupported in search results.");
    }
  }

  private static class SortedSectionsDataSource extends SectionsDataSource
  {
    @NonNull
    private final List<SortedBlock> mSortedBlocks;

    SortedSectionsDataSource(@NonNull DataSource<BookmarkCategory> dataSource,
                             @NonNull List<SortedBlock> sortedBlocks)
    {
      super(dataSource);
      mSortedBlocks = sortedBlocks;
    }

    private boolean isDescriptionSection(int sectionIndex)
    {
      return hasDescription() && sectionIndex == 0;
    }

    @NonNull
    private SortedBlock getSortedBlock(int sectionIndex)
    {
      if (isDescriptionSection(sectionIndex))
        throw new IllegalArgumentException("Invalid section index for sorted block.");
      int blockIndex = sectionIndex - (hasDescription() ? 1 : 0);
      return mSortedBlocks.get(blockIndex);
    }

    @Override
    public int getSectionsCount()
    {
      return mSortedBlocks.size() + (hasDescription() ? 1 : 0);
    }

    @Override
    public boolean isEditable(int sectionIndex)
    {
      return !isDescriptionSection(sectionIndex);
    }

    @Override
    public boolean hasTitle(int sectionIndex) { return true; }

    @Nullable
    public String getTitle(int sectionIndex, @NonNull Resources rs)
    {
      if (isDescriptionSection(sectionIndex))
        return rs.getString(R.string.description);
      return getSortedBlock(sectionIndex).getName();
    }

    @Override
    public int getItemsCount(int sectionIndex)
    {
      if (isDescriptionSection(sectionIndex))
        return 1;
      SortedBlock block = getSortedBlock(sectionIndex);
      if (block.isBookmarksBlock())
        return block.getBookmarkIds().size();
      return block.getTrackIds().size();
    }

    @Override
    public int getItemsType(int sectionIndex)
    {
      if (isDescriptionSection(sectionIndex))
        return TYPE_DESC;
      if (getSortedBlock(sectionIndex).isBookmarksBlock())
        return TYPE_BOOKMARK;
      return TYPE_TRACK;
    }

    @Override
    public void onDelete(@NonNull SectionPosition pos)
    {
      if (isDescriptionSection(pos.getSectionIndex()))
        throw new IllegalArgumentException("Delete failed. Invalid section index.");

      int blockIndex = pos.getSectionIndex() - (hasDescription() ? 1 : 0);
      SortedBlock block = mSortedBlocks.get(blockIndex);
      if (block.isBookmarksBlock())
      {
        block.getBookmarkIds().remove(pos.getItemIndex());
        if (block.getBookmarkIds().isEmpty())
          mSortedBlocks.remove(blockIndex);
        return;
      }

      block.getTrackIds().remove(pos.getItemIndex());
      if (block.getTrackIds().isEmpty())
        mSortedBlocks.remove(blockIndex);
    }

    public long getBookmarkId(@NonNull SectionPosition pos)
    {
      return getSortedBlock(pos.getSectionIndex()).getBookmarkIds().get(pos.getItemIndex());
    }

    public long getTrackId(@NonNull SectionPosition pos)
    {
      return getSortedBlock(pos.getSectionIndex()).getTrackIds().get(pos.getItemIndex());
    }
  }

  BookmarkListAdapter(@NonNull DataSource<BookmarkCategory> dataSource)
  {
    mDataSource = dataSource;
    refreshSections();
  }

  private void refreshSections()
  {
    if (mSearchResults != null)
      mSectionsDataSource = new SearchResultsSectionsDataSource(mDataSource, mSearchResults);
    else if (mSortedResults != null)
      mSectionsDataSource = new SortedSectionsDataSource(mDataSource, mSortedResults);
    else
      mSectionsDataSource = new CategorySectionsDataSource(mDataSource);
  }

  private SectionPosition getSectionPosition(int position)
  {
    int startSectionRow = 0;
    boolean hasTitle;
    int sectionsCount = mSectionsDataSource.getSectionsCount();
    for (int i = 0; i < sectionsCount; ++i)
    {
      hasTitle = mSectionsDataSource.hasTitle(i);
      int sectionRowsCount = mSectionsDataSource.getItemsCount(i) + (hasTitle ? 1 : 0);
      if (startSectionRow == position && hasTitle)
        return new SectionPosition(i, SectionPosition.INVALID_POSITION);
      if (startSectionRow + sectionRowsCount > position)
        return new SectionPosition(i, position - startSectionRow - (hasTitle ? 1 : 0));
      startSectionRow += sectionRowsCount;
    }
    return new SectionPosition(SectionPosition.INVALID_POSITION, SectionPosition.INVALID_POSITION);
  }

  void setSearchResults(@Nullable long[] searchResults)
  {
    if (searchResults != null)
    {
      mSearchResults = new ArrayList<>(searchResults.length);
      for (long id : searchResults)
        mSearchResults.add(id);
    }
    else
    {
      mSearchResults = null;
    }
    refreshSections();
  }

  void setSortedResults(@Nullable SortedBlock[] sortedResults)
  {
    if (sortedResults != null)
      mSortedResults = new ArrayList<>(Arrays.asList(sortedResults));
    else
      mSortedResults = null;
    refreshSections();
  }

  public void setOnClickListener(@Nullable RecyclerClickListener listener)
  {
    mClickListener = listener;
  }

  void setOnLongClickListener(@Nullable RecyclerLongClickListener listener)
  {
    mLongClickListener = listener;
  }

  public void setMoreListener(@Nullable RecyclerClickListener listener)
  {
    mMoreClickListener = listener;
  }

  public void setIconClickListener(IconClickListener listener)
  {
    mIconClickListener = listener;
  }

  @Override
  @NonNull
  public Holders.BaseBookmarkHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType)
  {
    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
    Holders.BaseBookmarkHolder holder = null;
    switch (viewType)
    {
      case TYPE_TRACK:
        Holders.TrackViewHolder trackHolder =
            new Holders.TrackViewHolder(inflater.inflate(R.layout.item_track, parent,
                                                         false));
        trackHolder.setOnClickListener(mClickListener);
        trackHolder.setOnLongClickListener(mLongClickListener);
        trackHolder.setTrackIconClickListener(mIconClickListener);
        trackHolder.setMoreButtonClickListener(mMoreClickListener);
        holder = trackHolder;
        break;
      case TYPE_BOOKMARK:
        Holders.BookmarkViewHolder bookmarkHolder =
            new Holders.BookmarkViewHolder(inflater.inflate(R.layout.item_bookmark, parent,
                                                            false));
        bookmarkHolder.setOnClickListener(mClickListener);
        bookmarkHolder.setOnLongClickListener(mLongClickListener);
        holder = bookmarkHolder;
        break;
      case TYPE_SECTION:
        TextView tv = (TextView) inflater.inflate(R.layout.item_category_title, parent, false);
        holder = new Holders.SectionViewHolder(tv);
        break;
      case TYPE_DESC:
        View desc = inflater.inflate(R.layout.item_category_description, parent, false);
        TextView moreBtn = desc.findViewById(R.id.more_btn);
        TextView text = desc.findViewById(R.id.text);
        TextView title = desc.findViewById(R.id.title);
        setMoreButtonVisibility(text, moreBtn);
        holder = new Holders.DescriptionViewHolder(desc, mSectionsDataSource.getCategory());
        text.setOnClickListener(v -> onMoreButtonClicked(text, moreBtn));
        moreBtn.setOnClickListener(v -> onMoreButtonClicked(text, moreBtn));
        title.setOnClickListener(v -> onMoreButtonClicked(text, moreBtn));
        break;
    }

    if (holder == null)
      throw new AssertionError("Unsupported view type: " + viewType);

    return holder;
  }

  @Override
  public void onBindViewHolder(@NonNull Holders.BaseBookmarkHolder holder, int position)
  {
    SectionPosition sp = getSectionPosition(position);
    holder.bind(sp, mSectionsDataSource);
  }

  @Override
  public int getItemViewType(int position)
  {
    SectionPosition sp = getSectionPosition(position);
    if (sp.isTitlePosition())
      return TYPE_SECTION;
    if (sp.isItemPosition())
      return mSectionsDataSource.getItemsType(sp.getSectionIndex());
    throw new IllegalArgumentException("Position not found: " + position);
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
    int sectionsCount = mSectionsDataSource.getSectionsCount();
    for (int i = 0; i < sectionsCount; ++i)
    {
      int sectionItemsCount = mSectionsDataSource.getItemsCount(i);
      if (sectionItemsCount == 0)
        continue;
      itemCount += sectionItemsCount;
      if (mSectionsDataSource.hasTitle(i))
        ++itemCount;
    }
    return itemCount;
  }

  void onDelete(int position)
  {
    SectionPosition sp = getSectionPosition(position);
    mSectionsDataSource.onDelete(sp);
    // In case of the search results editing reset cached sorted blocks.
    if (isSearchResults())
      mSortedResults = null;
  }

  boolean isSearchResults()
  {
    return mSearchResults != null;
  }

  public Object getItem(int position)
  {
    if (getItemViewType(position) == TYPE_DESC)
      throw new UnsupportedOperationException("Not supported here! Position = " + position);

    SectionPosition pos = getSectionPosition(position);
    if (getItemViewType(position) == TYPE_TRACK)
    {
      final long trackId = mSectionsDataSource.getTrackId(pos);
      return BookmarkManager.INSTANCE.getTrack(trackId);
    }
    else
    {
      final long bookmarkId = mSectionsDataSource.getBookmarkId(pos);
      BookmarkInfo info = BookmarkManager.INSTANCE.getBookmarkInfo(bookmarkId);
      if (info == null)
        throw new RuntimeException("Bookmark no longer exists " + bookmarkId);
      return info;
    }
  }

  private void setMoreButtonVisibility(TextView text, TextView moreBtn)
  {
    text.post(() -> setShortModeDescription(text, moreBtn));
  }

  private void onMoreButtonClicked(TextView textView, TextView moreBtn)
  {
    if (isShortModeDescription(textView))
    {
      setExpandedModeDescription(textView, moreBtn);
    }
    else
    {
      setShortModeDescription(textView, moreBtn);
    }
  }

  private boolean isShortModeDescription(TextView text)
  {
    return text.getMaxLines() == MAX_VISIBLE_LINES;
  }

  private void setExpandedModeDescription(TextView textView, TextView moreBtn)
  {
    textView.setMaxLines(Integer.MAX_VALUE);
    moreBtn.setVisibility(View.GONE);
  }

  private void setShortModeDescription(TextView textView, TextView moreBtn)
  {
    textView.setMaxLines(MAX_VISIBLE_LINES);

    boolean isDescriptionTooLong = textView.getLineCount() > MAX_VISIBLE_LINES;
    moreBtn.setVisibility(isDescriptionTooLong ? View.VISIBLE : View.GONE);
  }
}
