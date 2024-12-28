package app.organicmaps.util.log;

import android.Manifest;
import android.app.ActivityManager;
import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.location.LocationManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.os.Debug;
import android.util.Log;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import app.organicmaps.BuildConfig;
import app.organicmaps.MwmApplication;
import app.organicmaps.R;
import app.organicmaps.util.ROMUtils;
import app.organicmaps.util.StringUtils;

import net.jcip.annotations.ThreadSafe;

import java.io.File;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * By default uses Android's system logger.
 * After an initFileLogging() call can use a custom file logging implementation.
 *
 * Its important to have only system logging here to avoid infinite loop
 * (Logger calls getEnabledLogsFolder() in preparation to write).
 */
@ThreadSafe
public final class LogsManager
{
  public interface OnZipCompletedListener
  {
    // Called from the logger thread.
    public void onCompleted(final boolean success, @Nullable final String zipPath);
  }

  private final static String TAG = LogsManager.class.getSimpleName();

  public final static LogsManager INSTANCE = new LogsManager();
  final static ExecutorService EXECUTOR = Executors.newSingleThreadExecutor();

  @Nullable
  private Application mApplication;
  private boolean mIsFileLoggingEnabled = false;
  @Nullable
  private String mLogsFolder;

  private LogsManager()
  {
    Log.i(LogsManager.TAG, "Logging started");
  }

  public synchronized void initFileLogging(@NonNull Application application)
  {
    Log.i(TAG, "Init file logging");
    mApplication = application;

    final SharedPreferences prefs = MwmApplication.prefs(mApplication);
    mIsFileLoggingEnabled = prefs.getBoolean(mApplication.getString(R.string.pref_enable_logging), false);
    Log.i(TAG, "isFileLoggingEnabled preference: " + mIsFileLoggingEnabled);
    mIsFileLoggingEnabled = mIsFileLoggingEnabled && ensureLogsFolder() != null;

    // Set native logging level, save into shared preferences.
    switchFileLoggingEnabled(mIsFileLoggingEnabled);
  }

  private void assertFileLoggingInit()
  {
    assert mApplication != null : "mApplication must be initialized first by calling initFileLogging()";
  }

  /**
   * Returns logs folder path if file logging is enabled.
   * Switches off file logging if the path doesn't exist and can't be created.
   */
  @Nullable
  synchronized String getEnabledLogsFolder()
  {
    if (!mIsFileLoggingEnabled)
      return null;

    final String logsFolder = ensureLogsFolder();
    if (logsFolder == null)
      switchFileLoggingEnabled(false);

    return logsFolder;
  }

  /**
   * Ensures logs folder exists.
   * Tries to create it and/or re-get a path from the system, falling back to the internal storage.
   * NOTE: initFileLogging() must be called before.
   *
   * @return logs folder path, null if it can't be created
   */
  @Nullable
  private String ensureLogsFolder()
  {
    assertFileLoggingInit();

    if (mLogsFolder != null && createWritableDir(mLogsFolder))
      return mLogsFolder;

    mLogsFolder = createLogsFolder(mApplication.getExternalFilesDir(null));
    if (mLogsFolder == null)
      mLogsFolder = createLogsFolder(mApplication.getFilesDir());

    if (mLogsFolder == null)
      Log.e(TAG, "Can't create any logs folder");

    return mLogsFolder;
  }

  private boolean createWritableDir(@NonNull final String path)
  {
    final File dir = new File(path);
    if (!dir.exists())
    {
      Log.i(TAG, "Creating logs folder " + path);
      if (!dir.mkdirs())
      {
        Log.e(TAG, "Can't create a logs folder " + path);
        return false;
      }
    }
    if (!dir.canWrite())
    {
      Log.e(TAG, "Can't write to a logs folder " + path);
      return false;
    }
    if (dir.getUsableSpace() < 256)
    {
      Log.e(TAG, "There is no free space on storage with a logs folder " + path);
      return false;
    }
    return true;
  }

  @Nullable
  private String createLogsFolder(@Nullable final File dir)
  {
    if (dir != null)
    {
      final String path = dir.getPath() + File.separator + "logs";
      if (createWritableDir(path))
        return path;
    }
    return null;
  }

  private void switchFileLoggingEnabled(boolean enabled)
  {
    mIsFileLoggingEnabled = enabled;
    // Only Debug builds log DEBUG level to Android system log.
    nativeToggleCoreDebugLogs(enabled || BuildConfig.DEBUG);
    MwmApplication.prefs(mApplication)
                  .edit()
                  .putBoolean(mApplication.getString(R.string.pref_enable_logging), enabled)
                  .apply();
    Log.i(TAG, "Logging to " + (enabled ? "logs folder " + mLogsFolder : "system log"));
  }

  public synchronized boolean isFileLoggingEnabled()
  {
    return mIsFileLoggingEnabled;
  }

