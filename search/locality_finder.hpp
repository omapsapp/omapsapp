#pragma once

#include "search/cities_boundaries_table.hpp"

#include "indexer/feature_utils.hpp"
#include "indexer/mwm_set.hpp"
#include "indexer/rank_table.hpp"

#include "platform/preferred_languages.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"
#include "geometry/tree4d.hpp"

#include "base/macros.hpp"

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

class DataSource;

namespace search
{
class VillagesCache;

struct LocalityItem
{
  using Boundaries = CitiesBoundariesTable::Boundaries;

  LocalityItem(StringUtf8Multilang const & names, m2::PointD const & center,
               Boundaries && boundaries, uint64_t population, FeatureID const & id);

  bool GetName(int8_t lang, std::string_view & name) const { return m_names.GetString(lang, name); }

  bool GetSpecifiedOrDefaultName(int8_t lang, std::string_view & name) const
  {
    return GetName(lang, name) || GetName(StringUtf8Multilang::kDefaultCode, name);
  }

  bool GetReadableName(std::string_view & name) const
  {
    auto const mwmInfo = m_id.m_mwmId.GetInfo();
    if (!mwmInfo)
      return false;

    feature::NameParamsOut out;
    feature::GetReadableName({ m_names, mwmInfo->GetRegionData(), languages::GetCurrentMapLanguage(),
                               false /* allowTranslit */ }, out);

    name = out.primary;
    return !name.empty();
  }

  StringUtf8Multilang m_names;
  m2::PointD m_center;
  Boundaries m_boundaries;
  uint64_t m_population;
  FeatureID m_id;
};

std::string DebugPrint(LocalityItem const & item);

class LocalitySelector
{
public:
  LocalitySelector(m2::PointD const & p);

  void operator()(LocalityItem const & item);

  template <typename Fn>
  bool WithBestLocality(Fn && fn) const
  {
    if (!m_locality)
      return false;
    fn(*m_locality);
    return true;
  }

private:
  m2::PointD const m_p;

  bool m_inside = false;
  double m_score = std::numeric_limits<double>::max();
  LocalityItem const * m_locality = nullptr;
};

class LocalityFinder
{
public:
  class Holder
  {
  public:
    Holder(double radiusMeters);

    bool IsCovered(m2::RectD const & rect) const;
    void SetCovered(m2::PointD const & p);

    void Add(LocalityItem const & item);
    void ForEachInVicinity(m2::RectD const & rect, LocalitySelector & selector) const;

    m2::RectD GetRect(m2::PointD const & p) const;
    m2::RectD GetDRect(m2::PointD const & p) const;

    void Clear();

  private:
    double const m_radiusMeters;
    m4::Tree<bool> m_coverage;
    m4::Tree<LocalityItem> m_localities;

    DISALLOW_COPY_AND_MOVE(Holder);
  };

  LocalityFinder(DataSource const & dataSource, CitiesBoundariesTable const & boundaries,
                 VillagesCache & villagesCache);

  template <typename Fn>
  bool GetLocality(m2::PointD const & p, Fn && fn)
  {
    m2::RectD const crect = m_cities.GetRect(p);
    m2::RectD const vrect = m_villages.GetRect(p);

    LoadVicinity(p, !m_cities.IsCovered(crect) /* loadCities */,
                 !m_villages.IsCovered(vrect) /* loadVillages */);

    LocalitySelector selector(p);
    m_cities.ForEachInVicinity(crect, selector);
    m_villages.ForEachInVicinity(vrect, selector);

    return selector.WithBestLocality(std::forward<Fn>(fn));
  }

  void ClearCache();

private:
  void LoadVicinity(m2::PointD const & p, bool loadCities, bool loadVillages);
  void UpdateMaps();

  DataSource const & m_dataSource;
  CitiesBoundariesTable const & m_boundariesTable;
  VillagesCache & m_villagesCache;

  Holder m_cities;
  Holder m_villages;

  m4::Tree<MwmSet::MwmId> m_maps;
  MwmSet::MwmId m_worldId;
  bool m_mapsLoaded;

  std::unique_ptr<RankTable> m_ranks;

  std::map<MwmSet::MwmId, std::unordered_set<uint32_t>> m_loadedIds;
};
}  // namespace search
