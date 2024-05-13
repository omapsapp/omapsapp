package app.organicmaps.sdk.downloader;

import android.app.Activity;
import android.app.Application;
import android.text.TextUtils;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.UiThread;
import androidx.appcompat.app.AlertDialog;

import app.organicmaps.sdk.R;
import app.organicmaps.sdk.util.ConnectionState;
import app.organicmaps.sdk.util.Utils;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.lang.ref.WeakReference;
import java.util.List;

@UiThread
public final class MapManager
{
  // Used by JNI.
  @Keep
  @SuppressWarnings("unused")
  public static class StorageCallbackData
  {
    public final String countryId;
    public final int newStatus;
    public final int errorCode;
    public final boolean isLeafNode;

    public StorageCallbackData(String countryId, int newStatus, int errorCode, boolean isLeafNode)
    {
      this.countryId = countryId;
      this.newStatus = newStatus;
      this.errorCode = errorCode;
      this.isLeafNode = isLeafNode;
    }
  }

  public interface StorageCallback
  {
    // Called from JNI.
    @Keep
    @SuppressWarnings("unused")
    void onStatusChanged(List<StorageCallbackData> data);

    // Called from JNI.
    @Keep
    @SuppressWarnings("unused")
    void onProgress(String countryId, long localSize, long remoteSize);
  }

  public interface CurrentCountryChangedListener
  {
    // Called from JNI.
    @Keep
    @SuppressWarnings("unused")
    void onCurrentCountryChanged(String countryId);
  }

  private static WeakReference<AlertDialog> sCurrentErrorDialog;

  private MapManager() {}

  @StringRes
  public static int getErrorCodeStrRes(final int errorCode)
  {
    return switch (errorCode)
    {
      case CountryItem.ERROR_NO_INTERNET -> R.string.common_check_internet_connection_dialog;
      case CountryItem.ERROR_OOM -> R.string.downloader_no_space_title;
      default -> throw new IllegalArgumentException("Given error can not be displayed: " + errorCode);
    };
  }

  public static void showError(final Activity activity, final StorageCallbackData errorData,
                               @Nullable final Utils.Proc<Boolean> dialogClickListener)
  {
    if (!nativeIsAutoretryFailed())
      return;

    showErrorDialog(activity, errorData, dialogClickListener);
  }

  public static void showErrorDialog(final Activity activity, final StorageCallbackData errorData,
                                     @Nullable final Utils.Proc<Boolean> dialogClickListener)
  {
    if (sCurrentErrorDialog != null)
    {
      AlertDialog dlg = sCurrentErrorDialog.get();
      if (dlg != null && dlg.isShowing())
        return;
    }

    final AlertDialog dlg = new MaterialAlertDialogBuilder(activity, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.country_status_download_failed)
        .setMessage(getErrorCodeStrRes(errorData.errorCode))
        .setNegativeButton(R.string.cancel, (dialog, which) -> {
          sCurrentErrorDialog = null;
          if (dialogClickListener != null)
            dialogClickListener.invoke(false);
        })
        .setPositiveButton(R.string.downloader_retry, (dialog, which) -> {
          Application app = activity.getApplication();
          RetryFailedDownloadConfirmationListener listener
              = new ExpandRetryConfirmationListener(app, dialogClickListener);
          warn3gAndRetry(activity, errorData.countryId, listener);
        }).create();
    dlg.setCanceledOnTouchOutside(false);
    dlg.show();
    sCurrentErrorDialog = new WeakReference<>(dlg);
  }

  private static void notifyNoSpaceInternal(Activity activity)
  {
    new MaterialAlertDialogBuilder(activity, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.downloader_no_space_title)
        .setMessage(R.string.downloader_no_space_message)
        .setPositiveButton(android.R.string.ok, null)
        .show();
  }

  /**
   * @return true if there is no space to update the given {@code root}, so the alert dialog will be shown.
   */
  private static boolean notifyNoSpaceToUpdate(Activity activity, String root)
  {
    if (nativeHasSpaceToUpdate(root))
      return false;

    notifyNoSpaceInternal(activity);
    return true;
  }

