#pragma once

#include "drape_frontend/circles_pack_shape.hpp"
#include "drape_frontend/color_constants.hpp"
#include "drape_frontend/custom_features_context.hpp"
#include "drape_frontend/drape_api.hpp"
#include "drape_frontend/drape_api_builder.hpp"
#include "drape_frontend/drape_engine_params.hpp"
#include "drape_frontend/gps_track_point.hpp"
#include "drape_frontend/gui/layer_render.hpp"
#include "drape_frontend/gui/skin.hpp"
#include "drape_frontend/message.hpp"
#include "drape_frontend/my_position.hpp"
#include "drape_frontend/overlay_batcher.hpp"
#include "drape_frontend/postprocess_renderer.hpp"
#include "drape_frontend/render_node.hpp"
#include "drape_frontend/render_state_extension.hpp"
#include "drape_frontend/route_builder.hpp"
#include "drape_frontend/selection_shape.hpp"
#include "drape_frontend/tile_utils.hpp"
#include "drape_frontend/traffic_generator.hpp"
#include "drape_frontend/transit_scheme_builder.hpp"
#include "drape_frontend/user_event_stream.hpp"
#include "drape_frontend/user_mark_shapes.hpp"
#include "drape_frontend/user_marks_provider.hpp"

#include "drape/pointers.hpp"
#include "drape/render_bucket.hpp"
#include "drape/viewport.hpp"

#include "platform/location.hpp"

