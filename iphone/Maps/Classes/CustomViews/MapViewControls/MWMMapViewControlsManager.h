#import "MWMBottomMenuState.h"
#import "MWMMapDownloaderMode.h"
#import "MWMNavigationDashboardManager.h"
#import "MWMSearchManager.h"

@class MapViewController;
@class BottomTabBarViewController;
@class TrackRecordingViewController;
@protocol MWMFeatureHolder;

@interface MWMMapViewControlsManager : NSObject

+ (MWMMapViewControlsManager *)manager NS_SWIFT_NAME(manager());

@property(nonatomic) BOOL hidden;
@property(nonatomic) BOOL zoomHidden;
@property(nonatomic) BOOL sideButtonsHidden;
@property(nonatomic) BOOL trafficButtonHidden;
@property(nonatomic) MWMBottomMenuState menuState;
@property(nonatomic) MWMBottomMenuState menuRestoreState;
@property(nonatomic) BOOL isDirectionViewHidden;
@property(nonatomic) BottomTabBarViewController * tabBarController;
@property(nonatomic) TrackRecordingViewController * trackRecordingButton;

- (instancetype)init __attribute__((unavailable("init is not available")));
- (instancetype)initWithParentController:(MapViewController *)controller;

- (UIStatusBarStyle)preferredStatusBarStyle;

#pragma mark - Layout

- (UIView *)anchorView;

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator;

#pragma mark - MWMNavigationDashboardManager

- (void)onRoutePrepare;
- (void)onRouteRebuild;
- (void)onRouteReady:(BOOL)hasWarnings;
- (void)onRouteStart;
- (void)onRouteStop;

#pragma mark - MWMSearchManager

- (void)actionDownloadMaps:(MWMMapDownloaderMode)mode;
- (BOOL)searchText:(NSString *)text forInputLocale:(NSString *)locale;
- (void)searchTextOnMap:(NSString *)text forInputLocale:(NSString *)locale;
- (void)hideSearch;

#pragma mark - MWMFeatureHolder

- (id<MWMFeatureHolder>)featureHolder;

@end
