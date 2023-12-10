#include "map/search_mark.hpp"

#include "map/bookmark_manager.hpp"
#include "map/place_page_info.hpp"

#include "drape_frontend/drape_engine.hpp"

#include "indexer/scales.hpp"

#include "platform/platform.hpp"

#include "base/stl_helpers.hpp"

#include <algorithm>
#include <array>
#include <limits>


enum SearchMarkPoint::SearchMarkType : uint8_t
{
  Default = 0,
  Hotel,
  Cafe,
  Bakery,
  Bar,
  Pub,
  Restaurant,
  FastFood,
  Casino,
  Cinema,
  Marketplace,
  Nightclub,
  Playground,
  ShopAlcohol,
  ShopButcher,
  ShopClothes,
  ShopConfectionery,
  ShopConvenience,
  ShopCosmetics,
  ShopDepartmentStore,
  ShopGift,
  ShopGreengrocer,
  ShopJewelry,
  ShopMall,
  ShopSeafood,
  ShopShoes,
  ShopSports,
  ShopSupermarket,
  ShopToys,
  ThemePark,
  WaterPark,
  Zoo,

  NotFound,  // Service value used in developer tools.
  Count
};

using SearchMarkType = SearchMarkPoint::SearchMarkType;

namespace
{
df::ColorConstant const kPoiVisitedMaskColor = "PoiVisitedMask";
df::ColorConstant const kColorConstant = "SearchmarkDefault";

float const kVisitedSymbolOpacity = 0.7f;
float const kOutOfFiltersSymbolOpacity = 0.4f;

std::array<std::string, SearchMarkType::Count> const kSymbols = {
    "search-result",                        // Default.
    "norating-default-l",                   // Hotel
    "search-result-cafe",                   // Cafe.
    "search-result-bakery",                 // Bakery.
    "search-result-bar",                    // Bar.
    "search-result-pub",                    // Pub.
    "search-result-restaurant",             // Restaurant.
    "search-result-fastfood",               // FastFood.
    "search-result-casino",                 // Casino.
    "search-result-cinema",                 // Cinema.
    "search-result-marketplace",            // Marketplace.
    "search-result-nightclub",              // Nightclub.
    "search-result-playground",             // Playground.
    "search-result-shop-alcohol",           // ShopAlcohol.
    "search-result-shop-butcher",           // ShopButcher.
    "search-result-shop-clothes",           // ShopClothes.
    "search-result-shop-confectionery",     // ShopConfectionery.
    "search-result-shop-convenience",       // ShopConvenience.
    "search-result-shop-cosmetics",         // ShopCosmetics.
    "search-result-shop-department_store",  // ShopDepartmentStore.
    "search-result-shop-gift",              // ShopGift.
    "search-result-shop-greengrocer",       // ShopGreengrocer.
    "search-result-shop-jewelry",           // ShopJewelry.
    "search-result-shop-mall",              // ShopMall.
    "search-result-shop-seafood",           // ShopSeafood.
    "search-result-shop-shoes",             // ShopShoes.
    "search-result-shop-sports",            // ShopSports.
    "search-result-shop-supermarket",       // ShopSupermarket.
    "search-result-shop-toys",              // ShopToys.
    "search-result-theme-park",             // ThemePark.
    "search-result-water-park",             // WaterPark.
    "search-result-zoo",                    // Zoo.

    "non-found-search-result",  // NotFound.
};

std::string const & GetSymbol(SearchMarkType searchMarkType)
{
  ASSERT_LESS(searchMarkType, kSymbols.size(), ());
  return kSymbols[searchMarkType];
}

class SearchMarkTypeChecker
{
public:
  static SearchMarkTypeChecker & Instance()
  {
    static SearchMarkTypeChecker checker;
    return checker;
  }

  SearchMarkType GetSearchMarkType(uint32_t type) const
  {
    auto const it = std::partition_point(m_searchMarkTypes.cbegin(), m_searchMarkTypes.cend(),
                                         [type](auto && t) { return t.first < type; });
    if (it == m_searchMarkTypes.cend() || it->first != type)
      return SearchMarkType::Default;

    return it->second;
  }

private:
  using Type = std::pair<uint32_t, SearchMarkType>;

