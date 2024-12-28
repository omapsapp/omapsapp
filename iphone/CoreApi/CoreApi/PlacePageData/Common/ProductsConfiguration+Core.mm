#import "ProductsConfiguration+Core.h"
#import "Product+Core.h"

@implementation ProductsConfiguration (Core)

- (nonnull instancetype)init:(products::ProductsConfig const &)config {
  auto const & coreProducts = config.GetProducts();
  NSMutableArray<Product *> * products = [[NSMutableArray<Product *> alloc] initWithCapacity:coreProducts.size()];
  for (auto const & product : coreProducts)
    [products addObject:[[Product alloc] init:product]];
  self = [self initWithPlacePagePrompt:[NSString stringWithCString:config.GetPlacePagePrompt().c_str() encoding:NSUTF8StringEncoding] products:products];
  return self;
}

@end
