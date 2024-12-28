#import "MWMActionBarButton.h"
#import "MWMButton.h"
#import "MWMCircularProgress.h"
#import "SwiftBridge.h"

static NSString * const kUDDidHighlightRouteToButton = @"kUDDidHighlightPoint2PointButton";

NSString *titleForButton(MWMActionBarButtonType type, BOOL isSelected) {
  switch (type) {
    case MWMActionBarButtonTypeDownload:
      return L(@"download");
    case MWMActionBarButtonTypeBooking:
    case MWMActionBarButtonTypeOpentable:
      return L(@"book_button");
    case MWMActionBarButtonTypeBookingSearch:
      return L(@"booking_search");
    case MWMActionBarButtonTypeCall:
      return L(@"placepage_call_button");
    case MWMActionBarButtonTypeBookmark:
    case MWMActionBarButtonTypeTrack:
      return L(isSelected ? @"delete" : @"save");
    case MWMActionBarButtonTypeRouteFrom:
      return L(@"p2p_from_here");
    case MWMActionBarButtonTypeRouteTo:
      return L(@"p2p_to_here");
    case MWMActionBarButtonTypeMore:
      return L(@"placepage_more_button");
    case MWMActionBarButtonTypeRouteAddStop:
      return L(@"placepage_add_stop");
    case MWMActionBarButtonTypeRouteRemoveStop:
      return L(@"placepage_remove_stop");
    case MWMActionBarButtonTypeAvoidToll:
      return L(@"avoid_tolls");
    case MWMActionBarButtonTypeAvoidDirty:
      return L(@"avoid_unpaved");
    case MWMActionBarButtonTypeAvoidFerry:
      return L(@"avoid_ferry");
  }
}

@interface MWMActionBarButton () <MWMCircularProgressProtocol>

@property(nonatomic) MWMActionBarButtonType type;
@property(nonatomic) MWMCircularProgress *mapDownloadProgress;
@property(weak, nonatomic) IBOutlet MWMButton *button;
@property(weak, nonatomic) IBOutlet UILabel *label;
@property(weak, nonatomic) IBOutlet UIView *extraBackground;
@property(weak, nonatomic) id<MWMActionBarButtonDelegate> delegate;

@end

@implementation MWMActionBarButton

