#include "drape_frontend/read_manager.hpp"
#include "drape_frontend/message_subclasses.hpp"
#include "drape_frontend/metaline_manager.hpp"
#include "drape_frontend/visual_params.hpp"

#include "base/buffer_vector.hpp"

#include <algorithm>
#include <functional>

namespace df
{
namespace
{
struct LessCoverageCell
{
  bool operator()(std::shared_ptr<TileInfo> const & l,
                  TileKey const & r) const
  {
    return l->GetTileKey() < r;
  }

  bool operator()(TileKey const & l,
                  std::shared_ptr<TileInfo> const & r) const
  {
    return l < r->GetTileKey();
  }

  bool operator()(std::shared_ptr<TileInfo> const & l,
                  std::shared_ptr<TileInfo> const & r) const
  {
    return l->GetTileKey() < r->GetTileKey();
  }
};
}  // namespace

bool ReadManager::LessByTileInfo::operator()(std::shared_ptr<TileInfo> const & l,
                                             std::shared_ptr<TileInfo> const & r) const
{
  return *l < *r;
}

ReadManager::ReadManager(ref_ptr<ThreadsCommutator> commutator, MapDataProvider & model,
                         bool allow3dBuildings, bool trafficEnabled, bool isolinesEnabled)
  : m_commutator(commutator)
  , m_model(model)
  , m_have3dBuildings(false)
  , m_allow3dBuildings(allow3dBuildings)
  , m_trafficEnabled(trafficEnabled)
  , m_isolinesEnabled(isolinesEnabled)
  , m_modeChanged(false)
  , m_mapLangIndex(StringUtf8Multilang::kDefaultCode)
  , m_tasksPool(64, ReadMWMTaskFactory(m_model))
  , m_counter(0)
  , m_generationCounter(0)
  , m_userMarksGenerationCounter(0)
{
  Start();
}

void ReadManager::Start()
{
  if (m_pool != nullptr)
    return;

  ASSERT_EQUAL(m_counter, 0, ());

  m_pool = make_unique_dp<base::ThreadPool>(kReadingThreadsCount,
                              std::bind(&ReadManager::OnTaskFinished, this, std::placeholders::_1));
}

void ReadManager::Stop()
{
  InvalidateAll();
  if (m_pool != nullptr)
    m_pool->Stop();
  m_pool.reset();
}

void ReadManager::Restart()
{
  Stop();
  Start();
}

void ReadManager::OnTaskFinished(threads::IRoutine * task)
{
  ASSERT(dynamic_cast<ReadMWMTask *>(task) != NULL, ());
  auto t = static_cast<ReadMWMTask *>(task);

  // finish tiles
  {
    std::lock_guard lock(m_finishedTilesMutex);

    // decrement counter
    ASSERT_GREATER(m_counter, 0, ());
    if (--m_counter == 0)
    {
      m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                make_unique_dp<FinishReadingMessage>(),
                                MessagePriority::Normal);
    }

    auto const & key = t->GetTileKey();
    if (!task->IsCancelled())
    {
      auto const it = m_activeTiles.find(key);
      ASSERT(it != m_activeTiles.end(), ());

      // Use the tile key from active tiles with the actual user marks generation.
      TTilesCollection tiles;
      tiles.emplace(*it);

      m_activeTiles.erase(it);

      m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                                make_unique_dp<FinishTileReadMessage>(std::move(tiles),
                                                                      true /* forceUpdateUserMarks */),
                                MessagePriority::Normal);
    }
  }

  t->Reset();
  m_tasksPool.Return(t);
}

