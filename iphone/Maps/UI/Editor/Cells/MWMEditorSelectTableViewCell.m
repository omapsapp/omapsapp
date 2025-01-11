#import "MWMEditorSelectTableViewCell.h"
#import <CoreApi/MWMCommon.h>
#import "SwiftBridge.h"

@interface MWMEditorSelectTableViewCell ()

@property(weak, nonatomic) IBOutlet UIImageView * icon;
@property(weak, nonatomic) IBOutlet UILabel * label;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * labelLeadingOffset;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * labelTrailingOffset;
@property(weak, nonatomic) IBOutlet UIImageView * grayArrow;

@property(weak, nonatomic) id<MWMEditorCellProtocol> delegate;

@end

@implementation MWMEditorSelectTableViewCell

- (void)configWithDelegate:(id<MWMEditorCellProtocol>)delegate
                      icon:(UIImage *)icon
                      text:(NSString *)text
               placeholder:(NSString *)placeholder
{
  self.delegate = delegate;
  self.icon.hidden = NO;
  self.icon.image = icon;
  self.icon.styleName = @"MWMBlack";
  if (text && text.length != 0)
  {
    self.label.text = text;
   [self.label setStyleNameAndApply: @"blackPrimaryText"];
  }
  else
  {
    self.label.text = placeholder;
    [self.label setStyleNameAndApply: @"blackHintText"];
  }
  self.label.preferredMaxLayoutWidth =
      self.width - self.labelLeadingOffset.constant - self.labelTrailingOffset.constant;

  if (isInterfaceRightToLeft())
    self.grayArrow.transform = CGAffineTransformMakeScale(-1, 1);
}

- (IBAction)selectAction { [self.delegate cellDidPressButton:self]; }
@end
