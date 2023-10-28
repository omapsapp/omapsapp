#pragma once

#include "drape_frontend/gui/skin.hpp"
#include "drape_frontend/gui/shape.hpp"

#include "shaders/program_manager.hpp"

#include "drape/texture_manager.hpp"

#include "geometry/screenbase.hpp"

#include "base/macros.hpp"

#include <map>

namespace dp
{
  class GraphicsContext;
}  // namespace dp

namespace gui
{
class LayerRenderer
{
public:
  LayerRenderer() = default;
  ~LayerRenderer();

  void Build(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng);
  void Render(ref_ptr<dp::GraphicsContext> context, ref_ptr<gpu::ProgramManager> mng, bool routingActive,
              ScreenBase const & screen);
  void Merge(ref_ptr<LayerRenderer> other);
  void SetLayout(gui::TWidgetsLayoutInfo const & info);

  bool OnTouchDown(m2::RectD const & touchArea);
  void OnTouchUp(m2::RectD const & touchArea);
  void OnTouchCancel(m2::RectD const & touchArea);

  bool HasWidget(EWidget widget) const;

private:
  void DestroyRenderers();

  friend class LayerCacher;
  void AddShapeRenderer(EWidget widget, drape_ptr<ShapeRenderer> && shape);

private:
  using TRenderers = std::map<EWidget, drape_ptr<ShapeRenderer>>;
  TRenderers m_renderers;

  ref_ptr<gui::Handle> m_activeOverlay;
  FeatureID m_activeOverlayId;

  DISALLOW_COPY_AND_MOVE(LayerRenderer);
};

class LayerCacher
{
public:
  drape_ptr<LayerRenderer> RecacheWidgets(ref_ptr<dp::GraphicsContext> context,
                                          TWidgetsInitInfo const & initInfo,
                                          ref_ptr<dp::TextureManager> textures);
  drape_ptr<LayerRenderer> RecacheChoosePositionMark(ref_ptr<dp::GraphicsContext> context,
                                                     ref_ptr<dp::TextureManager> textures);

#ifdef RENDER_DEBUG_INFO_LABELS
  drape_ptr<LayerRenderer> RecacheDebugLabels(ref_ptr<dp::GraphicsContext> context,
                                              ref_ptr<dp::TextureManager> textures);
#endif

private:
  m2::PointF CacheCompass(ref_ptr<dp::GraphicsContext> context, Position const & position,
                          ref_ptr<LayerRenderer> renderer, ref_ptr<dp::TextureManager> textures);
  m2::PointF CacheRuler(ref_ptr<dp::GraphicsContext> context, Position const & position,
                        ref_ptr<LayerRenderer> renderer, ref_ptr<dp::TextureManager> textures);
  m2::PointF CacheCopyright(ref_ptr<dp::GraphicsContext> context, Position const & position,
                            ref_ptr<LayerRenderer> renderer, ref_ptr<dp::TextureManager> textures);
  m2::PointF CacheScaleFpsLabel(ref_ptr<dp::GraphicsContext> context, Position const & position,
                                ref_ptr<LayerRenderer> renderer,
                                ref_ptr<dp::TextureManager> textures);
  m2::PointF CacheWatermark(ref_ptr<dp::GraphicsContext> context, Position const & position,
                            ref_ptr<LayerRenderer> renderer, ref_ptr<dp::TextureManager> textures);
  m2::PointF CacheSpeedLimit(ref_ptr<dp::GraphicsContext> context, Position const & position,
                             ref_ptr<LayerRenderer> renderer, ref_ptr<dp::TextureManager> textures);
};
}  // namespace gui
