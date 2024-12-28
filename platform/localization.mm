#include "platform/localization.hpp"

#include <algorithm>

#import <Foundation/Foundation.h>

namespace platform
{
std::string GetLocalizedTypeName(std::string const & type)
{
  auto key = "type." + type;
  std::replace(key.begin(), key.end(), '-', '.');
  std::replace(key.begin(), key.end(), ':', '_');

  return [NSLocalizedString(@(key.c_str()), @"") UTF8String];
}

std::string GetLocalizedBrandName(std::string const & brand)
{
  auto const key = "brand." + brand;
  return [NSLocalizedStringWithDefaultValue(@(key.c_str()), nil, NSBundle.mainBundle, @(brand.c_str()), @"") UTF8String];
}

std::string GetLocalizedString(std::string const & key)
{
  return [NSLocalizedString(@(key.c_str()), @"") UTF8String];
}

std::string GetCurrencySymbol(std::string const & currencyCode)
{
  NSLocale * locale = [NSLocale currentLocale];
  NSString * symbol = [locale displayNameForKey:NSLocaleCurrencySymbol
                                          value:@(currencyCode.c_str())];
  if (!symbol)
    return currencyCode;

  return [symbol UTF8String];
}

std::string GetLocalizedMyPositionBookmarkName()
{
  NSDate * now = [NSDate date];
  return [NSDateFormatter localizedStringFromDate:now
                                        dateStyle:NSDateFormatterLongStyle
                                        timeStyle:NSDateFormatterShortStyle].UTF8String;
}
}  // namespace platform
