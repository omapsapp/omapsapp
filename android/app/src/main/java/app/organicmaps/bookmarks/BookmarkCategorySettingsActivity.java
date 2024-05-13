package app.organicmaps.bookmarks;

import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import app.organicmaps.R;
import app.organicmaps.base.BaseMwmFragmentActivity;
import app.organicmaps.sdk.bookmarks.data.BookmarkCategory;

public class BookmarkCategorySettingsActivity extends BaseMwmFragmentActivity
{
  public static final int REQUEST_CODE = 107;
  public static final String EXTRA_BOOKMARK_CATEGORY = "bookmark_category";

  @Override
  protected int getContentLayoutResId()
  {
    return R.layout.fragment_container_layout;
  }

  @Override
  protected int getFragmentContentResId()
  {
    return R.id.fragment_container;
  }

  @Override
  protected Class<? extends Fragment> getFragmentClass()
  {
    return BookmarkCategorySettingsFragment.class;
  }

  public static void startForResult(@NonNull Fragment fragment,
                                           @NonNull BookmarkCategory category)
  {
    android.content.Intent intent = new Intent(fragment.requireActivity(), BookmarkCategorySettingsActivity.class)
        .putExtra(EXTRA_BOOKMARK_CATEGORY, category);
    fragment.startActivityForResult(intent, REQUEST_CODE);
  }
}
