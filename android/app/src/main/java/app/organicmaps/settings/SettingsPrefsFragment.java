package app.organicmaps.settings;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.TwoStatePreference;
import app.organicmaps.Framework;
import app.organicmaps.R;
import app.organicmaps.downloader.MapManager;
import app.organicmaps.downloader.OnmapDownloader;
import app.organicmaps.editor.OsmOAuth;
import app.organicmaps.editor.LanguagesFragment;
import app.organicmaps.editor.ProfileActivity;
import app.organicmaps.editor.data.Language;
import app.organicmaps.help.HelpActivity;
import app.organicmaps.location.LocationHelper;
import app.organicmaps.location.LocationProviderFactory;
import app.organicmaps.routing.RoutingOptions;
import app.organicmaps.util.Config;
import app.organicmaps.util.NetworkPolicy;
import app.organicmaps.util.PowerManagment;
import app.organicmaps.util.SharedPropertiesUtils;
import app.organicmaps.util.ThemeSwitcher;
import app.organicmaps.util.Utils;
import app.organicmaps.util.log.LogsManager;
import app.organicmaps.search.SearchRecents;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.util.Locale;

public class SettingsPrefsFragment extends BaseXmlSettingsFragment implements LanguagesFragment.Listener
{
  @Override
  protected int getXmlResources()
  {
    return R.xml.prefs_main;
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    initStoragePrefCallbacks();
    initMeasureUnitsPrefsCallbacks();
    initZoomPrefsCallbacks();
    initMapStylePrefsCallbacks();
    initAutoDownloadPrefsCallbacks();
    initLargeFontSizePrefsCallbacks();
    initTransliterationPrefsCallbacks();
    init3dModePrefsCallbacks();
    initPerspectivePrefsCallbacks();
    initAutoZoomPrefsCallbacks();
    initLoggingEnabledPrefsCallbacks();
    initEmulationBadStorage();
    initUseMobileDataPrefsCallbacks();
    initPowerManagementPrefsCallbacks();
    initPlayServicesPrefsCallbacks();
    initSearchPrivacyPrefsCallbacks();
    initDisplayKayakPrefsCallbacks();
    initScreenSleepEnabledPrefsCallbacks();
    initShowOnLockScreenPrefsCallbacks();
  }

  private void updateVoiceInstructionsPrefsSummary()
  {
    final Preference pref = getPreference(getString(R.string.pref_tts_screen));
    pref.setSummary(Config.TTS.isEnabled() ? R.string.on : R.string.off);
  }

  private void updateMapLanguageCodeSummary()
  {
    final Preference pref = getPreference(getString(R.string.pref_map_locale));
    Locale locale = new Locale(MapLanguageCode.getMapLanguageCode());
    pref.setSummary(locale.getDisplayLanguage());
  }

  private void updateRoutingSettingsPrefsSummary()
  {
    final Preference pref = getPreference(getString(R.string.prefs_routing));
    pref.setSummary(RoutingOptions.hasAnyOptions() ? R.string.on : R.string.off);
  }

  private void updateProfileSettingsPrefsSummary()
  {
    final Preference pref = getPreference(getString(R.string.pref_osm_profile));
    if (OsmOAuth.isAuthorized(requireContext()))
    {
      final String username = OsmOAuth.getUsername(requireContext());
      pref.setSummary(username);
    }
    else
      pref.setSummary(R.string.not_signed_in);
  }

  @Override
  public void onResume()
  {
    super.onResume();

    updateProfileSettingsPrefsSummary();
    updateVoiceInstructionsPrefsSummary();
    updateRoutingSettingsPrefsSummary();
    updateMapLanguageCodeSummary();
  }

  @Override
  public boolean onPreferenceTreeClick(Preference preference)
  {
    final String key = preference.getKey();
    if (key != null)
    {
      if (key.equals(getString(R.string.pref_osm_profile)))
      {
        startActivity(new Intent(requireActivity(), ProfileActivity.class));
      }
      else if (key.equals(getString(R.string.pref_tts_screen)))
      {
        getSettingsActivity().stackFragment(VoiceInstructionsSettingsFragment.class, getString(R.string.pref_tts_enable_title), null);
      }
      else if (key.equals(getString(R.string.pref_help)))
      {
        startActivity(new Intent(requireActivity(), HelpActivity.class));
      }
      else if (key.equals(getString(R.string.pref_map_locale)))
      {
        LanguagesFragment langFragment = (LanguagesFragment)getSettingsActivity().stackFragment(LanguagesFragment.class, getString(R.string.change_map_locale), null);
        langFragment.setListener(this);
      }
    }
    return super.onPreferenceTreeClick(preference);
  }