  /**
   * @return true if there is no space to download the given {@code root}, so the alert dialog will be shown.
   */
  private static boolean notifyNoSpace(Activity activity, String root)
  {
    if (nativeHasSpaceToDownloadCountry(root))
      return false;

    notifyNoSpaceInternal(activity);
    return true;
  }

  /**
   * @return true if there is no space to download {@code size} bytes, so the alert dialog will be shown.
   */
  private static boolean notifyNoSpace(Activity activity, long size)
  {
    if (nativeHasSpaceToDownloadAmount(size))
      return false;

    notifyNoSpaceInternal(activity);
    return true;
  }

  private static boolean warnOn3gInternal(Activity activity, @NonNull final Runnable onAcceptListener)
  {
    if (nativeIsDownloadOn3gEnabled() || !ConnectionState.INSTANCE.isMobileConnected())
    {
      onAcceptListener.run();
      return false;
    }

    new MaterialAlertDialogBuilder(activity, R.style.MwmTheme_AlertDialog)
        .setTitle(R.string.download_over_mobile_header)
        .setMessage(R.string.download_over_mobile_message)
        .setNegativeButton(R.string.cancel, null)
        .setPositiveButton(R.string.ok, (dlg, which) -> {
          nativeEnableDownloadOn3g();
          onAcceptListener.run();
        }).show();

    return true;
  }

  public static boolean warnOn3gUpdate(Activity activity, @Nullable String countryId, @NonNull final Runnable onAcceptListener)
  {
    //noinspection SimplifiableIfStatement
    if (TextUtils.isEmpty(countryId) || !notifyNoSpaceToUpdate(activity, countryId))
      return warnOn3gInternal(activity, onAcceptListener);

    return true;
  }

  public static boolean warnOn3g(Activity activity, @Nullable String countryId, @NonNull final Runnable onAcceptListener)
  {
    //noinspection SimplifiableIfStatement
    if (TextUtils.isEmpty(countryId) || !notifyNoSpace(activity, countryId))
      return warnOn3gInternal(activity, onAcceptListener);

    return true;
  }

  public static boolean warnOn3g(Activity activity, long size, @NonNull Runnable onAcceptListener)
  {
    return !notifyNoSpace(activity, size) && warnOn3gInternal(activity, onAcceptListener);
  }

  public static boolean warn3gAndDownload(Activity activity, final String countryId, @Nullable final Runnable onAcceptListener)
  {
    return warnOn3g(activity, countryId, () -> {
      if (onAcceptListener != null)
        onAcceptListener.run();
      nativeDownload(countryId);
    });
  }

  public static boolean warn3gAndRetry(Activity activity, final String countryId, @Nullable final Runnable onAcceptListener)
  {
    return warnOn3g(activity, countryId, () -> {
      if (onAcceptListener != null)
        onAcceptListener.run();
      nativeRetry(countryId);
    });
  }

  /**
   * Retrieves ID of root node.
   */
  public static native String nativeGetRoot();

  /**
   * Moves a file from one place to another.
   */
  public static native boolean nativeMoveFile(String oldFile, String newFile);

  /**
   * Returns {@code true} if there is enough storage space to download specified amount of data. Or {@code false} otherwise.
   */
  public static native boolean nativeHasSpaceToDownloadAmount(long bytes);

  /**
   * Returns {@code true} if there is enough storage space to download maps with specified {@code root}. Or {@code false} otherwise.
   */
  public static native boolean nativeHasSpaceToDownloadCountry(String root);

  /**
   * Returns {@code true} if there is enough storage space to update maps with specified {@code root}. Or {@code false} otherwise.
   */
  public static native boolean nativeHasSpaceToUpdate(String root);

  /**
   * Return count of fully downloaded maps (excluding fake MWMs).
   */
  public static native int nativeGetDownloadedCount();

  /**
   * Returns info about updatable data under given {@code root} or null on error.
   */
  public static native @Nullable UpdateInfo nativeGetUpdateInfo(@Nullable String root);

