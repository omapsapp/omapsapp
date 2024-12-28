package app.organicmaps.base;

import android.os.Bundle;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.widget.Toolbar;
import androidx.core.view.ViewCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.FragmentManager;
import app.organicmaps.R;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.WindowInsetUtils.PaddingInsetsListener;

public abstract class BaseToolbarActivity extends BaseMwmFragmentActivity
{
  @Nullable
  private String mLastTitle;

  @CallSuper
  @Override
  protected void onSafeCreate(@Nullable Bundle savedInstanceState)
  {
    super.onSafeCreate(savedInstanceState);

    Toolbar toolbar = getToolbar();
    if (toolbar != null)
    {
      int title = getToolbarTitle();
      if (title == 0)
        toolbar.setTitle(getTitle());
      else
        toolbar.setTitle(title);

      setupHomeButton(toolbar);
      displayToolbarAsActionBar();

      ViewCompat.setOnApplyWindowInsetsListener(toolbar, PaddingInsetsListener.excludeBottom());
    }
  }

  protected void setupHomeButton(@NonNull Toolbar toolbar)
  {
    UiUtils.showHomeUpButton(toolbar);
  }

  @StringRes
  protected int getToolbarTitle()
  {
    return 0;
  }

  @Override
  protected Class<? extends Fragment> getFragmentClass()
  {
    throw new RuntimeException("Must be implemented in child classes!");
  }

  @Override
  protected int getContentLayoutResId()
  {
    return R.layout.activity_fragment_and_toolbar;
  }

  @Override
  protected int getFragmentContentResId()
  {
    return R.id.fragment_container;
  }

  public Fragment stackFragment(@NonNull Class<? extends Fragment> fragmentClass,
                            @Nullable String title, @Nullable Bundle args)
  {
    final int resId = getFragmentContentResId();
    if (resId <= 0 || findViewById(resId) == null)
      throw new IllegalStateException("Fragment can't be added, since getFragmentContentResId() " +
          "isn't implemented or returns wrong resourceId.");

    String name = fragmentClass.getName();
    final FragmentManager manager = getSupportFragmentManager();
    final FragmentFactory factory = manager.getFragmentFactory();
    final Fragment fragment = factory.instantiate(getClassLoader(), name);
    fragment.setArguments(args);
    manager.beginTransaction()
        .replace(resId, fragment, name)
        .addToBackStack(null)
        .commitAllowingStateLoss();
    manager.executePendingTransactions();

    if (title != null)
    {
      Toolbar toolbar = getToolbar();
      if (toolbar != null && toolbar.getTitle() != null)
      {
        mLastTitle = toolbar.getTitle().toString();
        toolbar.setTitle(title);
      }
    }

    return fragment;
  }

  @Override
  public void onBackPressed()
  {
    if (mLastTitle != null)
      getToolbar().setTitle(mLastTitle);

    super.onBackPressed();
  }
}