  SearchMarkTypeChecker()
  {
    auto const & c = classif();
    std::vector<std::pair<std::vector<std::string>, SearchMarkType>> const table = {
      {{"amenity", "cafe"},          SearchMarkType::Cafe},
      {{"shop", "bakery"},           SearchMarkType::Bakery},
      {{"amenity", "bar"},           SearchMarkType::Bar},
      {{"amenity", "pub"},           SearchMarkType::Pub},
      {{"amenity", "biergarten"},    SearchMarkType::Pub},
      {{"amenity", "restaurant"},    SearchMarkType::Restaurant},
      {{"amenity", "fast_food"},     SearchMarkType::FastFood},
      {{"amenity", "casino"},        SearchMarkType::Casino},
      {{"amenity", "cinema"},        SearchMarkType::Cinema},
      {{"amenity", "marketplace"},   SearchMarkType::Marketplace},
      {{"amenity", "nightclub"},     SearchMarkType::Nightclub},
      {{"leisure", "playground"},    SearchMarkType::Playground},
      {{"shop", "alcohol"},          SearchMarkType::ShopAlcohol},
      {{"shop", "beverages"},        SearchMarkType::ShopAlcohol},
      {{"shop", "wine"},             SearchMarkType::ShopAlcohol},
      {{"shop", "butcher"},          SearchMarkType::ShopButcher},
      {{"shop", "clothes"},          SearchMarkType::ShopClothes},
      {{"shop", "confectionery"},    SearchMarkType::ShopConfectionery},
      {{"shop", "convenience"},      SearchMarkType::ShopConvenience},
      {{"shop", "variety_store"},    SearchMarkType::ShopConvenience},
      {{"shop", "cosmetics"},        SearchMarkType::ShopCosmetics},
      {{"shop", "department_store"}, SearchMarkType::ShopDepartmentStore},
      {{"shop", "gift"},             SearchMarkType::ShopGift},
      {{"shop", "greengrocer"},      SearchMarkType::ShopGreengrocer},
      {{"shop", "jewelry"},          SearchMarkType::ShopJewelry},
      {{"shop", "mall"},             SearchMarkType::ShopMall},
      {{"shop", "seafood"},          SearchMarkType::ShopSeafood},
      {{"shop", "shoes"},            SearchMarkType::ShopShoes},
      {{"shop", "sports"},           SearchMarkType::ShopSports},
      {{"shop", "supermarket"},      SearchMarkType::ShopSupermarket},
      {{"shop", "toys"},             SearchMarkType::ShopToys},
      {{"tourism", "theme_park"},    SearchMarkType::ThemePark},
      {{"leisure", "water_park"},    SearchMarkType::WaterPark},
      {{"tourism", "zoo"},           SearchMarkType::Zoo}
    };

    m_searchMarkTypes.reserve(table.size());
    for (auto const & p : table)
      m_searchMarkTypes.push_back({c.GetTypeByPath(p.first), p.second});

    std::sort(m_searchMarkTypes.begin(), m_searchMarkTypes.end());
  }

  std::vector<Type> m_searchMarkTypes;
};

SearchMarkType GetSearchMarkType(uint32_t type)
{
  auto const & checker = SearchMarkTypeChecker::Instance();
  return checker.GetSearchMarkType(type);
}
}  // namespace

SearchMarkPoint::SearchMarkPoint(m2::PointD const & ptOrg)
: UserMark(ptOrg, UserMark::Type::SEARCH)
, m_type(SearchMarkType::Default)
, m_isPreparing(false)
, m_hasSale(false)
, m_isSelected(false)
, m_isVisited(false)
, m_isAvailable(true)
{
}

m2::PointD SearchMarkPoint::GetPixelOffset() const
{
  if (!IsHotel())
    return {0.0, 4.0};
  return {0.0, 0.0};
}

drape_ptr<df::UserPointMark::SymbolNameZoomInfo> SearchMarkPoint::GetSymbolNames() const
{
  auto const symbolName = GetSymbolName();
  if (symbolName == nullptr)
    return nullptr;

  auto symbolZoomInfo = make_unique_dp<SymbolNameZoomInfo>();
  symbolZoomInfo->Emplace(1, *symbolName);
  return symbolZoomInfo;
}

drape_ptr<df::UserPointMark::SymbolOffsets> SearchMarkPoint::GetSymbolOffsets() const
{
  m2::PointF offset(0, 0);
  if (!IsHotel())
    offset.y = 1;
  return make_unique_dp<SymbolOffsets>(static_cast<size_t>(scales::UPPER_STYLE_SCALE), offset);
}

bool SearchMarkPoint::IsMarkAboveText() const
{
  return true;
}

float SearchMarkPoint::GetSymbolOpacity() const
{
  if (!m_isAvailable)
    return kOutOfFiltersSymbolOpacity;
  return m_isVisited ? kVisitedSymbolOpacity : 1.0f;
}

df::ColorConstant SearchMarkPoint::GetColorConstant() const
{
  return kColorConstant;
}

drape_ptr<df::UserPointMark::TitlesInfo> SearchMarkPoint::GetTitleDecl() const
{
  return {};
}

int SearchMarkPoint::GetMinTitleZoom() const
{
  return scales::GetUpperCountryScale();
}