  /**
   * Retrieves list of country items with its status info.
   * if {@code root} is {@code null}, list of downloaded countries is returned.
   */
  public static native void nativeListItems(@Nullable String root, double lat, double lon, boolean hasLocation, boolean myMapsMode, List<CountryItem> result);

  /**
   * Sets following attributes of the given {@code item}:
   * <pre>
   * <ul>
   *   <li>name;</li>
   *   <li>directParentId;</li>
   *   <li>topmostParentId;</li>
   *   <li>directParentName;</li>
   *   <li>topmostParentName;</li>
   *   <li>description;</li>
   *   <li>size;</li>
   *   <li>enqueuedSize;</li>
   *   <li>totalSize;</li>
   *   <li>childCount;</li>
   *   <li>totalChildCount;</li>
   *   <li>status;</li>
   *   <li>errorCode;</li>
   *   <li>present;</li>
   *   <li>progress</li>
   * </ul>
   * </pre>
   */
  public static native void nativeGetAttributes(CountryItem item);

  /**
   * Returns status for given {@code root} node.
   */
  public static native int nativeGetStatus(String root);

  /**
   * Returns downloading error for given {@code root} node.
   */
  public static native int nativeGetError(String root);

  /**
   * Returns localized name for given {@code root} node.
   */
  public static native String nativeGetName(String root);

  /**
   * Returns country ID corresponding to given coordinates or {@code null} on error.
   */
  public static native @Nullable String nativeFindCountry(double lat, double lon);

  /**
   * Determines whether something is downloading now.
   */
  public static native boolean nativeIsDownloading();

  /**
   * Enqueues given {@code root} node and its children in downloader.
   */
  public static native void nativeDownload(String root);

  /**
   * Enqueues failed items under given {@code root} node in downloader.
   */
  public static native void nativeRetry(String root);

  /**
   * Enqueues given {@code root} node with its children in downloader.
   */
  public static native void nativeUpdate(String root);

  /**
   * Removes given currently downloading {@code root} node and its children from downloader.
   */
  public static native void nativeCancel(String root);

  /**
   * Deletes given installed {@code root} node with its children.
   */
  public static native void nativeDelete(String root);

  /**
   * Registers {@code callback} of storage status changed. Returns slot ID which should be used to unsubscribe in {@link #nativeUnsubscribe(int)}.
   */
  public static native int nativeSubscribe(StorageCallback callback);

  /**
   * Unregisters storage status changed callback.
   * @param slot Slot ID returned from {@link #nativeSubscribe(StorageCallback)} while registering.
   */
  public static native void nativeUnsubscribe(int slot);

  /**
   * Sets callback about current country change. Single subscriber only.
   */
  public static native void nativeSubscribeOnCountryChanged(CurrentCountryChangedListener listener);

  /**
   * Removes callback about current country change.
   */
  public static native void nativeUnsubscribeOnCountryChanged();

  /**
   * Determines if there are unsaved editor changes present for given {@code root}.
   */
  public static native boolean nativeHasUnsavedEditorChanges(String root);

  /**
   * Fills given {@code result} list with intermediate nodes from the root node (including) to the given {@code root} (excluding).
   * For instance, for {@code root == "Florida"} the resulting list is filled with values: {@code { "United States of America", "Countries" }}.
   */
  public static native void nativeGetPathTo(String root, List<String> result);

  /**
   * Calculates joint progress of downloading countries specified by {@code countries} array.
   * @return 0 to 100 percent.
   */
  public static native int nativeGetOverallProgress(String[] countries);

  /**
   * Returns {@code true} if the core will NOT do attempts to download failed maps anymore.
   */
  public static native boolean nativeIsAutoretryFailed();

  /**
   * Returns {@code true} if the core is allowed to download maps while on 3g network. {@code false} otherwise.
   */
  public static native boolean nativeIsDownloadOn3gEnabled();

  /**
   * Sets flag which allows to download maps on 3G.
   */
  public static native void nativeEnableDownloadOn3g();

  /**
   * Returns country ID which the current PP object points to, or {@code null}.
   */
  public static native @Nullable String nativeGetSelectedCountry();
}
