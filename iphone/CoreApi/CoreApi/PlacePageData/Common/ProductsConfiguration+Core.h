#include <CoreApi/CoreApi-Swift.h>
#include <CoreApi/Framework.h>

#include "platform/products.hpp"

NS_ASSUME_NONNULL_BEGIN

@interface ProductsConfiguration (Core)

- (nonnull instancetype)init:(products::ProductsConfig const &)config;

@end

NS_ASSUME_NONNULL_END
