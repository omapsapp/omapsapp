#import "Product+Core.h"

@implementation Product (Core)

- (nonnull instancetype)init:(products::ProductsConfig::Product const &)product {
  self = [self initWithTitle:[NSString stringWithCString:product.GetTitle().c_str() encoding:NSUTF8StringEncoding]
                        link:[NSString stringWithCString:product.GetLink().c_str() encoding:NSUTF8StringEncoding]];
  return self;
}

@end
