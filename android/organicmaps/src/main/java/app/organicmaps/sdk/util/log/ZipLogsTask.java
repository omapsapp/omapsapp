package app.organicmaps.sdk.util.log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

class ZipLogsTask implements Runnable
{
  private static final String TAG = ZipLogsTask.class.getSimpleName();

  @NonNull
  private final String mLogsPath;
  @NonNull
  private final String mZipPath;
  @Nullable
  private final LogsManager.OnZipCompletedListener mOnCompletedListener;

  ZipLogsTask(@NonNull String logsPath, @NonNull String zipPath,
              @NonNull LogsManager.OnZipCompletedListener onCompletedListener)
  {
    mLogsPath = logsPath;
    mZipPath = zipPath;
    mOnCompletedListener = onCompletedListener;
  }

  @Override
  public void run()
  {
    saveSystemLogcat(mLogsPath);
    final boolean success = zipFileAtPath(mLogsPath, mZipPath);
    if (mOnCompletedListener != null)
      mOnCompletedListener.onCompleted(success, mZipPath);
  }

  private boolean zipFileAtPath(@NonNull String sourcePath, @NonNull String toLocation)
  {
    File sourceFile = new File(sourcePath);
    if (!sourceFile.isDirectory())
      return false;
    try (
        FileOutputStream dest = new FileOutputStream(toLocation, false);
        ZipOutputStream out = new ZipOutputStream(new BufferedOutputStream(dest)))
    {
      zipSubFolder(out, sourceFile, sourceFile.getParent().length());
    }
    catch (Exception e)
    {
      Logger.e(TAG, "Failed to zip file '" + sourcePath + "' to location '" + toLocation + "'", e);
      return false;
    }
    return true;
  }

  private void zipSubFolder(ZipOutputStream out, File folder,
                            int basePathLength) throws IOException
  {
    File[] fileList = folder.listFiles();
    if (fileList == null)
      throw new IOException("Can't get files list of " + folder.getPath());

    for (File file : fileList)
    {
      if (file.isDirectory())
      {
        zipSubFolder(out, file, basePathLength);
      }
      else
      {
        final int bufSize = 8 * 1024;
        byte[] data = new byte[bufSize];
        String unmodifiedFilePath = file.getPath();
        String relativePath = unmodifiedFilePath.substring(basePathLength);
        try (
            FileInputStream fi = new FileInputStream(unmodifiedFilePath);
            BufferedInputStream origin = new BufferedInputStream(fi, bufSize))
        {
          ZipEntry entry = new ZipEntry(relativePath);
          out.putNextEntry(entry);
          int count;
          while ((count = origin.read(data, 0, bufSize)) != -1)
          {
            out.write(data, 0, count);
          }
        }
      }
    }
  }

  private void saveSystemLogcat(String path)
  {
    final String cmd = "logcat -d -v time";
    Process process;
    try
    {
      process = Runtime.getRuntime().exec(cmd);
    }
    catch (IOException e)
    {
      Logger.e(TAG, "Failed to get system logcat", e);
      return;
    }

    path += File.separator + "logcat.log";
    final File file = new File(path);
    try (
        InputStreamReader reader = new InputStreamReader(process.getInputStream());
        FileWriter writer = new FileWriter(file))
    {
      writer.write(LogsManager.INSTANCE.getSystemInformation());
      char[] buffer = new char[10000];
      do
      {
        int n = reader.read(buffer, 0, buffer.length);
        if (n == -1)
          break;
        writer.write(buffer, 0, n);
      } while (true);
    }
    catch (Throwable e)
    {
      Logger.e(TAG, "Failed to save system logcat to " + path, e);
    }
  }
}
