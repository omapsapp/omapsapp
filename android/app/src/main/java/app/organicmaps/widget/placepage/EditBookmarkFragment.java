package app.organicmaps.widget.placepage;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.Toolbar;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.FragmentManager;
import app.organicmaps.R;
import app.organicmaps.base.BaseMwmDialogFragment;
import app.organicmaps.bookmarks.ChooseBookmarkCategoryFragment;
import app.organicmaps.bookmarks.ChooseBookmarkCategoryFragment.Listener;
import app.organicmaps.bookmarks.data.BookmarkCategory;
import app.organicmaps.bookmarks.data.BookmarkInfo;
import app.organicmaps.bookmarks.data.BookmarkManager;
import app.organicmaps.bookmarks.data.Icon;
import app.organicmaps.bookmarks.data.Track;
import app.organicmaps.util.Graphics;
import app.organicmaps.util.InputUtils;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.WindowInsetUtils.PaddingInsetsListener;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import java.util.List;

public class EditBookmarkFragment extends BaseMwmDialogFragment implements View.OnClickListener, Listener
{
  public static final String EXTRA_CATEGORY_ID = "CategoryId";
  public static final String EXTRA_ID = "BookmarkTrackId";
  public static final String EXTRA_BOOKMARK_TYPE = "BookmarkType";
  public static final int TYPE_BOOKMARK = 1;
  public static final int TYPE_TRACK = 2;

  private TextInputEditText mEtDescription;
  private TextInputEditText mEtName;
  @NonNull
  private TextInputLayout clearNameBtn;
  private TextView mTvBookmarkGroup;
  private ImageView mIvColor;
  private BookmarkCategory mBookmarkCategory;
  @Nullable
  private Icon mIcon;
  @Nullable
  private BookmarkInfo mBookmark;
  @Nullable
  private EditBookmarkListener mListener;
  private Track mTrack;
  private int mType;
  private int mColor = -1;

  public interface EditBookmarkListener
  {
    void onBookmarkSaved(long bookmarkId, boolean movedFromCategory);
  }

  public static void editBookmark(long categoryId, long bookmarkId, @NonNull Context context,
                                  @NonNull FragmentManager manager,
                                  @Nullable EditBookmarkListener listener)
  {
    final Bundle args = new Bundle();
    args.putInt(EXTRA_BOOKMARK_TYPE, TYPE_BOOKMARK);
    args.putLong(EXTRA_CATEGORY_ID, categoryId);
    args.putLong(EXTRA_ID, bookmarkId);
    String name = EditBookmarkFragment.class.getName();
    final FragmentFactory factory = manager.getFragmentFactory();
    final EditBookmarkFragment fragment = (EditBookmarkFragment) factory.instantiate(context.getClassLoader(), name);
    fragment.setArguments(args);
    fragment.setEditBookmarkListener(listener);
    fragment.show(manager, name);
  }

  public static void editTrack(long categoryId, long trackId, Context context, FragmentManager manager, @Nullable EditBookmarkListener listener)
  {
    final Bundle args = new Bundle();
    args.putInt(EXTRA_BOOKMARK_TYPE, TYPE_TRACK);
    args.putLong(EXTRA_CATEGORY_ID, categoryId);
    args.putLong(EXTRA_ID, trackId);
    String name = EditBookmarkFragment.class.getName();
    final FragmentFactory factory = manager.getFragmentFactory();
    final EditBookmarkFragment fragment = (EditBookmarkFragment) factory.instantiate(context.getClassLoader(), name);
    fragment.setArguments(args);
    fragment.setEditBookmarkListener(listener);
    fragment.show(manager, name);
  }

  public EditBookmarkFragment() {}

  @Override
  protected int getCustomTheme()
  {
    return getFullscreenTheme();
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    return inflater.inflate(R.layout.fragment_edit_bookmark, container, false);
  }

