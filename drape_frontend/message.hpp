#pragma once

#include <string>

namespace df
{
class Message
{
public:
  enum class Type
  {
    Unknown,
    TileReadStarted,
    TileReadEnded,
    FinishReading,
    FinishTileRead,
    FlushTile,
    FlushOverlays,
    MapShapeReaded,
    OverlayMapShapeReaded,
    UpdateReadManager,
    InvalidateRect,
    InvalidateReadManagerRect,
    UpdateUserMarkGroup,
    ClearUserMarkGroup,
    ChangeUserMarkGroupVisibility,
    UpdateUserMarks,
    InvalidateUserMarks,
    FlushUserMarks,
    GuiLayerRecached,
    GuiRecache,
    GuiLayerLayout,
    UpdateMyPositionRoutingOffset,
    MapShapesRecache,
    MapShapes,
    ChangeMyPositionMode,
    CompassInfo,
    GpsInfo,
    SelectObject,
    CheckSelectionGeometry,
    FlushSelectionGeometry,
    AddSubroute,
    RemoveSubroute,
    PrepareSubrouteArrows,
    CacheSubrouteArrows,
    FlushSubroute,
    FlushSubrouteArrows,
    FlushSubrouteMarkers,
    FollowRoute,
    DeactivateRouteFollowing,
    SetSubrouteVisibility,
    AddRoutePreviewSegment,
    RemoveRoutePreviewSegment,
    UpdateMapStyle,
    SwitchMapStyle,
    Invalidate,
    Allow3dMode,
    Allow3dBuildings,
    SetMapLangIndex,
    EnablePerspective,
    FlushCirclesPack,
    CacheCirclesPack,
    UpdateGpsTrackPoints,
    ClearGpsTrackPoints,
    ShowChoosePositionMark,
    SetKineticScrollEnabled,
    BlockTapEvents,
    OnEnterForeground,
    SetAddNewPlaceMode,
    AllowAutoZoom,
    RequestSymbolsSize,
    RecoverContextDependentResources,
    SetVisibleViewport,
    EnableTraffic,
    FlushTrafficGeometry,
    RegenerateTraffic,
    UpdateTraffic,
    FlushTrafficData,
    ClearTrafficData,
    SetSimplifiedTrafficColors,
    DrapeApiAddLines,
    DrapeApiRemove,
    DrapeApiFlush,
    SetCustomFeatures,
    RemoveCustomFeatures,
    SetTrackedFeatures,
    SetPostprocessStaticTextures,
    SetPosteffectEnabled,
    RunFirstLaunchAnimation,
    UpdateMetalines,
    PostUserEvent,
    FinishTexturesInitialization,
    CleanupTextures,
    EnableDebugRectRendering,
    EnableTransitScheme,
    UpdateTransitScheme,
    ClearTransitSchemeData,
    ClearAllTransitSchemeData,
    RegenerateTransitScheme,
    FlushTransitScheme,
    ShowDebugInfo,
    NotifyRenderThread,
    NotifyGraphicsReady,
    EnableIsolines,
    OnEnterBackground,
    Arrow3dRecache
  };

  virtual ~Message() = default;
  virtual Type GetType() const { return Type::Unknown; }
  virtual bool IsGraphicsContextDependent() const { return false; }
  virtual bool ContainsRenderState() const { return false; }
};

enum class MessagePriority
{
  // This is standard priority. It must be used for majority of messages.
  // This priority guarantees order of messages processing.
  Normal,
  // This priority is used for system messages where order of processing
  // could be neglected, so it does not guarantee order of messages processing.
  // Also it must be used for messages which stop threads.
  High,
  // It can be used for the only system message (UpdateReadManagerMessage) and
  // must not be used anywhere else.
  UberHighSingleton,
  // This priority allows to process messages after any other messages in queue.
  Low
};

std::string DebugPrint(Message::Type msgType);
}  // namespace df
