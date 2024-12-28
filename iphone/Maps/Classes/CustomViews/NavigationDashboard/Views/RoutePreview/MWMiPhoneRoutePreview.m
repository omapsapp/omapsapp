#import "MWMiPhoneRoutePreview.h"
#import "MWMAvailableAreaAffectDirection.h"

@interface MWMRoutePreview ()

@property(nonatomic) BOOL isVisible;

@end

@interface MWMiPhoneRoutePreview ()
@property(weak, nonatomic) IBOutlet UIButton * backButton;
@property(nonatomic) NSLayoutConstraint * verticalConstraint;
@end

@implementation MWMiPhoneRoutePreview

- (void)setupConstraints
{
  UIView * sv = self.superview;
  [self.leadingAnchor constraintEqualToAnchor:sv.leadingAnchor].active = YES;
  [self.trailingAnchor constraintEqualToAnchor:sv.trailingAnchor].active = YES;
  self.verticalConstraint = [self.bottomAnchor constraintEqualToAnchor:sv.topAnchor];
  self.verticalConstraint.active = YES;

  NSLayoutXAxisAnchor * backLeadingAnchor = sv.leadingAnchor;
  backLeadingAnchor = sv.safeAreaLayoutGuide.leadingAnchor;
  [self.backButton.leadingAnchor constraintEqualToAnchor:backLeadingAnchor].active = YES;

  [sv layoutIfNeeded];
}

- (void)setIsVisible:(BOOL)isVisible
{
  UIView * sv = self.superview;
  if (!sv)
    return;
  self.verticalConstraint.active = NO;
  NSLayoutYAxisAnchor * topAnchor = sv.topAnchor;
  NSLayoutYAxisAnchor * selfAnchor = isVisible ? self.topAnchor : self.bottomAnchor;
  CGFloat constant = 0;
  if (isVisible)
  {
    topAnchor = sv.topAnchor;
  }
  self.verticalConstraint = [selfAnchor constraintEqualToAnchor:topAnchor constant:constant];
  self.verticalConstraint.active = YES;
  [super setIsVisible:isVisible];
}

#pragma mark - AvailableArea / VisibleArea

- (MWMAvailableAreaAffectDirections)visibleAreaAffectDirections
{
  return MWMAvailableAreaAffectDirectionsTop;
}

#pragma mark - AvailableArea / SideButtonsArea

- (MWMAvailableAreaAffectDirections)sideButtonsAreaAffectDirections
{
  return MWMAvailableAreaAffectDirectionsTop;
}

#pragma mark - AvailableArea / TrafficButtonArea

- (MWMAvailableAreaAffectDirections)trafficButtonAreaAffectDirections
{
  return MWMAvailableAreaAffectDirectionsTop;
}

#pragma mark - AvailableArea / TrackRecordingButtonArea

- (MWMAvailableAreaAffectDirections)trackRecordingButtonAreaAffectDirections
{
  return MWMAvailableAreaAffectDirectionsTop;
}

@end