#include "geometry/polyline2d.hpp"
#include "geometry/rect2d.hpp"
#include "geometry/screenbase.hpp"
#include "geometry/triangle2d.hpp"

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace df
{
class BaseBlockingMessage : public Message
{
public:
  struct Blocker
  {
    void Wait()
    {
      std::unique_lock<std::mutex> lock(m_lock);
      m_signal.wait(lock, [this]{return !m_blocked;} );
    }

  private:
    friend class BaseBlockingMessage;

    void Signal()
    {
      std::lock_guard<std::mutex> lock(m_lock);
      m_blocked = false;
      m_signal.notify_one();
    }

  private:
    std::mutex m_lock;
    std::condition_variable m_signal;
    bool m_blocked = true;
  };

  explicit BaseBlockingMessage(Blocker & blocker)
    : m_blocker(blocker)
  {}

  ~BaseBlockingMessage() override
  {
    m_blocker.Signal();
  }

private:
  Blocker & m_blocker;
};

class BaseTileMessage : public Message
{
public:
  explicit BaseTileMessage(TileKey const & key)
    : m_tileKey(key)
  {}

  TileKey const & GetKey() const { return m_tileKey; }

private:
  TileKey m_tileKey;
};

class FinishReadingMessage : public Message
{
public:
  Type GetType() const override { return Type::FinishReading; }
};

class FinishTileReadMessage : public Message
{
public:
  template<typename T> FinishTileReadMessage(T && tiles, bool forceUpdateUserMarks)
    : m_tiles(std::forward<T>(tiles))
    , m_forceUpdateUserMarks(forceUpdateUserMarks)
  {}

  Type GetType() const override { return Type::FinishTileRead; }

  TTilesCollection const & GetTiles() const { return m_tiles; }
  TTilesCollection && MoveTiles() { return std::move(m_tiles); }
  bool NeedForceUpdateUserMarks() const { return m_forceUpdateUserMarks; }

private:
  TTilesCollection m_tiles;
  bool m_forceUpdateUserMarks;
};

class FlushRenderBucketMessage : public BaseTileMessage
{
public:
  FlushRenderBucketMessage(TileKey const & key, dp::RenderState const & state,
                           drape_ptr<dp::RenderBucket> && buffer)
    : BaseTileMessage(key)
    , m_state(state)
    , m_buffer(std::move(buffer))
  {}

  Type GetType() const override { return Type::FlushTile; }
  bool IsGraphicsContextDependent() const override { return true; }
  bool ContainsRenderState() const override { return true; }

  dp::RenderState const & GetState() const { return m_state; }
  drape_ptr<dp::RenderBucket> && AcceptBuffer() { return std::move(m_buffer); }

private:
  dp::RenderState m_state;
  drape_ptr<dp::RenderBucket> m_buffer;
};

template <typename RenderDataType, Message::Type MessageType>
class FlushRenderDataMessage : public Message
{
public:
  explicit FlushRenderDataMessage(RenderDataType && data) : m_data(std::move(data)) {}

  Type GetType() const override { return MessageType; }
  bool IsGraphicsContextDependent() const override { return true; }
  bool ContainsRenderState() const override { return true; }

  RenderDataType && AcceptRenderData() { return std::move(m_data); }

private:
  RenderDataType m_data;
};

using FlushOverlaysMessage = FlushRenderDataMessage<TOverlaysRenderData,
                                                    Message::Type::FlushOverlays>;

class InvalidateRectMessage : public Message
{
public:
  explicit InvalidateRectMessage(m2::RectD const & rect)
    : m_rect(rect) {}

  Type GetType() const override { return Type::InvalidateRect; }

  m2::RectD const & GetRect() const { return m_rect; }

private:
  m2::RectD m_rect;
};

class UpdateReadManagerMessage : public Message
{
public:
  Type GetType() const override { return Type::UpdateReadManager; }
};

class InvalidateReadManagerRectMessage : public BaseBlockingMessage
{
public:
  InvalidateReadManagerRectMessage(Blocker & blocker, TTilesCollection const & tiles)
    : BaseBlockingMessage(blocker)
    , m_tiles(tiles)
    , m_needRestartReading(false)
  {}

  explicit InvalidateReadManagerRectMessage(Blocker & blocker)
    : BaseBlockingMessage(blocker)
    , m_needRestartReading(true)
  {}

  Type GetType() const override { return Type::InvalidateReadManagerRect; }

  TTilesCollection const & GetTilesForInvalidate() const { return m_tiles; }
  bool NeedRestartReading() const { return m_needRestartReading; }

private:
  TTilesCollection m_tiles;
  bool m_needRestartReading;
};

class ClearUserMarkGroupMessage : public Message
{
public:
  explicit ClearUserMarkGroupMessage(kml::MarkGroupId groupId)
    : m_groupId(groupId)
  {}

  Type GetType() const override { return Type::ClearUserMarkGroup; }

  kml::MarkGroupId GetGroupId() const { return m_groupId; }

private:
  kml::MarkGroupId m_groupId;
};

class ChangeUserMarkGroupVisibilityMessage : public Message
{
public:
  ChangeUserMarkGroupVisibilityMessage(kml::MarkGroupId groupId, bool isVisible)
    : m_groupId(groupId)
    , m_isVisible(isVisible) {}

  Type GetType() const override { return Type::ChangeUserMarkGroupVisibility; }

  kml::MarkGroupId GetGroupId() const { return m_groupId; }
  bool IsVisible() const { return m_isVisible; }

private:
  kml::MarkGroupId m_groupId;
  bool m_isVisible;
};

class UpdateUserMarksMessage : public Message
{
public:
  UpdateUserMarksMessage(drape_ptr<IDCollections> && justCreatedIds,
                         drape_ptr<IDCollections> && removedIds,
                         drape_ptr<UserMarksRenderCollection> && marksRenderParams,
                         drape_ptr<UserLinesRenderCollection> && linesRenderParams)
    : m_justCreatedIds(std::move(justCreatedIds))
    , m_removedIds(std::move(removedIds))
    , m_marksRenderParams(std::move(marksRenderParams))
    , m_linesRenderParams(std::move(linesRenderParams))
  {}

  Type GetType() const override { return Type::UpdateUserMarks; }

  drape_ptr<UserMarksRenderCollection> AcceptMarkRenderParams() { return std::move(m_marksRenderParams); }
  drape_ptr<UserLinesRenderCollection> AcceptLineRenderParams() { return std::move(m_linesRenderParams); }
  drape_ptr<IDCollections> AcceptRemovedIds() { return std::move(m_removedIds); }
  drape_ptr<IDCollections> AcceptJustCreatedIds() { return std::move(m_justCreatedIds); }

private:
  drape_ptr<IDCollections> m_justCreatedIds;
  drape_ptr<IDCollections> m_removedIds;
  drape_ptr<UserMarksRenderCollection> m_marksRenderParams;
  drape_ptr<UserLinesRenderCollection> m_linesRenderParams;
};

class UpdateUserMarkGroupMessage : public Message
{
public:
  UpdateUserMarkGroupMessage(kml::MarkGroupId groupId,
                             drape_ptr<IDCollections> && ids)
    : m_groupId(groupId)
    , m_ids(std::move(ids))
  {}

  Type GetType() const override { return Type::UpdateUserMarkGroup; }

  kml::MarkGroupId GetGroupId() const { return m_groupId; }
  drape_ptr<IDCollections> AcceptIds() { return std::move(m_ids); }

private:
  kml::MarkGroupId m_groupId;
  drape_ptr<IDCollections> m_ids;
};

using FlushUserMarksMessage = FlushRenderDataMessage<TUserMarksRenderData,
                                                     Message::Type::FlushUserMarks>;

class InvalidateUserMarksMessage : public Message
{
public:
  InvalidateUserMarksMessage() = default;

  Type GetType() const override { return Type::InvalidateUserMarks; }
};

class GuiLayerRecachedMessage : public Message
{
public:
  GuiLayerRecachedMessage(drape_ptr<gui::LayerRenderer> && renderer, bool needResetOldGui)
    : m_renderer(std::move(renderer))
    , m_needResetOldGui(needResetOldGui)
  {}

  Type GetType() const override { return Type::GuiLayerRecached; }
  bool IsGraphicsContextDependent() const override { return true; }
  bool ContainsRenderState() const override { return true; }

  drape_ptr<gui::LayerRenderer> && AcceptRenderer() { return std::move(m_renderer); }
  bool NeedResetOldGui() const { return m_needResetOldGui; }

private:
  drape_ptr<gui::LayerRenderer> m_renderer;
  bool const m_needResetOldGui;
};

class GuiRecacheMessage : public Message
{
public:
  GuiRecacheMessage(gui::TWidgetsInitInfo const & initInfo, bool needResetOldGui)
    : m_initInfo(initInfo)
    , m_needResetOldGui(needResetOldGui)
  {}

  Type GetType() const override { return Type::GuiRecache;}
  bool IsGraphicsContextDependent() const override { return true; }

  gui::TWidgetsInitInfo const & GetInitInfo() const { return m_initInfo; }
  bool NeedResetOldGui() const { return m_needResetOldGui; }

private:
  gui::TWidgetsInitInfo m_initInfo;
  bool const m_needResetOldGui;
};

class MapShapesRecacheMessage : public Message
{
public:
  MapShapesRecacheMessage() = default;

  Type GetType() const override { return Type::MapShapesRecache; }
  bool IsGraphicsContextDependent() const override { return true; }
};

class GuiLayerLayoutMessage : public Message
{
public:
  explicit GuiLayerLayoutMessage(gui::TWidgetsLayoutInfo const & info)
    : m_layoutInfo(info)
  {}

  Type GetType() const override { return Type::GuiLayerLayout; }
  bool IsGraphicsContextDependent() const override { return true; }

  gui::TWidgetsLayoutInfo const & GetLayoutInfo() const { return m_layoutInfo; }
  gui::TWidgetsLayoutInfo AcceptLayoutInfo() { return std::move(m_layoutInfo); }

private:
  gui::TWidgetsLayoutInfo m_layoutInfo;
};

class UpdateMyPositionRoutingOffsetMessage: public Message
{
public:
  UpdateMyPositionRoutingOffsetMessage(bool useDefault, int offsetY)
    : m_useDefault(useDefault)
    , m_offsetY(offsetY)
  {}

  Type GetType() const override { return Type::UpdateMyPositionRoutingOffset; }

  bool UseDefault() const { return m_useDefault; }
  int GetOffsetY() const { return m_offsetY; }

private:
  bool m_useDefault;
  int m_offsetY;
};

class ShowChoosePositionMarkMessage : public Message
{
public:
  ShowChoosePositionMarkMessage() = default;
  Type GetType() const override { return Type::ShowChoosePositionMark; }
};

class SetKineticScrollEnabledMessage : public Message
{
public:
  explicit SetKineticScrollEnabledMessage(bool enabled)
    : m_enabled(enabled)
  {}

  Type GetType() const override { return Type::SetKineticScrollEnabled; }

  bool IsEnabled() const { return m_enabled; }

private:
  bool m_enabled;
};

class SetAddNewPlaceModeMessage : public Message
{
public:
  SetAddNewPlaceModeMessage(bool enable, std::vector<m2::TriangleD> && boundArea,
                            bool enableKineticScroll, m2::PointD const * optionalPosition)
    : m_enable(enable)
    , m_boundArea(std::move(boundArea))
    , m_enableKineticScroll(enableKineticScroll)
  {
    if (optionalPosition)
      m_position = *optionalPosition;
  }

  Type GetType() const override { return Type::SetAddNewPlaceMode; }

  std::vector<m2::TriangleD> && AcceptBoundArea() { return std::move(m_boundArea); }
  bool IsEnabled() const { return m_enable; }
  bool IsKineticScrollEnabled() const { return m_enableKineticScroll; }
  auto const & GetOptionalPosition() const { return m_position; }

private:
  bool m_enable;
  std::vector<m2::TriangleD> m_boundArea;
  bool m_enableKineticScroll;
  std::optional<m2::PointD> m_position;
};

class BlockTapEventsMessage : public Message
{
public:
  explicit BlockTapEventsMessage(bool block)
    : m_needBlock(block)
  {}

  Type GetType() const override { return Type::BlockTapEvents; }

  bool NeedBlock() const { return m_needBlock; }

private:
  bool const m_needBlock;
};

class MapShapesMessage : public Message
{
public:
  MapShapesMessage(drape_ptr<MyPosition> && shape, drape_ptr<SelectionShape> && selection,
                   Arrow3d::PreloadedData preloadedArrow3dData)
    : m_shape(std::move(shape))
    , m_selection(std::move(selection))
    , m_preloadedArrow3dData(std::move(preloadedArrow3dData))
  {}

  Type GetType() const override { return Type::MapShapes; }
  bool ContainsRenderState() const override { return true; }
  bool IsGraphicsContextDependent() const override { return true; }

  drape_ptr<MyPosition> && AcceptShape() { return std::move(m_shape); }
  drape_ptr<SelectionShape> AcceptSelection() { return std::move(m_selection); }
  Arrow3d::PreloadedData && AcceptPeloadedArrow3dData()
  {
    return std::move(m_preloadedArrow3dData);
  }

private:
  drape_ptr<MyPosition> m_shape;
  drape_ptr<SelectionShape> m_selection;
  Arrow3d::PreloadedData m_preloadedArrow3dData;
};

class ChangeMyPositionModeMessage : public Message
{
public:
  enum EChangeType
  {
    SwitchNextMode,
    LoseLocation,
    StopFollowing
  };

  explicit ChangeMyPositionModeMessage(EChangeType changeType)
    : m_changeType(changeType)
  {}

  EChangeType GetChangeType() const { return m_changeType; }
  Type GetType() const override { return Type::ChangeMyPositionMode; }

private:
  EChangeType const m_changeType;
};

class CompassInfoMessage : public Message
{
public:
  explicit CompassInfoMessage(location::CompassInfo const & info)
    : m_info(info)
  {}

  Type GetType() const override { return Type::CompassInfo; }

  location::CompassInfo const & GetInfo() const { return m_info; }

private:
  location::CompassInfo const m_info;
};

class GpsInfoMessage : public Message
{
public:
  GpsInfoMessage(location::GpsInfo const & info, bool isNavigable,
                 location::RouteMatchingInfo const & routeInfo)
    : m_info(info)
    , m_isNavigable(isNavigable)
    , m_routeInfo(routeInfo)
  {}

  Type GetType() const override { return Type::GpsInfo; }

  location::GpsInfo const & GetInfo() const { return m_info; }
  bool IsNavigable() const { return m_isNavigable; }
  location::RouteMatchingInfo const & GetRouteInfo() const { return m_routeInfo; }

private:
  location::GpsInfo const m_info;
  bool const m_isNavigable;
  location::RouteMatchingInfo const m_routeInfo;
};

class SelectObjectMessage : public Message
{
public:
  struct DismissTag {};

  explicit SelectObjectMessage(DismissTag)
    : m_selected(SelectionShape::OBJECT_EMPTY)
    , m_glbPoint(m2::PointD::Zero())
    , m_isAnim(false)
    , m_isDismiss(true)
    , m_isGeometrySelectionAllowed(false)
    , m_isSelectionShapeVisible(false)
  {}

  SelectObjectMessage(SelectionShape::ESelectedObject selectedObject, m2::PointD const & glbPoint,
                      FeatureID const & featureID, bool isAnim, bool isGeometrySelectionAllowed,
                      bool isSelectionShapeVisible)
    : m_selected(selectedObject)
    , m_glbPoint(glbPoint)
    , m_featureID(featureID)
    , m_isAnim(isAnim)
    , m_isDismiss(false)
    , m_isGeometrySelectionAllowed(isGeometrySelectionAllowed)
    , m_isSelectionShapeVisible(isSelectionShapeVisible)
  {}

  Type GetType() const override { return Type::SelectObject; }
  bool IsGraphicsContextDependent() const override { return false; }

  m2::PointD const & GetPosition() const { return m_glbPoint; }
  SelectionShape::ESelectedObject GetSelectedObject() const { return m_selected; }
  FeatureID const & GetFeatureID() const { return m_featureID; }
  bool IsAnim() const { return m_isAnim; }
  bool IsDismiss() const { return m_isDismiss; }
  bool IsGeometrySelectionAllowed() const { return m_isGeometrySelectionAllowed; }
  bool IsSelectionShapeVisible() const { return m_isSelectionShapeVisible; }

private:
  SelectionShape::ESelectedObject m_selected;
  m2::PointD m_glbPoint;
  FeatureID m_featureID;
  bool m_isAnim;
  bool m_isDismiss;
  bool m_isGeometrySelectionAllowed;
  bool m_isSelectionShapeVisible;
};

class CheckSelectionGeometryMessage : public Message
{
public:
  CheckSelectionGeometryMessage(FeatureID const & feature, int recacheId)
    : m_feature(feature)
    , m_recacheId(recacheId)
  {}

  Type GetType() const override { return Type::CheckSelectionGeometry; }

  FeatureID const & GetFeature() const { return m_feature; };
  int GetRecacheId() const { return m_recacheId; }

private:
  FeatureID const m_feature;
  int const m_recacheId;
};

using BaseFlushSelectionGeometryMessage = FlushRenderDataMessage<drape_ptr<RenderNode>,
                                                                 Message::Type::FlushSelectionGeometry>;
class FlushSelectionGeometryMessage : public BaseFlushSelectionGeometryMessage
{
public:
  FlushSelectionGeometryMessage(drape_ptr<RenderNode> && renderNode, int recacheId)
    : BaseFlushSelectionGeometryMessage(std::move(renderNode))
    , m_recacheId(recacheId)
  {}

  int GetRecacheId() const { return m_recacheId; }

private:
  int const m_recacheId;
};

class AddSubrouteMessage : public Message
{
public:
  AddSubrouteMessage(dp::DrapeID subrouteId, SubrouteConstPtr subroute)
    : AddSubrouteMessage(subrouteId, subroute, -1 /* invalid recache id */)
  {}

  AddSubrouteMessage(dp::DrapeID subrouteId, SubrouteConstPtr subroute, int recacheId)
    : m_subrouteId(subrouteId)
    , m_subroute(subroute)
    , m_recacheId(recacheId)
  {}

  Type GetType() const override { return Type::AddSubroute; }

  dp::DrapeID GetSubrouteId() const { return m_subrouteId; };
  SubrouteConstPtr GetSubroute() const { return m_subroute; }
  int GetRecacheId() const { return m_recacheId; }

private:
  dp::DrapeID m_subrouteId;
  SubrouteConstPtr m_subroute;
  int const m_recacheId;
};

class PrepareSubrouteArrowsMessage : public Message
{
public:
  PrepareSubrouteArrowsMessage(dp::DrapeID subrouteId,
                               std::vector<ArrowBorders> && borders)
    : m_subrouteId(subrouteId)
    , m_borders(std::move(borders))
  {}

  Type GetType() const override { return Type::PrepareSubrouteArrows; }
  dp::DrapeID GetSubrouteId() const { return m_subrouteId; }
  std::vector<ArrowBorders> && AcceptBorders() { return std::move(m_borders); }

private:
  dp::DrapeID m_subrouteId;
  std::vector<ArrowBorders> m_borders;
};

class CacheSubrouteArrowsMessage : public Message
{
public:
  CacheSubrouteArrowsMessage(dp::DrapeID subrouteId,
                             std::vector<ArrowBorders> const & borders,
                             int recacheId)
    : m_subrouteId(subrouteId)
    , m_borders(borders)
    , m_recacheId(recacheId)
  {}

  Type GetType() const override { return Type::CacheSubrouteArrows; }
  dp::DrapeID GetSubrouteId() const { return m_subrouteId; }
  std::vector<ArrowBorders> const & GetBorders() const { return m_borders; }
  int GetRecacheId() const { return m_recacheId; }

private:
  dp::DrapeID m_subrouteId;
  std::vector<ArrowBorders> m_borders;
  int const m_recacheId;
};

class RemoveSubrouteMessage : public Message
{
public:
  RemoveSubrouteMessage(dp::DrapeID segmentId, bool deactivateFollowing)
    : m_subrouteId(segmentId)
    , m_deactivateFollowing(deactivateFollowing)
  {}

  Type GetType() const override { return Type::RemoveSubroute; }

  dp::DrapeID GetSegmentId() const { return m_subrouteId; }
  bool NeedDeactivateFollowing() const { return m_deactivateFollowing; }

private:
  dp::DrapeID m_subrouteId;
  bool m_deactivateFollowing;
};

using FlushSubrouteMessage = FlushRenderDataMessage<drape_ptr<SubrouteData>,
                                                    Message::Type::FlushSubroute>;
using FlushSubrouteArrowsMessage = FlushRenderDataMessage<drape_ptr<SubrouteArrowsData>,
                                                          Message::Type::FlushSubrouteArrows>;
using FlushSubrouteMarkersMessage = FlushRenderDataMessage<drape_ptr<SubrouteMarkersData>,
                                                           Message::Type::FlushSubrouteMarkers>;

class AddRoutePreviewSegmentMessage : public Message
{
public:
  AddRoutePreviewSegmentMessage(dp::DrapeID segmentId, m2::PointD const & startPt,
                                m2::PointD const & finishPt)
    : m_segmentId(segmentId)
    , m_startPoint(startPt)
    , m_finishPoint(finishPt)
  {}

  Type GetType() const override { return Type::AddRoutePreviewSegment; }

  dp::DrapeID GetSegmentId() const { return m_segmentId; };
  m2::PointD const & GetStartPoint() const { return m_startPoint; }
  m2::PointD const & GetFinishPoint() const { return m_finishPoint; }

private:
  dp::DrapeID m_segmentId;
  m2::PointD m_startPoint;
  m2::PointD m_finishPoint;
};

class RemoveRoutePreviewSegmentMessage : public Message
{
public:
  RemoveRoutePreviewSegmentMessage() = default;

  explicit RemoveRoutePreviewSegmentMessage(dp::DrapeID segmentId)
    : m_segmentId(segmentId)
    , m_needRemoveAll(false)
  {}

  Type GetType() const override { return Type::RemoveRoutePreviewSegment; }

  dp::DrapeID GetSegmentId() const { return m_segmentId; }
  bool NeedRemoveAll() const { return m_needRemoveAll; }

private:
  dp::DrapeID m_segmentId = 0;
  bool m_needRemoveAll = true;
};

class SetSubrouteVisibilityMessage : public Message
{
public:
  SetSubrouteVisibilityMessage(dp::DrapeID subrouteId, bool isVisible)
    : m_subrouteId(subrouteId)
    , m_isVisible(isVisible)
  {}

  Type GetType() const override { return Type::SetSubrouteVisibility; }

  dp::DrapeID GetSubrouteId() const { return m_subrouteId; }
  bool IsVisible() const { return m_isVisible; }

private:
  dp::DrapeID m_subrouteId;
  bool m_isVisible;
};

class UpdateMapStyleMessage : public Message
{
public:
  Type GetType() const override { return Type::UpdateMapStyle; }
};

class FollowRouteMessage : public Message
{
public:
  FollowRouteMessage(int preferredZoomLevel, int preferredZoomLevelIn3d, bool enableAutoZoom,
                     bool isArrowGlued)
    : m_preferredZoomLevel(preferredZoomLevel)
    , m_preferredZoomLevelIn3d(preferredZoomLevelIn3d)
    , m_enableAutoZoom(enableAutoZoom)
    , m_isArrowGlued(isArrowGlued)
  {}

  Type GetType() const override { return Type::FollowRoute; }

  int GetPreferredZoomLevel() const { return m_preferredZoomLevel; }
  int GetPreferredZoomLevelIn3d() const { return m_preferredZoomLevelIn3d; }
  bool EnableAutoZoom() const { return m_enableAutoZoom; }
  bool IsArrowGlued() const { return m_isArrowGlued; }

private:
  int const m_preferredZoomLevel;
  int const m_preferredZoomLevelIn3d;
  bool const m_enableAutoZoom;
  bool const m_isArrowGlued;
};

class SwitchMapStyleMessage : public BaseBlockingMessage
{
public:
  using FilterMessagesHandler = std::function<void()>;

  SwitchMapStyleMessage(Blocker & blocker, FilterMessagesHandler && filterMessagesHandler)
    : BaseBlockingMessage(blocker)
    , m_filterMessagesHandler(std::move(filterMessagesHandler))
  {}

  Type GetType() const override { return Type::SwitchMapStyle; }

  void FilterDependentMessages()
  {
    if (m_filterMessagesHandler)
      m_filterMessagesHandler();
  }

private:
  FilterMessagesHandler m_filterMessagesHandler;
};

class InvalidateMessage : public Message
{
public:
  Type GetType() const override { return Type::Invalidate; }
};

class RecoverContextDependentResourcesMessage : public Message
{
public:
  Type GetType() const override { return Type::RecoverContextDependentResources; }
  bool IsGraphicsContextDependent() const override { return true; }
};

class SetVisibleViewportMessage : public Message
{
public:
  explicit SetVisibleViewportMessage(m2::RectD const & rect)
    : m_rect(rect)
  {}

  Type GetType() const override { return Type::SetVisibleViewport;  }

  m2::RectD const & GetRect() const { return m_rect; }

private:
  m2::RectD m_rect;
};

class DeactivateRouteFollowingMessage : public Message
{
public:
  Type GetType() const override { return Type::DeactivateRouteFollowing; }
};

class Allow3dModeMessage : public Message
{
public:
  Allow3dModeMessage(bool allowPerspective, bool allow3dBuildings)
    : m_allowPerspective(allowPerspective)
    , m_allow3dBuildings(allow3dBuildings)
  {}

  Type GetType() const override { return Type::Allow3dMode; }

  bool AllowPerspective() const { return m_allowPerspective; }
  bool Allow3dBuildings() const { return m_allow3dBuildings; }

private:
  bool const m_allowPerspective;
  bool const m_allow3dBuildings;
};

class AllowAutoZoomMessage : public Message
{
public:
  explicit AllowAutoZoomMessage(bool allowAutoZoom)
    : m_allowAutoZoom(allowAutoZoom)
  {}

  Type GetType() const override { return Type::AllowAutoZoom; }

  bool AllowAutoZoom() const { return m_allowAutoZoom; }

private:
  bool const m_allowAutoZoom;
};

class Allow3dBuildingsMessage : public Message
{
public:
  explicit Allow3dBuildingsMessage(bool allow3dBuildings)
    : m_allow3dBuildings(allow3dBuildings)
  {}

  Type GetType() const override { return Type::Allow3dBuildings; }

  bool Allow3dBuildings() const { return m_allow3dBuildings; }

private:
  bool const m_allow3dBuildings;
};

class SetMapLangIndexMessage : public Message
{
public:
  explicit SetMapLangIndexMessage(int8_t mapLangIndex)
    : m_mapLangIndex(mapLangIndex)
  {}

  Type GetType() const override { return Type::SetMapLangIndex; }

  int8_t MapLangIndex() const { return m_mapLangIndex; }

private:
  int8_t const m_mapLangIndex;
};


class EnablePerspectiveMessage : public Message
{
public:
  EnablePerspectiveMessage() = default;

  Type GetType() const override { return Type::EnablePerspective; }
};

class CacheCirclesPackMessage : public Message
{
public:
  enum Destination
  {
    GpsTrack,
    RoutePreview
  };

  CacheCirclesPackMessage(uint32_t pointsCount, Destination dest)
    : m_pointsCount(pointsCount)
    , m_destination(dest)
  {}

  Type GetType() const override { return Type::CacheCirclesPack; }

  uint32_t GetPointsCount() const { return m_pointsCount; }
  Destination GetDestination() const { return m_destination; }

private:
  uint32_t m_pointsCount;
  Destination m_destination;
};

using BaseFlushCirclesPackMessage = FlushRenderDataMessage<drape_ptr<CirclesPackRenderData>,
                                                           Message::Type::FlushCirclesPack>;
class FlushCirclesPackMessage : public BaseFlushCirclesPackMessage
{
public:
  FlushCirclesPackMessage(drape_ptr<CirclesPackRenderData> && renderData,
                          CacheCirclesPackMessage::Destination dest)
    : BaseFlushCirclesPackMessage(std::move(renderData))
    , m_destination(dest)
  {}

  CacheCirclesPackMessage::Destination GetDestination() const { return m_destination; }

private:
  CacheCirclesPackMessage::Destination m_destination;
};

class UpdateGpsTrackPointsMessage : public Message
{
public:
  UpdateGpsTrackPointsMessage(std::vector<GpsTrackPoint> && toAdd,
                              std::vector<uint32_t> && toRemove)
    : m_pointsToAdd(std::move(toAdd))
    , m_pointsToRemove(std::move(toRemove))
  {}

  Type GetType() const override { return Type::UpdateGpsTrackPoints; }

  std::vector<GpsTrackPoint> const & GetPointsToAdd() { return m_pointsToAdd; }
  std::vector<uint32_t> const & GetPointsToRemove() { return m_pointsToRemove; }

private:
  std::vector<GpsTrackPoint> m_pointsToAdd;
  std::vector<uint32_t> m_pointsToRemove;
};

class ClearGpsTrackPointsMessage : public Message
{
public:
  Type GetType() const override { return Type::ClearGpsTrackPoints; }
};

class OnEnterForegroundMessage : public Message
{
public:
  explicit OnEnterForegroundMessage(double time)
    : m_time(time)
  {}

  Type GetType() const override { return Type::OnEnterForeground; }

  double GetTime() const { return m_time; }

private:
  double m_time;
};

class OnEnterBackgroundMessage : public Message
{
public:
  Type GetType() const override { return Type::OnEnterBackground; }
};

class RequestSymbolsSizeMessage : public Message
{
public:
  using Sizes = std::map<std::string, m2::PointF>;
  using RequestSymbolsSizeCallback = std::function<void(Sizes &&)>;

  RequestSymbolsSizeMessage(std::vector<std::string> const & symbols,
                            RequestSymbolsSizeCallback const & callback)
    : m_symbols(symbols)
    , m_callback(callback)
  {}

  Type GetType() const override { return Type::RequestSymbolsSize; }

  std::vector<std::string> const & GetSymbols() const { return m_symbols; }

  void InvokeCallback(Sizes && sizes)
  {
    if (m_callback)
      m_callback(std::move(sizes));
  }

private:
  std::vector<std::string> m_symbols;
  RequestSymbolsSizeCallback m_callback;
};

class NotifyGraphicsReadyMessage : public Message
{
public:
  using GraphicsReadyCallback = std::function<void()>;

  explicit NotifyGraphicsReadyMessage(GraphicsReadyCallback const & callback, bool needInvalidate)
    : m_needInvalidate(needInvalidate)
    , m_callback(callback)
  {}

  Type GetType() const override { return Type::NotifyGraphicsReady; }

  bool NeedInvalidate() const { return m_needInvalidate; }
  GraphicsReadyCallback GetCallback() { return m_callback; }

private:
  bool m_needInvalidate;
  GraphicsReadyCallback m_callback;
};

class EnableTrafficMessage : public Message
{
public:
  explicit EnableTrafficMessage(bool trafficEnabled)
    : m_trafficEnabled(trafficEnabled)
  {}

  Type GetType() const override { return Type::EnableTraffic; }

  bool IsTrafficEnabled() const { return m_trafficEnabled; }

private:
  bool const m_trafficEnabled;
};

class FlushTrafficGeometryMessage : public BaseTileMessage
{
public:
  FlushTrafficGeometryMessage(TileKey const & tileKey, TrafficSegmentsGeometry && segments)
    : BaseTileMessage(tileKey)
    , m_segments(std::move(segments))
  {}

  Type GetType() const override { return Type::FlushTrafficGeometry; }

  TrafficSegmentsGeometry & GetSegments() { return m_segments; }

private:
  TrafficSegmentsGeometry m_segments;
};

class RegenerateTrafficMessage : public Message
{
public:
  Type GetType() const override { return Type::RegenerateTraffic; }
};

class UpdateTrafficMessage : public Message
{
public:
  explicit UpdateTrafficMessage(TrafficSegmentsColoring && segmentsColoring)
    : m_segmentsColoring(std::move(segmentsColoring))
  {}

  Type GetType() const override { return Type::UpdateTraffic; }

  TrafficSegmentsColoring & GetSegmentsColoring() { return m_segmentsColoring; }

private:
  TrafficSegmentsColoring m_segmentsColoring;
};

using FlushTrafficDataMessage = FlushRenderDataMessage<TrafficRenderData,
                                                       Message::Type::FlushTrafficData>;

class ClearTrafficDataMessage : public Message
{
public:
  explicit ClearTrafficDataMessage(MwmSet::MwmId const & mwmId)
    : m_mwmId(mwmId)
  {}

  Type GetType() const override { return Type::ClearTrafficData; }

  MwmSet::MwmId const & GetMwmId() { return m_mwmId; }

private:
  MwmSet::MwmId m_mwmId;
};

class SetSimplifiedTrafficColorsMessage : public Message
{
public:
  SetSimplifiedTrafficColorsMessage(bool isSimplified)
    : m_isSimplified(isSimplified)
  {}

  Type GetType() const override { return Type::SetSimplifiedTrafficColors; }

  bool IsSimplified() const { return m_isSimplified; }

private:
  bool const m_isSimplified;
};

class EnableIsolinesMessage : public Message
{
public:
  explicit EnableIsolinesMessage(bool isEnabled)
    : m_isEnabled(isEnabled)
  {}

  Type GetType() const override { return Type::EnableIsolines; }

  bool IsEnabled() const { return m_isEnabled; }

private:
  bool m_isEnabled = false;
};

class EnableTransitSchemeMessage : public Message
{
public:
  explicit EnableTransitSchemeMessage(bool isEnabled)
    : m_isEnabled(isEnabled)
  {}

  Type GetType() const override { return Type::EnableTransitScheme; }

  bool IsEnabled() const { return m_isEnabled; }

private:
  bool m_isEnabled = false;
};

class ClearTransitSchemeDataMessage : public Message
{
public:
  explicit ClearTransitSchemeDataMessage(MwmSet::MwmId const & mwmId)
    : m_mwmId(mwmId)
  {}

  Type GetType() const override { return Type::ClearTransitSchemeData; }

  MwmSet::MwmId const & GetMwmId() const { return m_mwmId; }

private:
  MwmSet::MwmId m_mwmId;
};

class ClearAllTransitSchemeDataMessage : public Message
{
public:
  Type GetType() const override { return Type::ClearAllTransitSchemeData; }
};

class UpdateTransitSchemeMessage : public Message
{
public:
  UpdateTransitSchemeMessage(TransitDisplayInfos && transitInfos)
    : m_transitInfos(std::move(transitInfos))
  {}

  Type GetType() const override { return Type::UpdateTransitScheme; }

  TransitDisplayInfos const & GetTransitDisplayInfos() const { return m_transitInfos; }

private:
  TransitDisplayInfos m_transitInfos;
};

class RegenerateTransitMessage : public Message
{
public:
  Type GetType() const override { return Type::RegenerateTransitScheme; }
};

using FlushTransitSchemeMessage = FlushRenderDataMessage<TransitRenderData,
                                                         Message::Type::FlushTransitScheme>;

class DrapeApiAddLinesMessage : public Message
{
public:
  explicit DrapeApiAddLinesMessage(DrapeApi::TLines const & lines)
    : m_lines(lines)
  {}

  Type GetType() const override { return Type::DrapeApiAddLines; }

  DrapeApi::TLines & GetLines() { return m_lines; }

private:
  DrapeApi::TLines m_lines;
};

class DrapeApiRemoveMessage : public Message
{
public:
  explicit DrapeApiRemoveMessage(std::string const & id, bool removeAll = false)
    : m_id(id)
    , m_removeAll(removeAll)
  {}

  Type GetType() const override { return Type::DrapeApiRemove; }

  std::string const & GetId() const { return m_id; }
  bool NeedRemoveAll() const { return m_removeAll; }

private:
  std::string m_id;
  bool m_removeAll;
};

class DrapeApiFlushMessage : public Message
{
public:
  using TProperties = std::vector<drape_ptr<DrapeApiRenderProperty>>;

  explicit DrapeApiFlushMessage(TProperties && properties)
    : m_properties(std::move(properties))
  {}

  Type GetType() const override { return Type::DrapeApiFlush; }

  bool IsGraphicsContextDependent() const override { return true; }
  bool ContainsRenderState() const override { return true; }

  TProperties && AcceptProperties() { return std::move(m_properties); }

private:
  TProperties m_properties;
};

class SetCustomFeaturesMessage : public Message
{
public:
  explicit SetCustomFeaturesMessage(CustomFeatures && ids)
    : m_features(std::move(ids))
  {}

  Type GetType() const override { return Type::SetCustomFeatures; }

  CustomFeatures && AcceptFeatures() { return std::move(m_features); }

private:
  CustomFeatures m_features;
};

class RemoveCustomFeaturesMessage : public Message
{
public:
  RemoveCustomFeaturesMessage() = default;
  explicit RemoveCustomFeaturesMessage(MwmSet::MwmId const & mwmId)
    : m_mwmId(mwmId), m_removeAll(false)
  {}

  Type GetType() const override { return Type::RemoveCustomFeatures; }
  bool NeedRemoveAll() const { return m_removeAll; }
  MwmSet::MwmId const & GetMwmId() const { return m_mwmId; }

private:
  MwmSet::MwmId m_mwmId;
  bool m_removeAll = true;
};

class SetTrackedFeaturesMessage : public Message
{
public:
  explicit SetTrackedFeaturesMessage(std::vector<FeatureID> && features)
    : m_features(std::move(features))
  {}

  Type GetType() const override { return Type::SetTrackedFeatures; }

  std::vector<FeatureID> && AcceptFeatures() { return std::move(m_features); }

private:
  std::vector<FeatureID> m_features;
};

class SetPostprocessStaticTexturesMessage : public Message
{
public:
  explicit SetPostprocessStaticTexturesMessage(drape_ptr<PostprocessStaticTextures> && textures)
    : m_textures(std::move(textures))
  {}

  Type GetType() const override { return Type::SetPostprocessStaticTextures; }
  bool IsGraphicsContextDependent() const override { return true; }

  drape_ptr<PostprocessStaticTextures> && AcceptTextures() { return std::move(m_textures); }

private:
  drape_ptr<PostprocessStaticTextures> m_textures;
};

class SetPosteffectEnabledMessage : public Message
{
public:
  SetPosteffectEnabledMessage(PostprocessRenderer::Effect effect, bool enabled)
    : m_effect(effect)
    , m_enabled(enabled)
  {}

  Type GetType() const override { return Type::SetPosteffectEnabled; }
  PostprocessRenderer::Effect GetEffect() const { return m_effect; }
  bool IsEnabled() const { return m_enabled; }

private:
  PostprocessRenderer::Effect const m_effect;
  bool const m_enabled;
};

class EnableDebugRectRenderingMessage : public Message
{
public:
  explicit EnableDebugRectRenderingMessage(bool enabled)
    : m_enabled(enabled)
  {}

  Type GetType() const override { return Type::EnableDebugRectRendering; }
  bool IsEnabled() const { return m_enabled; }

private:
  bool const m_enabled;
};

class RunFirstLaunchAnimationMessage : public Message
{
public:
  Type GetType() const override { return Type::RunFirstLaunchAnimation; }
};

class UpdateMetalinesMessage : public Message
{
public:
  Type GetType() const override { return Type::UpdateMetalines; }
};

class PostUserEventMessage : public Message
{
public:
  explicit PostUserEventMessage(drape_ptr<UserEvent> && event)
    : m_event(std::move(event))
  {}

  Type GetType() const override { return Type::PostUserEvent; }

  drape_ptr<UserEvent> && AcceptEvent() { return std::move(m_event); }

private:
  drape_ptr<UserEvent> m_event;
};

class FinishTexturesInitializationMessage : public Message
{
public:
  bool IsGraphicsContextDependent() const override { return true; }
  Type GetType() const override { return Type::FinishTexturesInitialization; }
};

class CleanupTexturesMessage : public Message
{
public:
  explicit CleanupTexturesMessage(std::vector<drape_ptr<dp::HWTexture>> && textures)
    : m_textures(std::move(textures))
  {}
  Type GetType() const override { return Type::CleanupTextures; }
  bool IsGraphicsContextDependent() const override { return true; }

private:
  std::vector<drape_ptr<dp::HWTexture>> m_textures;
};

class ShowDebugInfoMessage : public Message
{
public:
  explicit ShowDebugInfoMessage(bool shown)
    : m_shown(shown)
  {}

  Type GetType() const override { return Type::ShowDebugInfo; }
  bool IsShown() const { return m_shown; }

private:
  bool const m_shown;
};

class NotifyRenderThreadMessage : public Message
{
public:
  using Functor = std::function<void(uint64_t notifyId)>;
  NotifyRenderThreadMessage(Functor const & functor, uint64_t notifyId)
    : m_functor(functor)
    , m_notifyId(notifyId)
  {}

  // We can not notify render threads without active graphics context.
  bool IsGraphicsContextDependent() const override { return true; }

  Type GetType() const override { return Type::NotifyRenderThread; }

  void InvokeFunctor()
  {
    if (m_functor)
      m_functor(m_notifyId);
  }

private:
  Functor m_functor;
  uint64_t const m_notifyId;
};

class Arrow3dRecacheMessage : public Message
{
public:
  Arrow3dRecacheMessage(std::optional<Arrow3dCustomDecl> arrow3dCustomDecl)
    : m_arrow3dCustomDecl(std::move(arrow3dCustomDecl))
  {}

  Type GetType() const override { return Type::Arrow3dRecache; }

  std::optional<Arrow3dCustomDecl> const & GetArrow3dCustomDecl() const
  {
    return m_arrow3dCustomDecl;
  }

private:
  std::optional<Arrow3dCustomDecl> m_arrow3dCustomDecl;
};
}  // namespace df