  /**
   * Returns false if file logging can't be enabled.
   *
   * NOTE: initFileLogging() must be called before.
   */
  public synchronized boolean setFileLoggingEnabled(boolean enabled)
  {
    assertFileLoggingInit();

    if (mIsFileLoggingEnabled != enabled)
    {
      Log.i(TAG, "Switching isFileLoggingEnabled to " + enabled);
      if (enabled && ensureLogsFolder() == null)
      {
        Log.e(TAG, "Can't enable file logging: no logs folder.");
        return false;
      }
      else
        switchFileLoggingEnabled(enabled);
    }

    return true;
  }

  /**
   * NOTE: initFileLogging() must be called before.
   */
  public synchronized void zipLogs(@NonNull OnZipCompletedListener listener)
  {
    assertFileLoggingInit();

    if (ensureLogsFolder() == null)
    {
      Log.e(TAG, "Can't zip log files: no logs folder.");
      listener.onCompleted(false, null);
      return;
    }

    Log.i(TAG, "Zipping log files in " + mLogsFolder);
    final Runnable task = new ZipLogsTask(mLogsFolder, mLogsFolder + ".zip", listener);
    EXECUTOR.execute(task);
  }

  /**
   * NOTE: initFileLogging() must be called before.
   */
  @NonNull
  String getSystemInformation()
  {
    assertFileLoggingInit();

    final DateFormat fmt = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US);
    final StringBuilder sb = new StringBuilder(512);
    sb.append("Datetime: ").append(fmt.format(new Date()))
      .append("\n\nAndroid version: ")
      .append(Build.VERSION.CODENAME.equals("REL") ? Build.VERSION.RELEASE : Build.VERSION.CODENAME)
      .append(" (API ").append(Build.VERSION.SDK_INT).append(')');
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
      sb.append(", security patch level: ").append(Build.VERSION.SECURITY_PATCH);
    sb.append(", os.version: ").append(System.getProperty("os.version", "N/A"))
      .append("\nDevice: ");
    if (!StringUtils.toLowerCase(Build.MODEL).startsWith(StringUtils.toLowerCase(Build.MANUFACTURER)))
      sb.append(Build.MANUFACTURER).append(' ');
    sb.append(Build.MODEL).append(" (").append(Build.DEVICE).append(')');
    sb.append("\nIs custom ROM: ").append(ROMUtils.isCustomROM());
    sb.append("\nSupported ABIs:");
    for (String abi : Build.SUPPORTED_ABIS)
      sb.append(' ').append(abi);
    sb.append("\nApp version: ").append(BuildConfig.APPLICATION_ID).append(' ').append(BuildConfig.VERSION_NAME)
      .append("\nLocale: ").append(Locale.getDefault())
      .append("\nNetworks: ");
    final ConnectivityManager manager = (ConnectivityManager) mApplication.getSystemService(Context.CONNECTIVITY_SERVICE);
    if (manager != null)
    {
      for (Network network : manager.getAllNetworks())
      {
        final NetworkCapabilities cap = manager.getNetworkCapabilities(network);
        sb.append("\n\tid=").append(network.toString()).append("\n").append(cap != null ? cap.toString() : "null");
      }
    }
    sb.append("\nLocation providers:");
    final LocationManager locMngr = (android.location.LocationManager) mApplication.getSystemService(Context.LOCATION_SERVICE);
    if (locMngr != null)
      for (String provider : locMngr.getProviders(true))
        sb.append(' ').append(provider);

    sb.append("\nLocation permissions:");
    if (ContextCompat.checkSelfPermission(mApplication, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED)
      sb.append(' ').append("coarse");
    if (ContextCompat.checkSelfPermission(mApplication, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED)
      sb.append(' ').append("fine");

    sb.append("\n\n");

    return sb.toString();
  }

  // Called from JNI.
  @SuppressWarnings("unused")
  @NonNull
  @Keep
  public static String getMemoryInfo(@NonNull Context context)
  {
    final Debug.MemoryInfo debugMI = new Debug.MemoryInfo();
    Debug.getMemoryInfo(debugMI);
    final ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
    final ActivityManager activityManager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
    activityManager.getMemoryInfo(mi);

    final StringBuilder log = new StringBuilder(256);
    log.append("Memory info: ")
       .append(" Debug.getNativeHeapSize() = ")
       .append(Debug.getNativeHeapSize() / 1024)
       .append("KB; Debug.getNativeHeapAllocatedSize() = ")
       .append(Debug.getNativeHeapAllocatedSize() / 1024)
       .append("KB; Debug.getNativeHeapFreeSize() = ")
       .append(Debug.getNativeHeapFreeSize() / 1024)
       .append("KB; debugMI.getTotalPrivateDirty() = ")
       .append(debugMI.getTotalPrivateDirty())
       .append("KB; debugMI.getTotalPss() = ")
       .append(debugMI.getTotalPss())
       .append("KB; mi.availMem = ")
       .append(mi.availMem / 1024)
       .append("KB; mi.threshold = ")
       .append(mi.threshold / 1024)
       .append("KB; mi.lowMemory = ")
       .append(mi.lowMemory)
       .append("; mi.totalMem = ")
       .append(mi.totalMem / 1024)
       .append("KB;");

    return log.toString();
  }

  private static native void nativeToggleCoreDebugLogs(boolean enabled);
}
