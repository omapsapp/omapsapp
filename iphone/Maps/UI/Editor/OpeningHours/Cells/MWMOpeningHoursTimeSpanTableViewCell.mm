#import "MWMOpeningHoursTimeSpanTableViewCell.h"

@interface MWMOpeningHoursTimeSpanTableViewCell ()

@property (weak, nonatomic) IBOutlet UILabel * openTimeLabel;
@property (weak, nonatomic) IBOutlet UILabel * closeTimeLabel;

@end

@implementation MWMOpeningHoursTimeSpanTableViewCell

+ (CGFloat)heightForWidth:(CGFloat)width
{
  return 64.0;
}

- (void)refresh
{
  [super refresh];
  NSCalendar * calendar = NSCalendar.currentCalendar;

  MWMOpeningHoursSection * section = self.section;
  NSUInteger const row = self.row;
  NSDate * openDate = [calendar dateFromComponents:[section timeForRow:row isStart:YES]];
  NSDate * closeDate = [calendar dateFromComponents:[section timeForRow:row isStart:NO]];

  NSDateFormatterStyle timeStyle = NSDateFormatterShortStyle;
  NSDateFormatterStyle dateStyle = NSDateFormatterNoStyle;
  self.openTimeLabel.text = [DateTimeFormatter dateStringFrom:openDate dateStyle:dateStyle timeStyle:timeStyle];
  self.closeTimeLabel.text = [DateTimeFormatter dateStringFrom:closeDate dateStyle:dateStyle timeStyle:timeStyle];

  UIColor * clr = [section isRowSelected:row] ? [UIColor linkBlue] : [UIColor blackSecondaryText];
  self.openTimeLabel.textColor = clr;
  self.closeTimeLabel.textColor = clr;
}

#pragma mark - Actions

- (IBAction)expandTap
{
  if (self.isVisible)
  {
    MWMOpeningHoursSection * section = self.section;
    NSUInteger const row = self.row;
    section.selectedRow = [section isRowSelected:row] ? nil : @(row);
    [section refresh:NO];
  }
}

@end