  private void initLargeFontSizePrefsCallbacks()
  {
    final Preference pref = getPreference(getString(R.string.pref_large_fonts_size));

    ((TwoStatePreference)pref).setChecked(Config.isLargeFontsSize());
    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      final boolean oldVal = Config.isLargeFontsSize();
      final boolean newVal = (Boolean) newValue;
      if (oldVal != newVal)
        Config.setLargeFontsSize(newVal);

      return true;
    });
  }

  private void initTransliterationPrefsCallbacks()
  {
    final Preference pref = getPreference(getString(R.string.pref_transliteration));

    ((TwoStatePreference)pref).setChecked(Config.isTransliteration());
    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      final boolean oldVal = Config.isTransliteration();
      final boolean newVal = (Boolean) newValue;
      if (oldVal != newVal)
        Config.setTransliteration(newVal);

      return true;
    });
  }

  private void initUseMobileDataPrefsCallbacks()
  {
    final ListPreference mobilePref = getPreference(getString(R.string.pref_use_mobile_data));

    NetworkPolicy.Type curValue = Config.getUseMobileDataSettings();
    if (curValue == NetworkPolicy.Type.NOT_TODAY || curValue == NetworkPolicy.Type.TODAY)
        curValue = NetworkPolicy.Type.ASK;
    mobilePref.setValue(curValue.name());
    mobilePref.setOnPreferenceChangeListener((preference, newValue) -> {
      final String valueStr = (String) newValue;
      NetworkPolicy.Type type = NetworkPolicy.Type.valueOf(valueStr);
      Config.setUseMobileDataSettings(type);
      return true;
    });
  }

  private void initPowerManagementPrefsCallbacks()
  {
    final ListPreference powerManagementPref = getPreference(getString(R.string.pref_power_management));

    @PowerManagment.SchemeType
    int curValue = PowerManagment.getScheme();
    powerManagementPref.setValue(String.valueOf(curValue));

    powerManagementPref.setOnPreferenceChangeListener((preference, newValue) ->
    {
      @PowerManagment.SchemeType
      int scheme = Integer.parseInt((String) newValue);

      PowerManagment.setScheme(scheme);

      disableOrEnable3DBuildingsForPowerMode(scheme);

      return true;
    });
  }

  private void initLoggingEnabledPrefsCallbacks()
  {
    final Preference pref = getPreference(getString(R.string.pref_enable_logging));

    ((TwoStatePreference) pref).setChecked(LogsManager.INSTANCE.isFileLoggingEnabled());
    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      if (!LogsManager.INSTANCE.setFileLoggingEnabled((Boolean) newValue))
      {
        // It's a very rare condition when debugging, so we can do without translation.
        Utils.showSnackbar(requireView(), "ERROR: Can't create a logs folder!");
        return false;
      }
      return true;
    });
  }

  private void initEmulationBadStorage()
  {
    final Preference pref = findPreference(getString(R.string.pref_emulate_bad_external_storage));
    if (pref == null)
      return;

    if (!SharedPropertiesUtils.shouldShowEmulateBadStorageSetting(requireContext()))
      removePreference(getString(R.string.pref_settings_general), pref);
  }

  private void initAutoZoomPrefsCallbacks()
  {
    final TwoStatePreference pref = getPreference(getString(R.string.pref_auto_zoom));

    boolean autozoomEnabled = Framework.nativeGetAutoZoomEnabled();
    pref.setChecked(autozoomEnabled);
    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      Framework.nativeSetAutoZoomEnabled((boolean) newValue);
      return true;
    });
  }

  private void initPlayServicesPrefsCallbacks()
  {
    final Preference pref = findPreference(getString(R.string.pref_play_services));
    if (pref == null)
      return;

    if (!LocationProviderFactory.isGoogleLocationAvailable(requireActivity().getApplicationContext()))
      removePreference(getString(R.string.pref_privacy), pref);
    else
    {
      ((TwoStatePreference) pref).setChecked(Config.useGoogleServices());
      pref.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener()
      {
        @SuppressLint("MissingPermission")
        @Override
        public boolean onPreferenceChange(@NonNull Preference preference, Object newValue)
        {
          final LocationHelper locationHelper = LocationHelper.from(requireContext());
          boolean oldVal = Config.useGoogleServices();
          boolean newVal = (Boolean) newValue;
          if (oldVal != newVal)
          {
            Config.setUseGoogleService(newVal);
            if (locationHelper.isActive())
            {
              locationHelper.stop();
              locationHelper.start();
            }
          }
          return true;
        }
      });
    }
  }

  private void initSearchPrivacyPrefsCallbacks()
  {
    final Preference pref = findPreference(getString(R.string.pref_search_history));
    if (pref == null)
      return;

    final boolean isHistoryEnabled = Config.isSearchHistoryEnabled();
    ((TwoStatePreference) pref).setChecked(isHistoryEnabled);
    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      boolean newVal = (Boolean) newValue;
      if (newVal != Config.isSearchHistoryEnabled())
      {
        Config.setSearchHistoryEnabled(newVal);
        if (newVal)
          SearchRecents.refresh();
        else
          SearchRecents.clear();
      }
      return true;
    });
  }

  private void initDisplayKayakPrefsCallbacks()
  {
    final TwoStatePreference pref = getPreference(getString(R.string.pref_display_kayak));

    pref.setChecked(Config.isKayakDisplayEnabled());
    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      final boolean oldVal = Config.isKayakDisplayEnabled();
      final boolean newVal = (Boolean) newValue;
      if (oldVal != newVal)
        Config.setKayakDisplay(newVal);

      return true;
    });
  }

  private void init3dModePrefsCallbacks()
  {
    final TwoStatePreference pref = getPreference(getString(R.string.pref_3d_buildings));

    final Framework.Params3dMode _3d = new Framework.Params3dMode();
    Framework.nativeGet3dMode(_3d);

    // Read power managements preference.
    final ListPreference powerManagementPref = getPreference(getString(R.string.pref_power_management));
    final String powerManagementValueStr = powerManagementPref.getValue();
    final Integer powerManagementValue = (powerManagementValueStr!=null) ? Integer.parseInt(powerManagementValueStr) : null;
    disableOrEnable3DBuildingsForPowerMode(powerManagementValue);

    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      Framework.nativeSet3dMode(_3d.enabled, (Boolean)newValue);
      return true;
    });
  }

  // Argument `powerManagementValue` could be null on the first app run.
  private void disableOrEnable3DBuildingsForPowerMode(@Nullable Integer powerManagementValue)
  {
    final TwoStatePreference pref = getPreference(getString(R.string.pref_3d_buildings));

    if (powerManagementValue != null && powerManagementValue == PowerManagment.HIGH)
    {
      pref.setShouldDisableView(true);
      pref.setEnabled(false);
      pref.setSummary(getString(R.string.pref_map_3d_buildings_disabled_summary));
      pref.setChecked(false);
    }
    else
    {
      final Framework.Params3dMode _3d = new Framework.Params3dMode();
      Framework.nativeGet3dMode(_3d);

      pref.setShouldDisableView(false);
      pref.setEnabled(true);
      pref.setSummary("");
      pref.setChecked(_3d.buildings);
    }
  }

  private void initPerspectivePrefsCallbacks()
  {
    final TwoStatePreference pref = getPreference(getString(R.string.pref_3d));

    final Framework.Params3dMode _3d = new Framework.Params3dMode();
    Framework.nativeGet3dMode(_3d);

    pref.setChecked(_3d.enabled);

    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      Framework.nativeSet3dMode((Boolean) newValue, _3d.buildings);
      return true;
    });
  }

  private void initAutoDownloadPrefsCallbacks()
  {
    final TwoStatePreference pref = getPreference(getString(R.string.pref_autodownload));

    pref.setChecked(Config.isAutodownloadEnabled());
    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      final boolean value = (boolean) newValue;
      Config.setAutodownloadEnabled(value);

      if (value)
        OnmapDownloader.setAutodownloadLocked(false);

      return true;
    });
  }

  private void initMapStylePrefsCallbacks()
  {
    final ListPreference pref = getPreference(getString(R.string.pref_map_style));

    String curTheme = Config.getUiThemeSettings(requireContext());
    pref.setValue(curTheme);
    pref.setSummary(pref.getEntry());
    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      final String themeName = (String) newValue;
      if (!Config.setUiThemeSettings(requireContext(), themeName))
        return true;

      ThemeSwitcher.INSTANCE.restart(false);

      ThemeMode mode = ThemeMode.getInstance(requireContext().getApplicationContext(), themeName);
      CharSequence summary = pref.getEntries()[mode.ordinal()];
      pref.setSummary(summary);
      return true;
    });
  }

  private void initZoomPrefsCallbacks()
  {
    final Preference pref = getPreference(getString(R.string.pref_show_zoom_buttons));

    ((TwoStatePreference)pref).setChecked(Config.showZoomButtons());
    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      Config.setShowZoomButtons((boolean) newValue);
      return true;
    });
  }

  private void initMeasureUnitsPrefsCallbacks()
  {
    final Preference pref = getPreference(getString(R.string.pref_munits));

    ((ListPreference)pref).setValue(String.valueOf(UnitLocale.getUnits()));
    pref.setOnPreferenceChangeListener((preference, newValue) -> {
      UnitLocale.setUnits(Integer.parseInt((String) newValue));
      return true;
    });
  }

  private void initStoragePrefCallbacks()
  {
    final Preference pref = getPreference(getString(R.string.pref_storage));
    pref.setOnPreferenceClickListener(preference -> {
      if (MapManager.nativeIsDownloading())
      {
        new MaterialAlertDialogBuilder(requireActivity(), R.style.MwmTheme_AlertDialog)
            .setTitle(R.string.downloading_is_active)
            .setMessage(R.string.cant_change_this_setting)
            .setPositiveButton(R.string.ok, null)
            .show();
      }
      else
      {
        getSettingsActivity().stackFragment(StoragePathFragment.class, getString(R.string.maps_storage), null);
      }

      return true;
    });
  }

  private void initScreenSleepEnabledPrefsCallbacks()
  {
    final Preference pref = getPreference(getString(R.string.pref_keep_screen_on));

    final boolean isKeepScreenOnEnabled = Config.isKeepScreenOnEnabled();
    ((TwoStatePreference) pref).setChecked(isKeepScreenOnEnabled);
    pref.setOnPreferenceChangeListener((preference, newValue) ->
    {
      boolean newVal = (Boolean) newValue;
      if (isKeepScreenOnEnabled != newVal)
      {
        Config.setKeepScreenOnEnabled(newVal);
        // No need to call Utils.keepScreenOn() here, as relevant activities do it when starting / stopping.
      }
      return true;
    });
  }

  private void initShowOnLockScreenPrefsCallbacks()
  {
    final Preference pref = getPreference(getString(R.string.pref_show_on_lock_screen));

    final boolean isShowOnLockScreenEnabled = Config.isShowOnLockScreenEnabled();
    ((TwoStatePreference) pref).setChecked(isShowOnLockScreenEnabled);
    pref.setOnPreferenceChangeListener((preference, newValue) ->
    {
      boolean newVal = (Boolean) newValue;
      if (isShowOnLockScreenEnabled != newVal)
      {
        Config.setShowOnLockScreenEnabled(newVal);
        Utils.showOnLockScreen(newVal, requireActivity());
      }
      return true;
    });
  }

  private void removePreference(@NonNull String categoryKey, @NonNull Preference preference)
  {
    final PreferenceCategory category = getPreference(categoryKey);

    category.removePreference(preference);
  }

  @Override
  public void onLanguageSelected(Language language)
  {
    MapLanguageCode.setMapLanguageCode(language.code);
    getSettingsActivity().onBackPressed();
  }

  enum ThemeMode
  {
    DEFAULT(R.string.theme_default),
    NIGHT(R.string.theme_night),
    AUTO(R.string.theme_auto),
    NAV_AUTO(R.string.theme_nav_auto);

    private final int mModeStringId;

    ThemeMode(@StringRes int modeStringId)
    {
      mModeStringId = modeStringId;
    }

    @NonNull
    public static ThemeMode getInstance(@NonNull Context context, @NonNull String src)
    {
      for (ThemeMode each : values())
      {
        if (context.getResources().getString(each.mModeStringId).equals(src))
          return each;
      }
      return AUTO;
    }
  }

  public enum SpeedCameraMode
  {
    AUTO,
    ALWAYS,
    NEVER
  }
}
