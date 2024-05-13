package app.organicmaps.sdk.util.log;

import android.util.Log;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import app.organicmaps.sdk.BuildConfig;
import net.jcip.annotations.ThreadSafe;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

@ThreadSafe
public final class Logger
{
  private static final String TAG = Logger.class.getSimpleName();
  private static final String CORE_TAG = "OMcore";
  private static final String FILENAME = "app.log";

  public static void v(String tag)
  {
    log(Log.VERBOSE, tag, "", null);
  }

  public static void v(String tag, String msg)
  {
    log(Log.VERBOSE, tag, msg, null);
  }

  public static void v(String tag, String msg, Throwable tr)
  {
    log(Log.VERBOSE, tag, msg, tr);
  }

  public static void d(String tag)
  {
    log(Log.DEBUG, tag, "", null);
  }

  public static void d(String tag, String msg)
  {
    log(Log.DEBUG, tag, msg, null);
  }

  public static void d(String tag, String msg, Throwable tr)
  {
    log(Log.DEBUG, tag, msg, tr);
  }

  public static void i(String tag)
  {
    log(Log.INFO, tag, "", null);
  }

  public static void i(String tag, String msg)
  {
    log(Log.INFO, tag, msg, null);
  }

  public static void i(String tag, String msg, Throwable tr)
  {
    log(Log.INFO, tag, msg, tr);
  }

  public static void w(String tag, String msg)
  {
    log(Log.WARN, tag, msg, null);
  }

  public static void w(String tag, String msg, Throwable tr)
  {
    log(Log.WARN, tag, msg, tr);
  }

  public static void e(String tag, String msg)
  {
    log(Log.ERROR, tag, msg, null);
  }

  public static void e(String tag, String msg, Throwable tr)
  {
    log(Log.ERROR, tag, msg, tr);
  }

  @NonNull
  private static String getSourcePoint()
  {
    final StackTraceElement[] stackTrace = new Throwable().getStackTrace();
    // Skip the chain of Logger.x() -> Logger.log() calls.
    int f = 0;
    for (; f < stackTrace.length && stackTrace[f].getClassName().equals(Logger.class.getName()); f++);
    // The stack trace should have at least one non-logger frame, but who wants to crash here if it doesn't?
    if (f == stackTrace.length)
      return "Unknown";
    final StackTraceElement st = stackTrace[f];
    StringBuilder sb = new StringBuilder(80);
    final String fileName = st.getFileName();
    if (fileName != null)
    {
      sb.append(fileName);
      final int lineNumber = st.getLineNumber();
      if (lineNumber >= 0)
        sb.append(':').append(lineNumber);
      sb.append(' ');
    }
    sb.append(st.getMethodName()).append("()");
    return sb.toString();
  }

  // Also called from JNI to proxy native code logging (with tag == null).
  @Keep
  private static void log(int level, @Nullable String tag, @NonNull String msg, @Nullable Throwable tr)
  {
    final String logsFolder = LogsManager.INSTANCE.getEnabledLogsFolder();

    if (logsFolder != null || BuildConfig.DEBUG || level >= Log.INFO)
    {
      final StringBuilder sb = new StringBuilder(180);
      // Add source point info for file logging, debug builds and ERRORs if its not from core.
      if (tag != null && (logsFolder != null || BuildConfig.DEBUG || level == Log.ERROR))
        sb.append(getSourcePoint()).append(": ");
      sb.append(msg);
      if (tr != null)
        sb.append('\n').append(Log.getStackTraceString(tr));
      if (tag == null)
        tag = CORE_TAG;

      if (logsFolder == null || BuildConfig.DEBUG)
        Log.println(level, tag, sb.toString());

      if (logsFolder != null)
      {
        sb.insert(0, String.valueOf(getLevelChar(level)) + '/' + tag + ": ");
        LogsManager.EXECUTOR.execute(new WriteTask(logsFolder + File.separator + FILENAME,
                                                   sb.toString(), Thread.currentThread().getName()));
      }
    }
  }

  private static char getLevelChar(int level)
  {
    switch (level)
    {
      case Log.VERBOSE:
        return 'V';
      case Log.DEBUG:
        return 'D';
      case Log.INFO:
        return 'I';
      case Log.WARN:
        return 'W';
      case Log.ERROR:
        return 'E';
    }
    assert false : "Unknown log level " + level;
    return '_';
  }

  private static class WriteTask implements Runnable
  {
    private static final int MAX_SIZE = 3000000;
    @NonNull
    private final String mFilePath;
    @NonNull
    private final String mData;
    @NonNull
    private final String mCallingThread;

    private WriteTask(@NonNull String filePath, @NonNull String data, @NonNull String callingThread)
    {
      mFilePath = filePath;
      mData = data;
      mCallingThread = callingThread;
    }

    @Override
    public void run()
    {
      FileWriter fw = null;
      try
      {
        File file = new File(mFilePath);
        if (!file.exists() || file.length() > MAX_SIZE)
        {
          fw = new FileWriter(file, false);
          fw.write(LogsManager.INSTANCE.getSystemInformation());
        }
        else
        {
          fw = new FileWriter(mFilePath, true);
        }
        final DateFormat fmt = new SimpleDateFormat("MM-dd HH:mm:ss.SSS", Locale.US);
        fw.write(fmt.format(new Date()) + " (" + mCallingThread + ") " + mData + "\n");
      }
      catch (IOException e)
      {
        Log.e(TAG, "Failed to write to " + mFilePath + ": " + mData, e);
      }
      finally
      {
        if (fw != null)
          try
          {
            fw.close();
          }
          catch (IOException e)
          {
            Log.e(TAG, "Failed to close file " + mFilePath, e);
          }
      }
    }
  }
}
