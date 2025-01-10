#import "MWMOpeningHoursDaysSelectorTableViewCell.h"
#import "SwiftBridge.h"

@interface MWMOpeningHoursDaysSelectorTableViewCell ()

@property (nonatomic) IBOutletCollection(UIButton) NSArray * buttons;
@property (nonatomic) IBOutletCollection(UILabel) NSArray * labels;
@property (nonatomic) IBOutletCollection(UIImageView) NSArray * images;

@property (nonatomic) NSUInteger firstWeekday;

@end

using namespace osmoh;

@implementation MWMOpeningHoursDaysSelectorTableViewCell

+ (CGFloat)heightForWidth:(CGFloat)width
{
  return 76.0;
}

- (void)awakeFromNib
{
  [super awakeFromNib];
  NSCalendar * cal = NSCalendar.currentCalendar;
  cal.locale = NSLocale.currentLocale;
  self.firstWeekday = cal.firstWeekday;
  NSArray<NSString *> * weekdaySymbols = cal.shortStandaloneWeekdaySymbols;
  for (UILabel * label in self.labels)
  {
    label.text = weekdaySymbols[[self tag2SymbolIndex:label.tag]];
  }
}

- (NSUInteger)tag2SymbolIndex:(NSUInteger)tag
{
  NSUInteger idx = tag + self.firstWeekday - 1;
  NSUInteger const weekDaysCount = 7;
  if (idx >= weekDaysCount)
    idx -= weekDaysCount;
  return idx;
}

- (Weekday)tag2Weekday:(NSUInteger)tag
{
  return static_cast<Weekday>([self tag2SymbolIndex:tag] + 1);
}

- (void)makeDay:(NSUInteger)tag selected:(BOOL)selected refresh:(BOOL)refresh
{
  if (refresh)
  {
    Weekday const wd = [self tag2Weekday:tag];
    MWMOpeningHoursSection * section = self.section;
    if (selected)
      [section addSelectedDay:wd];
    else
      [section removeSelectedDay:wd];
  }
  for (UIButton * btn in self.buttons)
  {
    if (btn.tag == tag)
      btn.selected = selected;
  }
  for (UILabel * label in self.labels)
  {
    if (label.tag == tag)
      label.textColor = (selected ? [UIColor blackPrimaryText] : [UIColor blackHintText]);
  }
  for (UIImageView * image in self.images)
  {
    if (image.tag == tag)
    {
      image.image = [UIImage imageNamed:selected ? @"radioBtnOn" : @"radioBtnOff"];
      [image setStyleNameAndApply: selected ? @"MWMBlue" : @"MWMGray"];
    }
  }
}

- (void)refresh
{
  [super refresh];
  for (UILabel * label in self.labels)
  {
    NSUInteger const tag = label.tag;
    BOOL const selected = [self.section containsSelectedDay:[self tag2Weekday:tag]];
    [self makeDay:tag selected:selected refresh:NO];
  }
}

#pragma mark - Actions

- (IBAction)selectDay:(UIButton *)sender
{
  [self makeDay:sender.tag selected:!sender.isSelected refresh:YES];
}

@end
