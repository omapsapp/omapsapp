#import "MWMRoutePreview.h"
#import "MWMCircularProgress.h"
#import "MWMLocationManager.h"
#import "MWMRouter.h"
#import "UIButton+Orientation.h"
#import "UIImageView+Coloring.h"
#import "SwiftBridge.h"

#include "platform/platform.hpp"

static CGFloat const kDrivingOptionsHeight = 48;

@interface MWMRoutePreview ()<MWMCircularProgressProtocol>

@property(nonatomic) BOOL isVisible;
@property(nonatomic) BOOL actualVisibilityValue;
@property(weak, nonatomic) IBOutlet UIButton * backButton;
@property(weak, nonatomic) IBOutlet UIView * bicycle;
@property(weak, nonatomic) IBOutlet UIView * contentView;
@property(weak, nonatomic) IBOutlet UIView * pedestrian;
@property(weak, nonatomic) IBOutlet UIView * publicTransport;
@property(weak, nonatomic) IBOutlet UIView * ruler;
@property(weak, nonatomic) IBOutlet UIView * vehicle;
@property(strong, nonatomic) IBOutlet NSLayoutConstraint * drivingOptionHeightConstraint;
@property(strong, nonatomic) IBOutlet UIButton * drivingOptionsButton;

@end

@implementation MWMRoutePreview
{
  std::map<MWMRouterType, MWMCircularProgress *> m_progresses;
}

- (void)awakeFromNib
{
  [super awakeFromNib];
  self.actualVisibilityValue = NO;
  self.translatesAutoresizingMaskIntoConstraints = NO;
  [self setupProgresses];
  [self.backButton matchInterfaceOrientation];
  self.drivingOptionHeightConstraint.constant = -kDrivingOptionsHeight;
  [self applyContentViewShadow];
}

- (void)applyContentViewShadow {
  self.contentView.layer.shadowOffset = CGSizeZero;
  self.contentView.layer.shadowRadius = 2.0;
  self.contentView.layer.shadowOpacity = 0.7;
  self.contentView.layer.shadowColor = UIColor.blackColor.CGColor;
  [self resizeShadow];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self.vehicle setNeedsLayout];
  [self resizeShadow];
}

- (void)resizeShadow {
  CGFloat shadowSize = 1.0;
  CGRect contentFrame = self.contentView.bounds;
  CGRect shadowFrame = CGRectMake(contentFrame.origin.x - shadowSize,
                                  contentFrame.size.height,
                                  contentFrame.size.width + (2 * shadowSize),
                                  shadowSize);
  self.contentView.layer.shadowPath = [UIBezierPath bezierPathWithRect: shadowFrame].CGPath;
}

- (void)setupProgresses
{
  [self addProgress:self.vehicle imageName:@"ic_car" routerType:MWMRouterTypeVehicle];
  [self addProgress:self.pedestrian imageName:@"ic_pedestrian" routerType:MWMRouterTypePedestrian];
  [self addProgress:self.publicTransport
          imageName:@"ic_train"
         routerType:MWMRouterTypePublicTransport];
  [self addProgress:self.bicycle imageName:@"ic_bike" routerType:MWMRouterTypeBicycle];
  [self addProgress:self.ruler imageName:@"ic_ruler_route" routerType:MWMRouterTypeRuler];
}

- (void)addProgress:(UIView *)parentView
          imageName:(NSString *)imageName
         routerType:(MWMRouterType)routerType
{
  MWMCircularProgress * progress = [[MWMCircularProgress alloc] initWithParentView:parentView];
  MWMCircularProgressStateVec imageStates = @[@(MWMCircularProgressStateNormal),
    @(MWMCircularProgressStateProgress), @(MWMCircularProgressStateSpinner)];

  [progress setImageName:imageName forStates:imageStates];
  [progress setImageName:[imageName stringByAppendingString:@"_selected"] forStates:@[@(MWMCircularProgressStateSelected), @(MWMCircularProgressStateCompleted)]];
  [progress setImageName:@"ic_error" forStates:@[@(MWMCircularProgressStateFailed)]];

  [progress setColoring:MWMButtonColoringWhiteText
              forStates:@[@(MWMCircularProgressStateFailed), @(MWMCircularProgressStateSelected),
                         @(MWMCircularProgressStateProgress), @(MWMCircularProgressStateSpinner),
                         @(MWMCircularProgressStateCompleted)]];

  [progress setSpinnerBackgroundColor:UIColor.clearColor];
  [progress setColor:UIColor.whiteColor
           forStates:@[@(MWMCircularProgressStateProgress), @(MWMCircularProgressStateSpinner)]];

  progress.delegate = self;
  m_progresses[routerType] = progress;
}

