package app.organicmaps.util;

import android.animation.Animator;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.AnyRes;
import androidx.annotation.AttrRes;
import androidx.annotation.ColorInt;
import androidx.annotation.DimenRes;
import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.appcompat.widget.Toolbar;
import androidx.core.content.ContextCompat;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.RecyclerView;
import app.organicmaps.MwmApplication;
import app.organicmaps.R;
import com.google.android.material.textfield.TextInputLayout;

import java.util.Objects;

public final class UiUtils
{
  public static final int NO_ID = -1;
  public static final String NEW_STRING_DELIMITER = "\n";

  public static void bringViewToFrontOf(@NonNull View frontView, @NonNull View backView)
  {
    frontView.setZ(backView.getZ() + 1);
  }

  public static class SimpleAnimatorListener implements Animator.AnimatorListener
  {
    @Override
    public void onAnimationStart(Animator animation) {}

    @Override
    public void onAnimationEnd(Animator animation) {}

    @Override
    public void onAnimationCancel(Animator animation) {}

    @Override
    public void onAnimationRepeat(Animator animation) {}
  }

  public static void waitLayout(final View view, @NonNull final ViewTreeObserver.OnGlobalLayoutListener callback) {
    view.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {

      @Override
      public void onGlobalLayout() {
        // viewTreeObserver can be dead(isAlive() == false), we should get a new one here.
        view.getViewTreeObserver().removeOnGlobalLayoutListener(this);
        callback.onGlobalLayout();
      }
    });
  }

  public static void hide(@NonNull View view)
  {
    view.setVisibility(View.GONE);
  }

  public static void hide(@NonNull View... views)
  {
    for (final View v : views)
      v.setVisibility(View.GONE);
  }

  public static void hide(View frame, @IdRes int viewId)
  {
    View view = frame.findViewById(viewId);
    if (view == null)
      return;

    hide(view);
  }

  public static void hide(View frame, @IdRes int... viewIds)
  {
    for (final int id : viewIds)
      hide(frame, id);
  }

  public static void show(View view)
  {
    view.setVisibility(View.VISIBLE);
  }

  public static void show(View... views)
  {
    for (final View v : views)
      v.setVisibility(View.VISIBLE);
  }

  public static void invisible(View... views)
  {
    for (final View v : views)
      v.setVisibility(View.INVISIBLE);
  }

  public static boolean isVisible(View view)
  {
    return view.getVisibility() == View.VISIBLE;
  }

  public static void visibleIf(boolean condition, View view)
  {
    view.setVisibility(condition ? View.VISIBLE : View.INVISIBLE);
  }

  public static void visibleIf(boolean condition, View... views)
  {
    if (condition)
      show(views);
    else
      invisible(views);
  }

  public static void showIf(boolean condition, View view)
  {
    view.setVisibility(condition ? View.VISIBLE : View.GONE);
  }

  public static void hideIf(boolean condition, View... views)
  {
    if (condition)
    {
      hide(views);
    }
    else
    {
      show(views);
    }
  }

  public static void showIf(boolean condition, View... views)
  {
    if (condition)
      show(views);
    else
      hide(views);
  }

  public static void showIf(boolean condition, View parent, @IdRes int... viewIds)
  {
    for (@IdRes int id : viewIds)
    {
      if (condition)
        show(parent.findViewById(id));
      else
        hide(parent.findViewById(id));
    }
  }

  public static void setTextAndShow(TextView tv, CharSequence text)
  {
    tv.setText(text);
    show(tv);
  }

  public static void setTextAndHideIfEmpty(TextView tv, CharSequence text)
  {
    tv.setText(text);
    showIf(!TextUtils.isEmpty(text), tv);
  }

  public static void clearTextAndHide(TextView tv)
  {
    tv.setText("");
    hide(tv);
  }

  public static void showHomeUpButton(Toolbar toolbar)
  {
    toolbar.setNavigationIcon(ThemeUtils.getResource(toolbar.getContext(), androidx.appcompat.R.attr.homeAsUpIndicator));
  }

  public static boolean isTablet(@NonNull Context context)
  {
    return MwmApplication.from(context).getResources().getBoolean(R.bool.tabletLayout);
  }

  public static int dimen(@NonNull Context context, @DimenRes int id)
  {
    return context.getResources().getDimensionPixelSize(id);
  }

  public static int getDisplayTotalHeight(Context context)
  {
    DisplayMetrics metrics = new DisplayMetrics();
    WindowManager windowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
    windowManager.getDefaultDisplay().getRealMetrics(metrics);
    return metrics.heightPixels;
  }

  public static void updateRedButton(Button button)
  {
    button.setTextColor(ThemeUtils.getColor(button.getContext(), button.isEnabled() ? R.attr.redButtonTextColor
                                                                                    : R.attr.redButtonTextColorDisabled));
  }

  public static void setInputError(@NonNull TextInputLayout layout, @StringRes int error)
  {
    setInputError(layout, error == 0 ? null : layout.getContext().getString(error));
  }
  
  public static void setInputError(@NonNull TextInputLayout layout, String error)
  {
    layout.getEditText().setError(error);
    layout.getEditText().setTextColor(error == null ? ThemeUtils.getColor(layout.getContext(), android.R.attr.textColorPrimary)
                                                 : ContextCompat.getColor(layout.getContext(), R.color.base_red));
  }

  public static void setFullscreen(@NonNull Activity activity, boolean fullscreen)
  {
    final Window window = activity.getWindow();

    if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.R)
    {
      // On older versions of Android there is layout issue on exit from fullscreen mode.
      // For such versions we use old-style fullscreen mode.
      // See https://github.com/organicmaps/organicmaps/pull/8551 for details
      if (fullscreen)
        window.setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
      else
        window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
    }
    else
    {
      final View decorView = window.getDecorView();
      WindowInsetsControllerCompat wic = Objects.requireNonNull(WindowCompat.getInsetsController(window, decorView));
      if (fullscreen)
      {
        wic.hide(WindowInsetsCompat.Type.systemBars());
        wic.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
      }
      else
        wic.show(WindowInsetsCompat.Type.systemBars());
    }
  }

  public static void setLightStatusBar(@NonNull Activity activity, boolean isLight)
  {
    final Window window = activity.getWindow();
    final View decorView = window.getDecorView();
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
    {
      // It should not be possible for Window insets controller to be null
      WindowInsetsControllerCompat wic = Objects.requireNonNull(WindowCompat.getInsetsController(window, decorView));
      if (wic.isAppearanceLightStatusBars() != isLight)
        wic.setAppearanceLightStatusBars(isLight);
    }
    else
    {
      @ColorInt final int color = isLight
                                  ? ResourcesCompat.getColor(activity.getResources(), R.color.bg_statusbar_translucent, null)
                                  : Color.TRANSPARENT;
      window.setStatusBarColor(color);
    }
  }

  public static void setViewInsetsPaddingBottom(View view, WindowInsetsCompat windowInsets)
  {
    final Insets systemInsets = windowInsets.getInsets(WindowInsetUtils.TYPE_SAFE_DRAWING);
    view.setPaddingRelative(view.getPaddingStart(), view.getPaddingTop(),
                    view.getPaddingEnd(), systemInsets.bottom);
  }

  public static void setViewInsetsPaddingNoBottom(View view, WindowInsetsCompat windowInsets)
  {
    final Insets systemInsets = windowInsets.getInsets(WindowInsetUtils.TYPE_SAFE_DRAWING);
    view.setPadding(systemInsets.left, systemInsets.top,
                    systemInsets.right, view.getPaddingBottom());
  }

  public static void setupNavigationIcon(@NonNull Toolbar toolbar,
                                         @NonNull View.OnClickListener listener)
  {
    View customNavigationButton = toolbar.findViewById(R.id.back);
    if (customNavigationButton != null)
    {
      customNavigationButton.setOnClickListener(listener);
    }
    else
    {
      setupHomeUpButtonAsNavigationIcon(toolbar, listener);
    }
  }

  public static void setupHomeUpButtonAsNavigationIcon(@NonNull Toolbar toolbar,
                                                       @NonNull View.OnClickListener listener)
  {
    UiUtils.showHomeUpButton(toolbar);
    toolbar.setNavigationOnClickListener(listener);
  }

  public static void clearHomeUpButton(@NonNull Toolbar toolbar)
  {
    toolbar.setNavigationIcon(null);
    toolbar.setNavigationOnClickListener(null);
  }

  @AnyRes
  public static int getStyledResourceId(@NonNull Context context, @AttrRes int res)
  {
    TypedArray a = null;
    try
    {
      a = context.obtainStyledAttributes(new int[] {res});
      return a.getResourceId(0, NO_ID);
    }
    finally
    {
      if (a != null)
        a.recycle();
    }
  }

  public static void expandTouchAreaForView(@NonNull final View view, final int top, final int left,
                                            final int bottom, final int right)
  {
    final View parent = (View) view.getParent();
    parent.post(() ->
                {
                  Rect rect = new Rect();
                  view.getHitRect(rect);
                  rect.top -= top;
                  rect.left -= left;
                  rect.right += right;
                  rect.bottom += bottom;
                  parent.setTouchDelegate(new TouchDelegate(rect, view));
                });
  }

  public static void showRecyclerItemView(boolean show, @NonNull View view)
  {
    if (show)
    {
      view.setLayoutParams(new RecyclerView.LayoutParams(
          ViewGroup.LayoutParams.MATCH_PARENT,
          ViewGroup.LayoutParams.WRAP_CONTENT));
      UiUtils.show(view);
    }
    else
    {
      view.setLayoutParams(new RecyclerView.LayoutParams(0, 0));
      UiUtils.hide(view);
    }
  }

  @SuppressWarnings("deprecation") // https://github.com/organicmaps/organicmaps/issues/3630
  public static void startActivityForResult(@NonNull Fragment fragment, @NonNull Intent intent, int requestCode)
  {
    fragment.startActivityForResult(intent, requestCode);
  }

  // utility class
  private UiUtils() {}
}
