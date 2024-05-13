package app.organicmaps.sdk.util.concurrency;

import android.os.Handler;
import android.os.Looper;

public class UiThread
{
  private static final Handler sUiHandler = new Handler(Looper.getMainLooper());

  public static boolean isUiThread()
  {
    return Looper.getMainLooper().getThread() == Thread.currentThread();
  }

  /**
   * Executes something on UI thread. If called from UI thread then given task will be executed synchronously.
   *
   * @param task the code that must be executed on UI thread.
   */
  public static void run(Runnable task)
  {
    if (isUiThread())
      task.run();
    else
      sUiHandler.post(task);
  }

  /**
   * Executes something on UI thread after last message queued in the application's main looper.
   *
   * @param task the code that must be executed later on UI thread.
   */
  public static void runLater(Runnable task)
  {
    runLater(task, 0);
  }

  /**
   * Executes something on UI thread after a given delayMillis.
   *
   * @param task  the code that must be executed on UI thread after given delayMillis.
   * @param delayMillis The delayMillis until the code will be executed.
   */
  public static void runLater(Runnable task, long delayMillis)
  {
    sUiHandler.postDelayed(task, delayMillis);
  }

  /**
   * Cancels execution of the given task that was previously queued with {@link #run(Runnable)},
   * {@link #runLater(Runnable)} or {@link #runLater(Runnable, long)} if it was not started yet.
   *
   * @param task the code that must be cancelled.
   */
  public static void cancelDelayedTasks(Runnable task)
  {
    sUiHandler.removeCallbacks(task);
  }
}