df::DepthLayer SearchMarkPoint::GetDepthLayer() const
{
  return df::DepthLayer::SearchMarkLayer;
}

void SearchMarkPoint::SetFoundFeature(FeatureID const & feature)
{
  SetAttributeValue(m_featureID, feature);
}

void SearchMarkPoint::SetMatchedName(std::string const & name)
{
  SetAttributeValue(m_matchedName, name);
}

void SearchMarkPoint::SetFromType(uint32_t type)
{
  SetAttributeValue(m_type, GetSearchMarkType(type));
}

void SearchMarkPoint::SetHotelType()
{
  SetAttributeValue(m_type, SearchMarkType::Hotel);
}

void SearchMarkPoint::SetNotFoundType()
{
  SetAttributeValue(m_type, SearchMarkType::NotFound);
}

#define SET_BOOL_ATTRIBUTE(dest, src) if (dest != src) { dest = src; SetDirty(); }

void SearchMarkPoint::SetPreparing(bool isPreparing)
{
  SET_BOOL_ATTRIBUTE(m_isPreparing, isPreparing);
}

void SearchMarkPoint::SetSale(bool hasSale)
{
  SET_BOOL_ATTRIBUTE(m_hasSale, hasSale);
}

void SearchMarkPoint::SetSelected(bool isSelected)
{
  SET_BOOL_ATTRIBUTE(m_isSelected, isSelected);
}

void SearchMarkPoint::SetVisited(bool isVisited)
{
  SET_BOOL_ATTRIBUTE(m_isVisited, isVisited);
}

void SearchMarkPoint::SetAvailable(bool isAvailable)
{
  SET_BOOL_ATTRIBUTE(m_isAvailable, isAvailable);
}

#undef SET_BOOL_ATTRIBUTE

void SearchMarkPoint::SetReason(std::string const & reason)
{
  SetAttributeValue(m_reason, reason);
}

bool SearchMarkPoint::IsSelected() const { return m_isSelected; }

bool SearchMarkPoint::IsAvailable() const { return m_isAvailable; }

std::string const & SearchMarkPoint::GetReason() const { return m_reason; }

bool SearchMarkPoint::IsHotel() const { return m_type == SearchMarkType::Hotel; }

bool SearchMarkPoint::HasReason() const { return !m_reason.empty(); }

std::string const * SearchMarkPoint::GetSymbolName() const
{
  std::string const * symbolName = nullptr;

  if (!SearchMarks::HaveSizes())
    return symbolName;

  if (m_type >= SearchMarkType::Count)
  {
    ASSERT(false, (m_type));
    symbolName = &GetSymbol(SearchMarkType::Default);
  }
  else
  {
    symbolName = &GetSymbol(m_type);
  }

  if (!SearchMarks::GetSize(*symbolName))
  {
    ASSERT(false, (*symbolName));
    symbolName = nullptr;
  }

  return symbolName;
}

// static
std::map<std::string, m2::PointF> SearchMarks::s_markSizes;

SearchMarks::SearchMarks()
  : m_bmManager(nullptr)
{}

void SearchMarks::SetDrapeEngine(ref_ptr<df::DrapeEngine> engine)
{
  m_drapeEngine.Set(engine);
  if (engine == nullptr)
    return;

  std::vector<std::string> symbols;
  for (uint8_t t = 0; t < SearchMarkType::Count; ++t)
    symbols.push_back(GetSymbol(static_cast<SearchMarkType>(t)));

  base::SortUnique(symbols);

  m_drapeEngine.SafeCall(&df::DrapeEngine::RequestSymbolsSize, symbols,
                         [this](std::map<std::string, m2::PointF> && sizes)
  {
    GetPlatform().RunTask(Platform::Thread::Gui, [this, sizes = std::move(sizes)]() mutable
    {
      s_markSizes = std::move(sizes);
      UpdateMaxDimension();
    });
  });
}

void SearchMarks::SetBookmarkManager(BookmarkManager * bmManager)
{
  m_bmManager = bmManager;
}

m2::PointD SearchMarks::GetMaxDimension(ScreenBase const & modelView) const
{
  return m_maxDimension * modelView.GetScale();
}

// static
std::optional<m2::PointD> SearchMarks::GetSize(std::string const & symbolName)
{
  auto const it = s_markSizes.find(symbolName);
  if (it == s_markSizes.end())
    return {};
  return m2::PointD(it->second);
}

void SearchMarks::SetPreparingState(std::vector<FeatureID> const & features, bool isPreparing)
{
  if (features.empty())
    return;

  ProcessMarks([&features, isPreparing](SearchMarkPoint * mark) -> base::ControlFlow
  {
    ASSERT(std::is_sorted(features.cbegin(), features.cend()), ());
    if (std::binary_search(features.cbegin(), features.cend(), mark->GetFeatureID()))
      mark->SetPreparing(isPreparing);
    return base::ControlFlow::Continue;
  });
}