  @Override
  public void onViewCreated(@NonNull View view, Bundle savedInstanceState)
  {
    final Bundle args = getArguments();
    mType = args.getInt(EXTRA_BOOKMARK_TYPE);
    mEtName = view.findViewById(R.id.et__bookmark_name);
    clearNameBtn = view.findViewById(R.id.edit_bookmark_name_input);
    clearNameBtn.setEndIconOnClickListener(v -> clearAndFocus(mEtName));
    mEtName.addTextChangedListener(new TextWatcher()
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
    mEtDescription = view.findViewById(R.id.et__description);
    mTvBookmarkGroup = view.findViewById(R.id.tv__bookmark_set);
    mTvBookmarkGroup.setOnClickListener(this);
    mIvColor = view.findViewById(R.id.iv__bookmark_color);
    mIvColor.setOnClickListener(this);

    //For tracks an bookmarks same category is used so this portion is common for both
    long categoryId = args.getLong(EXTRA_CATEGORY_ID);
    mBookmarkCategory = BookmarkManager.INSTANCE.getCategoryById(categoryId);
    long id = args.getLong(EXTRA_ID);

    switch (mType)
    {
      case TYPE_BOOKMARK ->
      {
        mBookmark = BookmarkManager.INSTANCE.getBookmarkInfo(id);
        if (mBookmark != null)
          mIcon = mBookmark.getIcon();
        refreshBookmark();
      }
      case TYPE_TRACK ->
      {
        mTrack = BookmarkManager.INSTANCE.getTrack(id);
        mColor = mTrack.getColor();
        refreshTrack();
      }
    }
    initToolbar(view);
  }

  @Override
  public void onStart()
  {
    super.onStart();

    // Focus name and show keyboard for "Unknown Place" bookmarks
    if (mBookmark != null && mBookmark.getName().equals(getString(R.string.core_placepage_unknown_place)))
    {
      mEtName.requestFocus();
      mEtName.selectAll();
      // Recommended way of showing the keyboard on activity start
      // https://developer.android.com/develop/ui/views/touch-and-input/keyboard-input/visibility#ShowReliably
      WindowCompat.getInsetsController(requireActivity().getWindow(), mEtName).show(WindowInsetsCompat.Type.ime());
    }
  }

  private void initToolbar(View view)
  {
    Toolbar toolbar = view.findViewById(R.id.toolbar);

    ViewCompat.setOnApplyWindowInsetsListener(toolbar, PaddingInsetsListener.excludeBottom());

    final ImageView imageView = toolbar.findViewById(R.id.save);
    switch (mType)
    {
      case TYPE_BOOKMARK ->
      {
        imageView.setOnClickListener(v -> saveBookmark());
        toolbar.setTitle(R.string.placepage_edit_bookmark_button);
      }
      case TYPE_TRACK ->
      {
        imageView.setOnClickListener(v -> saveTrack());
        toolbar.setTitle(R.string.edit_track);
      }
    }
    UiUtils.showHomeUpButton(toolbar);
    toolbar.setNavigationOnClickListener(v -> dismiss());
  }

  private void saveBookmark()
  {
    if (mBookmark == null)
    {
      dismiss();
      return;
    }
    boolean movedFromCategory = mBookmark.getCategoryId() != mBookmarkCategory.getId();
    if (movedFromCategory)
      BookmarkManager.INSTANCE.notifyCategoryChanging(mBookmark, mBookmarkCategory.getId());
    BookmarkManager.INSTANCE.notifyParametersUpdating(mBookmark, mEtName.getText().toString(),
                                                      mIcon, mEtDescription.getText().toString());

    if (mListener != null)
      mListener.onBookmarkSaved(mBookmark.getBookmarkId(), movedFromCategory);
    dismiss();
  }

  private void saveTrack()
  {
    if (mTrack == null)
    {
      dismiss();
      return;
    }
    boolean movedFromCategory = mTrack.getCategoryId() != mBookmarkCategory.getId();
    if (movedFromCategory)
      BookmarkManager.INSTANCE.notifyCategoryChanging(mTrack, mBookmarkCategory.getId());
    BookmarkManager.INSTANCE.notifyParametersUpdating(mTrack, mEtName.getText().toString(),
                                                      mColor, mEtDescription.getText().toString());
    if (mListener != null)
      mListener.onBookmarkSaved(mTrack.getTrackId(), movedFromCategory);
    dismiss();
  }

  @Override
  public void onClick(View v)
  {
    final int id = v.getId();
    if (id == R.id.iv__bookmark_color)
      selectBookmarkColor();
    else if (id == R.id.tv__bookmark_set)
      selectBookmarkSet();
  }

  private void selectBookmarkSet()
  {
    if (mBookmark == null && mTrack == null)
      return;

    final Bundle args = new Bundle();
    final List<BookmarkCategory> categories = BookmarkManager.INSTANCE.getCategories();
    final int index = categories.indexOf(mBookmarkCategory);
    args.putInt(ChooseBookmarkCategoryFragment.CATEGORY_POSITION, index);

    final FragmentManager manager = getChildFragmentManager();
    String className = ChooseBookmarkCategoryFragment.class.getName();
    final FragmentFactory factory = manager.getFragmentFactory();
    final ChooseBookmarkCategoryFragment frag =
        (ChooseBookmarkCategoryFragment) factory.instantiate(getContext().getClassLoader(), className);
    frag.setArguments(args);
    frag.show(manager, null);
  }

  private void selectBookmarkColor()
  {
    if (mIcon == null && mTrack == null)
      return;

    final Bundle args = new Bundle();
    if (mTrack != null)
      args.putInt(BookmarkColorDialogFragment.ICON_TYPE, mTrack.getColor());
    else
      args.putInt(BookmarkColorDialogFragment.ICON_TYPE, mIcon.getColor());
    final FragmentManager manager = getChildFragmentManager();
    String className = BookmarkColorDialogFragment.class.getName();
    final FragmentFactory factory = manager.getFragmentFactory();
    final BookmarkColorDialogFragment dialogFragment =
        (BookmarkColorDialogFragment) factory.instantiate(getContext().getClassLoader(), className);
    dialogFragment.setArguments(args);
    switch (mType)
    {
      case TYPE_BOOKMARK ->
        dialogFragment.setOnColorSetListener(colorPos -> {
          final Icon newIcon = BookmarkManager.ICONS.get(colorPos);
          if (mIcon.getColor() == newIcon.getColor())
            return;

          mIcon = newIcon;
          refreshColorMarker();
        });
      case TYPE_TRACK ->
        dialogFragment.setOnColorSetListener(colorPos -> {
          int from = mTrack.getColor();
          int to = BookmarkManager.ICONS.get(colorPos).argb();
          if (from == to)
            return;
          mColor = to;
          refreshTrackColor();
        });
    }

    dialogFragment.show(requireActivity().getSupportFragmentManager(), null);
  }

  private void refreshColorMarker()
  {
    if (mIcon != null)
    {
      Drawable circle = Graphics.drawCircleAndImage(mIcon.argb(),
                                                    R.dimen.track_circle_size,
                                                    R.drawable.ic_bookmark_none,
                                                    R.dimen.bookmark_icon_size,
                                                    requireContext());
      mIvColor.setImageDrawable(circle);
    }
  }

  public void refreshTrackColor()
  {
    if (mColor == -1)
      return;
    Drawable circle = Graphics.drawCircle(mColor,
                                          R.dimen.track_circle_size,
                                          requireContext().getResources());
    mIvColor.setImageDrawable(circle);
  }

  private void refreshCategory()
  {
    mTvBookmarkGroup.setText(mBookmarkCategory.getName());
  }

  private void refreshBookmark()
  {
    if (mBookmark == null)
      return;

    if (TextUtils.isEmpty(mEtName.getText()))
      mEtName.setText(mBookmark.getName());

    if (TextUtils.isEmpty(mEtDescription.getText()))
    {
      mEtDescription.setText(
          BookmarkManager.INSTANCE.getBookmarkDescription(mBookmark.getBookmarkId()));
    }
    refreshCategory();
    refreshColorMarker();
  }

  private void refreshTrack()
  {
    if (mTrack == null)
      return;

    if (TextUtils.isEmpty(mEtName.getText()))
      mEtName.setText(mTrack.getName());

    if (TextUtils.isEmpty(mEtDescription.getText()))
    {
      mEtDescription.setText(BookmarkManager.INSTANCE.getTrackDescription(mTrack.getTrackId()));
    }
    refreshCategory();
    refreshTrackColor();
  }

  @Override
  public void onCategoryChanged(BookmarkCategory newCategory)
  {
    mBookmarkCategory = newCategory;
    refreshCategory();
  }

  public void setEditBookmarkListener(@Nullable EditBookmarkListener listener)
  {
    mListener = listener;
  }
  private void clearAndFocus(TextView textView)
  {
    textView.getEditableText().clear();
    textView.requestFocus();
    InputUtils.showKeyboard(textView);
  }
}
