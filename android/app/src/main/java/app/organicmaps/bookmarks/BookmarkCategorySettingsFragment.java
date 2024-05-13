package app.organicmaps.bookmarks;

import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import app.organicmaps.R;
import app.organicmaps.base.BaseMwmToolbarFragment;
import app.organicmaps.sdk.bookmarks.data.BookmarkCategory;
import app.organicmaps.sdk.bookmarks.data.BookmarkManager;
import app.organicmaps.util.Utils;

import app.organicmaps.util.InputUtils;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import java.util.Objects;

public class BookmarkCategorySettingsFragment extends BaseMwmToolbarFragment
{
  private static final int TEXT_LENGTH_LIMIT = 60;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private BookmarkCategory mCategory;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private TextInputEditText mEditDescView;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private TextInputEditText mEditCategoryNameView;

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    final Bundle args = requireArguments();
    mCategory = Objects.requireNonNull(Utils.getParcelable(args,
        BookmarkCategorySettingsActivity.EXTRA_BOOKMARK_CATEGORY, BookmarkCategory.class));
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.fragment_bookmark_category_settings, container, false);
    setHasOptionsMenu(true);
    initViews(root);
    return root;
  }

  private void initViews(@NonNull View root)
  {
    mEditCategoryNameView = root.findViewById(R.id.edit_list_name_view);
    TextInputLayout clearNameBtn = root.findViewById(R.id.edit_list_name_input);
    clearNameBtn.setEndIconOnClickListener(v -> clearAndFocus(mEditCategoryNameView));
    mEditCategoryNameView.setText(mCategory.getName());
    InputFilter[] f = { new InputFilter.LengthFilter(TEXT_LENGTH_LIMIT) };
    mEditCategoryNameView.setFilters(f);
    mEditCategoryNameView.requestFocus();
    mEditCategoryNameView.addTextChangedListener(new TextWatcher()
    {
      @Override
      public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

      @Override
      public void onTextChanged(CharSequence charSequence, int i, int i1, int i2)
      {
        clearNameBtn.setEndIconVisible(charSequence.length() > 0);
      }

      @Override
      public void afterTextChanged(Editable editable) {}
    });
    mEditDescView = root.findViewById(R.id.edit_description);
    mEditDescView.setText(mCategory.getDescription());
  }

  @Override
  public void onCreateOptionsMenu(@NonNull Menu menu, MenuInflater inflater)
  {
    inflater.inflate(R.menu.menu_done, menu);
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item)
  {
    if (item.getItemId() == R.id.done)
    {
      onEditDoneClicked();
      return true;
    }
    return super.onOptionsItemSelected(item);
  }

  private void onEditDoneClicked()
  {
    final String newCategoryName = getEditableCategoryName();
    if (!validateCategoryName(newCategoryName))
      return;

    if (isCategoryNameChanged())
      BookmarkManager.INSTANCE.setCategoryName(mCategory.getId(), newCategoryName);

    if (isCategoryDescChanged())
      BookmarkManager.INSTANCE.setCategoryDescription(mCategory.getId(), getEditableCategoryDesc());

    requireActivity().finish();
  }

  private boolean isCategoryNameChanged()
  {
    String categoryName = getEditableCategoryName();
    return !TextUtils.equals(categoryName, mCategory.getName());
  }

  private boolean validateCategoryName(@Nullable String name)
  {
    if (TextUtils.isEmpty(name))
    {
      new MaterialAlertDialogBuilder(requireActivity(), R.style.MwmTheme_AlertDialog)
          .setTitle(R.string.bookmarks_error_title_empty_list_name)
          .setMessage(R.string.bookmarks_error_message_empty_list_name)
          .setPositiveButton(R.string.ok, null)
          .show();
      return false;
    }

    if (BookmarkManager.INSTANCE.isUsedCategoryName(name) && !TextUtils.equals(name, mCategory.getName()))
    {
      new MaterialAlertDialogBuilder(requireActivity(), R.style.MwmTheme_AlertDialog)
          .setTitle(R.string.bookmarks_error_title_list_name_already_taken)
          .setMessage(R.string.bookmarks_error_message_list_name_already_taken)
          .setPositiveButton(R.string.ok, null)
          .show();
      return false;
    }
    return true;
  }

  @NonNull
  private String getEditableCategoryName()
  {
    return mEditCategoryNameView.getEditableText().toString().trim();
  }

  @NonNull
  private String getEditableCategoryDesc()
  {
    return mEditDescView.getEditableText().toString().trim();
  }

  private boolean isCategoryDescChanged()
  {
    String categoryDesc = getEditableCategoryDesc();
    return !TextUtils.equals(mCategory.getDescription(), categoryDesc);
  }

  private void clearAndFocus(TextView textView)
  {
    textView.getEditableText().clear();
    textView.requestFocus();
    InputUtils.showKeyboard(textView);
  }
}
