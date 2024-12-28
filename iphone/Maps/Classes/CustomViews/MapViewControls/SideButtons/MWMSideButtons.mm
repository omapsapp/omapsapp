#import "MWMSideButtons.h"
#import "MWMButton.h"
#import "MWMLocationManager.h"
#import "MWMMapViewControlsManager.h"
#import "MWMRouter.h"
#import "MWMSettings.h"
#import "MWMSideButtonsView.h"
#import "SwiftBridge.h"

#include <CoreApi/Framework.h>

namespace
{
NSString * const kMWMSideButtonsViewNibName = @"MWMSideButtonsView";
NSString * const kUDDidShowLongTapToShowSideButtonsToast = @"kUDDidShowLongTapToShowSideButtonsToast";
}  // namespace

@interface MWMMapViewControlsManager ()

@property(nonatomic) MWMSideButtons * sideButtons;

@end

@interface MWMSideButtons ()

@property(nonatomic) IBOutlet MWMSideButtonsView * sideView;
@property(weak, nonatomic) IBOutlet MWMButton * zoomInButton;
@property(weak, nonatomic) IBOutlet MWMButton * zoomOutButton;
@property(weak, nonatomic) IBOutlet MWMButton * locationButton;

@property(nonatomic) BOOL zoomSwipeEnabled;
@property(nonatomic, readonly) BOOL isZoomEnabled;

@property(nonatomic) MWMMyPositionMode locationMode;

@end

@implementation MWMSideButtons

- (UIView *)view {
  return self.sideView;
}

+ (MWMSideButtons *)buttons { return [MWMMapViewControlsManager manager].sideButtons; }
- (instancetype)initWithParentView:(UIView *)view
{
  self = [super init];
  if (self)
  {
    [NSBundle.mainBundle loadNibNamed:kMWMSideButtonsViewNibName owner:self options:nil];
    [view addSubview:self.sideView];
    [self.sideView setNeedsLayout];
    self.zoomSwipeEnabled = NO;
    self.zoomHidden = NO;
  }
  return self;
}

+ (void)updateAvailableArea:(CGRect)frame { [[self buttons].sideView updateAvailableArea:frame]; }
- (void)zoomIn
{
  GetFramework().Scale(Framework::SCALE_MAG, true);
}

- (void)zoomOut
{
  GetFramework().Scale(Framework::SCALE_MIN, true);
}

- (void)processMyPositionStateModeEvent:(MWMMyPositionMode)mode
{
  [self refreshLocationButtonState:mode];
  self.locationMode = mode;
}

#pragma mark - Location button

- (void)refreshLocationButtonState:(MWMMyPositionMode)state
{
  MWMButton * locBtn = self.locationButton;
  [locBtn.imageView stopRotation];
  switch (state)
  {
    case MWMMyPositionModePendingPosition:
    {
      [locBtn setStyleAndApply: @"ButtonPending"];
      [locBtn.imageView startRotation:1];
      break;
    }
    case MWMMyPositionModeNotFollow:
    case MWMMyPositionModeNotFollowNoPosition: [locBtn setStyleAndApply: @"ButtonGetPosition"]; break;
    case MWMMyPositionModeFollow: [locBtn setStyleAndApply: @"ButtonFollow"]; break;
    case MWMMyPositionModeFollowAndRotate: [locBtn setStyleAndApply: @"ButtonFollowAndRotate"]; break;
  }
}

#pragma mark - Actions

- (IBAction)zoomTouchDown:(UIButton *)sender { self.zoomSwipeEnabled = YES; }
- (IBAction)zoomTouchUpInside:(UIButton *)sender
{
  self.zoomSwipeEnabled = NO;
  if ([sender isEqual:self.zoomInButton])
    [self zoomIn];
  else
    [self zoomOut];
}

- (IBAction)zoomTouchUpOutside:(UIButton *)sender { self.zoomSwipeEnabled = NO; }
- (IBAction)zoomSwipe:(UIPanGestureRecognizer *)sender
{
  if (!self.zoomSwipeEnabled)
    return;
  UIView * const superview = self.sideView.superview;
  CGFloat const translation =
      -[sender translationInView:superview].y / superview.bounds.size.height;

  CGFloat const scaleFactor = exp(translation);
  GetFramework().Scale(scaleFactor, false);
}

- (IBAction)locationTouchUpInside
{
  [MWMLocationManager enableLocationAlert];
  GetFramework().SwitchMyPositionNextMode();
}

#pragma mark - Properties

- (BOOL)zoomHidden { return self.sideView.zoomHidden; }
- (void)setZoomHidden:(BOOL)zoomHidden
{
  if ([MWMRouter isRoutingActive])
    self.sideView.zoomHidden = NO;
  else
    self.sideView.zoomHidden = [MWMSettings zoomButtonsEnabled] ? zoomHidden : YES;
}

- (BOOL)hidden { return self.sideView.hidden; }
- (void)setHidden:(BOOL)hidden 
{
  if (!self.hidden && hidden)
    [[MWMToast toastWithText:L(@"long_tap_toast")] show];

  return [self.sideView setHidden:hidden animated:YES];
}

@end
