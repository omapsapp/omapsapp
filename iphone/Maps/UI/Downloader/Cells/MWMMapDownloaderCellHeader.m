#import "MWMMapDownloaderCellHeader.h"
#import "SwiftBridge.h"

@implementation MWMMapDownloaderCellHeader

- (instancetype)initWithFrame:(CGRect)frame
{
  self = [super initWithFrame:frame];
  if (self)
  {
    [self setStyleNameAndApply:@"regular12:blackSecondaryText"];
  }
  return self;
}

- (void)drawTextInRect:(CGRect)rect
{
  rect = UIEdgeInsetsInsetRect(rect, UIEdgeInsetsMake(0, 16, 0, 0));
  rect = UIEdgeInsetsInsetRect(rect, self.safeAreaInsets);
  [super drawTextInRect:rect];
}

@end
