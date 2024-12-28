package app.organicmaps.downloader;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.core.graphics.Insets;
import androidx.core.view.OnApplyWindowInsetsListener;
import androidx.core.view.WindowInsetsCompat;
import androidx.recyclerview.widget.RecyclerView;
import app.organicmaps.R;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.WindowInsetUtils;

final class DownloaderInsetsListener implements OnApplyWindowInsetsListener
{

  @NonNull
  private final Context mContext;
  @NonNull
  private final View mToolbar;
  @NonNull
  private final View mFab;
  @NonNull
  private final View mButton;
  @NonNull
  private final RecyclerView mRecyclerView;

  DownloaderInsetsListener(@NonNull View fragmentView)
  {
    mContext = fragmentView.getContext();
    mToolbar = fragmentView.findViewById(R.id.toolbar);
    mFab = fragmentView.findViewById(R.id.fab);
    mButton = fragmentView.findViewById(R.id.action);
    mRecyclerView = fragmentView.findViewById(R.id.recycler);
    mRecyclerView.setClipToPadding(false);
  }

  @NonNull
  @Override
  public WindowInsetsCompat onApplyWindowInsets(@NonNull View v, @NonNull WindowInsetsCompat insets)
  {
    final Insets safeInsets = insets.getInsets(WindowInsetUtils.TYPE_SAFE_DRAWING);

    mToolbar.setPadding(safeInsets.left, safeInsets.top, safeInsets.right, mToolbar.getPaddingBottom());

    boolean isAnyButtonVisible = UiUtils.isVisible(mFab) || UiUtils.isVisible(mButton);
    if (isAnyButtonVisible)
      applyInsetsToButtons(safeInsets);

    applyInsetsToRecyclerView(safeInsets, isAnyButtonVisible);

    // have to consume all insets here, so child views won't handle them again
    return WindowInsetsCompat.CONSUMED;
  }

  private void applyInsetsToButtons(Insets insets)
  {
    int baseMargin = UiUtils.dimen(mContext, R.dimen.margin_base);

    ViewGroup.MarginLayoutParams fabParams = (ViewGroup.MarginLayoutParams) mFab.getLayoutParams();
    ViewGroup.MarginLayoutParams buttonParams = (ViewGroup.MarginLayoutParams) mButton.getLayoutParams();

    final boolean isButtonVisible = UiUtils.isVisible(mButton);

    buttonParams.bottomMargin = insets.bottom;
    mButton.setPadding(insets.left, mButton.getPaddingTop(), insets.right, mButton.getPaddingBottom());

    fabParams.rightMargin = insets.right + baseMargin;
    if (isButtonVisible)
      fabParams.bottomMargin = baseMargin;
    else
      fabParams.bottomMargin = insets.bottom + baseMargin;

    mFab.requestLayout();
    mButton.requestLayout();
  }

  private void applyInsetsToRecyclerView(Insets insets, boolean isAnyButtonVisible)
  {
    int bottomInset = isAnyButtonVisible ? 0 : insets.bottom;

    mRecyclerView.setPadding(
        mRecyclerView.getPaddingLeft(),
        mRecyclerView.getPaddingTop(),
        mRecyclerView.getPaddingRight(),
        bottomInset);

    final ViewGroup.MarginLayoutParams layoutParams = (ViewGroup.MarginLayoutParams) mRecyclerView.getLayoutParams();
    layoutParams.rightMargin = insets.right;
    layoutParams.leftMargin = insets.left;

    mRecyclerView.requestLayout();
  }
}