void ReadManager::UpdateCoverage(ScreenBase const & screen, bool have3dBuildings,
                                 bool forceUpdate, bool forceUpdateUserMarks,
                                 TTilesCollection const & tiles,
                                 ref_ptr<dp::TextureManager> texMng,
                                 ref_ptr<MetalineManager> metalineMng)
{
  m_modeChanged |= (m_have3dBuildings != have3dBuildings);
  m_have3dBuildings = have3dBuildings;

  if (m_modeChanged || forceUpdate || MustDropAllTiles(screen))
  {
    m_modeChanged = false;

    for (auto const & info : m_tileInfos)
      CancelTileInfo(info);
    m_tileInfos.clear();

    IncreaseCounter(tiles.size());
    ++m_generationCounter;
    ++m_userMarksGenerationCounter;

    for (auto const & tileKey : tiles)
      PushTaskBackForTileKey(tileKey, texMng, metalineMng);
  }
  else
  {
    // Find rects that go out from viewport.
    TTileInfoCollection outdatedTiles;
    std::set_difference(m_tileInfos.begin(), m_tileInfos.end(),
                        tiles.begin(), tiles.end(),
                        std::back_inserter(outdatedTiles), LessCoverageCell());

    for (auto const & info : outdatedTiles)
      ClearTileInfo(info);

    // Find rects that go in into viewport.
    buffer_vector<TileKey, 8> newTiles;
    std::set_difference(tiles.begin(), tiles.end(),
                        m_tileInfos.begin(), m_tileInfos.end(),
                        std::back_inserter(newTiles), LessCoverageCell());

    // Find ready tiles.
    TTileInfoCollection readyTiles;
    std::set_difference(m_tileInfos.begin(), m_tileInfos.end(),
                        outdatedTiles.begin(), outdatedTiles.end(),
                        std::back_inserter(readyTiles), LessCoverageCell());

    IncreaseCounter(newTiles.size());

    if (forceUpdateUserMarks)
      ++m_userMarksGenerationCounter;
    CheckFinishedTiles(readyTiles, forceUpdateUserMarks);

    for (auto const & tileKey : newTiles)
      PushTaskBackForTileKey(tileKey, texMng, metalineMng);
  }

  m_currentViewport = screen;
}

void ReadManager::Invalidate(TTilesCollection const & keyStorage)
{
  TTileSet tilesToErase;
  for (auto const & info : m_tileInfos)
  {
    if (keyStorage.find(info->GetTileKey()) != keyStorage.end())
      tilesToErase.insert(info);
  }

  for (auto const & info : tilesToErase)
  {
    CancelTileInfo(info);
    m_tileInfos.erase(info);
  }
}

void ReadManager::InvalidateAll()
{
  for (auto const & info : m_tileInfos)
    CancelTileInfo(info);
  m_tileInfos.clear();

  m_modeChanged = true;
}

bool ReadManager::CheckTileKey(TileKey const & tileKey) const
{
  for (auto const & tileInfo : m_tileInfos)
  {
    if (tileInfo->GetTileKey() == tileKey)
      return !tileInfo->IsCancelled();
  }
  return false;
}

bool ReadManager::MustDropAllTiles(ScreenBase const & screen) const
{
  int const oldScale = df::GetDrawTileScale(m_currentViewport);
  int const newScale = df::GetDrawTileScale(screen);
  return (oldScale != newScale) || !m_currentViewport.GlobalRect().IsIntersect(screen.GlobalRect());
}

void ReadManager::PushTaskBackForTileKey(TileKey const & tileKey,
                                         ref_ptr<dp::TextureManager> texMng,
                                         ref_ptr<MetalineManager> metalineMng)
{
  ASSERT(m_pool != nullptr, ());
  auto context = make_unique_dp<EngineContext>(TileKey(tileKey, m_generationCounter,
                                                       m_userMarksGenerationCounter),
                                               m_commutator, texMng, metalineMng,
                                               m_customFeaturesContext,
                                               m_have3dBuildings && m_allow3dBuildings,
                                               m_trafficEnabled, m_isolinesEnabled, m_mapLangIndex);
  std::shared_ptr<TileInfo> tileInfo = std::make_shared<TileInfo>(std::move(context));
  m_tileInfos.insert(tileInfo);

  /// @todo Do we really need ReadMWMTask pool? Avoid "new" with hand-written bicycle? ;)
  ReadMWMTask * task = m_tasksPool.Get();

  task->Init(tileInfo);
  /// @todo Can we update m_activeTiles once like IncreaseCounter and lock mutex once?
  /// Or order is important here?
  {
    std::lock_guard lock(m_finishedTilesMutex);
    m_activeTiles.insert(TileKey(tileKey, m_generationCounter, m_userMarksGenerationCounter));
  }
  m_pool->PushBack(task);
}