- (void)configButton:(BOOL)isSelected enabled:(BOOL)isEnabled {
  self.label.text = titleForButton(self.type, isSelected);
  self.extraBackground.hidden = YES;
  self.button.coloring = MWMButtonColoringBlack;
  
  switch (self.type) {
    case MWMActionBarButtonTypeDownload: {
      if (self.mapDownloadProgress)
        return;

      self.mapDownloadProgress = [MWMCircularProgress downloaderProgressForParentView:self.button];
      self.mapDownloadProgress.delegate = self;

      MWMCircularProgressStateVec affectedStates =
        @[@(MWMCircularProgressStateNormal), @(MWMCircularProgressStateSelected)];

      [self.mapDownloadProgress setImageName:@"ic_download" forStates:affectedStates];
      [self.mapDownloadProgress setColoring:MWMButtonColoringBlue forStates:affectedStates];
      break;
    }
    case MWMActionBarButtonTypeBooking:
      [self.button setImage:[UIImage imageNamed:@"ic_booking_logo"] forState:UIControlStateNormal];
      self.label.styleName = @"PPActionBarTitlePartner";
      self.backgroundColor = [UIColor bookingBackground];
      if (!IPAD) {
        self.extraBackground.backgroundColor = [UIColor bookingBackground];
        self.extraBackground.hidden = NO;
      }
      break;
    case MWMActionBarButtonTypeBookingSearch:
      [self.button setImage:[UIImage imageNamed:@"ic_booking_search"] forState:UIControlStateNormal];
      self.label.styleName = @"PPActionBarTitlePartner";
      self.backgroundColor = [UIColor bookingBackground];
      if (!IPAD) {
        self.extraBackground.backgroundColor = [UIColor bookingBackground];
        self.extraBackground.hidden = NO;
      }
      break;
    case MWMActionBarButtonTypeOpentable:
      [self.button setImage:[UIImage imageNamed:@"ic_opentable"] forState:UIControlStateNormal];
      self.label.styleName = @"PPActionBarTitlePartner";
      self.backgroundColor = [UIColor opentableBackground];
      if (!IPAD) {
        self.extraBackground.backgroundColor = [UIColor opentableBackground];
        self.extraBackground.hidden = NO;
      }
      break;
    case MWMActionBarButtonTypeCall:
      [self.button setImage:[UIImage imageNamed:@"ic_placepage_phone_number"] forState:UIControlStateNormal];
      break;
    case MWMActionBarButtonTypeBookmark:
      [self setupBookmarkButton:isSelected];
      break;
    case MWMActionBarButtonTypeTrack:
      [self.button setImage:[[UIImage imageNamed:@"ic_route_manager_trash"] imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
      self.button.coloring = MWMButtonColoringRed;
      break;
    case MWMActionBarButtonTypeRouteFrom:
      [self.button setImage:[UIImage imageNamed:@"ic_route_from"] forState:UIControlStateNormal];
      break;
    case MWMActionBarButtonTypeRouteTo:
      [self.button setImage:[UIImage imageNamed:@"ic_route_to"] forState:UIControlStateNormal];
      if ([self needsToHighlightRouteToButton])
        self.button.coloring = MWMButtonColoringBlue;
      break;
    case MWMActionBarButtonTypeMore:
      [self.button setImage:[UIImage imageNamed:@"ic_placepage_more"] forState:UIControlStateNormal];
      break;
    case MWMActionBarButtonTypeRouteAddStop:
      [self.button setImage:[UIImage imageNamed:@"ic_add_route_point"] forState:UIControlStateNormal];
      break;
    case MWMActionBarButtonTypeRouteRemoveStop:
      [self.button setImage:[UIImage imageNamed:@"ic_remove_route_point"] forState:UIControlStateNormal];
      break;
    case MWMActionBarButtonTypeAvoidToll:
      [self.button setImage:[UIImage imageNamed:@"ic_avoid_tolls"] forState:UIControlStateNormal];
      break;
    case MWMActionBarButtonTypeAvoidDirty:
      [self.button setImage:[UIImage imageNamed:@"ic_avoid_dirty"] forState:UIControlStateNormal];
      break;
    case MWMActionBarButtonTypeAvoidFerry:
      [self.button setImage:[UIImage imageNamed:@"ic_avoid_ferry"] forState:UIControlStateNormal];
      break;
  }
  
  self.button.enabled = isEnabled;
}

+ (MWMActionBarButton *)buttonWithDelegate:(id<MWMActionBarButtonDelegate>)delegate
                                buttonType:(MWMActionBarButtonType)type
                                isSelected:(BOOL)isSelected
                                isEnabled:(BOOL)isEnabled {
  MWMActionBarButton *button = [NSBundle.mainBundle loadNibNamed:[self className] owner:nil options:nil].firstObject;
  button.delegate = delegate;
  button.type = type;
  [button configButton:isSelected enabled:isEnabled];
  return button;
}

- (void)progressButtonPressed:(MWMCircularProgress *)progress {
  [self.delegate tapOnButtonWithType:self.type];
}

- (IBAction)tap {
  if (self.type == MWMActionBarButtonTypeRouteTo)
    [self disableRouteToButtonHighlight];
  
  [self.delegate tapOnButtonWithType:self.type];
}

- (void)setBookmarkButtonState:(MWMBookmarksButtonState)state {
  switch (state) {
    case MWMBookmarksButtonStateSave:
      self.label.text = L(@"save");
      self.button.selected = false;
      break;
    case MWMBookmarksButtonStateDelete:
      self.label.text = L(@"delete");
      if (!self.button.selected)
        [self.button.imageView startAnimating];
      self.button.selected = true;
      break;
    case MWMBookmarksButtonStateRecover:
      self.label.text = L(@"restore");
      self.button.selected = false;
      break;
  }
}

- (void)setupBookmarkButton:(BOOL)isSelected {
  MWMButton *btn = self.button;
  [btn setImage:[UIImage imageNamed:@"ic_bookmarks_off"] forState:UIControlStateNormal];
  [btn setImage:[UIImage imageNamed:@"ic_bookmarks_on"] forState:UIControlStateSelected];
  [btn setImage:[UIImage imageNamed:@"ic_bookmarks_on"] forState:UIControlStateHighlighted];
  [btn setImage:[UIImage imageNamed:@"ic_bookmarks_on"] forState:UIControlStateDisabled];

  [btn setSelected:isSelected];

  NSUInteger const animationImagesCount = 11;
  NSMutableArray *animationImages = [NSMutableArray arrayWithCapacity:animationImagesCount];
  for (NSUInteger i = 0; i < animationImagesCount; ++i) {
    UIImage *image = [UIImage imageNamed:[NSString stringWithFormat:@"ic_bookmarks_%@", @(i + 1)]];
    animationImages[i] = image;
  }
  UIImageView *animationIV = btn.imageView;
  animationIV.animationImages = animationImages;
  animationIV.animationRepeatCount = 1;
}

- (BOOL)needsToHighlightRouteToButton {
  return ![NSUserDefaults.standardUserDefaults boolForKey:kUDDidHighlightRouteToButton];
}

- (void)disableRouteToButtonHighlight {
  [NSUserDefaults.standardUserDefaults setBool:true forKey:kUDDidHighlightRouteToButton];
}

- (void)traitCollectionDidChange:(UITraitCollection *)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 13.0, *)) {
    if ([self.traitCollection hasDifferentColorAppearanceComparedToTraitCollection:previousTraitCollection])
      // Update button for the current selection state.
      [self.button setSelected:self.button.isSelected];
  }
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event {
  return [self pointInside:point withEvent:event] ? self.button : nil;
}

@end
