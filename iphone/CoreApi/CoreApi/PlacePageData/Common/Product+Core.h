#include <CoreApi/CoreApi-Swift.h>
#include <CoreApi/Framework.h>

#include "platform/products.hpp"

@interface Product (Core)

- (nonnull instancetype)init:(products::ProductsConfig::Product const &)product;

@end