void ReadManager::CheckFinishedTiles(TTileInfoCollection const & requestedTiles, bool forceUpdateUserMarks)
{
  if (requestedTiles.empty())
    return;

  TTilesCollection finishedTiles;

  std::lock_guard<std::mutex> lock(m_finishedTilesMutex);

  for (auto const & tile : requestedTiles)
  {
    auto const it = m_activeTiles.find(tile->GetTileKey());
    if (it == m_activeTiles.end())
    {
      finishedTiles.emplace(tile->GetTileKey(), m_generationCounter, m_userMarksGenerationCounter);
    }
    else if (forceUpdateUserMarks)
    {
      // In case of active tile reading update its user marks generation.
      // User marks will be generated after finishing of tile reading with correct tile key.
      auto tileKey = *it;
      tileKey.m_userMarksGeneration = m_userMarksGenerationCounter;
      m_activeTiles.erase(it);
      m_activeTiles.insert(tileKey);
    }
  }

  if (!finishedTiles.empty())
  {
    m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                              make_unique_dp<FinishTileReadMessage>(std::move(finishedTiles),
                                                                    forceUpdateUserMarks),
                              MessagePriority::Normal);
  }
}

void ReadManager::CancelTileInfo(std::shared_ptr<TileInfo> const & tileToCancel)
{
  std::lock_guard<std::mutex> lock(m_finishedTilesMutex);
  m_activeTiles.erase(tileToCancel->GetTileKey());
  tileToCancel->Cancel();
}

void ReadManager::ClearTileInfo(std::shared_ptr<TileInfo> const & tileToClear)
{
  CancelTileInfo(tileToClear);
  m_tileInfos.erase(tileToClear);
}

void ReadManager::IncreaseCounter(size_t value)
{
  if (value == 0)
    return;

  std::lock_guard<std::mutex> lock(m_finishedTilesMutex);

  ASSERT_GREATER_OR_EQUAL(m_counter, 0, ());
  m_counter += value;
}

void ReadManager::Allow3dBuildings(bool allow3dBuildings)
{
  if (m_allow3dBuildings != allow3dBuildings)
  {
    m_modeChanged = true;
    m_allow3dBuildings = allow3dBuildings;
  }
}

void ReadManager::SetMapLangIndex(int8_t mapLangIndex)
{
  if (m_mapLangIndex != mapLangIndex)
  {
    m_modeChanged = true;
    m_mapLangIndex = mapLangIndex;
  }
}

void ReadManager::SetTrafficEnabled(bool trafficEnabled)
{
  if (m_trafficEnabled != trafficEnabled)
  {
    m_modeChanged = true;
    m_trafficEnabled = trafficEnabled;
  }
}

void ReadManager::SetIsolinesEnabled(bool isolinesEnabled)
{
  if (m_isolinesEnabled != isolinesEnabled)
  {
    m_modeChanged = true;
    m_isolinesEnabled = isolinesEnabled;
  }
}

void ReadManager::SetCustomFeatures(CustomFeatures && ids)
{
  m_customFeaturesContext = std::make_shared<CustomFeaturesContext>(std::move(ids));
}

std::vector<FeatureID> ReadManager::GetCustomFeaturesArray() const
{
  if (!m_customFeaturesContext)
    return {};
  std::vector<FeatureID> features;
  features.reserve(m_customFeaturesContext->m_features.size());
  for (auto const & s : m_customFeaturesContext->m_features)
    features.push_back(s.first);
  return features;
}

bool ReadManager::RemoveCustomFeatures(MwmSet::MwmId const & mwmId)
{
  if (!m_customFeaturesContext)
    return false;

  CustomFeatures features;
  for (auto const & s : m_customFeaturesContext->m_features)
  {
    if (s.first.m_mwmId != mwmId)
      features.insert(s);
  }
  if (features.size() == m_customFeaturesContext->m_features.size())
    return false;

  m_customFeaturesContext = std::make_shared<CustomFeaturesContext>(std::move(features));
  return true;
}

bool ReadManager::RemoveAllCustomFeatures()
{
  if (!m_customFeaturesContext || m_customFeaturesContext->m_features.empty())
    return false;

  m_customFeaturesContext = std::make_shared<CustomFeaturesContext>(CustomFeatures());
  return true;
}

} // namespace df
