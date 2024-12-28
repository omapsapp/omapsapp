#import "MWMOpeningHoursTimeSelectorTableViewCell.h"

@interface MWMOpeningHoursTimeSelectorTableViewCell ()

@property (weak, nonatomic) IBOutlet UIDatePicker * openTimePicker;
@property (weak, nonatomic) IBOutlet UIDatePicker * closeTimePicker;

@property (nonatomic) NSCalendar * calendar;

@end

@implementation MWMOpeningHoursTimeSelectorTableViewCell

+ (CGFloat)heightForWidth:(CGFloat)width
{
  return 180.0;
}

- (void)awakeFromNib
{
  [super awakeFromNib];
  self.calendar = NSCalendar.currentCalendar;
  self.calendar.locale = NSLocale.currentLocale;
}

- (void)refresh
{
  [super refresh];
  MWMOpeningHoursSection * section = self.section;
  NSUInteger const row = section.selectedRow.unsignedIntegerValue;
  NSDate * openDate = [self.calendar dateFromComponents:[section timeForRow:row isStart:YES]];
  NSDate * closeDate = [self.calendar dateFromComponents:[section timeForRow:row isStart:NO]];

  [self.openTimePicker setDate:openDate animated:NO];
  [self.closeTimePicker setDate:closeDate animated:NO];
}

#pragma mark - Actions

- (IBAction)openValueChanged
{
  NSDate * date = self.openTimePicker.date;
  NSCalendarUnit const  components = NSCalendarUnitHour | NSCalendarUnitMinute;
  self.section.cachedStartTime = [self.calendar components:components fromDate:date];
}

- (IBAction)closeValueChanged
{
  NSDate * date = self.closeTimePicker.date;
  NSCalendarUnit const components = NSCalendarUnitHour | NSCalendarUnitMinute;
  self.section.cachedEndTime = [self.calendar components:components fromDate:date];
}

@end