void SearchMarks::SetSales(std::vector<FeatureID> const & features, bool hasSale)
{
  if (features.empty())
    return;

  ProcessMarks([&features, hasSale](SearchMarkPoint * mark) -> base::ControlFlow
  {
    ASSERT(std::is_sorted(features.cbegin(), features.cend()), ());
    if (std::binary_search(features.cbegin(), features.cend(), mark->GetFeatureID()))
      mark->SetSale(hasSale);
    return base::ControlFlow::Continue;
  });
}

bool SearchMarks::IsThereSearchMarkForFeature(FeatureID const & featureId) const
{
  for (auto const markId : m_bmManager->GetUserMarkIds(UserMark::Type::SEARCH))
  {
    if (m_bmManager->GetUserMark(markId)->GetFeatureID() == featureId)
      return true;
  }
  return false;
}

void SearchMarks::OnActivate(FeatureID const & featureId)
{
  m_selectedFeature = featureId;
  m_visitedSearchMarks.erase(featureId);
  ProcessMarks([&featureId](SearchMarkPoint * mark) -> base::ControlFlow
  {
    if (featureId != mark->GetFeatureID())
      return base::ControlFlow::Continue;
    mark->SetVisited(false);
    mark->SetSelected(true);
    return base::ControlFlow::Break;
  });
}

void SearchMarks::OnDeactivate(FeatureID const & featureId)
{
  m_selectedFeature = {};
  m_visitedSearchMarks.insert(featureId);
  ProcessMarks([&featureId](SearchMarkPoint * mark) -> base::ControlFlow
  {
    if (featureId != mark->GetFeatureID())
      return base::ControlFlow::Continue;
    mark->SetVisited(true);
    mark->SetSelected(false);
    return base::ControlFlow::Break;
  });
}

/*
void SearchMarks::SetUnavailable(SearchMarkPoint & mark, std::string const & reasonKey)
{
  {
    std::scoped_lock<std::mutex> lock(m_lock);
    m_unavailable.insert_or_assign(mark.GetFeatureID(), reasonKey);
  }
  mark.SetAvailable(false);
  mark.SetReason(platform::GetLocalizedString(reasonKey));
}

void SearchMarks::SetUnavailable(std::vector<FeatureID> const & features,
                                 std::string const & reasonKey)
{
  if (features.empty())
    return;

  ProcessMarks([this, &features, &reasonKey](SearchMarkPoint * mark) -> base::ControlFlow
  {
    ASSERT(std::is_sorted(features.cbegin(), features.cend()), ());
    if (std::binary_search(features.cbegin(), features.cend(), mark->GetFeatureID()))
      SetUnavailable(*mark, reasonKey);
    return base::ControlFlow::Continue;
  });
}

bool SearchMarks::IsUnavailable(FeatureID const & id) const
{
  std::scoped_lock<std::mutex> lock(m_lock);
  return m_unavailable.find(id) != m_unavailable.cend();
}
*/

void SearchMarks::SetVisited(FeatureID const & id)
{
  m_visitedSearchMarks.insert(id);
}

bool SearchMarks::IsVisited(FeatureID const & id) const
{
  return m_visitedSearchMarks.find(id) != m_visitedSearchMarks.cend();
}

void SearchMarks::SetSelected(FeatureID const & id)
{
  m_selectedFeature = id;
}

bool SearchMarks::IsSelected(FeatureID const & id) const
{
  return id == m_selectedFeature;
}

void SearchMarks::ClearTrackedProperties()
{
//  {
//    std::scoped_lock<std::mutex> lock(m_lock);
//    m_unavailable.clear();
//  }
  m_selectedFeature = {};
}

void SearchMarks::ProcessMarks(std::function<base::ControlFlow(SearchMarkPoint *)> && processor) const
{
  if (m_bmManager == nullptr || processor == nullptr)
    return;

  auto editSession = m_bmManager->GetEditSession();
  for (auto markId : m_bmManager->GetUserMarkIds(UserMark::Type::SEARCH))
  {
    auto * mark = editSession.GetMarkForEdit<SearchMarkPoint>(markId);
    if (processor(mark) == base::ControlFlow::Break)
      break;
  }
}

void SearchMarks::UpdateMaxDimension()
{
  // Use only generic search mark as 'filter' dimension. Users claim about missing results.
  // https://github.com/organicmaps/organicmaps/issues/2070
  auto const it = s_markSizes.find(kSymbols[SearchMarkType::Default]);
  if (it != s_markSizes.end())
    m_maxDimension = it->second;
}
