package app.organicmaps.bookmarks;

import android.app.Activity;
import android.app.ProgressDialog;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;

import app.organicmaps.R;
import app.organicmaps.sdk.bookmarks.data.BookmarkCategory;
import app.organicmaps.sdk.bookmarks.data.BookmarkManager;
import app.organicmaps.sdk.bookmarks.data.BookmarkSharingResult;
import app.organicmaps.util.SharingUtils;
import app.organicmaps.sdk.util.log.Logger;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.util.ArrayList;
import java.util.List;


public enum BookmarksSharingHelper
{
  INSTANCE;

  private static final String TAG = BookmarksSharingHelper.class.getSimpleName();

  @Nullable
  private ProgressDialog mProgressDialog;

  public void prepareBookmarkCategoryForSharing(@NonNull Activity context, long catId)
  {
    showProgressDialog(context);
    BookmarkManager.INSTANCE.prepareCategoriesForSharing(new long[]{catId});
  }

  private void showProgressDialog(@NonNull Activity context)
  {
    mProgressDialog = new ProgressDialog(context, R.style.MwmTheme_ProgressDialog);
    mProgressDialog.setMessage(context.getString(R.string.please_wait));
    mProgressDialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
    mProgressDialog.setIndeterminate(true);
    mProgressDialog.setCancelable(false);
    mProgressDialog.show();
  }

  public void onPreparedFileForSharing(@NonNull FragmentActivity context,
                                       @NonNull BookmarkSharingResult result)
  {
    if (mProgressDialog != null && mProgressDialog.isShowing())
      mProgressDialog.dismiss();

    switch (result.getCode())
    {
      case BookmarkSharingResult.SUCCESS ->
          SharingUtils.shareBookmarkFile(context, result.getSharingPath());
      case BookmarkSharingResult.EMPTY_CATEGORY ->
          new MaterialAlertDialogBuilder(context, R.style.MwmTheme_AlertDialog)
              .setTitle(R.string.bookmarks_error_title_share_empty)
              .setMessage(R.string.bookmarks_error_message_share_empty)
              .setPositiveButton(R.string.ok, null)
              .show();
      case BookmarkSharingResult.ARCHIVE_ERROR, BookmarkSharingResult.FILE_ERROR ->
      {
        new MaterialAlertDialogBuilder(context, R.style.MwmTheme_AlertDialog)
            .setTitle(R.string.dialog_routing_system_error)
            .setMessage(R.string.bookmarks_error_message_share_general)
            .setPositiveButton(R.string.ok, null)
            .show();
        List<String> names = new ArrayList<>();
        for (long categoryId : result.getCategoriesIds())
          names.add(BookmarkManager.INSTANCE.getCategoryById(categoryId).getName());
        Logger.e(TAG, "Failed to share bookmark categories " + names + ", error code: " + result.getCode());
      }
      default -> throw new AssertionError("Unsupported bookmark sharing code: " + result.getCode());
    }
  }

  public void prepareBookmarkCategoriesForSharing(@NonNull Activity context)
  {
    showProgressDialog(context);
    List<BookmarkCategory> categories = BookmarkManager.INSTANCE.getCategories();
    long[] categoryIds = new long[categories.size()];
    for (int i = 0; i < categories.size(); i++)
      categoryIds[i] = categories.get(i).getId();
    BookmarkManager.INSTANCE.prepareCategoriesForSharing(categoryIds);
  }
}
