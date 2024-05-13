package app.organicmaps.util;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.Html;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.text.style.AbsoluteSizeSpan;
import android.util.AndroidRuntimeException;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.DimenRes;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.app.NavUtils;
import androidx.core.os.BundleCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import com.google.android.material.snackbar.Snackbar;

import app.organicmaps.BuildConfig;
import app.organicmaps.MwmActivity;
import app.organicmaps.MwmApplication;
import app.organicmaps.R;
import app.organicmaps.sdk.util.Distance;
import app.organicmaps.sdk.util.StorageUtils;
import app.organicmaps.sdk.util.concurrency.UiThread;
import app.organicmaps.sdk.util.log.Logger;
import app.organicmaps.sdk.util.log.LogsManager;

import java.io.Closeable;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.text.DecimalFormatSymbols;
import java.util.Currency;
import java.util.Locale;
import java.util.Map;

@Keep
public class Utils
{
  private static final String TAG = Utils.class.getSimpleName();

  @StringRes
  public static final int INVALID_ID = 0;
  public static final String UTF_8 = "utf-8";
  public static final String TEXT_HTML = "text/html; charset=utf-8";

  private Utils()
  {
  }

  /**
   * Enable to keep screen on.
   * Disable to let system turn it off automatically.
   */
  public static void keepScreenOn(boolean enable, Window w)
  {
    Logger.i(TAG, "KeepScreenOn = " + enable + " window = " + w);
    if (enable)
      w.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    else
      w.clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
  }

  private static void showOnLockScreenOld(boolean enable, Activity activity)
  {
    if (enable)
      activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
    else
      activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
  }

