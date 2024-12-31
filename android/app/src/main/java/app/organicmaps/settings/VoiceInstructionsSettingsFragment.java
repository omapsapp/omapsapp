package app.organicmaps.settings;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.speech.tts.TextToSpeech;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.View;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.SeekBarPreference;
import androidx.preference.TwoStatePreference;

import app.organicmaps.Framework;
import app.organicmaps.R;
import app.organicmaps.sound.LanguageData;
import app.organicmaps.sound.TtsPlayer;
import app.organicmaps.util.Config;
import app.organicmaps.util.UiUtils;
import app.organicmaps.util.Utils;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class VoiceInstructionsSettingsFragment extends BaseXmlSettingsFragment
{

  @NonNull
  @SuppressWarnings("NotNullFieldNotInitialized")
  private TwoStatePreference mTtsPrefEnabled;
  @NonNull
  @SuppressWarnings("NotNullFieldNotInitialized")
  private ListPreference mTtsPrefLanguages;
  @NonNull
  @SuppressWarnings("NotNullFieldNotInitialized")
  private SeekBarPreference mTtsVolume;
  @NonNull
  @SuppressWarnings("NotNullFieldNotInitialized")
  private TwoStatePreference mTtsPrefStreetNames;
  @NonNull
  @SuppressWarnings("NotNullFieldNotInitialized")
  private ListPreference mPrefLanguages;
  @NonNull
  @SuppressWarnings("NotNullFieldNotInitialized")
  private Preference mTtsLangInfo;
  @NonNull
  @SuppressWarnings("NotNullFieldNotInitialized")
  private Preference mTtsVoiceTest;

  private List<String> mTtsTestStringArray;
  private int mTestStringIndex;

  @NonNull
  private final Map<String, LanguageData> mLanguages = new HashMap<>();
  private LanguageData mCurrentLanguage;
  private String mSelectedLanguage;

  private final ActivityResultLauncher<Intent> startInstallDataIntentForResult = registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), activityResult -> {

    if(activityResult.getResultCode() == Activity.RESULT_OK)
      {
        onInstallDataResult();
      }
  });

  private final Preference.OnPreferenceChangeListener mEnabledListener = (preference, newValue) -> {
    final boolean set = (Boolean) newValue;
    if (!set)
    {
      TtsPlayer.setEnabled(false);
      mTtsPrefLanguages.setVisible(false);
      mTtsPrefStreetNames.setVisible(false);
      mTtsVolume.setVisible(false);
      mTtsLangInfo.setSummary(R.string.prefs_languages_information_off);
      mTtsVoiceTest.setVisible(false);
      return true;
    }

    mTtsLangInfo.setSummary(R.string.prefs_languages_information);
    mTtsPrefLanguages.setVisible(true);
    mTtsPrefStreetNames.setVisible(true);
    mTtsVolume.setVisible(true);
    mTtsVoiceTest.setVisible(true);

    if (mCurrentLanguage != null && mCurrentLanguage.downloaded)
    {
      setLanguage(mCurrentLanguage);
      return true;
    }

    return false;
  };

  private final Preference.OnPreferenceChangeListener mLangListener = (preference, newValue) -> {
    if (newValue == null)
      return false;

    mSelectedLanguage = (String) newValue;
    final LanguageData lang = mLanguages.get(mSelectedLanguage);
    if (lang == null)
      return false;

    if (lang.downloaded)
      setLanguage(lang);
    else
      UiUtils.startActivityForResult(startInstallDataIntentForResult, new Intent(TextToSpeech.Engine.ACTION_INSTALL_TTS_DATA));

    return false;
  };

  private final Preference.OnPreferenceChangeListener mStreetNameListener = (preference, newValue) -> {
    boolean set = (Boolean) newValue;
    Config.TTS.setAnnounceStreets(set);

    return true;
  };

  @Override
  protected int getXmlResources()
  {
    return R.xml.prefs_voice_instructions;
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    mTtsPrefEnabled = getPreference(getString(R.string.pref_tts_enabled));
    mTtsPrefLanguages = getPreference(getString(R.string.pref_tts_language));
    mTtsPrefStreetNames = findPreference(getString(R.string.pref_tts_street_names));
    mTtsLangInfo = getPreference(getString(R.string.pref_tts_info));

    Preference mTtsOpenSystemSettings = getPreference(getString(R.string.pref_tts_open_system_settings));
    mTtsOpenSystemSettings.setOnPreferenceClickListener(pref -> {
      try
      {
        final Intent intent = new Intent()
                .setAction("com.android.settings.TTS_SETTINGS")
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
        return true;
      }
      catch(ActivityNotFoundException e)
      {
        CharSequence noTtsSettingString = getString(R.string.pref_tts_no_system_tts);
        Toast.makeText(super.getSettingsActivity(), noTtsSettingString, Toast.LENGTH_LONG).show();
        return false;
      }
    });

    mTtsVoiceTest = getPreference(getString(R.string.pref_tts_test_voice));
    mTtsVoiceTest.setOnPreferenceClickListener(pref -> {
      if (mTtsTestStringArray == null)
        return false;

      Utils.showSnackbar(view, getString(R.string.pref_tts_playing_test_voice));
      TtsPlayer.INSTANCE.speak(mTtsTestStringArray.get(mTestStringIndex));
      mTestStringIndex++;
      if (mTestStringIndex >= mTtsTestStringArray.size())
        mTestStringIndex = 0;
      return true;
    });

    initVolume();
    initTtsLangInfoLink();
    initSpeedCamerasPrefs();
    updateTts();
  }

  @Override
  public void onResume()
  {
    super.onResume();

    updateTts();
  }

  private void onInstallDataResult()
  {
    updateTts();

    LanguageData lang = mLanguages.get(mSelectedLanguage);
    if (lang != null && lang.downloaded)
      setLanguage(lang);
  }

  private void enableListeners(boolean enable)
  {
    mTtsPrefEnabled.setOnPreferenceChangeListener(enable ? mEnabledListener : null);
    mTtsPrefLanguages.setOnPreferenceChangeListener(enable ? mLangListener : null);
    mTtsPrefStreetNames.setOnPreferenceChangeListener(enable ? mStreetNameListener : null);
  }

  private void setLanguage(@NonNull LanguageData lang)
  {
    TtsPlayer.setEnabled(true);
    TtsPlayer.INSTANCE.setLanguage(lang);
    mTtsPrefLanguages.setSummary(lang.name);

    updateTts();
  }

  private void updateTts()
  {
    enableListeners(false);

    final List<LanguageData> languages = TtsPlayer.INSTANCE.refreshLanguages();
    mLanguages.clear();
    mCurrentLanguage = null;
    mTtsTestStringArray = null;

    if (languages.isEmpty())
    {
      mTtsPrefEnabled.setChecked(false);
      mTtsPrefEnabled.setEnabled(false);
      mTtsPrefEnabled.setSummary(R.string.pref_tts_unavailable);
      mTtsPrefStreetNames.setVisible(false);
      mTtsPrefLanguages.setVisible(false);
      mTtsPrefLanguages.setSummary(null);
      mTtsVolume.setVisible(false);
      mTtsVoiceTest.setVisible(false);
      mTtsLangInfo.setSummary(R.string.prefs_languages_information_off);

      enableListeners(true);
      return;
    }

    final boolean enabled = TtsPlayer.isEnabled();
    mTtsLangInfo.setSummary(enabled ? R.string.prefs_languages_information
        : R.string.prefs_languages_information_off);

    final CharSequence[] entries = new CharSequence[languages.size()];
    final CharSequence[] values = new CharSequence[languages.size()];
    for (int i = 0; i < languages.size(); i++)
    {
      LanguageData lang = languages.get(i);
      entries[i] = lang.name;
      values[i] = lang.internalCode;

      mLanguages.put(lang.internalCode, lang);
    }

    mTtsPrefLanguages.setEntries(entries);
    mTtsPrefLanguages.setEntryValues(values);

    mCurrentLanguage = TtsPlayer.getSelectedLanguage(languages);
    final boolean available = (mCurrentLanguage != null && mCurrentLanguage.downloaded);
    mTtsPrefEnabled.setChecked(available && TtsPlayer.isEnabled());
    mTtsPrefLanguages.setVisible(available && TtsPlayer.isEnabled());
    mTtsPrefLanguages.setSummary(available ? mCurrentLanguage.name : null);
    mTtsPrefLanguages.setValue(available ? mCurrentLanguage.internalCode : null);
    mTtsPrefStreetNames.setVisible(enabled && available && TtsPlayer.isEnabled());
    mTtsVolume.setVisible(enabled && available && TtsPlayer.isEnabled());
    mTtsVoiceTest.setVisible(enabled && available && TtsPlayer.isEnabled());

    if (available)
    {
      // Update array of TTS test strings. Strings are taken from resources using selected TTS language.
      final Configuration config = new Configuration(getResources().getConfiguration());
      config.setLocale(mCurrentLanguage.locale);
      mTtsTestStringArray = Arrays.asList(requireContext().createConfigurationContext(config).getResources().getStringArray(R.array.app_tips));
      Collections.shuffle(mTtsTestStringArray);
      mTestStringIndex = 0;
    }

    enableListeners(true);
  }

  @SuppressWarnings("ConstantConditions")
  private void initVolume()
  {
    mTtsVolume = getPreference(getString(R.string.pref_tts_volume));
    mTtsVolume.setMin((int) (Config.TTS.Defaults.VOLUME_MIN * 100));
    mTtsVolume.setMax((int) (Config.TTS.Defaults.VOLUME_MAX * 100));
    mTtsVolume.setUpdatesContinuously(true);
    mTtsVolume.setOnPreferenceClickListener(preference -> {
      setTtsVolume(Config.TTS.Defaults.VOLUME);
      return true;
    });
    mTtsVolume.setOnPreferenceChangeListener((preference, newValue) -> {
      setTtsVolume(((float) ((int) newValue)) / 100);
      return true;
    });
    setTtsVolume(TtsPlayer.INSTANCE.getVolume());
  }

  private void setTtsVolume(final float volume)
  {
    final int volumeInt = (int) (volume * 100);
    mTtsVolume.setValue(volumeInt);
    mTtsVolume.setSummary(Integer.toString(volumeInt));
    TtsPlayer.INSTANCE.setVolume(volume);
  }

  private void initTtsLangInfoLink()
  {
    final Preference ttsLangInfoLink = getPreference(getString(R.string.pref_tts_info_link));
    final String ttsLinkText = getString(R.string.prefs_languages_information_off_link);
    final Spannable link = new SpannableString(ttsLinkText + "↗");
    // Set link color.
    link.setSpan(new ForegroundColorSpan(ContextCompat.getColor(requireContext(),
            UiUtils.getStyledResourceId(requireContext(), androidx.appcompat.R.attr.colorAccent))),
        0, ttsLinkText.length(), 0);
    ttsLangInfoLink.setSummary(link);

    final String ttsInfoUrl = requireActivity().getString(R.string.tts_info_link);
    ttsLangInfoLink.setOnPreferenceClickListener(preference -> {
      Utils.openUrl(requireContext(), ttsInfoUrl);
      return false;
    });
  }

  private void initSpeedCamerasPrefs()
  {
    final ListPreference pref = getPreference(getString(R.string.pref_tts_speed_cameras));
    pref.setSummary(pref.getEntry());
    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      final String speedCamModeValue = (String) newValue;
      final SettingsPrefsFragment.SpeedCameraMode newCamMode = SettingsPrefsFragment.SpeedCameraMode.valueOf(speedCamModeValue);
      final CharSequence summary = pref.getEntries()[newCamMode.ordinal()];
      pref.setSummary(summary);
      if (pref.getValue().equals(newValue))
        return true;

      onSpeedCamerasPrefChanged(newCamMode);
      return true;
    });
  }

  private void onSpeedCamerasPrefChanged(@NonNull SettingsPrefsFragment.SpeedCameraMode newCamMode)
  {
    Framework.setSpeedCamerasMode(newCamMode);
  }
}
