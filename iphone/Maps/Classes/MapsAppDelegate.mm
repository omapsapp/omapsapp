#import "MapsAppDelegate.h"

#import "EAGLView.h"
#import "MWMAuthorizationCommon.h"
#import "MWMCoreRouterType.h"
#import "MWMFrameworkListener.h"
#import "MWMFrameworkObservers.h"
#import "MWMMapViewControlsManager.h"
#import "MWMRoutePoint+CPP.h"
#import "MWMRouter.h"
#import "MWMSearch+CoreSpotlight.h"
#import "MWMTextToSpeech.h"
#import "MapViewController.h"
#import "NSDate+TimeDistance.h"
#import "SwiftBridge.h"


#import <CarPlay/CarPlay.h>
#import <CoreSpotlight/CoreSpotlight.h>
#import <CoreTelephony/CTTelephonyNetworkInfo.h>
#import <UserNotifications/UserNotifications.h>

#import <CoreApi/Framework.h>
#import <CoreApi/MWMFrameworkHelper.h>

#include "map/gps_tracker.hpp"

#include "platform/background_downloader_ios.h"
#include "platform/http_thread_apple.h"
#include "platform/local_country_file_utils.hpp"

#include "base/assert.hpp"

#include "private.h"
// If you have a "missing header error" here, then please run configure.sh script in the root repo
// folder.

namespace {
NSString *const kUDLastLaunchDateKey = @"LastLaunchDate";
NSString *const kUDSessionsCountKey = @"SessionsCount";
NSString *const kUDFirstVersionKey = @"FirstVersion";
NSString *const kUDLastShareRequstDate = @"LastShareRequestDate";
NSString *const kUDAutoNightModeOff = @"AutoNightModeOff";
NSString *const kIOSIDFA = @"IFA";
NSString *const kBundleVersion = @"BundleVersion";

/// Adds needed localized strings to C++ code
/// @TODO Refactor localization mechanism to make it simpler
void InitLocalizedStrings() {
  Framework &f = GetFramework();

  f.AddString("core_entrance", L(@"core_entrance").UTF8String);
  f.AddString("core_exit", L(@"core_exit").UTF8String);
  f.AddString("core_my_places", L(@"core_my_places").UTF8String);
  f.AddString("core_my_position", L(@"core_my_position").UTF8String);
  f.AddString("core_placepage_unknown_place", L(@"core_placepage_unknown_place").UTF8String);
  f.AddString("postal_code", L(@"postal_code").UTF8String);
}
}  // namespace

using namespace osm_auth_ios;

@interface MapsAppDelegate () <MWMStorageObserver,
                               CPApplicationDelegate>

@property(nonatomic) NSInteger standbyCounter;
@property(nonatomic) MWMBackgroundFetchScheduler *backgroundFetchScheduler;

@end

@implementation MapsAppDelegate

+ (MapsAppDelegate *)theApp {
  return (MapsAppDelegate *)UIApplication.sharedApplication.delegate;
}

- (BOOL)isDrapeEngineCreated {
  return self.mapViewController.mapView.drapeEngineCreated;
}

- (void)searchText:(NSString *)searchString {
  if (!self.isDrapeEngineCreated) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self searchText:searchString];
    });
    return;
  }

  [[MWMMapViewControlsManager manager] searchText:[searchString stringByAppendingString:@" "]
                                   forInputLocale:[MWMSettings spotlightLocaleLanguageId]];
}

- (void)commonInit {
  [HttpThreadImpl setDownloadIndicatorProtocol:self];
  InitLocalizedStrings();
  GetFramework().SetupMeasurementSystem();
  [[MWMStorage sharedStorage] addObserver:self];
  [MapsAppDelegate customizeAppearance];

  self.standbyCounter = 0;
  NSTimeInterval const minimumBackgroundFetchIntervalInSeconds = 6 * 60 * 60;
  [UIApplication.sharedApplication setMinimumBackgroundFetchInterval:minimumBackgroundFetchIntervalInSeconds];
  [self updateApplicationIconBadgeNumber];
  [TrackRecordingManager.shared setup];
}

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  NSLog(@"application:didFinishLaunchingWithOptions: %@", launchOptions);

  [HttpThreadImpl setDownloadIndicatorProtocol:self];

  InitLocalizedStrings();
  [MWMThemeManager invalidate];

  [self commonInit];

  if ([FirstSession isFirstSession]) {
    [self firstLaunchSetup];
  } else {
    [self incrementSessionCount];
  }
  [self enableTTSForTheFirstTime];

  if (![MapsAppDelegate isTestsEnvironment])
    [[iCloudSynchronizaionManager shared] start];
  
  [[DeepLinkHandler shared] applicationDidFinishLaunching:launchOptions];
  // application:openUrl:options is called later for deep links if YES is returned.
  return YES;
}