  public static void showOnLockScreen(boolean enable, Activity activity)
  {
    Logger.i(TAG, "showOnLockScreen = " + enable + " window = " + activity.getWindow());
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O_MR1)
      showOnLockScreenOld(enable, activity);
    else
      activity.setShowWhenLocked(enable);
  }

  public static void showSnackbar(@NonNull View view, @NonNull String message)
  {
    Snackbar snackbar = Snackbar.make(view, message, Snackbar.LENGTH_LONG);
    setSnackbarMaxLines(snackbar, 3);
    snackbar.show();
  }

  public static void showSnackbarAbove(@NonNull View view, @NonNull View viewAbove, @NonNull String message)
  {
    Snackbar snackbar = Snackbar.make(view, message, Snackbar.LENGTH_LONG);
    setSnackbarMaxLines(snackbar, 3);
    snackbar.setAnchorView(viewAbove);
    snackbar.show();
  }

  private static void setSnackbarMaxLines(@NonNull final Snackbar snackbar, final int maxLinesCount)
  {
    TextView snackTextView = snackbar.getView().findViewById(com.google.android.material.R.id.snackbar_text);
    snackTextView.setMaxLines(maxLinesCount);
  }

  public static void showSnackbar(@NonNull Context context, @NonNull View view, int messageResId)
  {
    showSnackbar(context, view, null, messageResId);
  }

  public static void showSnackbar(@NonNull Context context, @NonNull View view,
                                  @Nullable View viewAbove, int messageResId)
  {
    final String message = context.getString(messageResId);
    if (viewAbove == null)
      showSnackbar(view, message);
    else
      showSnackbarAbove(view, viewAbove, message);
  }

  @SuppressWarnings("deprecated")
  private static @Nullable ResolveInfo resolveActivity(@NonNull PackageManager pm, @NonNull Intent intent, int flags)
  {
    return pm.resolveActivity(intent, flags);
  }

  public static boolean isIntentSupported(@NonNull Context context, @NonNull Intent intent)
  {
    final PackageManager pm = context.getPackageManager();
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU)
      return resolveActivity(pm, intent, 0) != null;
    return pm.resolveActivity(intent, PackageManager.ResolveInfoFlags.of(0)) != null;
  }

  public static @Nullable Intent makeSystemLocationSettingIntent(@NonNull Context context)
  {
    Intent intent = new Intent(android.provider.Settings.ACTION_LOCATION_SOURCE_SETTINGS);
    if (isIntentSupported(context, intent))
      return intent;
    intent = new Intent(android.provider.Settings.ACTION_SECURITY_SETTINGS);
    if (isIntentSupported(context, intent))
      return intent;
    intent = new Intent(android.provider.Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
    Uri uri = Uri.fromParts("package", context.getPackageName(), null);
    intent.setData(uri);
    if (isIntentSupported(context, intent))
      return intent;
    return null;
  }

  public static void checkNotNull(Object object)
  {
    if (null == object) throw new NullPointerException("Argument here must not be NULL");
  }

  public static void copyTextToClipboard(Context context, String text)
  {
    final android.content.ClipboardManager clipboard =
        (android.content.ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
    final ClipData clip = ClipData.newPlainText("Organic Maps: " + text, text);
    clipboard.setPrimaryClip(clip);
  }

  public static <K, V> String mapPrettyPrint(Map<K, V> map)
  {
    if (map == null)
      return "[null]";
    if (map.isEmpty())
      return "[]";


    String joined = "";
    for (final K key : map.keySet())
    {
      final String keyVal = key + "=" + map.get(key);
      if (!joined.isEmpty())
        joined = TextUtils.join(",", new Object[]{joined, keyVal});
      else
        joined = keyVal;
    }

    return "[" + joined + "]";
  }

  public static Uri buildMailUri(String to, String subject, String body)
  {
    String uriString = Constants.Url.MAILTO_SCHEME + Uri.encode(to) +
        Constants.Url.MAIL_SUBJECT + Uri.encode(subject) +
        Constants.Url.MAIL_BODY + Uri.encode(body);

    return Uri.parse(uriString);
  }

  public static void openAppInMarket(Activity activity, String url)
  {
    final Intent marketIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
    marketIntent.addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
    marketIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);

    try
    {
      activity.startActivity(marketIntent);
    } catch (ActivityNotFoundException e)
    {
      Logger.e(TAG, "Failed to start activity", e);
    }
  }

  public static void showFacebookPage(Activity activity)
  {
    try
    {
      // Exception is thrown if we don't have installed Facebook application.
      getPackageInfo(activity.getPackageManager(), Constants.Package.FB_PACKAGE, 0);
      activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(Constants.Url.FB_OM_COMMUNITY_NATIVE)));
    } catch (final Exception e)
    {
      activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(Constants.Url.FB_OM_COMMUNITY_HTTP)));
    }
  }

  public static void openUrl(@NonNull Context context, @Nullable String url)
  {
    if (TextUtils.isEmpty(url))
      return;

    final Intent intent = new Intent(Intent.ACTION_VIEW);
    Uri uri = isHttpOrHttpsScheme(url)
               ? Uri.parse(url)
               : new Uri.Builder().scheme("http").appendEncodedPath(url).build();
    intent.setData(uri);
    try
    {
      context.startActivity(intent);
    }
    catch (ActivityNotFoundException e)
    {
      Toast.makeText(context, context.getString(R.string.browser_not_available), Toast.LENGTH_LONG).show();
      Logger.e(TAG, "ActivityNotFoundException", e);
    }
    catch (AndroidRuntimeException e)
    {
      Logger.e(TAG, "AndroidRuntimeException", e);
      intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
      context.startActivity(intent);
    }
  }

  public static boolean openUri(@NonNull Context context, @NonNull Uri uri)
  {
    final Intent intent = new Intent(Intent.ACTION_VIEW);
    intent.setData(uri);
    try
    {
      context.startActivity(intent);
      return true;
    }
    catch (ActivityNotFoundException e)
    {
      Logger.e(TAG, "ActivityNotFoundException", e);
      return false;
    }
    catch (AndroidRuntimeException e)
    {
      Logger.e(TAG, "AndroidRuntimeException", e);
      intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
      context.startActivity(intent);
      return false;
    }
  }

  private static boolean isHttpOrHttpsScheme(@NonNull String url)
  {
    return url.startsWith("http://") || url.startsWith("https://");
  }

  public static void closeSafely(@NonNull Closeable... closeable)
  {
    for (Closeable each : closeable)
    {
      if (each != null)
      {
        try
        {
          each.close();
        }
        catch (IOException e)
        {
          Logger.e(TAG, "Failed to close '" + each + "'", e);
        }
      }
    }
  }

  // subject is optional (could be an empty string).

  /**
   * @param subject could be an empty string
   */
  public static void sendBugReport(@NonNull Activity activity, @NonNull String subject, @NonNull String body)
  {
    subject = "Organic Maps Bugreport" + (TextUtils.isEmpty(subject) ? "" : ": " + subject);
    LogsManager.INSTANCE.zipLogs(new SupportInfoWithLogsCallback(activity, subject, body, Constants.Email.SUPPORT));
  }

  // TODO: Don't send logs with general feedback, send system information only (version, device name, connectivity, etc.)
  public static void sendFeedback(@NonNull Activity activity)
  {
    LogsManager.INSTANCE.zipLogs(new SupportInfoWithLogsCallback(activity, "Organic Maps Feedback", "",
                                                                 Constants.Email.SUPPORT));
  }

  public static void navigateToParent(@NonNull Activity activity)
  {
    if (activity instanceof MwmActivity)
      ((MwmActivity) activity).customOnNavigateUp();
    else
      NavUtils.navigateUpFromSameTask(activity);
  }

  public static SpannableStringBuilder formatTime(Context context, @DimenRes int size, @DimenRes int units, String dimension, String unitText)
  {
    final SpannableStringBuilder res = new SpannableStringBuilder(dimension).append("\u00A0").append(unitText);
    res.setSpan(new AbsoluteSizeSpan(UiUtils.dimen(context, size), false), 0, dimension.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    res.setSpan(new AbsoluteSizeSpan(UiUtils.dimen(context, units), false), dimension.length(), res.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    return res;
  }

  @NonNull
  public static Spannable formatDistance(Context context, @NonNull Distance distance)
  {
    final SpannableStringBuilder res = new SpannableStringBuilder(distance.toString(context));
    res.setSpan(
        new AbsoluteSizeSpan(UiUtils.dimen(context, R.dimen.text_size_nav_number), false),
        0, distance.mDistanceStr.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    res.setSpan(
        new AbsoluteSizeSpan(UiUtils.dimen(context, R.dimen.text_size_nav_dimension), false),
        distance.mDistanceStr.length(), res.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
    return res;
  }

  public static void sendTo(@NonNull Context context, @NonNull String email)
  {
    sendTo(context, email, "", "");
  }

  public static void sendTo(@NonNull Context context, @NonNull String email, @NonNull String subject)
  {
    sendTo(context, email, subject, "");
  }

  public static void sendTo(@NonNull Context context, @NonNull String email, @NonNull String subject, @NonNull String body)
  {
    Intent intent = new Intent(Intent.ACTION_SENDTO);
    intent.setData(Utils.buildMailUri(email, subject, body));
    context.startActivity(intent);
  }

  public static void callPhone(@NonNull Context context, @NonNull String phone)
  {
    Intent intent = new Intent(Intent.ACTION_DIAL);
    intent.setData(Uri.parse("tel:" + phone));
    try
    {
      context.startActivity(intent);
    }
    catch (ActivityNotFoundException e)
    {
      Logger.e(TAG, "Failed to call phone", e);
    }
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @Nullable
  public static String getCurrencyCode()
  {
    Locale[] locales = { Locale.getDefault(), Locale.US };
    for (Locale locale : locales)
    {
      Currency currency = getCurrencyForLocale(locale);
      if (currency != null)
        return currency.getCurrencyCode();
    }
    return null;
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @NonNull
  public static String getCountryCode()
  {
    return Locale.getDefault().getCountry();
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @NonNull
  public static String getLanguageCode()
  {
    return Locale.getDefault().getLanguage();
  }


  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @NonNull
  public static String getDecimalSeparator()
  {
    return String.valueOf(DecimalFormatSymbols.getInstance().getDecimalSeparator());
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @NonNull
  public static String getGroupingSeparator()
  {
    return String.valueOf(DecimalFormatSymbols.getInstance().getGroupingSeparator());
  }

  @Nullable
  public static Currency getCurrencyForLocale(@NonNull Locale locale)
  {
    try
    {
      return Currency.getInstance(locale);
    }
    catch (Throwable e)
    {
      Logger.e(TAG, "Failed to obtain a currency for locale: " + locale, e);
      return null;
    }
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @NonNull
  public static String getCurrencySymbol(@NonNull String currencyCode)
  {
    try
    {
      return Currency.getInstance(currencyCode).getSymbol(Locale.getDefault());
    }
    catch (Throwable e)
    {
      Logger.e(TAG, "Failed to obtain currency symbol by currency code = " + currencyCode, e);
    }

    return currencyCode;
  }

  static String makeUrlSafe(@NonNull final String url)
  {
    return url.replaceAll("(token|password|key)=([^&]+)", "***");
  }

  @StringRes
  @SuppressLint("DiscouragedApi")
  public static int getStringIdByKey(@NonNull Context context, @NonNull String key)
  {
    try
    {
      Resources res = context.getResources();
      @StringRes
      int nameId = res.getIdentifier(key, "string", context.getPackageName());
      if (nameId == INVALID_ID || nameId == View.NO_ID)
        throw new Resources.NotFoundException("String id '" + key + "' is not found");
      return nameId;
    }
    catch (RuntimeException e)
    {
      Logger.e(TAG, "Failed to get string with id '" + key + "'", e);
      if (BuildConfig.BUILD_TYPE.equals("debug") || BuildConfig.BUILD_TYPE.equals("beta"))
      {
        Toast.makeText(context, "Add string id for '" + key + "'!", Toast.LENGTH_LONG).show();
      }
    }
    return INVALID_ID;
  }

  /**
   * Returns a string value for the specified key. If the value is not found then its key will be
   * returned.
   *
   * @return string value or its key if there is no string for the specified key.
   */
  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @NonNull
  public static String getStringValueByKey(@NonNull Context context, @NonNull String key)
  {
    try
    {
      return context.getString(getStringIdByKey(context, key));
    }
    catch (Resources.NotFoundException e)
    {
      Logger.e(TAG, "Failed to get value for string '" + key + "'", e);
    }
    return key;
  }

  /**
   * Returns a name for a new bookmark created off the current GPS location.
   * The name includes current time and date in locale-specific format.
   *
   * @return bookmark name with time and date.
   */
  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @NonNull
  public static String getMyPositionBookmarkName(@NonNull Context context)
  {
    return DateUtils.formatDateTime(context, System.currentTimeMillis(),
                                    DateUtils.FORMAT_SHOW_TIME | DateUtils.FORMAT_SHOW_DATE | DateUtils.FORMAT_SHOW_YEAR);
  }

  // Called from JNI.
  @NonNull
  @Keep
  @SuppressWarnings("unused")
  public static String getDeviceName()
  {
    return Build.MANUFACTURER;
  }

  // Called from JNI.
  @NonNull
  @Keep
  @SuppressWarnings("unused")
  public static String getDeviceModel()
  {
    return Build.MODEL;
  }

  // Called from JNI.
  @NonNull
  @Keep
  @SuppressWarnings("unused")
  public static String getVersion()
  {
    return BuildConfig.VERSION_NAME;
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  public static int getIntVersion()
  {
    // Please sync with getVersion() in build.gradle
    // - % 100000000 removes prefix for special markets, e.g Huawei.
    // - / 100 removes the number of commits in the current day.
    return (BuildConfig.VERSION_CODE % 1_00_00_00_00) / 100;
  }

  public static void detachFragmentIfCoreNotInitialized(@NonNull Context context,
                                                        @NonNull Fragment fragment)
  {
    if (MwmApplication.from(context).arePlatformAndCoreInitialized())
      return;

    FragmentManager manager = fragment.getFragmentManager();
    if (manager == null)
      return;

    manager.beginTransaction().detach(fragment).commit();
  }

  public static String capitalize(@Nullable String src)
  {
    if (TextUtils.isEmpty(src))
      return src;

    if (src.length() == 1)
      return Character.toString(Character.toUpperCase(src.charAt(0)));

    return Character.toUpperCase(src.charAt(0)) + src.substring(1);
  }

  public static String unCapitalize(@Nullable String src)
  {
    if (TextUtils.isEmpty(src))
      return src;

    if (src.length() == 1)
      return Character.toString(Character.toLowerCase(src.charAt(0)));

    return Character.toLowerCase(src.charAt(0)) + src.substring(1);
  }

  public interface Proc<T>
  {
    void invoke(@NonNull T param);
  }

  @NonNull
  private static String getLocalizedFeatureByKey(@NonNull Context context, @NonNull String key)
  {
    return getStringValueByKey(context, key);
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @NonNull
  public static String getLocalizedFeatureType(@NonNull Context context, @Nullable String type)
  {
    if (TextUtils.isEmpty(type))
      return "";

    String key = "type." + type.replace('-', '.')
                               .replace(':', '_');
    return getLocalizedFeatureByKey(context, key);
  }

  // Called from JNI.
  @Keep
  @SuppressWarnings("unused")
  @NonNull
  public static String getLocalizedBrand(@NonNull Context context, @Nullable String brand)
  {
    if (TextUtils.isEmpty(brand))
      return "";

    try
    {
      @StringRes
      int nameId = context.getResources().getIdentifier("brand." + brand, "string", context.getPackageName());
      if (nameId == INVALID_ID || nameId == View.NO_ID)
        return brand;
      return context.getString(nameId);
    }
    catch (Resources.NotFoundException e)
    {
    }
    return brand;
  }

  private static class SupportInfoWithLogsCallback implements LogsManager.OnZipCompletedListener
  {
    @NonNull
    private final WeakReference<Activity> mActivityRef;
    @NonNull
    private final String mSubject;
    @NonNull
    private final String mBody;
    @NonNull
    private final String mEmail;

    private SupportInfoWithLogsCallback(@NonNull Activity activity, @NonNull String subject,
                                        @NonNull String body, @NonNull String email)
    {
      mActivityRef = new WeakReference<>(activity);
      mSubject = subject;
      mBody = body;
      mEmail = email;
    }

    @Override
    public void onCompleted(final boolean success, @Nullable final String zipPath)
    {
      // TODO: delete zip file after its sent.
      UiThread.run(() -> {
        final Activity activity = mActivityRef.get();
        if (activity == null)
          return;

        final Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(Intent.EXTRA_EMAIL, new String[] { mEmail });
        intent.putExtra(Intent.EXTRA_SUBJECT, "[" + BuildConfig.VERSION_NAME + "] " + mSubject);
        // TODO: Send a short text attachment with system info and logs if zipping logs failed
        if (success)
        {
          final Uri uri = StorageUtils.getUriForFilePath(activity, zipPath);
          intent.putExtra(Intent.EXTRA_STREAM, uri);
          // Properly set permissions for intent, see
          // https://developer.android.com/reference/androidx/core/content/FileProvider#include-the-permission-in-an-intent
          intent.setDataAndType(uri, "message/rfc822");
          intent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
          if (android.os.Build.VERSION.SDK_INT <= android.os.Build.VERSION_CODES.LOLLIPOP_MR1) {
            intent.setClipData(ClipData.newRawUri("", uri));
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
          }
        }
        else
        {
          intent.setType("message/rfc822");
        }
        // Do this so some email clients don't complain about empty body.
        intent.putExtra(Intent.EXTRA_TEXT, mBody);
        try
        {
          activity.startActivity(intent);
        }
        catch (ActivityNotFoundException e)
        {
          Logger.w(TAG, "No activities found which can handle sending a support message.", e);
        }
      });
    }
  }

  public static <T> T getParcelable(@NonNull Bundle in, @Nullable String key,
                                    @NonNull Class<T> clazz)
  {
    in.setClassLoader(clazz.getClassLoader());
    return BundleCompat.getParcelable(in, key, clazz);
  }

  @SuppressWarnings("deprecation")
  private static Spanned fromHtmlOld(@NonNull String htmlDescription)
  {
    return Html.fromHtml(htmlDescription);
  }

  public static Spanned fromHtml(@NonNull String htmlDescription)
  {
    if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.N)
      return fromHtmlOld(htmlDescription);
    return Html.fromHtml(htmlDescription, Html.FROM_HTML_MODE_LEGACY);
  }

  @SuppressWarnings("deprecation")
  private static ApplicationInfo getApplicationInfoOld(@NonNull PackageManager manager, @NonNull String packageName, int flags)
      throws PackageManager.NameNotFoundException
  {
    return manager.getApplicationInfo(packageName, flags);
  }

  public static ApplicationInfo getApplicationInfo(@NonNull PackageManager manager, @NonNull String packageName,
                                                   int flags)
      throws PackageManager.NameNotFoundException
  {
    if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU)
      return getApplicationInfoOld(manager, packageName, flags);
    return manager.getApplicationInfo(packageName, PackageManager.ApplicationInfoFlags.of(flags));
  }

  @SuppressWarnings("deprecation")
  private static PackageInfo getPackageInfoOld(@NonNull PackageManager manager, @NonNull String packageName, int flags)
      throws PackageManager.NameNotFoundException
  {
    return manager.getPackageInfo(packageName, flags);
  }

  public static PackageInfo getPackageInfo(@NonNull PackageManager manager, @NonNull String packageName, int flags)
      throws PackageManager.NameNotFoundException
  {
    if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU)
      return getPackageInfoOld(manager, packageName, flags);
    return manager.getPackageInfo(packageName, PackageManager.PackageInfoFlags.of(flags));
  }
}
