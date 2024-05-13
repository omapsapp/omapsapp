package app.organicmaps.sdk.sound;

import android.content.Context;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import app.organicmaps.sdk.R;
import app.organicmaps.sdk.OrganicMaps;
import app.organicmaps.sdk.util.Config;
import app.organicmaps.sdk.util.concurrency.UiThread;
import app.organicmaps.sdk.util.log.Logger;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * {@code TtsPlayer} class manages available TTS voice languages.
 * Single TTS language is described by {@link LanguageData} item.
 * <p>
 * We support a set of languages listed in {@code strings-tts.xml} file.
 * During loading each item in this list is marked as {@code downloaded} or {@code not downloaded},
 * unsupported voices are excluded.
 * <p>
 * At startup we check whether currently selected language is in our list of supported voices and its data is downloaded.
 * If not, we check system default locale. If failed, the same check is made for English language.
 * Finally, if mentioned checks fail we manually disable TTS, so the user must go to the settings and select
 * preferred voice language by hand.
 * <p>
 * If no core supported languages can be used by the system, TTS is locked down and can not be enabled and used.
 */
public enum TtsPlayer
{
  INSTANCE;

  private static final String TAG = TtsPlayer.class.getSimpleName();
  private static final Locale DEFAULT_LOCALE = Locale.US;
  private static final float SPEECH_RATE = 1.0f;
  private static final int TTS_SPEAK_DELAY_MILLIS = 50;

  private TextToSpeech mTts;
  private boolean mInitializing;
  private AudioFocusManager mAudioFocusManager;

  private final Bundle mParams = new Bundle();

  private final Handler delayHandler = new Handler(Looper.getMainLooper());

  @SuppressWarnings("NotNullFieldNotInitialized")
  @NonNull
  private Context mContext;

  // TTS is locked down due to absence of supported languages
  private boolean mUnavailable;

  TtsPlayer() {}

  private static @Nullable LanguageData findSupportedLanguage(String internalCode, List<LanguageData> langs)
  {
    if (TextUtils.isEmpty(internalCode))
      return null;

    for (LanguageData lang : langs)
      if (lang.matchesInternalCode(internalCode))
        return lang;

    return null;
  }

  private static @Nullable LanguageData findSupportedLanguage(Locale locale, List<LanguageData> langs)
  {
    if (locale == null)
      return null;

    for (LanguageData lang : langs)
      if (lang.matchesLocale(locale))
        return lang;

    return null;
  }

  private boolean setLanguageInternal(LanguageData lang)
  {
    try
    {
      mTts.setLanguage(lang.locale);
      nativeSetTurnNotificationsLocale(lang.internalCode);
      Config.TTS.setLanguage(lang.internalCode);

      return true;
    }
    catch (IllegalArgumentException e)
    {
      lockDown();
      return false;
    }
  }

  public boolean setLanguage(LanguageData lang)
  {
    return (lang != null && setLanguageInternal(lang));
  }

  private static @Nullable LanguageData getDefaultLanguage(List<LanguageData> langs)
  {
    LanguageData res;

    Locale defLocale = Locale.getDefault();
    if (defLocale != null)
    {
      res = findSupportedLanguage(defLocale, langs);
      if (res != null && res.downloaded)
        return res;
    }

    res = findSupportedLanguage(DEFAULT_LOCALE, langs);
    if (res != null && res.downloaded)
      return res;

    return null;
  }

  public static @Nullable LanguageData getSelectedLanguage(List<LanguageData> langs)
  {
    return findSupportedLanguage(Config.TTS.getLanguage(), langs);
  }

  private void lockDown()
  {
    mUnavailable = true;
    setEnabled(false);
  }

