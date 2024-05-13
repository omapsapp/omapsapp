package app.organicmaps.sdk.sound;

import android.content.Context;
import android.media.MediaPlayer;
import android.os.AsyncTask;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RawRes;

public class MediaPlayerWrapper
{
  private static final int UNDEFINED_SOUND_STREAM = -1;

  @Nullable
  private MediaPlayer mPlayer;
  private int mStreamResId = UNDEFINED_SOUND_STREAM;
  @NonNull
  final private Context mContext;

  public MediaPlayerWrapper(@NonNull Context context)
  {
    mContext = context;
  }

  private boolean isCurrentSoundStream(@RawRes int streamResId)
  {
    return mStreamResId == streamResId;
  }

  private void onInitializationCompleted(@NonNull InitializationResult initializationResult)
  {
    release();
    mStreamResId = initializationResult.getStreamResId();
    mPlayer = initializationResult.getPlayer();
    if (mPlayer == null)
      return;

    mPlayer.start();
  }

  public void release()
  {
    if (mPlayer == null)
      return;

    stop();
    mPlayer.release();
    mPlayer = null;
    mStreamResId = UNDEFINED_SOUND_STREAM;
  }

  @NonNull
  @SuppressWarnings("deprecation") // https://github.com/organicmaps/organicmaps/issues/3632
  private static AsyncTask<Integer, Void, InitializationResult> makeInitTask(@NonNull MediaPlayerWrapper wrapper)
  {
    return new InitPlayerTask(wrapper);
  }

  @SuppressWarnings("deprecation") // https://github.com/organicmaps/organicmaps/issues/3632
  public void playback(@RawRes int streamResId)
  {
    if (isCurrentSoundStream(streamResId) && mPlayer == null)
      return;

    if (isCurrentSoundStream(streamResId) && mPlayer != null)
    {
      mPlayer.start();
      return;
    }

    mStreamResId = streamResId;
    AsyncTask<Integer, Void, InitializationResult> task = makeInitTask(this);
    task.execute(streamResId);
  }

  public void stop()
  {
    if (mPlayer == null)
      return;

    mPlayer.stop();
  }

  @SuppressWarnings("deprecation") // https://github.com/organicmaps/organicmaps/issues/3632
  private static class InitPlayerTask extends AsyncTask<Integer, Void, InitializationResult>
  {
    @NonNull
    private final MediaPlayerWrapper mWrapper;

    InitPlayerTask(@NonNull MediaPlayerWrapper wrapper)
    {
      mWrapper = wrapper;
    }

    @Override
    protected InitializationResult doInBackground(Integer... params)
    {
      if (params.length == 0)
        throw new IllegalArgumentException("Params not found");
      int resId = params[0];
      MediaPlayer player = MediaPlayer.create(mWrapper.mContext, resId);
      return new InitializationResult(player, resId);
    }

    @Override
    protected void onPostExecute(InitializationResult initializationResult)
    {
      super.onPostExecute(initializationResult);
      mWrapper.onInitializationCompleted(initializationResult);
    }
  }

  private static class InitializationResult
  {
    @Nullable
    private final MediaPlayer mPlayer;
    @RawRes
    private final int mStreamResId;

    private InitializationResult(@Nullable MediaPlayer player, @RawRes int streamResId)
    {
      mPlayer = player;
      mStreamResId = streamResId;
    }

    @RawRes
    private int getStreamResId()
    {
      return mStreamResId;
    }

    @Nullable
    private MediaPlayer getPlayer()
    {
      return mPlayer;
    }
  }
}
