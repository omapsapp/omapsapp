#include "platform/products.hpp"
#include "platform/platform.hpp"

#include "base/logging.hpp"
#include "base/assert.hpp"
#include "base/string_utils.hpp"

#include "defines.hpp"

#include "coding/file_writer.hpp"

#include "cppjansson/cppjansson.hpp"

namespace products {

char const kPlacePagePrompt[] = "placePagePrompt";
char const kProducts[] = "products";
char const kProductTitle[] = "title";
char const kProductLink[] = "link";

std::string GetProductsFilePath()
{
  return GetPlatform().SettingsPathForFile(PRODUCTS_SETTINGS_FILE_NAME);
}

ProductsSettings::ProductsSettings()
{
  std::lock_guard guard(m_mutex);
  auto const path = GetProductsFilePath();
  if (Platform::IsFileExistsByFullPath(path))
  {
    try
    {
      std::string outValue;
      auto dataReader = GetPlatform().GetReader(path);
      dataReader->ReadAsString(outValue);
      m_productsConfig = ProductsConfig::Parse(outValue);
    }
    catch (std::exception const & ex)
    {
      LOG(LERROR, ("Error reading ProductsConfig file.", ex.what()));
    }
  }
  LOG(LWARNING, ("ProductsConfig file not found."));
}

ProductsSettings & ProductsSettings::Instance()
{
  static ProductsSettings instance;
  return instance;
}

std::optional<ProductsConfig> ProductsSettings::Get()
{
  std::lock_guard guard(m_mutex);
  return m_productsConfig;
}

void ProductsSettings::Update(std::string const & jsonStr)
{
  std::lock_guard guard(m_mutex);
  if (jsonStr.empty())
    FileWriter::DeleteFileX(GetProductsFilePath());
  else
  {
    try
    {
      FileWriter file(GetProductsFilePath());
      file.Write(jsonStr.data(), jsonStr.size());
      m_productsConfig = ProductsConfig::Parse(jsonStr);
    }
    catch (std::exception const & ex)
    {
      LOG(LERROR, ("Error writing ProductsConfig file.", ex.what()));
    }
  }
}

std::optional<ProductsConfig> ProductsConfig::Parse(std::string const & jsonStr)
{
  const base::Json root(jsonStr.c_str());
  auto const json = root.get();
  auto const productsObj = json_object_get(json, kProducts);
  if (!json_is_object(json) || !productsObj || !json_is_array(productsObj))
  {
    LOG(LWARNING, ("Failed to parse ProductsConfig:", jsonStr));
    return std::nullopt;
  }

  ProductsConfig config;
  auto const placePagePrompt = json_object_get(json, kPlacePagePrompt);
  if (placePagePrompt && json_is_string(placePagePrompt))
    config.m_placePagePrompt = json_string_value(placePagePrompt);

  for (size_t i = 0; i < json_array_size(productsObj); ++i)
  {
    json_t * product = json_array_get(productsObj, i);
    if (!product || !json_is_object(product))
    {
      LOG(LWARNING, ("Failed to parse Product:", jsonStr));
      continue;
    }
    json_t * title = json_object_get(product, kProductTitle);
    json_t * link = json_object_get(product, kProductLink);
    if (title && link && json_is_string(title) && json_is_string(link))
      config.m_products.push_back({json_string_value(title), json_string_value(link)});
    else
      LOG(LWARNING, ("Failed to parse Product:", jsonStr));
  }
  if (config.m_products.empty())
  {
    LOG(LWARNING, ("Products list is empty"));
    return std::nullopt;
  }
  return config;
}

} // namespace products
