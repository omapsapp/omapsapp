package app.organicmaps.editor;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.R;
import app.organicmaps.sdk.editor.data.FeatureCategory;
import app.organicmaps.util.UiUtils;

public class FeatureCategoryAdapter extends RecyclerView.Adapter<FeatureCategoryAdapter.FeatureViewHolder>
{
  private FeatureCategory[] mCategories;
  private final FeatureCategoryFragment mFragment;
  private final FeatureCategory mSelectedCategory;

  public FeatureCategoryAdapter(@NonNull FeatureCategoryFragment host, @NonNull FeatureCategory[] categories, @Nullable FeatureCategory category)
  {
    mFragment = host;
    mCategories = categories;
    mSelectedCategory = category;
  }

  public void setCategories(FeatureCategory[] categories)
  {
    mCategories = categories;
    notifyDataSetChanged();
  }

  @Override
  public FeatureViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    return new FeatureViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.item_feature_category, parent, false));
  }

  @Override
  public void onBindViewHolder(FeatureViewHolder holder, int position)
  {
    holder.bind(position);
  }

  @Override
  public int getItemCount()
  {
    return mCategories.length;
  }

  protected class FeatureViewHolder extends RecyclerView.ViewHolder
  {
    @NonNull
    private final TextView mName;
    @NonNull
    private final View mSelected;

    FeatureViewHolder(@NonNull View itemView)
    {
      super(itemView);
      mName = itemView.findViewById(R.id.name);
      mSelected = itemView.findViewById(R.id.selected);
      UiUtils.hide(mSelected);
      itemView.setOnClickListener(v -> onCategorySelected(getBindingAdapterPosition()));
    }

    public void bind(int position)
    {
      mName.setText(mCategories[position].getLocalizedTypeName());
      boolean showCondition = mSelectedCategory != null
                              && mCategories[position].getType().equals(mSelectedCategory.getType());
      UiUtils.showIf(showCondition, mSelected);
    }
  }

  private void onCategorySelected(int adapterPosition)
  {
    mFragment.selectCategory(mCategories[adapterPosition]);
  }
}
