package app.organicmaps.bookmarks;

import android.app.Activity;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import app.organicmaps.R;
import app.organicmaps.base.BaseMwmDialogFragment;
import app.organicmaps.sdk.bookmarks.data.BookmarkCategory;
import app.organicmaps.sdk.bookmarks.data.BookmarkManager;
import app.organicmaps.dialog.EditTextDialogFragment;

import java.util.List;

public class ChooseBookmarkCategoryFragment extends BaseMwmDialogFragment
                                         implements ChooseBookmarkCategoryAdapter.CategoryListener
{
  public static final String CATEGORY_POSITION = "ExtraCategoryPosition";

  private ChooseBookmarkCategoryAdapter mAdapter;
  private RecyclerView mRecycler;

  public interface Listener
  {
    void onCategoryChanged(@NonNull BookmarkCategory newCategory);
  }
  private Listener mListener;

  @Override
  protected int getStyle()
  {
    return STYLE_NO_TITLE;
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.choose_bookmark_category_fragment, container, false);
    mRecycler = root.findViewById(R.id.recycler);
    mRecycler.setLayoutManager(new LinearLayoutManager(requireActivity()));
    return root;
  }

  @Override
  public void onViewCreated(View view, Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    final Bundle args = getArguments();
    final int catPosition = args.getInt(CATEGORY_POSITION, 0);
    List<BookmarkCategory> items = BookmarkManager.INSTANCE.getCategories();
    mAdapter = new ChooseBookmarkCategoryAdapter(requireActivity(), catPosition, items);
    mAdapter.setListener(this);
    mRecycler.setAdapter(mAdapter);
  }

  @Override
  public void onAttach(Activity activity)
  {
    if (mListener == null)
    {
      final Fragment parent = getParentFragment();
      if (parent instanceof Listener)
        mListener = (Listener) parent;
      else if (activity instanceof Listener)
        mListener = (Listener) activity;
    }

    super.onAttach(activity);
  }

  private void createCategory(@NonNull String name)
  {
    BookmarkManager.INSTANCE.createCategory(name);

    List<BookmarkCategory> bookmarkCategories = mAdapter.getBookmarkCategories();

    if (bookmarkCategories.isEmpty())
      throw new AssertionError("BookmarkCategories are empty");

    int categoryPosition = -1;

    for (int i = 0; i < bookmarkCategories.size(); i++)
    {
      if (bookmarkCategories.get(i).getName().equals(name))
      {
        categoryPosition = i;
        break;
      }
    }

    if (categoryPosition == -1)
      throw new AssertionError("No selected category in the list");

    mAdapter.chooseItem(categoryPosition);

    if (mListener != null)
    {
      BookmarkCategory newCategory = bookmarkCategories.get(categoryPosition);
      mListener.onCategoryChanged(newCategory);
    }
    dismiss();
  }

  @Override
  public void onCategorySet(int categoryPosition)
  {
    mAdapter.chooseItem(categoryPosition);
    if (mListener != null)
    {
      final BookmarkCategory category = mAdapter.getBookmarkCategories().get(categoryPosition);
      mListener.onCategoryChanged(category);
    }
    dismiss();
  }

  @Override
  public void onCategoryCreate()
  {
    EditTextDialogFragment dialogFragment =
        EditTextDialogFragment.show(getString(R.string.bookmark_set_name),
                                    null,
                                    getString(R.string.ok),
                                    null,
                                    this,
                                    new CategoryValidator());
    dialogFragment.setTextSaveListener(this::createCategory);
  }
}
