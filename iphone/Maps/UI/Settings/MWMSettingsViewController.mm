#import "MWMSettingsViewController.h"
#import "MWMAuthorizationCommon.h"
#import "MWMTextToSpeech+CPP.h"
#import "SwiftBridge.h"
#import "MWMActivityViewController.h"

#import <CoreApi/Framework.h>

#include "map/gps_tracker.hpp"

using namespace power_management;

static NSString * const kUDDidShowICloudSynchronizationEnablingAlert = @"kUDDidShowICloudSynchronizationEnablingAlert";

@interface MWMSettingsViewController ()<SettingsTableViewSwitchCellDelegate>

@property(weak, nonatomic) IBOutlet SettingsTableViewLinkCell *profileCell;

@property(weak, nonatomic) IBOutlet SettingsTableViewLinkCell *unitsCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewSwitchCell *zoomButtonsCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewSwitchCell *is3dCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewSwitchCell *autoDownloadCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewLinkCell *mobileInternetCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewLinkCell *powerManagementCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewSwitchCell *fontScaleCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewSwitchCell *transliterationCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewSwitchCell *compassCalibrationCell;

@property(weak, nonatomic) IBOutlet SettingsTableViewLinkCell *nightModeCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewSwitchCell *perspectiveViewCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewSwitchCell *autoZoomCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewLinkCell *voiceInstructionsCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewLinkCell *drivingOptionsCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewiCloudSwitchCell *iCloudSynchronizationCell;
@property(weak, nonatomic) IBOutlet SettingsTableViewDetailedSwitchCell *enableLoggingCell;

@end

@implementation MWMSettingsViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = L(@"settings");
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self configCells];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self highlightFeatureIfNeeded];
}

- (void)highlightFeatureIfNeeded {
  UITableViewCell * cell = nil;
  DeepLinkInAppFeatureHighlightData * featureToHighlight = [DeepLinkHandler.shared getInAppFeatureHighlightData];
  if (!featureToHighlight || featureToHighlight.urlType != DeeplinkUrlTypeSettings)
    return;
  switch (featureToHighlight.feature) {
    case InAppFeatureHighlightTypeNone:
    case InAppFeatureHighlightTypeTrackRecorder:
      // Еhere is no options for the track recorder yet.
      break;
    case InAppFeatureHighlightTypeICloud:
      cell = self.iCloudSynchronizationCell;
      break;
  }
  NSIndexPath * indexPath = [self.tableView indexPathForCell:cell];
  if (!cell || !indexPath)
    return;
  [self.tableView scrollToRowAtIndexPath:indexPath atScrollPosition:UITableViewScrollPositionMiddle animated:YES];
  [cell highlight];
}

- (void)configCells {
  [self configProfileSection];
  [self configCommonSection];
  [self configNavigationSection];
}

- (void)configProfileSection {
  NSString * userName = osm_auth_ios::OSMUserName();
  [self.profileCell configWithTitle:L(@"profile") info:userName.length != 0 ? userName : @""];
}

