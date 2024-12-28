#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>

namespace products {

struct ProductsConfig
{
  struct Product
  {
  private:
    std::string m_title;
    std::string m_link;

  public:
    Product(std::string const & title, std::string const & link)
    : m_title(title), m_link(link)
    {}

    std::string const & GetTitle() const { return m_title; }
    std::string const & GetLink() const { return m_link; }
  };

private:
  std::string m_placePagePrompt;
  std::vector<Product> m_products;

public:
  std::string const GetPlacePagePrompt() const { return m_placePagePrompt; }
  std::vector<Product> const & GetProducts() const { return m_products; }
  bool HasProducts() const { return !m_products.empty(); }

  static std::optional<ProductsConfig> Parse(std::string const & jsonStr);
};

class ProductsSettings
{
private:
  ProductsSettings();

  std::optional<ProductsConfig> m_productsConfig;
  mutable std::mutex m_mutex;

public:
  static ProductsSettings & Instance();

  void Update(std::string const & jsonStr);
  std::optional<ProductsConfig> Get();
};

inline void Update(std::string const & jsonStr)
{
  ProductsSettings::Instance().Update(jsonStr);
}

inline std::optional<ProductsConfig> GetProductsConfiguration()
{
  return ProductsSettings::Instance().Get();
}

} // namespace products
