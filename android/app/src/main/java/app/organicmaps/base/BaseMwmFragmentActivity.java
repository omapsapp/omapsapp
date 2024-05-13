package app.organicmaps.base;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.os.Bundle;
import android.view.MenuItem;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.FragmentManager;

import app.organicmaps.MwmApplication;
import app.organicmaps.R;
import app.organicmaps.SplashActivity;
import app.organicmaps.sdk.util.Config;
import app.organicmaps.sdk.util.log.Logger;
import app.organicmaps.util.RtlUtils;
import app.organicmaps.util.ThemeUtils;
import app.organicmaps.util.concurrency.UiThread;

import java.util.Objects;

public abstract class BaseMwmFragmentActivity extends AppCompatActivity
{
  private static final String TAG = BaseMwmFragmentActivity.class.getSimpleName();

  private boolean mSafeCreated;

  @NonNull
  private String mThemeName;

  @StyleRes
  protected int getThemeResourceId(@NonNull String theme)
  {
    Context context = getApplicationContext();

    if (ThemeUtils.isDefaultTheme(context, theme))
        return R.style.MwmTheme;

    if (ThemeUtils.isNightTheme(context, theme))
      return R.style.MwmTheme_Night;

    throw new IllegalArgumentException("Attempt to apply unsupported theme: " + theme);
  }

  /**
   * Shows splash screen and initializes the core in case when it was not initialized.
   *
   * Do not override this method!
   * Use {@link #onSafeCreate(Bundle savedInstanceState)}
   */
  @CallSuper
  @Override
  protected final void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    mThemeName = Config.getCurrentUiTheme(getApplicationContext());
    setTheme(getThemeResourceId(mThemeName));
    RtlUtils.manageRtl(this);
    if (!MwmApplication.from(this).arePlatformAndCoreInitialized())
    {
      final Intent intent = Objects.requireNonNull(getIntent());
      intent.setComponent(new ComponentName(this, SplashActivity.class));
      startActivity(intent);
      finish();
      return;
    }

    onSafeCreate(savedInstanceState);
  }

  /**
   * Use this safe method instead of {@link #onCreate(Bundle savedInstanceState)}.
   * When this method is called, the core is already initialized.
   */
  @CallSuper
  protected void onSafeCreate(@Nullable Bundle savedInstanceState)
  {
    setVolumeControlStream(AudioManager.STREAM_MUSIC);
    final int layoutId = getContentLayoutResId();
    if (layoutId != 0)
      setContentView(layoutId);

    attachDefaultFragment();
    mSafeCreated = true;
  }

  @CallSuper
  @Override
  protected final void onDestroy()
  {
    super.onDestroy();

    if (!mSafeCreated)
      return;

    onSafeDestroy();
  }

  /**
   * Use this safe method instead of {@link #onDestroy()}.
   * When this method is called, the core is already initialized and
   * {@link #onSafeCreate(Bundle savedInstanceState)} was called.
   */
  @CallSuper
  protected void onSafeDestroy()
  {
    mSafeCreated = false;
  }

  @CallSuper
  @Override
  public void onPostResume()
  {
    super.onPostResume();
    if (!mThemeName.equals(Config.getCurrentUiTheme(getApplicationContext())))
    {
      // Workaround described in https://code.google.com/p/android/issues/detail?id=93731
      UiThread.runLater(this::recreate);
    }
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item)
  {
    if (item.getItemId() == android.R.id.home)
    {
      onHomeOptionItemSelected();
      return true;
    }
    return super.onOptionsItemSelected(item);
  }

  protected void onHomeOptionItemSelected()
  {
    onBackPressed();
  }

  protected Toolbar getToolbar()
  {
    return findViewById(R.id.toolbar);
  }

  protected void displayToolbarAsActionBar()
  {
    setSupportActionBar(getToolbar());
  }

  @Override
  public void onBackPressed()
  {
    if (getFragmentClass() == null)
    {
      super.onBackPressed();
      return;
    }
    FragmentManager manager = getSupportFragmentManager();
    String name = getFragmentClass().getName();
    Fragment fragment = manager.findFragmentByTag(name);

    if (fragment == null)
    {
      super.onBackPressed();
      return;
    }

    if (onBackPressedInternal(fragment))
      return;

    super.onBackPressed();
  }

  private boolean onBackPressedInternal(@NonNull Fragment currentFragment)
  {
    try
    {
      OnBackPressListener listener = (OnBackPressListener) currentFragment;
      return listener.onBackPressed();
    }
    catch (ClassCastException e)
    {
      Logger.i(TAG, "Fragment '" + currentFragment + "' doesn't handle back press by itself.");
      return false;
    }
  }

  /**
   * Override to set custom content view.
   * @return layout resId.
   */
  protected int getContentLayoutResId()
  {
    return 0;
  }

  protected void attachDefaultFragment()
  {
    Class<? extends Fragment> clazz = getFragmentClass();
    if (clazz != null)
      replaceFragment(clazz, getIntent().getExtras(), null);
  }

  /**
   * Replace attached fragment with the new one.
   */
  public void replaceFragment(@NonNull Class<? extends Fragment> fragmentClass, @Nullable Bundle args, @Nullable Runnable completionListener)
  {
    final int resId = getFragmentContentResId();
    if (resId <= 0 || findViewById(resId) == null)
      throw new IllegalStateException("Fragment can't be added, since getFragmentContentResId() isn't implemented or returns wrong resourceId.");

    String name = fragmentClass.getName();
    Fragment potentialInstance = getSupportFragmentManager().findFragmentByTag(name);
    if (potentialInstance == null)
    {
      final FragmentManager manager = getSupportFragmentManager();
      final FragmentFactory factory = manager.getFragmentFactory();
      final Fragment fragment = factory.instantiate(getClassLoader(), name);
      fragment.setArguments(args);
      manager.beginTransaction()
          .replace(resId, fragment, name)
          .commitAllowingStateLoss();
      manager.executePendingTransactions();
      if (completionListener != null)
        completionListener.run();
    }
  }

  /**
   * Override to automatically attach fragment in onCreate. Tag applied to fragment in back stack is set to fragment name, too.
   * WARNING : if custom layout for activity is set, getFragmentContentResId() must be implemented, too.
   * @return class of the fragment, eg FragmentClass.getClass()
   */
  protected Class<? extends Fragment> getFragmentClass()
  {
    return null;
  }

  /**
   * Get resource id for the fragment. That must be implemented to return correct resource id, if custom layout is set.
   * @return resourceId for the fragment
   */
  protected int getFragmentContentResId()
  {
    return android.R.id.content;
  }
}