- (void)configCommonSection {
  NSString * units = nil;
  switch ([MWMSettings measurementUnits]) {
    case MWMUnitsMetric:
      units = L(@"kilometres");
      break;
    case MWMUnitsImperial:
      units = L(@"miles");
      break;
  }
  [self.unitsCell configWithTitle:L(@"measurement_units") info:units];

  [self.zoomButtonsCell configWithDelegate:self title:L(@"pref_zoom_title") isOn:[MWMSettings zoomButtonsEnabled]];

  bool on = true, _ = true;
  GetFramework().Load3dMode(_, on);
  if (GetFramework().GetPowerManager().GetScheme() == Scheme::EconomyMaximum)
  {
    self.is3dCell.isEnabled = false;
    [self.is3dCell configWithDelegate:self title:L(@"pref_map_3d_buildings_title") isOn:false];
    UITapGestureRecognizer * tapRecogniser = [[UITapGestureRecognizer alloc] initWithTarget:self
                                                                                     action:@selector(show3dBuildingsAlert:)];

    self.is3dCell.gestureRecognizers = @[tapRecogniser];
  }
  else
  {
    self.is3dCell.isEnabled = true;
    [self.is3dCell configWithDelegate:self title:L(@"pref_map_3d_buildings_title") isOn:on];
    self.is3dCell.gestureRecognizers = nil;
  }

  [self.autoDownloadCell configWithDelegate:self
                                      title:L(@"autodownload")
                                       isOn:[MWMSettings autoDownloadEnabled]];

  NSString * mobileInternet = nil;
  switch ([MWMNetworkPolicy sharedPolicy].permission) {
    case MWMNetworkPolicyPermissionAlways:
      mobileInternet = L(@"mobile_data_option_always");
      break;
    case MWMNetworkPolicyPermissionNever:
      mobileInternet = L(@"mobile_data_option_never");
      break;
    case MWMNetworkPolicyPermissionToday:
    case MWMNetworkPolicyPermissionNotToday:
    case MWMNetworkPolicyPermissionAsk:
      mobileInternet = L(@"mobile_data_option_ask");
      break;
  }
  [self.mobileInternetCell configWithTitle:L(@"mobile_data") info:mobileInternet];

  NSString * powerManagement = nil;
  switch (GetFramework().GetPowerManager().GetScheme()) {
    case Scheme::None:
      break;
    case Scheme::Normal:
      powerManagement = L(@"power_managment_setting_never");
      break;
    case Scheme::EconomyMedium:
      break;
    case Scheme::EconomyMaximum:
      powerManagement = L(@"power_managment_setting_manual_max");
      break;
    case Scheme::Auto:
      powerManagement = L(@"power_managment_setting_auto");
      break;
  }
  [self.powerManagementCell configWithTitle:L(@"power_managment_title") info:powerManagement];

  [self.fontScaleCell configWithDelegate:self title:L(@"big_font") isOn:[MWMSettings largeFontSize]];

  [self.transliterationCell configWithDelegate:self
                                         title:L(@"transliteration_title")
                                          isOn:[MWMSettings transliteration]];

  [self.compassCalibrationCell configWithDelegate:self
                                            title:L(@"pref_calibration_title")
                                             isOn:[MWMSettings compassCalibrationEnabled]];

  NSString * nightMode = nil;
  switch ([MWMSettings theme]) {
    case MWMThemeVehicleDay:
      NSAssert(false, @"Invalid case");
    case MWMThemeDay:
      nightMode = L(@"pref_appearance_light");
      break;
    case MWMThemeVehicleNight:
      NSAssert(false, @"Invalid case");
    case MWMThemeNight:
      nightMode = L(@"pref_appearance_dark");
      break;
    case MWMThemeAuto:
      nightMode = L(@"auto");
      break;
  }
  [self.nightModeCell configWithTitle:L(@"pref_appearance_title") info:nightMode];

  [self.iCloudSynchronizationCell configWithDelegate:self
                                               title:@"iCloud Synchronization (Beta)"
                                                isOn:[MWMSettings iCLoudSynchronizationEnabled]];

  __weak __typeof(self) weakSelf = self;
  [iCloudSynchronizaionManager.shared addObserver:self synchronizationStateDidChangeHandler:^(SynchronizationManagerState * state) {
    __strong auto strongSelf = weakSelf;
    [strongSelf.iCloudSynchronizationCell updateWithSynchronizationState:state];
  }];

  [self.enableLoggingCell configWithDelegate:self title:L(@"enable_logging") isOn:MWMSettings.isFileLoggingEnabled];
  [self updateLogFileSize];
}

- (void)updateLogFileSize {
  uint64_t logFileSize = [Logger getLogFileSize];
  NSString * detailString = logFileSize == 0 ? nil : [NSString stringWithFormat:L(@"log_file_size"), formattedSize(logFileSize)];
  [self.enableLoggingCell setDetail:detailString];
}

