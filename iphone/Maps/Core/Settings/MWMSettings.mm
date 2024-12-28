#import "MWMSettings.h"
#import "MWMCoreUnits.h"
#import "MWMMapViewControlsManager.h"
#import "SwiftBridge.h"

#include <CoreApi/Framework.h>
#include <CoreApi/Logger.h>

namespace
{
char const * kAutoDownloadEnabledKey = "AutoDownloadEnabled";
char const * kZoomButtonsEnabledKey = "ZoomButtonsEnabled";
char const * kCompassCalibrationEnabledKey = "CompassCalibrationEnabled";
char const * kRoutingDisclaimerApprovedKey = "IsDisclaimerApproved";

// TODO(igrechuhin): Remove outdated kUDAutoNightModeOff
NSString * const kUDAutoNightModeOff = @"AutoNightModeOff";
NSString * const kThemeMode = @"ThemeMode";
NSString * const kSpotlightLocaleLanguageId = @"SpotlightLocaleLanguageId";
NSString * const kUDTrackWarningAlertWasShown = @"TrackWarningAlertWasShown";
NSString * const kiCLoudSynchronizationEnabledKey = @"iCLoudSynchronizationEnabled";
NSString * const kUDFileLoggingEnabledKey = @"FileLoggingEnabledKey";
}  // namespace

@implementation MWMSettings

+ (BOOL)autoDownloadEnabled
{
  bool autoDownloadEnabled = true;
  UNUSED_VALUE(settings::Get(kAutoDownloadEnabledKey, autoDownloadEnabled));
  return autoDownloadEnabled;
}

+ (void)setAutoDownloadEnabled:(BOOL)autoDownloadEnabled
{
  settings::Set(kAutoDownloadEnabledKey, static_cast<bool>(autoDownloadEnabled));
}

+ (MWMUnits)measurementUnits
{
  auto units = measurement_utils::Units::Metric;
  UNUSED_VALUE(settings::Get(settings::kMeasurementUnits, units));
  return mwmUnits(units);
}

+ (void)setMeasurementUnits:(MWMUnits)measurementUnits
{
  settings::Set(settings::kMeasurementUnits, coreUnits(measurementUnits));
  GetFramework().SetupMeasurementSystem();
}

+ (BOOL)zoomButtonsEnabled
{
  bool enabled = true;
  UNUSED_VALUE(settings::Get(kZoomButtonsEnabledKey, enabled));
  return enabled;
}

+ (void)setZoomButtonsEnabled:(BOOL)zoomButtonsEnabled
{
  settings::Set(kZoomButtonsEnabledKey, static_cast<bool>(zoomButtonsEnabled));
  [MWMMapViewControlsManager manager].zoomHidden = !zoomButtonsEnabled;
}

+ (BOOL)compassCalibrationEnabled
{
  bool enabled = true;
  UNUSED_VALUE(settings::Get(kCompassCalibrationEnabledKey, enabled));
  return enabled;
}

+ (void)setCompassCalibrationEnabled:(BOOL)compassCalibrationEnabled
{
  settings::Set(kCompassCalibrationEnabledKey, static_cast<bool>(compassCalibrationEnabled));
}

+ (MWMTheme)theme
{
  if ([MWMCarPlayService shared].isCarplayActivated) {
    UIUserInterfaceStyle style = [[MWMCarPlayService shared] interfaceStyle];
    switch (style) {
      case UIUserInterfaceStyleLight: return MWMThemeDay;
      case UIUserInterfaceStyleDark: return MWMThemeNight;
      case UIUserInterfaceStyleUnspecified: break;
    }
  }
  auto ud = NSUserDefaults.standardUserDefaults;
  if (![ud boolForKey:kUDAutoNightModeOff])
    return MWMThemeAuto;
  return static_cast<MWMTheme>([ud integerForKey:kThemeMode]);
}

+ (void)setTheme:(MWMTheme)theme
{
  if ([self theme] == theme)
    return;
  auto ud = NSUserDefaults.standardUserDefaults;
  [ud setInteger:theme forKey:kThemeMode];
  BOOL const autoOff = theme != MWMThemeAuto;
  [ud setBool:autoOff forKey:kUDAutoNightModeOff];
  [MWMThemeManager invalidate];
}

+ (BOOL)routingDisclaimerApproved
{
  bool enabled = false;
  UNUSED_VALUE(settings::Get(kRoutingDisclaimerApprovedKey, enabled));
  return enabled;
}

+ (void)setRoutingDisclaimerApproved { settings::Set(kRoutingDisclaimerApprovedKey, true); }
+ (NSString *)spotlightLocaleLanguageId
{
  return [NSUserDefaults.standardUserDefaults stringForKey:kSpotlightLocaleLanguageId];
}

+ (void)setSpotlightLocaleLanguageId:(NSString *)spotlightLocaleLanguageId
{
  NSUserDefaults * ud = NSUserDefaults.standardUserDefaults;
  [ud setObject:spotlightLocaleLanguageId forKey:kSpotlightLocaleLanguageId];
}

+ (BOOL)largeFontSize { return GetFramework().LoadLargeFontsSize(); }
+ (void)setLargeFontSize:(BOOL)largeFontSize
{
  GetFramework().SetLargeFontsSize(static_cast<bool>(largeFontSize));
}

+ (BOOL)transliteration { return GetFramework().LoadTransliteration(); }
+ (void)setTransliteration:(BOOL)transliteration
{
  bool const isTransliteration = static_cast<bool>(transliteration);
  auto & f = GetFramework();
  f.SaveTransliteration(isTransliteration);
  f.AllowTransliteration(isTransliteration);
}

+ (BOOL)isTrackWarningAlertShown
{
  return [NSUserDefaults.standardUserDefaults boolForKey:kUDTrackWarningAlertWasShown];
}

+ (void)setTrackWarningAlertShown:(BOOL)shown
{
  NSUserDefaults * ud = NSUserDefaults.standardUserDefaults;
  [ud setBool:shown forKey:kUDTrackWarningAlertWasShown];
}

+ (NSString *)donateUrl
{
  std::string url;
  return settings::Get(settings::kDonateUrl, url) ? @(url.c_str()) : nil;
}

+ (BOOL)isNY
{
  bool isNY;
  return settings::Get("NY", isNY) ? isNY : false;
}

+ (BOOL)iCLoudSynchronizationEnabled
{
  return [NSUserDefaults.standardUserDefaults boolForKey:kiCLoudSynchronizationEnabledKey];
}

+ (void)setICLoudSynchronizationEnabled:(BOOL)iCLoudSyncEnabled
{
  [NSUserDefaults.standardUserDefaults setBool:iCLoudSyncEnabled forKey:kiCLoudSynchronizationEnabledKey];
  [NSNotificationCenter.defaultCenter postNotificationName:NSNotification.iCloudSynchronizationDidChangeEnabledState object:nil];
}

+ (void)initializeLogging {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    [self setFileLoggingEnabled:[self isFileLoggingEnabled]];
  });
}

+ (BOOL)isFileLoggingEnabled {
  return [NSUserDefaults.standardUserDefaults boolForKey:kUDFileLoggingEnabledKey];
}

+ (void)setFileLoggingEnabled:(BOOL)fileLoggingEnabled {
  [NSUserDefaults.standardUserDefaults setBool:fileLoggingEnabled forKey:kUDFileLoggingEnabledKey];
  [Logger setFileLoggingEnabled:fileLoggingEnabled];
}

@end