  public void initialize(@NonNull Context context)
  {
    mContext = context;

    if (mTts != null || mInitializing || mUnavailable)
      return;

    mInitializing = true;
    // TextToSpeech.OnInitListener() can be called from a non-main thread
    // on LineageOS '20.0-20231127-RELEASE-thyme' 'Xiaomi/thyme/thyme'.
    // https://github.com/organicmaps/organicmaps/issues/6903
    mTts = new TextToSpeech(context, status -> UiThread.run(() -> {
      if (status == TextToSpeech.ERROR)
      {
        Logger.e(TAG, "Failed to initialize TextToSpeech");
        lockDown();
        mInitializing = false;
        return;
      }
      refreshLanguages();
      mTts.setSpeechRate(SPEECH_RATE);
      mTts.setOnUtteranceProgressListener(new UtteranceProgressListener() {
        @Override
        public void onStart(String utteranceId) {
          mAudioFocusManager.requestAudioFocus();
        }

        @Override
        public void onDone(String utteranceId) {
          mAudioFocusManager.releaseAudioFocus();
        }

        @Override
        @SuppressWarnings("deprecated") // abstract method must be implemented
        public void onError(String utteranceId)
        {
          mAudioFocusManager.releaseAudioFocus();
        }

        @Override
        public void onError(String utteranceId, int errorCode) {
          mAudioFocusManager.releaseAudioFocus();
        }
      });
      mAudioFocusManager = new AudioFocusManager(context);
      mParams.putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, Config.TTS.getVolume());
      mInitializing = false;
    }));
  }

  private static boolean isReady()
  {
    return (INSTANCE.mTts != null && !INSTANCE.mUnavailable && !INSTANCE.mInitializing);
  }

  public void speak(String textToSpeak)
  {
    if (Config.TTS.isEnabled())
      try
      {
        boolean isMusicActive = mAudioFocusManager.requestAudioFocus();
        if (isMusicActive)
          delayHandler.postDelayed(() -> mTts.speak(textToSpeak, TextToSpeech.QUEUE_ADD, mParams, textToSpeak), TTS_SPEAK_DELAY_MILLIS);
        else
          mTts.speak(textToSpeak, TextToSpeech.QUEUE_ADD, mParams, textToSpeak);
      }
      catch (IllegalArgumentException e)
      {
        lockDown();
      }
  }

  public void playTurnNotifications(@NonNull String[] turnNotifications)
  {
    if (isReady())
      for (String textToSpeak : turnNotifications)
        speak(textToSpeak);
  }

  public void stop()
  {
    if (isReady())
      try
      {
        mAudioFocusManager.releaseAudioFocus();
        mTts.stop();
      }
      catch (IllegalArgumentException e)
      {
        lockDown();
      }
  }

  public static boolean isEnabled()
  {
    return (isReady() && nativeAreTurnNotificationsEnabled());
  }

  public static void setEnabled(boolean enabled)
  {
    Config.TTS.setEnabled(enabled);
    nativeEnableTurnNotifications(enabled);
  }

  public float getVolume()
  {
    return Config.TTS.getVolume();
  }

  public void setVolume(final float volume)
  {
    mParams.putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, volume);
    Config.TTS.setVolume(volume);
  }

  private boolean getUsableLanguages(List<LanguageData> outList)
  {
    Resources resources = OrganicMaps.from(mContext).getResources();
    String[] codes = resources.getStringArray(R.array.tts_languages_supported);
    String[] names = resources.getStringArray(R.array.tts_language_names);

    for (int i = 0; i < codes.length; i++)
    {
      try
      {
        outList.add(new LanguageData(codes[i], names[i], mTts));
      }
      catch (LanguageData.NotAvailableException ignored) {
        Logger.w(TAG, "Failed to get usable languages " + ignored.getMessage());
      }
      catch (IllegalArgumentException e)
      {
        Logger.e(TAG, "Failed to get usable languages", e);
        lockDown();
        return false;
      }
    }

    return true;
  }

  private @Nullable LanguageData refreshLanguagesInternal(List<LanguageData> outList)
  {
    if (!getUsableLanguages(outList))
      return null;

    if (outList.isEmpty())
    {
      // No supported languages found, lock down TTS :(
      lockDown();
      return null;
    }

    LanguageData res = getSelectedLanguage(outList);
    if (res == null || !res.downloaded)
      // Selected locale is not available or not downloaded
      res = getDefaultLanguage(outList);

    if (res == null || !res.downloaded)
    {
      // Default locale can not be used too
      Config.TTS.setEnabled(false);
      return null;
    }

    return res;
  }

  public @NonNull List<LanguageData> refreshLanguages()
  {
    List<LanguageData> res = new ArrayList<>();
    if (mUnavailable || mTts == null)
      return res;

    LanguageData lang = refreshLanguagesInternal(res);
    setLanguage(lang);

    setEnabled(Config.TTS.isEnabled());
    return res;
  }

  private native static void nativeEnableTurnNotifications(boolean enable);
  private native static boolean nativeAreTurnNotificationsEnabled();
  private native static void nativeSetTurnNotificationsLocale(String code);
  private native static String nativeGetTurnNotificationsLocale();
}
