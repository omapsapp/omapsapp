#pragma once

#include "drape_frontend/engine_context.hpp"
#include "drape_frontend/read_mwm_task.hpp"
#include "drape_frontend/tile_info.hpp"
#include "drape_frontend/tile_utils.hpp"

#include "geometry/screenbase.hpp"

#include "drape/object_pool.hpp"
#include "drape/pointers.hpp"

#include "base/thread_pool.hpp"

#include <memory>
#include <mutex>
#include <set>
#include <vector>

namespace dp
{
class TextureManager;
}  // namespace dp

namespace df
{
class MapDataProvider;
class MetalineManager;

uint8_t constexpr kReadingThreadsCount = 2;

class ReadManager
{
public:
  ReadManager(ref_ptr<ThreadsCommutator> commutator, MapDataProvider & model,
              bool allow3dBuildings, bool trafficEnabled, bool isolinesEnabled);

  void Start();
  void Stop();
  void Restart();

  void UpdateCoverage(ScreenBase const & screen, bool have3dBuildings,
                      bool forceUpdate, bool forceUpdateUserMarks,
                      TTilesCollection const & tiles, ref_ptr<dp::TextureManager> texMng,
                      ref_ptr<MetalineManager> metalineMng);
  void Invalidate(TTilesCollection const & keyStorage);
  void InvalidateAll();

  bool CheckTileKey(TileKey const & tileKey) const;
  void Allow3dBuildings(bool allow3dBuildings);

  void SetMapLangIndex(int8_t mapLangIndex);

  void SetTrafficEnabled(bool trafficEnabled);
  void SetIsolinesEnabled(bool isolinesEnabled);

  void SetCustomFeatures(CustomFeatures && ids);
  std::vector<FeatureID> GetCustomFeaturesArray() const;
  bool RemoveCustomFeatures(MwmSet::MwmId const & mwmId);
  bool RemoveAllCustomFeatures();

  bool IsModeChanged() const { return m_modeChanged; }

  void EnableUGCRendering(bool enabled);

  MapDataProvider & GetMapDataProvider() { return m_model; }

private:
  void OnTaskFinished(threads::IRoutine * task);
  bool MustDropAllTiles(ScreenBase const & screen) const;

  void PushTaskBackForTileKey(TileKey const & tileKey, ref_ptr<dp::TextureManager> texMng,
                              ref_ptr<MetalineManager> metalineMng);

  ref_ptr<ThreadsCommutator> m_commutator;

  MapDataProvider & m_model;

  drape_ptr<base::ThreadPool> m_pool;

  ScreenBase m_currentViewport;
  bool m_have3dBuildings;
  bool m_allow3dBuildings;
  bool m_trafficEnabled;
  bool m_isolinesEnabled;
  bool m_modeChanged;
  int8_t m_mapLangIndex;

  struct LessByTileInfo
  {
    bool operator ()(std::shared_ptr<TileInfo> const & l,
                     std::shared_ptr<TileInfo> const & r) const;
  };

  using TTileSet = std::set<std::shared_ptr<TileInfo>, LessByTileInfo>;
  TTileSet m_tileInfos;

  dp::ObjectPool<ReadMWMTask, ReadMWMTaskFactory> m_tasksPool;

  int m_counter;
  std::mutex m_finishedTilesMutex;
  uint64_t m_generationCounter;
  uint64_t m_userMarksGenerationCounter;

  using TTileInfoCollection = buffer_vector<std::shared_ptr<TileInfo>, 8>;
  TTilesCollection m_activeTiles;

  CustomFeaturesContextPtr m_customFeaturesContext;

  void CancelTileInfo(std::shared_ptr<TileInfo> const & tileToCancel);
  void ClearTileInfo(std::shared_ptr<TileInfo> const & tileToClear);
  void IncreaseCounter(size_t value);
  void CheckFinishedTiles(TTileInfoCollection const & requestedTiles, bool forceUpdateUserMarks);
};
}  // namespace df
