#import "MWMObjectsCategorySelectorDataSource.h"

#include "LocaleTranslator.h"

#include <CoreApi/Framework.h>

#include "platform/localization.hpp"


namespace
{
using Category = std::pair<std::string, osm::NewFeatureCategories::TypeName>;
using Categories = std::vector<Category>;

std::string locale()
{
  return locale_translator::bcp47ToTwineLanguage(NSLocale.currentLocale.localeIdentifier);
}
}  // namespace

@interface MWMObjectsCategorySelectorDataSource()
{
  osm::NewFeatureCategories m_categories;
  Categories m_categoriesList;
}

@end

@implementation MWMObjectsCategorySelectorDataSource

- (instancetype)init
{
  self = [super init];
  if (self)
    [self load];
    
  return self;
}

- (void)initializeList:(osm::NewFeatureCategories::TypeNames const &)types
{
  m_categoriesList.clear();
  for (auto const & type : types)
    m_categoriesList.emplace_back(platform::GetLocalizedTypeName(type), type);

  std::sort(m_categoriesList.begin(), m_categoriesList.end());
}

- (void)load
{
  m_categories = GetFramework().GetEditorCategories();
  m_categories.AddLanguage(locale());
  m_categories.AddLanguage("en");

  auto const & types = m_categories.GetAllCreatableTypeNames();
  m_categoriesList.reserve(types.size());

  [self initializeList: types];
}

- (void)search:(NSString *)query
{
  if (query.length == 0)
    [self initializeList: m_categories.GetAllCreatableTypeNames()];
  else
    [self initializeList: m_categories.Search([query UTF8String])];
}

- (NSString *)getTranslation:(NSInteger)row
{
  return @(m_categoriesList[row].first.c_str());
}

- (NSString *)getType:(NSInteger)row
{
  return @(m_categoriesList[row].second.c_str());
}

- (NSInteger)size
{
  return m_categoriesList.size();
}

@end
