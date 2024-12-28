#include "testing/testing.hpp"

#include "platform/servers_list.hpp"

#include "platform/products.hpp"

#include "cppjansson/cppjansson.hpp"

using namespace downloader;

UNIT_TEST(MetaConfig_JSONParser_OldFormat)
{
  std::string oldFormatJson = R"(["http://url1", "http://url2", "http://url3"])";
  auto result = ParseMetaConfig(oldFormatJson);
  TEST(result.has_value(), ());
  TEST_EQUAL(result->m_serversList.size(), 3, ());
  TEST_EQUAL(result->m_serversList[0], "http://url1", ());
  TEST_EQUAL(result->m_serversList[1], "http://url2", ());
  TEST_EQUAL(result->m_serversList[2], "http://url3", ());
  TEST(result->m_settings.empty(), ());
  TEST(result->m_productsConfig.empty(), ());
}

UNIT_TEST(MetaConfig_JSONParser_InvalidJSON)
{
  std::string invalidJson = R"({"servers": ["http://url1", "http://url2")";
  auto result = ParseMetaConfig(invalidJson);
  TEST(!result.has_value(), ());
}

UNIT_TEST(MetaConfig_JSONParser_EmptyServersList)
{
  std::string emptyServersJson = R"({"servers": []})";
  auto result = ParseMetaConfig(emptyServersJson);
  TEST(!result.has_value(), ());
}

UNIT_TEST(MetaConfig_JSONParser_NewFormatWithoutProducts)
{
  std::string newFormatJson = R"({
    "servers": ["http://url1", "http://url2"],
    "settings": {
      "key1": "value1",
      "key2": "value2"
    }
  })";
  auto result = ParseMetaConfig(newFormatJson);
  TEST(result.has_value(), ());
  TEST_EQUAL(result->m_serversList.size(), 2, ());
  TEST_EQUAL(result->m_serversList[0], "http://url1", ());
  TEST_EQUAL(result->m_serversList[1], "http://url2", ());
  TEST_EQUAL(result->m_settings.size(), 2, ());
  TEST_EQUAL(result->m_settings["key1"], "value1", ());
  TEST_EQUAL(result->m_settings["key2"], "value2", ());
  TEST(result->m_productsConfig.empty(), ());
}

UNIT_TEST(MetaConfig_JSONParser_NewFormatWithProducts)
{
  std::string newFormatJson = R"({
    "servers": ["http://url1", "http://url2"],
    "settings": {
      "key1": "value1",
      "key2": "value2"
    },
    "productsConfig": {
      "placePagePrompt": "prompt1",
      "aboutScreenPrompt": "prompt2",
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
    }
  })";

  auto result = ParseMetaConfig(newFormatJson);
  TEST(result.has_value(), ());
  TEST_EQUAL(result->m_serversList.size(), 2, ());
  TEST_EQUAL(result->m_serversList[0], "http://url1", ());
  TEST_EQUAL(result->m_serversList[1], "http://url2", ());
  TEST_EQUAL(result->m_settings.size(), 2, ());
  TEST_EQUAL(result->m_settings["key1"], "value1", ());
  TEST_EQUAL(result->m_settings["key2"], "value2", ());

  TEST(!result->m_productsConfig.empty(), ());
  auto const productsConfigResult = products::ProductsConfig::Parse(result->m_productsConfig);
  TEST(productsConfigResult.has_value(), ());
  auto const productsConfig = productsConfigResult.value();
  TEST_EQUAL(productsConfig.GetPlacePagePrompt(), "prompt1", ());
  TEST(productsConfig.HasProducts(), ());
  auto const products = productsConfig.GetProducts();
  TEST_EQUAL(products.size(), 2, ());
}

UNIT_TEST(MetaConfig_JSONParser_MissingServersKey)
{
  std::string missingServersJson = R"({
    "settings": {
      "key1": "value1"
    }
  })";
  auto result = ParseMetaConfig(missingServersJson);
  TEST(!result.has_value(), ("JSON shouldn't be parsed without 'servers' key"));
}