- (void)application:(UIApplication *)application
  performActionForShortcutItem:(UIApplicationShortcutItem *)shortcutItem
             completionHandler:(void (^)(BOOL))completionHandler {
  [self.mapViewController performAction:shortcutItem.type];
  completionHandler(YES);
}

- (void)runBackgroundTasks:(NSArray<BackgroundFetchTask *> *_Nonnull)tasks
         completionHandler:(void (^_Nullable)(UIBackgroundFetchResult))completionHandler {
  self.backgroundFetchScheduler = [[MWMBackgroundFetchScheduler alloc] initWithTasks:tasks
                                                                   completionHandler:^(UIBackgroundFetchResult result) {
                                                                     if (completionHandler)
                                                                       completionHandler(result);
                                                                   }];
  [self.backgroundFetchScheduler run];
}

- (void)applicationWillTerminate:(UIApplication *)application {
  [self.mapViewController onTerminate];
  // Global cleanup
  DeleteFramework();
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
  LOG(LINFO, ("applicationDidEnterBackground - begin"));
  [DeepLinkHandler.shared reset];
  if (m_activeDownloadsCounter) {
    m_backgroundTask = [application beginBackgroundTaskWithExpirationHandler:^{
      [application endBackgroundTask:self->m_backgroundTask];
      self->m_backgroundTask = UIBackgroundTaskInvalid;
    }];
  }

  auto tasks = @[[[MWMBackgroundEditsUpload alloc] init]];
  [self runBackgroundTasks:tasks completionHandler:nil];

  [MWMRouter saveRouteIfNeeded];
  LOG(LINFO, ("applicationDidEnterBackground - end"));
}

- (void)applicationWillResignActive:(UIApplication *)application {
  LOG(LINFO, ("applicationWillResignActive - begin"));
  [self.mapViewController onGetFocus:NO];
  auto &f = GetFramework();
  // On some devices we have to free all belong-to-graphics memory
  // because of new OpenGL driver powered by Metal.
  if ([AppInfo sharedInfo].openGLDriver == MWMOpenGLDriverMetalPre103) {
    f.SetRenderingDisabled(true);
    f.OnDestroySurface();
  } else {
    f.SetRenderingDisabled(false);
  }
  [MWMLocationManager applicationWillResignActive];
  f.EnterBackground();
  LOG(LINFO, ("applicationWillResignActive - end"));
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
  LOG(LINFO, ("applicationWillEnterForeground - begin"));
  if (!GpsTracker::Instance().IsEnabled())
    return;

  MWMViewController *topVc =
    static_cast<MWMViewController *>(self.mapViewController.navigationController.topViewController);
  if (![topVc isKindOfClass:[MWMViewController class]])
    return;

  if ([MWMSettings isTrackWarningAlertShown])
    return;

  [topVc.alertController presentTrackWarningAlertWithCancelBlock:^{
    GpsTracker::Instance().SetEnabled(false);
  }];

  [MWMSettings setTrackWarningAlertShown:YES];
  LOG(LINFO, ("applicationWillEnterForeground - end"));
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
  LOG(LINFO, ("applicationDidBecomeActive - begin"));

  auto & f = GetFramework();
  f.EnterForeground();
  [self.mapViewController onGetFocus:YES];
  f.SetRenderingEnabled();
  // On some devices we have to free all belong-to-graphics memory
  // because of new OpenGL driver powered by Metal.
  if ([AppInfo sharedInfo].openGLDriver == MWMOpenGLDriverMetalPre103) {
    CGSize const objcSize = self.mapViewController.mapView.pixelSize;
    f.OnRecoverSurface(static_cast<int>(objcSize.width), static_cast<int>(objcSize.height),
                       true /* recreateContextDependentResources */);
  }
  [MWMLocationManager applicationDidBecomeActive];
  [MWMSearch addCategoriesToSpotlight];
  [MWMKeyboard applicationDidBecomeActive];
  [MWMTextToSpeech applicationDidBecomeActive];
  LOG(LINFO, ("applicationDidBecomeActive - end"));
}

