#import "MWMOpeningHoursCommon.h"
#import "CoreApi/CoreApi-Swift.h"

NSDateComponents * dateComponentsFromTime(osmoh::Time const & time)
{
  NSDateComponents * dc = [[NSDateComponents alloc] init];
  dc.hour = time.GetHoursCount();
  dc.minute = time.GetMinutesCount();
  return dc;
}

NSDate * dateFromTime(osmoh::Time const & time)
{
  NSCalendar * cal = NSCalendar.currentCalendar;
  cal.locale = NSLocale.currentLocale;
  return [cal dateFromComponents:dateComponentsFromTime(time)];
}

NSString * stringFromTime(osmoh::Time const & time)
{
  return [DateTimeFormatter dateStringFrom:dateFromTime(time)
                                 dateStyle:NSDateFormatterNoStyle
                                 timeStyle:NSDateFormatterShortStyle];
}

NSString * stringFromOpeningDays(editor::ui::OpeningDays const & openingDays)
{
  NSCalendar * cal = NSCalendar.currentCalendar;
  cal.locale = NSLocale.currentLocale;
  NSUInteger const firstWeekday = cal.firstWeekday - 1;

  NSArray<NSString *> * weekdaySymbols = cal.shortStandaloneWeekdaySymbols;
  NSMutableArray<NSString *> * spanNames = [NSMutableArray arrayWithCapacity:2];
  NSMutableArray<NSString *> * spans = [NSMutableArray array];

  auto weekdayFromDay = ^(NSUInteger day)
  {
    NSUInteger idx = day + 1;
    if (idx > static_cast<NSUInteger>(osmoh::Weekday::Saturday))
      idx -= static_cast<NSUInteger>(osmoh::Weekday::Saturday);
    return static_cast<osmoh::Weekday>(idx);
  };

  auto joinSpanNames = ^
  {
    NSUInteger const spanNamesCount = spanNames.count;
    if (spanNamesCount == 0)
      return;
    else if (spanNamesCount == 1)
      [spans addObject:spanNames[0]];
    else if (spanNamesCount == 2)
      [spans addObject:[spanNames componentsJoinedByString:@"-"]];
    else
      ASSERT(false, ("Invalid span names count."));
    [spanNames removeAllObjects];
  };
  NSUInteger const weekDaysCount = 7;
  for (NSUInteger i = 0, day = firstWeekday; i < weekDaysCount; ++i, ++day)
  {
    osmoh::Weekday const wd = weekdayFromDay(day);
    if (openingDays.find(wd) == openingDays.end())
      joinSpanNames();
    else
      spanNames[(spanNames.count == 0 ? 0 : 1)] = weekdaySymbols[static_cast<NSInteger>(wd) - 1];
  }
  joinSpanNames();
  return [spans componentsJoinedByString:@", "];
}

BOOL isEveryDay(editor::ui::TimeTable const & timeTable)
{
  return timeTable.GetOpeningDays().size() == 7;
}
