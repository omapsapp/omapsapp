#import <UIKit/UIKit.h>

#import "MWMTypes.h"

NS_ASSUME_NONNULL_BEGIN

static inline BOOL firstVersionIsLessThanSecond(NSString * first, NSString * second)
{
  NSArray<NSString *> * f = [first componentsSeparatedByString:@"."];
  NSArray<NSString *> * s = [second componentsSeparatedByString:@"."];
  NSUInteger iter = 0;
  while (f.count > iter && s.count > iter)
  {
    NSInteger fiv = f[iter].integerValue;
    NSInteger siv = s[iter].integerValue;

    if (fiv == siv)
      iter++;
    else
      return fiv < siv;
  }
  return f.count < s.count;
}

static inline BOOL isIOSVersionLessThanString(NSString * version)
{
  return firstVersionIsLessThanSecond(UIDevice.currentDevice.systemVersion, version);
}

static inline BOOL isIOSVersionLessThan(NSUInteger version)
{
  return isIOSVersionLessThanString([NSString stringWithFormat:@"%@", @(version)]);
}

static inline BOOL isInterfaceRightToLeft(void) NS_EXTENSION_UNAVAILABLE_IOS("Not available in extensions")
{
  return UIApplication.sharedApplication.userInterfaceLayoutDirection ==
         UIUserInterfaceLayoutDirectionRightToLeft;
}

static inline NSString * formattedSize(uint64_t size)
{
  return [NSByteCountFormatter stringFromByteCount:size
                                        countStyle:NSByteCountFormatterCountStyleFile];
}

// Use only for screen dimensions CGFloat comparison
static inline BOOL equalScreenDimensions(CGFloat left, CGFloat right)
{
  return fabs(left - right) < 0.5;
}

static inline CGFloat statusBarHeight(void) NS_EXTENSION_UNAVAILABLE_IOS("Not available in extensions")
{
  CGSize const statusBarSize = UIApplication.sharedApplication.statusBarFrame.size;
  return MIN(statusBarSize.height, statusBarSize.width);
}

static inline void performOnce(MWMVoidBlock block, NSString *key) {
  BOOL performed = [[NSUserDefaults standardUserDefaults] boolForKey:key];
  if (!performed) {
    block();
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:key];
  }
}

NS_ASSUME_NONNULL_END
