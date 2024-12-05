#import "MWMSideButtonsView.h"
#import "MWMButton.h"
#import "MWMMapViewControlsCommon.h"

#include "base/math.hpp"

namespace {
CGFloat const kLocationButtonSpacingMax = 52;
CGFloat const kLocationButtonSpacingMin = 8;
CGFloat const kButtonsTopOffset = 6;
CGFloat const kButtonsBottomOffset = 6;
}  // namespace

@interface MWMSideButtonsView ()

@property(weak, nonatomic) IBOutlet MWMButton *zoomIn;
@property(weak, nonatomic) IBOutlet MWMButton *zoomOut;
@property(weak, nonatomic) IBOutlet MWMButton *location;

@property(nonatomic) CGRect availableArea;

@end

@implementation MWMSideButtonsView

- (void)awakeFromNib {
  [super awakeFromNib];
  self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
}

- (void)layoutSubviews {
  CGFloat spacing = self.availableHeight - self.zoomOut.maxY - self.location.height;
  spacing = math::Clamp(spacing, kLocationButtonSpacingMin, kLocationButtonSpacingMax);

  self.location.minY = self.zoomOut.maxY + spacing;
  self.bounds = {{}, {self.zoomOut.width, self.location.maxY}};
  if (self.zoomHidden)
    self.height = self.location.height;
  self.location.maxY = self.height;

  [self animate];
  [super layoutSubviews];
}

- (void)layoutXPosition:(BOOL)hidden {
  if (UIApplication.sharedApplication.userInterfaceLayoutDirection == UIUserInterfaceLayoutDirectionRightToLeft) {
    if (hidden)
      self.maxX = 0;
    else
      self.minX = self.availableArea.origin.x + kViewControlsOffsetToBounds;
  } else {
    const auto availableAreaMaxX = self.availableArea.origin.x + self.availableArea.size.width;
    if (hidden)
      self.minX = self.superview.width;
    else
      self.maxX = availableAreaMaxX - kViewControlsOffsetToBounds;
  }
}

- (void)layoutYPosition {
  CGFloat const centerShift = (self.height - self.zoomIn.midY - self.zoomOut.midY) / 2;
  self.midY = centerShift + self.superview.height / 2;
  if (self.maxY > self.bottomBound)
    self.maxY = self.bottomBound;
}

- (void)fadeZoomButtonsShow:(BOOL)show {
  CGFloat const alpha = show ? 1.0 : 0.0;
  [UIView animateWithDuration:kDefaultAnimationDuration
                   animations:^{
                     self.zoomIn.alpha = alpha;
                     self.zoomOut.alpha = alpha;
                   }];
}

- (void)fadeLocationButtonShow:(BOOL)show {
  [UIView animateWithDuration:kDefaultAnimationDuration
                   animations:^{
                     self.location.alpha = show ? 1.0 : 0.0;
                   }];
}

// Show/hide zoom and location buttons depending on available vertical space.
- (void)animate {
  [self layoutYPosition];

  BOOL const isZoomHidden = self.zoomIn.alpha == 0.0;
  BOOL const willZoomHide = (self.location.maxY > self.availableHeight);
  if (willZoomHide != isZoomHidden)
    [self fadeZoomButtonsShow: !willZoomHide];

  BOOL const isLocationHidden = self.location.alpha == 0.0;
  BOOL const willLocationHide = (self.location.height > self.availableHeight);
  if (willLocationHide != isLocationHidden)
    [self fadeLocationButtonShow: !willLocationHide];
}

#pragma mark - Properties

- (void)setZoomHidden:(BOOL)zoomHidden {
  _zoomHidden = zoomHidden;
  self.zoomIn.hidden = zoomHidden;
  self.zoomOut.hidden = zoomHidden;
  [self setNeedsLayout];
}

- (void)setHidden:(BOOL)hidden animated:(BOOL)animated {
  if (animated) {
    if (self.hidden == hidden)
      return;
    // Side buttons should be visible during any our show/hide anamation.
    // Visibility should be detemined by alpha, not self.hidden.
    self.hidden = NO;
    [UIView animateWithDuration:kDefaultAnimationDuration
      animations:^{
        self.alpha = hidden ? 0.0 : 1.0;
        [self layoutXPosition:hidden];
      }
      completion:^(BOOL finished) {
        self.hidden = hidden;
      }];
  } else {
    self.hidden = hidden;
  }
}

- (void)updateAvailableArea:(CGRect)frame {
  if (CGRectEqualToRect(self.availableArea, frame))
    return;
  // If during our show/hide animation position is changed it is corrupted.
  // Such kind of animation has 2 keys (opacity and position).
  // But there are other animation cases like change of orientation.
  // So we can use condition below:
  // if (self.layer.animationKeys.count != 2)
  // More elegant way is to check if x values are changed.
  // If no - there is no need to update self x values.
  if (self.availableArea.origin.x != frame.origin.x || self.availableArea.size.width != frame.size.width)
  {
    self.availableArea = frame;
    [self layoutXPosition:self.hidden];
  }
  else
    self.availableArea = frame;
  [self setNeedsLayout];
}

- (CGFloat)availableHeight {
  return self.availableArea.size.height - kButtonsTopOffset - kButtonsBottomOffset;
}

- (CGFloat)topBound {
  return self.availableArea.origin.y + kButtonsTopOffset;
}
- (CGFloat)bottomBound {
  auto const area = self.availableArea;
  return area.origin.y + area.size.height - kButtonsBottomOffset;
}

@end
