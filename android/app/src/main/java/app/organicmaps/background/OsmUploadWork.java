package app.organicmaps.background;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.work.Constraints;
import androidx.work.NetworkType;
import androidx.work.OneTimeWorkRequest;
import androidx.work.WorkManager;
import androidx.work.WorkRequest;
import androidx.work.Worker;
import androidx.work.WorkerParameters;
import app.organicmaps.MwmApplication;
import app.organicmaps.editor.Editor;
import app.organicmaps.editor.OsmOAuth;
import app.organicmaps.util.log.Logger;

public class OsmUploadWork extends Worker
{

  private static final String TAG = OsmUploadWork.class.getSimpleName();
  private final Context mContext;
  private final WorkerParameters mWorkerParameters;

  public OsmUploadWork(@NonNull Context context, @NonNull WorkerParameters workerParams)
  {
    super(context, workerParams);
    this.mContext = context;
    this.mWorkerParameters = workerParams;
  }

  /**
   * Starts this worker to upload map edits to osm servers.
   */
  public static void startActionUploadOsmChanges(@NonNull Context context)
  {
    if (Editor.nativeHasSomethingToUpload() && OsmOAuth.isAuthorized(context))
    {
      final Constraints c = new Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build();
      final WorkRequest wr = new OneTimeWorkRequest.Builder(OsmUploadWork.class).setConstraints(c).build();
      WorkManager.getInstance(context).enqueue(wr);
    }
  }

  @NonNull
  @Override
  public Result doWork()
  {
    final MwmApplication app = MwmApplication.from(mContext);
    if (!app.getOrganicMaps().arePlatformAndCoreInitialized())
    {
      Logger.w(TAG, "Application is not initialized, ignoring " + mWorkerParameters);
      return Result.failure();
    }
    Editor.uploadChanges(mContext);
    return Result.success();
  }
}
