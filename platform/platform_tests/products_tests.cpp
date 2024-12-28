#include "testing/testing.hpp"

#include "platform/products.hpp"

#include "cppjansson/cppjansson.hpp"

using namespace products;

UNIT_TEST(ProductsConfig_ValidConfig)
{
  std::string jsonStr = R"({
    "placePagePrompt": "prompt1",
    "products": [
      {
        "title": "Product 1",
        "link": "http://product1"
      },
      {
        "title": "Product 2",
        "link": "http://product2"
      }
    ]
  })";

  auto const result = ProductsConfig::Parse(jsonStr);
  TEST(result.has_value(), ());
  auto const productsConfig = result.value();
  TEST_EQUAL(productsConfig.GetPlacePagePrompt(), "prompt1", ());

  auto const products = productsConfig.GetProducts();
  TEST_EQUAL(products.size(), 2, ());
  TEST_EQUAL(products[0].GetTitle(), "Product 1", ());
  TEST_EQUAL(products[0].GetLink(), "http://product1", ());
  TEST_EQUAL(products[1].GetTitle(), "Product 2", ());
  TEST_EQUAL(products[1].GetLink(), "http://product2", ());
}

UNIT_TEST(ProductsConfig_EmptyPrompts)
{
  std::string jsonStr = R"({
    "aboutScreenPrompt": "",
    "products": [
      {
        "title": "Product 1",
        "link": "http://product1"
      },
      {
        "title": "Product 2",
        "link": "http://product2"
      }
    ]
  })";

  auto const result = ProductsConfig::Parse(jsonStr);
  TEST(result.has_value(), ());
  auto const productsConfig = result.value();
  TEST_EQUAL(productsConfig.GetPlacePagePrompt(), "", ());
  TEST_EQUAL(productsConfig.GetProducts().size(), 2, ());
}

UNIT_TEST(ProductsConfig_InvalidProduct)
{
  std::string jsonStr = R"({
    "placePagePrompt": "prompt1",
    "products": [
      {
        "title": "Product 1"
      },
      {
        "title": "Product 2",
        "link": "http://product2"
      }
    ]
  })";

  auto const result = ProductsConfig::Parse(jsonStr);
  TEST(result.has_value(), ());
  auto const productsConfig = result.value();
  TEST_EQUAL(productsConfig.GetPlacePagePrompt(), "prompt1", ());

  auto const products = productsConfig.GetProducts();
  TEST_EQUAL(products.size(), 1, ());
  TEST_EQUAL(products[0].GetTitle(), "Product 2", ());
  TEST_EQUAL(products[0].GetLink(), "http://product2", ());
}

UNIT_TEST(ProductsConfig_EmptyProducts)
{
  std::string jsonStr = R"({
    "placePagePrompt": "prompt1",
    "products": []
  })";

  auto const result = ProductsConfig::Parse(jsonStr);
  TEST(!result.has_value(), ());
}

UNIT_TEST(ProductsConfig_MissedProductsField)
{
  std::string jsonStr = R"({
    "placePagePrompt": "prompt1"
  })";

  auto const result = ProductsConfig::Parse(jsonStr);
  TEST(!result.has_value(), ());
}

