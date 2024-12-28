#include "drape_frontend/engine_context.hpp"

#include "drape_frontend/message_subclasses.hpp"
#include "drape/texture_manager.hpp"

#include <utility>

namespace df
{
EngineContext::EngineContext(TileKey tileKey,
                             ref_ptr<ThreadsCommutator> commutator,
                             ref_ptr<dp::TextureManager> texMng,
                             ref_ptr<MetalineManager> metalineMng,
                             CustomFeaturesContextWeakPtr customFeaturesContext,
                             bool is3dBuildingsEnabled,
                             bool isTrafficEnabled,
                             bool isolinesEnabled,
                             int8_t mapLangIndex)
  : m_tileKey(tileKey)
  , m_commutator(commutator)
  , m_texMng(texMng)
  , m_metalineMng(metalineMng)
  , m_customFeaturesContext(customFeaturesContext)
  , m_3dBuildingsEnabled(is3dBuildingsEnabled)
  , m_trafficEnabled(isTrafficEnabled)
  , m_isolinesEnabled(isolinesEnabled)
  , m_mapLangIndex(mapLangIndex)
{}

ref_ptr<dp::TextureManager> EngineContext::GetTextureManager() const
{
  return m_texMng;
}

ref_ptr<MetalineManager> EngineContext::GetMetalineManager() const
{
  return m_metalineMng;
}

void EngineContext::BeginReadTile()
{
  PostMessage(make_unique_dp<TileReadStartMessage>(m_tileKey));
}

void EngineContext::Flush(TMapShapes && shapes)
{
  PostMessage(make_unique_dp<MapShapeReadedMessage>(m_tileKey, std::move(shapes)));
}

void EngineContext::FlushOverlays(TMapShapes && shapes)
{
  PostMessage(make_unique_dp<OverlayMapShapeReadedMessage>(m_tileKey, std::move(shapes)));
}

void EngineContext::FlushTrafficGeometry(TrafficSegmentsGeometry && geometry)
{
  m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
                            make_unique_dp<FlushTrafficGeometryMessage>(m_tileKey, std::move(geometry)),
                            MessagePriority::Low);
}

void EngineContext::EndReadTile()
{
  PostMessage(make_unique_dp<TileReadEndMessage>(m_tileKey));
}

void EngineContext::PostMessage(drape_ptr<Message> && message)
{
  m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread, std::move(message),
                            MessagePriority::Normal);
}
}  // namespace df