// TODO: Drape enabling and iCloud sync are skipped during the test run due to the app crashing in teardown. This is a temporary solution. Drape should be properly disabled instead of merely skipping the enabling process.
+ (BOOL)isTestsEnvironment {
  NSProcessInfo * processInfo = [NSProcessInfo processInfo];
  NSArray<NSString *> * launchArguments = [processInfo arguments];
  BOOL isTests = [launchArguments containsObject:@"-IsTests"];
  return isTests;
}

- (BOOL)application:(UIApplication *)application
  continueUserActivity:(NSUserActivity *)userActivity
    restorationHandler:(void (^)(NSArray<id<UIUserActivityRestoring>> *_Nullable))restorationHandler {
  if ([userActivity.activityType isEqualToString:CSSearchableItemActionType]) {
    NSString *searchStringKey = userActivity.userInfo[CSSearchableItemActivityIdentifier];
    NSString *searchString = L(searchStringKey);
    if (searchString) {
      [self searchText:searchString];
      return YES;
    }
  } else if ([userActivity.activityType isEqualToString:NSUserActivityTypeBrowsingWeb] &&
             userActivity.webpageURL != nil) {
    LOG(LINFO, ("application:continueUserActivity:restorationHandler: %@", userActivity.webpageURL));
    return [DeepLinkHandler.shared applicationDidReceiveUniversalLink:userActivity.webpageURL];
  }

  return NO;
}

- (void)disableDownloadIndicator {
  --m_activeDownloadsCounter;
  if (m_activeDownloadsCounter <= 0) {
    dispatch_async(dispatch_get_main_queue(), ^{
      UIApplication.sharedApplication.networkActivityIndicatorVisible = NO;
    });
    m_activeDownloadsCounter = 0;
    if (UIApplication.sharedApplication.applicationState == UIApplicationStateBackground) {
      [UIApplication.sharedApplication endBackgroundTask:m_backgroundTask];
      m_backgroundTask = UIBackgroundTaskInvalid;
    }
  }
}

- (void)enableDownloadIndicator {
  ++m_activeDownloadsCounter;
  dispatch_async(dispatch_get_main_queue(), ^{
    UIApplication.sharedApplication.networkActivityIndicatorVisible = YES;
  });
}

