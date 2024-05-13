package app.organicmaps.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.R;
import app.organicmaps.sdk.bookmarks.data.BookmarkCategory;

import java.util.List;

public class ChooseBookmarkCategoryAdapter extends BaseBookmarkCategoryAdapter<ChooseBookmarkCategoryAdapter.SingleChoiceHolder>
{
  public static final int VIEW_TYPE_CATEGORY = 0;
  public static final int VIEW_TYPE_ADD_NEW = 1;

  private int mCheckedPosition;

  public interface CategoryListener
  {
    void onCategorySet(int categoryPosition);

    void onCategoryCreate();
  }

  private CategoryListener mListener;

  public ChooseBookmarkCategoryAdapter(Context context, int pos,
                                       @NonNull List<BookmarkCategory> categories)
  {
    super(context, categories);
    mCheckedPosition = pos;
  }

  public void setListener(CategoryListener listener)
  {
    mListener = listener;
  }

  @Override
  public SingleChoiceHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    View view;
    final LayoutInflater inflater = LayoutInflater.from(parent.getContext());
    if (viewType == VIEW_TYPE_CATEGORY)
      view = inflater.inflate(R.layout.item_bookmark_category_choose, parent, false);
    else
      view = inflater.inflate(R.layout.item_bookmark_category_create, parent, false);

    final SingleChoiceHolder holder = new SingleChoiceHolder(view);

    view.setOnClickListener(v -> {
      if (mListener == null)
        return;

      if (holder.getItemViewType() == VIEW_TYPE_ADD_NEW)
        mListener.onCategoryCreate();
      else
        mListener.onCategorySet(holder.getBindingAdapterPosition());
    });

    return holder;
  }

  @Override
  public void onBindViewHolder(SingleChoiceHolder holder, int position)
  {
    if (holder.getItemViewType() == VIEW_TYPE_CATEGORY)
    {
      BookmarkCategory category = getCategoryByPosition(position);
      holder.name.setText(category.getName());
      holder.checked.setChecked(mCheckedPosition == position);
    }
  }

  @Override
  public int getItemViewType(int position)
  {
    return position == getItemCount() - 1 ? VIEW_TYPE_ADD_NEW : VIEW_TYPE_CATEGORY;
  }

  @Override
  public int getItemCount()
  {
    return super.getItemCount() + 1;
  }

  public void chooseItem(int position)
  {
    final int oldPosition = mCheckedPosition;
    mCheckedPosition = position;
    notifyItemChanged(oldPosition);
    notifyItemChanged(mCheckedPosition);
  }

  static class SingleChoiceHolder extends RecyclerView.ViewHolder
  {
    TextView name;
    RadioButton checked;

    public SingleChoiceHolder(View convertView)
    {
      super(convertView);
      name = convertView.findViewById(R.id.tv__set_name);
      checked = convertView.findViewById(R.id.rb__selected);
    }
  }
}