- (void)show3dBuildingsAlert:(UITapGestureRecognizer *)recognizer {
  UIAlertController * alert =
    [UIAlertController alertControllerWithTitle:L(@"pref_map_3d_buildings_title")
                                      message:L(@"pref_map_3d_buildings_disabled_summary")
                               preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction * okButton = [UIAlertAction actionWithTitle:@"OK"
                                                      style:UIAlertActionStyleDefault
                                                    handler:nil];
  [alert addAction:okButton];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)configNavigationSection {
  bool _ = true, on = true;
  auto &f = GetFramework();
  f.Load3dMode(on, _);
  [self.perspectiveViewCell configWithDelegate:self title:L(@"pref_map_3d_title") isOn:on];

  [self.autoZoomCell configWithDelegate:self title:L(@"pref_map_auto_zoom") isOn:GetFramework().LoadAutoZoom()];

  NSString * ttsEnabledString = [MWMTextToSpeech isTTSEnabled] ? L(@"on") : L(@"off");
  [self.voiceInstructionsCell configWithTitle:L(@"pref_tts_enable_title") info:ttsEnabledString];
  [self.drivingOptionsCell configWithTitle:L(@"driving_options_title") info:@""];
}

- (void)showICloudSynchronizationEnablingAlert:(void (^)(BOOL))isEnabled {
  UIAlertController * alertController = [UIAlertController alertControllerWithTitle:L(@"enable_icloud_synchronization_title")
                                                                           message:L(@"enable_icloud_synchronization_message")
                                                                    preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction * enableButton = [UIAlertAction actionWithTitle:L(@"enable")
                                                     style:UIAlertActionStyleDefault
                                                   handler:^(UIAlertAction * action) {
    [self setICloudSynchronizationEnablingAlertIsShown];
    isEnabled(YES);
  }];
  UIAlertAction * backupButton = [UIAlertAction actionWithTitle:L(@"backup")
                                                         style:UIAlertActionStyleDefault
                                                       handler:^(UIAlertAction * action) {
    [MWMBookmarksManager.sharedManager shareAllCategoriesWithCompletion:^(MWMBookmarksShareStatus status, NSURL * _Nonnull url) {
      switch (status) {
        case MWMBookmarksShareStatusSuccess: {
          MWMActivityViewController * shareController = [MWMActivityViewController shareControllerForURL:url message:L(@"share_bookmarks_email_body") completionHandler:^(UIActivityType  _Nullable activityType, BOOL completed, NSArray * _Nullable returnedItems, NSError * _Nullable activityError) {
            [self setICloudSynchronizationEnablingAlertIsShown];
            isEnabled(completed);
          }];
          [shareController presentInParentViewController:self anchorView:self.iCloudSynchronizationCell];
          break;
        }
        case MWMBookmarksShareStatusEmptyCategory:
          [[MWMToast toastWithText:L(@"bookmarks_error_title_share_empty")] show];
          isEnabled(NO);
          break;
        case MWMBookmarksShareStatusArchiveError:
        case MWMBookmarksShareStatusFileError:
          [[MWMToast toastWithText:L(@"dialog_routing_system_error")] show];
          isEnabled(NO);
          break;
      }
    }];
  }];
  UIAlertAction * cancelButton = [UIAlertAction actionWithTitle:L(@"cancel")
                                                         style:UIAlertActionStyleCancel
                                                       handler:^(UIAlertAction * action) {
    isEnabled(NO);
  }];

  [alertController addAction:cancelButton];
  if (![MWMBookmarksManager.sharedManager areAllCategoriesEmpty])
    [alertController addAction:backupButton];
  [alertController addAction:enableButton];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)setICloudSynchronizationEnablingAlertIsShown {
  [NSUserDefaults.standardUserDefaults setBool:YES forKey:kUDDidShowICloudSynchronizationEnablingAlert];
}

- (void)showICloudIsDisabledAlert {
  UIAlertController * alertController = [UIAlertController alertControllerWithTitle:L(@"icloud_disabled_title")
                                                                            message:L(@"icloud_disabled_message")
                                                                     preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction * okButton = [UIAlertAction actionWithTitle:L(@"ok")
                                                      style:UIAlertActionStyleCancel
                                                    handler:nil];
  [alertController addAction:okButton];
  [self presentViewController:alertController animated:YES completion:nil];
}

#pragma mark - SettingsTableViewSwitchCellDelegate

- (void)switchCell:(SettingsTableViewSwitchCell *)cell didChangeValue:(BOOL)value {
  if (cell == self.zoomButtonsCell) {
    [MWMSettings setZoomButtonsEnabled:value];
  } else if (cell == self.is3dCell) {
    auto &f = GetFramework();
    bool _ = true, is3dBuildings = true;
    f.Load3dMode(_, is3dBuildings);
    is3dBuildings = static_cast<bool>(value);
    f.Save3dMode(_, is3dBuildings);
    f.Allow3dMode(_, is3dBuildings);
  } else if (cell == self.autoDownloadCell) {
    [MWMSettings setAutoDownloadEnabled:value];
  } else if (cell == self.fontScaleCell) {
    [MWMSettings setLargeFontSize:value];
  } else if (cell == self.transliterationCell) {
    [MWMSettings setTransliteration:value];
  } else if (cell == self.compassCalibrationCell) {
    [MWMSettings setCompassCalibrationEnabled:value];
  } else if (cell == self.perspectiveViewCell) {
    auto &f = GetFramework();
    bool _ = true, is3d = true;
    f.Load3dMode(is3d, _);
    is3d = static_cast<bool>(value);
    f.Save3dMode(is3d, _);
    f.Allow3dMode(is3d, _);
  } else if (cell == self.autoZoomCell) {
    auto &f = GetFramework();
    f.AllowAutoZoom(value);
    f.SaveAutoZoom(value);
  } else if (cell == self.iCloudSynchronizationCell) {
    if (![NSUserDefaults.standardUserDefaults boolForKey:kUDDidShowICloudSynchronizationEnablingAlert]) {
      [self showICloudSynchronizationEnablingAlert:^(BOOL isEnabled) {
        [MWMSettings setICLoudSynchronizationEnabled:isEnabled];
      }];
    } else {
      [MWMSettings setICLoudSynchronizationEnabled:value];
    }
  } else if (cell == self.enableLoggingCell) {
    [MWMSettings setFileLoggingEnabled:value];
    [self updateLogFileSize];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:true];
  auto cell = [tableView cellForRowAtIndexPath:indexPath];
  if (cell == self.profileCell) {
    [self performSegueWithIdentifier:@"SettingsToProfileSegue" sender:nil];
  } else if (cell == self.unitsCell) {
    [self performSegueWithIdentifier:@"SettingsToUnits" sender:nil];
  } else if (cell == self.mobileInternetCell) {
    [self performSegueWithIdentifier:@"SettingsToMobileInternetSegue" sender:nil];
  } else if (cell == self.powerManagementCell) {
    [self performSegueWithIdentifier:@"SettingsToPowerManagementSegue" sender:nil];
  } else if (cell == self.nightModeCell) {
    [self performSegueWithIdentifier:@"SettingsToNightMode" sender:nil];
  } else if (cell == self.voiceInstructionsCell) {
    [self performSegueWithIdentifier:@"SettingsToTTSSegue" sender:nil];
  } else if (cell == self.drivingOptionsCell) {
    [self performSegueWithIdentifier:@"settingsToDrivingOptionsSegue" sender:nil];
  }
}

- (void)tableView:(UITableView *)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath *)indexPath {
  auto cell = [tableView cellForRowAtIndexPath:indexPath];
  if (cell == self.iCloudSynchronizationCell) {
    [self showICloudIsDisabledAlert];
  }
}

#pragma mark - UITableViewDataSource

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
  switch (section) {
    case 1:
      return L(@"general_settings");
    case 2:
      return L(@"prefs_group_route");
    case 3:
      return L(@"info");
    default:
      return nil;
  }
}

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
  switch (section) {
    case 1:
      return L(@"enable_logging_warning_message");
    default:
      return nil;
  }
}

@end