+ (void)customizeAppearanceForNavigationBar:(UINavigationBar *)navigationBar {
  auto backImage =
    [[UIImage imageNamed:@"ic_nav_bar_back_sys"] imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
  navigationBar.backIndicatorImage = backImage;
  navigationBar.backIndicatorTransitionMaskImage = backImage;
}

+ (void)customizeAppearance {
  [UIButton appearance].exclusiveTouch = YES;

  [self customizeAppearanceForNavigationBar:[UINavigationBar appearance]];

  UITextField *textField = [UITextField appearance];
  textField.keyboardAppearance = [UIColor isNightMode] ? UIKeyboardAppearanceDark : UIKeyboardAppearanceDefault;
}

- (BOOL)application:(UIApplication *)app
            openURL:(NSURL *)url
            options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options {
  NSLog(@"application:openURL: %@ options: %@", url, options);
  return [DeepLinkHandler.shared applicationDidOpenUrl:url];
}

- (void)showMap {
  [(UINavigationController *)self.window.rootViewController popToRootViewControllerAnimated:YES];
}

- (void)updateApplicationIconBadgeNumber {
  auto const number = [self badgeNumber];

  // Delay init because BottomTabBarViewController.controller is null here.
  dispatch_async(dispatch_get_main_queue(), ^{
    [UIApplication.sharedApplication setApplicationIconBadgeNumber:number];
    BottomTabBarViewController.controller.isApplicationBadgeHidden = (number == 0);
  });
}

- (NSUInteger)badgeNumber {
  auto &s = GetFramework().GetStorage();
  storage::Storage::UpdateInfo updateInfo{};
  s.GetUpdateInfo(s.GetRootId(), updateInfo);
  return updateInfo.m_numberOfMwmFilesToUpdate;
}

- (void)application:(UIApplication *)application
  handleEventsForBackgroundURLSession:(NSString *)identifier
                    completionHandler:(void (^)())completionHandler {
  [BackgroundDownloader sharedBackgroundMapDownloader].backgroundCompletionHandler = completionHandler;
}

#pragma mark - MWMStorageObserver

- (void)processCountryEvent:(NSString *)countryId {
  // Dispatch this method after delay since there are too many events for group mwms download.
  // We do not need to update badge frequently.
  // Update after 1 second delay (after last country event) is sure enough for app badge.
  SEL const updateBadge = @selector(updateApplicationIconBadgeNumber);
  [NSObject cancelPreviousPerformRequestsWithTarget:self selector:updateBadge object:nil];
  [self performSelector:updateBadge withObject:nil afterDelay:1.0];
}

#pragma mark - Properties

- (MapViewController *)mapViewController {
  for (id vc in [(UINavigationController *)self.window.rootViewController viewControllers]) {
    if ([vc isKindOfClass:[MapViewController class]])
      return vc;
  }
  NSAssert(false, @"Please check the logic");
  return nil;
}

- (MWMCarPlayService *)carplayService {
  return [MWMCarPlayService shared];
}

#pragma mark - TTS

- (void)enableTTSForTheFirstTime {
  if (![MWMTextToSpeech savedLanguage].length)
    [MWMTextToSpeech setTTSEnabled:YES];
}

#pragma mark - Standby

- (void)enableStandby {
  self.standbyCounter--;
}
- (void)disableStandby {
  self.standbyCounter++;
}
- (void)setStandbyCounter:(NSInteger)standbyCounter {
  _standbyCounter = MAX(0, standbyCounter);
  dispatch_async(dispatch_get_main_queue(), ^{
    [UIApplication sharedApplication].idleTimerDisabled = (self.standbyCounter != 0);
  });
}

#pragma mark - Alert logic

- (void)firstLaunchSetup {
  NSString *currentVersion = [NSBundle.mainBundle objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey];
  NSUserDefaults *standartDefaults = NSUserDefaults.standardUserDefaults;
  [standartDefaults setObject:currentVersion forKey:kUDFirstVersionKey];
  [standartDefaults setInteger:1 forKey:kUDSessionsCountKey];
  [standartDefaults setObject:NSDate.date forKey:kUDLastLaunchDateKey];
}

- (void)incrementSessionCount {
  NSUserDefaults *standartDefaults = NSUserDefaults.standardUserDefaults;
  NSUInteger sessionCount = [standartDefaults integerForKey:kUDSessionsCountKey];
  NSUInteger const kMaximumSessionCountForShowingShareAlert = 50;
  if (sessionCount > kMaximumSessionCountForShowingShareAlert)
    return;

  NSDate *lastLaunchDate = [standartDefaults objectForKey:kUDLastLaunchDateKey];
  if (lastLaunchDate.daysToNow > 0) {
    sessionCount++;
    [standartDefaults setInteger:sessionCount forKey:kUDSessionsCountKey];
    [standartDefaults setObject:NSDate.date forKey:kUDLastLaunchDateKey];
  }
}

#pragma mark - Rate

- (BOOL)userIsNew {
  NSString *currentVersion = [NSBundle.mainBundle objectForInfoDictionaryKey:(NSString *)kCFBundleVersionKey];
  NSString *firstVersion = [NSUserDefaults.standardUserDefaults stringForKey:kUDFirstVersionKey];
  if (!firstVersion.length || firstVersionIsLessThanSecond(firstVersion, currentVersion))
    return NO;

  return YES;
}

#pragma mark - CPApplicationDelegate implementation

- (void)application:(UIApplication *)application
  didConnectCarInterfaceController:(CPInterfaceController *)interfaceController
           toWindow:(CPWindow *)window API_AVAILABLE(ios(12.0)) {
  [self.carplayService setupWithWindow:window interfaceController:interfaceController];
}

- (void)application:(UIApplication *)application
  didDisconnectCarInterfaceController:(CPInterfaceController *)interfaceController
                           fromWindow:(CPWindow *)window API_AVAILABLE(ios(12.0)) {
  [self.carplayService destroy];
}

@end
