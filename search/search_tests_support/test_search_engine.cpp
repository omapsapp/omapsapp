#include "search/search_tests_support/test_search_engine.hpp"

#include "indexer/categories_holder.hpp"

#include "platform/platform.hpp"

#include <utility>

namespace search
{
namespace tests_support
{
using namespace std;

TestSearchEngine::TestSearchEngine(DataSource & dataSource, Engine::Params const & params, bool mockCountryInfo)
  : m_infoGetter(mockCountryInfo ?
                   make_unique<storage::CountryInfoGetterForTesting>() :
                   storage::CountryInfoReader::CreateCountryInfoGetter(GetPlatform()))
  , m_engine(dataSource, GetDefaultCategories(), *m_infoGetter, params)
{
}

weak_ptr<ProcessorHandle> TestSearchEngine::Search(SearchParams const & params)
{
  return m_engine.Search(params);
}
}  // namespace tests_support
}  // namespace search