- (void)statePrepare
{
  for (auto const & progress : m_progresses)
    progress.second.state = MWMCircularProgressStateNormal;
}

- (void)selectRouter:(MWMRouterType)routerType
{
  for (auto const & progress : m_progresses)
    progress.second.state = MWMCircularProgressStateNormal;
  m_progresses[routerType].state = MWMCircularProgressStateSelected;
}

- (void)router:(MWMRouterType)routerType setState:(MWMCircularProgressState)state
{
  m_progresses[routerType].state = state;
}

- (void)router:(MWMRouterType)routerType setProgress:(CGFloat)progress
{
  m_progresses[routerType].progress = progress;
}

- (IBAction)onDrivingOptions:(UIButton *)sender {
  [self.delegate routePreviewDidPressDrivingOptions:self];
}

#pragma mark - MWMCircularProgressProtocol

- (void)progressButtonPressed:(nonnull MWMCircularProgress *)progress
{
  MWMCircularProgressState const s = progress.state;
  if (s == MWMCircularProgressStateSelected || s == MWMCircularProgressStateCompleted)
    return;

  for (auto const & prg : m_progresses)
  {
    if (prg.second != progress)
      continue;
    auto const routerType = prg.first;
    if ([MWMRouter type] == routerType)
      return;
    [self selectRouter:routerType];
    [MWMRouter setType:routerType];
    [MWMRouter rebuildWithBestRouter:NO];
  }
}

- (void)addToView:(UIView *)superview
{
  NSAssert(superview != nil, @"Superview can't be nil");
  if ([superview.subviews containsObject:self])
    return;
  [superview addSubview:self];
  [self setupConstraints];
  self.actualVisibilityValue = YES;
  dispatch_async(dispatch_get_main_queue(), ^{
    self.isVisible = YES;
  });
}

- (void)remove {
  self.actualVisibilityValue = NO;
  self.isVisible = NO;
}

- (void)setupConstraints {}

- (void)setDrivingOptionsState:(MWMDrivingOptionsState)state {
  _drivingOptionsState = state;
  [self layoutIfNeeded];
  self.drivingOptionHeightConstraint.constant =
    (state == MWMDrivingOptionsStateNone) ? -kDrivingOptionsHeight : 0;
  [UIView animateWithDuration:kDefaultAnimationDuration animations:^{
    [self layoutIfNeeded];
  }];

  if (state == MWMDrivingOptionsStateDefine) {
    [self.drivingOptionsButton setImagePadding:0.0];
    [self.drivingOptionsButton setImage:nil
                               forState:UIControlStateNormal];
    [self.drivingOptionsButton setTitle:L(@"define_to_avoid_btn").uppercaseString
                               forState:UIControlStateNormal];
  } else if (state == MWMDrivingOptionsStateChange) {
    [self.drivingOptionsButton setImagePadding:5.0];
    [self.drivingOptionsButton setImage:[UIImage imageNamed:@"ic_options_warning"]
                               forState:UIControlStateNormal];
    [self.drivingOptionsButton setTitle:L(@"change_driving_options_btn").uppercaseString
                               forState:UIControlStateNormal];
  }
}

#pragma mark - Properties

- (void)setIsVisible:(BOOL)isVisible
{
  if (isVisible != self.actualVisibilityValue) { return; }
  _isVisible = isVisible;
  auto sv = self.superview;
  [sv setNeedsLayout];
  [UIView animateWithDuration:kDefaultAnimationDuration
      animations:^{
        [sv layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        if (!self.isVisible)
          [self removeFromSuperview];
      }];
}

@end
