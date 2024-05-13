package app.organicmaps.search;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.R;
import app.organicmaps.sdk.search.DisplayedCategories;
import app.organicmaps.util.ThemeUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class CategoriesAdapter extends RecyclerView.Adapter<CategoriesAdapter.ViewHolder>
{
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({ TYPE_CATEGORY })
  @interface ViewType {}
  private static final int TYPE_CATEGORY = 0;

  @StringRes
  private int[] mCategoryResIds;
  @DrawableRes
  private int[] mIconResIds;

  private final LayoutInflater mInflater;
  private final Resources mResources;

  interface CategoriesUiListener
  {
    void onSearchCategorySelected(@Nullable String category);
  }

  private CategoriesUiListener mListener;

  CategoriesAdapter(@NonNull Fragment fragment)
  {
    if (fragment instanceof CategoriesUiListener)
      mListener = (CategoriesUiListener) fragment;
    mResources = fragment.getResources();
    mInflater = LayoutInflater.from(fragment.requireActivity());
  }

  void updateCategories(@NonNull Fragment fragment)
  {
    final Activity activity = fragment.requireActivity();
    final String packageName = activity.getPackageName();

    final String[] keys = getAllCategories();
    final int numKeys = keys.length;

    mCategoryResIds = new int[numKeys];
    mIconResIds = new int[numKeys];
    for (int i = 0; i < numKeys; i++)
    {
      String key = keys[i];
      mCategoryResIds[i] = getStringResIdByKey(activity.getResources(), packageName, key);

      if (mCategoryResIds[i] == 0)
        throw new IllegalStateException("Can't get string resource id for category:" + key);

      mIconResIds[i] = getDrawableResIdByKey(activity.getApplicationContext(), packageName, key);
      if (mIconResIds[i] == 0)
        throw new IllegalStateException("Can't get icon resource id for category:" + key);
    }
  }

  @NonNull
  private static String[] getAllCategories()
  {
    String[] searchCategories = DisplayedCategories.getKeys();
    int amountSize = searchCategories.length;
    String[] allCategories = new String[amountSize];

    for (int i = 0, j = 0; i < amountSize; i++)
    {
      if (allCategories[i] == null)
      {
        allCategories[i] = searchCategories[j];
        j++;
      }
    }

    return allCategories;
  }

  @SuppressLint("DiscouragedApi")
  @StringRes
  private static int getStringResIdByKey(@NonNull Resources resources, @NonNull String packageName,
                                         @NonNull String key)
  {
    return resources.getIdentifier(key, "string", packageName);
  }

  @SuppressLint("DiscouragedApi")
  @DrawableRes
  private static int getDrawableResIdByKey(@NonNull Context context,
                                           @NonNull String packageName,
                                           @NonNull String key)
  {
    final boolean isNightTheme = ThemeUtils.isNightTheme(context);
    String iconId = "ic_" + key;
    if (isNightTheme)
      iconId = iconId + "_night";
    return context.getResources().getIdentifier(iconId, "drawable", packageName);
  }

  @Override
  @ViewType
  public int getItemViewType(int position)
  {
    return TYPE_CATEGORY;
  }

  @Override
  public ViewHolder onCreateViewHolder(ViewGroup parent, @ViewType int viewType)
  {
    View view;
    ViewHolder viewHolder;
    if (viewType == TYPE_CATEGORY)
    {
      view = mInflater.inflate(R.layout.item_search_category, parent, false);
      viewHolder = new ViewHolder(view, (TextView) view);
    }
    else
    {
      throw new AssertionError("Unsupported type detected: " + viewType);
    }

    viewHolder.setupClickListeners();
    return viewHolder;
  }

  @Override
  public void onBindViewHolder(ViewHolder holder, int position)
  {
    holder.setTextAndIcon(mCategoryResIds[position], mIconResIds[position]);
  }

  @Override
  public int getItemCount()
  {
    return mCategoryResIds.length;
  }

  class ViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener
  {
    @NonNull
    private final TextView mTitle;
    @NonNull
    private final View mView;

    ViewHolder(@NonNull View v, @NonNull TextView tv)
    {
      super(v);
      mView = v;
      mTitle = tv;
    }

    void setupClickListeners()
    {
      mView.setOnClickListener(this);
    }

    @Override
    public final void onClick(View v)
    {
      final int position = getBindingAdapterPosition();
      onItemClicked(position);
    }

    void onItemClicked(int position)
    {
      if (mListener != null)
      {
        @StringRes
        int categoryId = mCategoryResIds[position];
        mListener.onSearchCategorySelected(mResources.getString(categoryId) + " ");
      }
    }

    void setTextAndIcon(@StringRes int textResId, @DrawableRes int iconResId)
    {
      mTitle.setText(textResId);
      mTitle.setCompoundDrawablesRelativeWithIntrinsicBounds(iconResId, 0, 0, 0);
    }

  }
}
