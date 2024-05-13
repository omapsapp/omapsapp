package app.organicmaps.sdk.sound;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;

import androidx.annotation.Nullable;

import static android.media.AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK;

public class AudioFocusManager
{
  @Nullable
  private AudioManager mAudioManager = null;
  @Nullable
  private AudioManager.OnAudioFocusChangeListener mOnFocusChange = null;
  @Nullable
  private AudioFocusRequest mAudioFocusRequest = null;

  public AudioFocusManager(@Nullable Context context)
  {
    if (context != null)
      mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);

    if (Build.VERSION.SDK_INT < 26)
      mOnFocusChange = focusGain -> {};
    else
      mAudioFocusRequest = new AudioFocusRequest.Builder(AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK).setAudioAttributes(
          new AudioAttributes.Builder()
              .setUsage(AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE)
              .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
              .build()
      ).build();
  }

  public boolean requestAudioFocus()
  {
    boolean isMusicActive = false;

    if (mAudioManager != null)
    {
      isMusicActive = mAudioManager.isMusicActive();
      if (Build.VERSION.SDK_INT < 26)
        mAudioManager.requestAudioFocus(mOnFocusChange, AudioManager.STREAM_VOICE_CALL, AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK);
      else
        mAudioManager.requestAudioFocus(mAudioFocusRequest);
    }

    return isMusicActive;
  }

  public void releaseAudioFocus()
  {
    if (mAudioManager != null)
    {
      if (Build.VERSION.SDK_INT < 26)
        mAudioManager.abandonAudioFocus(mOnFocusChange);
      else
        mAudioManager.abandonAudioFocusRequest(mAudioFocusRequest);
    }
  }
}
