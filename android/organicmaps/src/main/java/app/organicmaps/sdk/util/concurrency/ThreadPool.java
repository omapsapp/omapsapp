package app.organicmaps.sdk.util.concurrency;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public class ThreadPool
{
  private static final ThreadPool sInstance = new ThreadPool();

  private final ThreadPoolExecutor mStorage;
  private final ThreadPoolExecutor mWorker;


  private static ThreadPoolExecutor create(int poolSize, int allowedTime)
  {
    ThreadPoolExecutor res = new ThreadPoolExecutor(poolSize, poolSize, allowedTime, TimeUnit.SECONDS,
                                                    new LinkedBlockingQueue<>());
    res.allowCoreThreadTimeOut(true);
    return res;
  }

  private ThreadPool()
  {
    mStorage = create(1, 500);
    mWorker = create(1, 250);
  }

  /**
   * Returns single thread for file operations.
   */
  public static ExecutorService getStorage()
  {
    return sInstance.mStorage;
  }

  /**
   * Returns single thread for various background tasks.
   */
  public static ExecutorService getWorker()
  {
    return sInstance.mWorker;
  }
}
