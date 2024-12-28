#include "drape_frontend/tile_info.hpp"
#include "drape_frontend/drape_measurer.hpp"
#include "drape_frontend/engine_context.hpp"
#include "drape_frontend/map_data_provider.hpp"
#include "drape_frontend/metaline_manager.hpp"
#include "drape_frontend/rule_drawer.hpp"
#include "drape_frontend/stylist.hpp"

#include "indexer/scales.hpp"

#include "platform/preferred_languages.hpp"
#include "platform/settings.hpp"

#include "base/scope_guard.hpp"
#include "base/logging.hpp"

#include <algorithm>
#include <functional>

using namespace std::placeholders;

namespace df
{
TileInfo::TileInfo(drape_ptr<EngineContext> && engineContext)
  : m_context(std::move(engineContext))
  , m_isCanceled(false)
{}

m2::RectD TileInfo::GetGlobalRect() const
{
  return GetTileKey().GetGlobalRect();
}

void TileInfo::ReadFeatureIndex(MapDataProvider const & model)
{
  if (!DoNeedReadIndex())
    return;

  ThrowIfCancelled();

  size_t const kAverageFeaturesCount = 256;
  m_featureInfo.reserve(kAverageFeaturesCount);

  MwmSet::MwmId lastMwm;
  model.ReadFeaturesID([this, &lastMwm](FeatureID const & id)
  {
    if (m_mwms.empty() || lastMwm != id.m_mwmId)
    {
      auto result = m_mwms.insert(id.m_mwmId);
      VERIFY(result.second, ());
      lastMwm = id.m_mwmId;
    }
    m_featureInfo.push_back(id);
  }, GetGlobalRect(), GetZoomLevel());
}

void TileInfo::ReadFeatures(MapDataProvider const & model)
{
#if defined(DRAPE_MEASURER_BENCHMARK) && defined(TILES_STATISTIC)
  DrapeMeasurer::Instance().StartTileReading();
#endif
  m_context->BeginReadTile();

  // Reading can be interrupted by exception throwing
  SCOPE_GUARD(ReleaseReadTile, std::bind(&EngineContext::EndReadTile, m_context.get()));

  ReadFeatureIndex(model);
  ThrowIfCancelled();

  m_context->GetMetalineManager()->Update(m_mwms);

  if (!m_featureInfo.empty())
  {
    std::sort(m_featureInfo.begin(), m_featureInfo.end());

    RuleDrawer drawer(std::bind(&TileInfo::IsCancelled, this), model.m_isCountryLoadedByName,
                      make_ref(m_context), m_context->GetMapLangIndex());
    model.ReadFeatures(std::bind<void>(std::ref(drawer), _1), m_featureInfo);
#ifdef DRAW_TILE_NET
    drawer.DrawTileNet();
#endif
  }
#if defined(DRAPE_MEASURER_BENCHMARK) && defined(TILES_STATISTIC)
  DrapeMeasurer::Instance().EndTileReading();
#endif
}

void TileInfo::Cancel()
{
  m_isCanceled = true;
}

/*
 * TODO: the following check throws an exception while IsCancelled() is used in most places to quit gracefully.
 * Looks like the latter was added later, so maybe the throwing version is not needed anymore.
 */
void TileInfo::ThrowIfCancelled() const
{
  // The exception is handled in ReadMWMTask::Do().
  if (m_isCanceled)
    MYTHROW(ReadCanceledException, ());
}

bool TileInfo::IsCancelled() const
{
  return m_isCanceled;
}

bool TileInfo::DoNeedReadIndex() const
{
  return m_featureInfo.empty();
}

int TileInfo::GetZoomLevel() const
{
  return ClipTileZoomByMaxDataZoom(m_context->GetTileKey().m_zoomLevel);
}
}  // namespace df
