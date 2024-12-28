#include "drape_frontend/message.hpp"

#include "base/assert.hpp"

namespace df
{
std::string DebugPrint(Message::Type msgType)
{
  switch (msgType)
  {
  case Message::Type::Unknown: return "Unknown";
  case Message::Type::TileReadStarted: return "TileReadStarted";
  case Message::Type::TileReadEnded: return "TileReadEnded";
  case Message::Type::FinishReading: return "FinishReading";
  case Message::Type::FinishTileRead: return "FinishTileRead";
  case Message::Type::FlushTile: return "FlushTile";
  case Message::Type::FlushOverlays: return "FlushOverlays";
  case Message::Type::MapShapeReaded: return "MapShapeReaded";
  case Message::Type::OverlayMapShapeReaded: return "OverlayMapShapeReaded";
  case Message::Type::UpdateReadManager: return "UpdateReadManager";
  case Message::Type::InvalidateRect: return "InvalidateRect";
  case Message::Type::InvalidateReadManagerRect: return "InvalidateReadManagerRect";
  case Message::Type::UpdateUserMarkGroup: return "UpdateUserMarkGroup";
  case Message::Type::ClearUserMarkGroup: return "ClearUserMarkGroup";
  case Message::Type::ChangeUserMarkGroupVisibility: return "ChangeUserMarkGroupVisibility";
  case Message::Type::UpdateUserMarks: return "UpdateUserMarks";
  case Message::Type::InvalidateUserMarks: return "InvalidateUserMarks";
  case Message::Type::FlushUserMarks: return "FlushUserMarks";
  case Message::Type::GuiLayerRecached: return "GuiLayerRecached";
  case Message::Type::GuiRecache: return "GuiRecache";
  case Message::Type::GuiLayerLayout: return "GuiLayerLayout";
  case Message::Type::UpdateMyPositionRoutingOffset: return "UpdateMyPositionRoutingOffset";
  case Message::Type::MapShapesRecache: return "MapShapesRecache";
  case Message::Type::MapShapes: return "MapShapes";
  case Message::Type::ChangeMyPositionMode: return "ChangeMyPositionMode";
  case Message::Type::CompassInfo: return "CompassInfo";
  case Message::Type::GpsInfo: return "GpsInfo";
  case Message::Type::SelectObject: return "SelectObject";
  case Message::Type::CheckSelectionGeometry: return "CheckSelectionGeometry";
  case Message::Type::FlushSelectionGeometry: return "FlushSelectionGeometry";
  case Message::Type::AddSubroute: return "AddSubroute";
  case Message::Type::RemoveSubroute: return "RemoveSubroute";
  case Message::Type::PrepareSubrouteArrows: return "PrepareSubrouteArrows";
  case Message::Type::CacheSubrouteArrows: return "CacheSubrouteArrows";
  case Message::Type::FlushSubroute: return "FlushSubroute";
  case Message::Type::FlushSubrouteArrows: return "FlushSubrouteArrows";
  case Message::Type::FlushSubrouteMarkers: return "FlushSubrouteMarkers";
  case Message::Type::FollowRoute: return "FollowRoute";
  case Message::Type::DeactivateRouteFollowing: return "DeactivateRouteFollowing";
  case Message::Type::SetSubrouteVisibility: return "SetSubrouteVisibility";
  case Message::Type::AddRoutePreviewSegment: return "AddRoutePreviewSegment";
  case Message::Type::RemoveRoutePreviewSegment: return "RemoveRoutePreviewSegment";
  case Message::Type::UpdateMapStyle: return "UpdateMapStyle";
  case Message::Type::SwitchMapStyle: return "SwitchMapStyle";
  case Message::Type::Invalidate: return "Invalidate";
  case Message::Type::Allow3dMode: return "Allow3dMode";
  case Message::Type::Allow3dBuildings: return "Allow3dBuildings";
  case Message::Type::SetMapLangIndex: return "SetMapLangIndex";
  case Message::Type::EnablePerspective: return "EnablePerspective";
  case Message::Type::FlushCirclesPack: return "FlushCirclesPack";
  case Message::Type::CacheCirclesPack: return "CacheCirclesPack";
  case Message::Type::UpdateGpsTrackPoints: return "UpdateGpsTrackPoints";
  case Message::Type::ClearGpsTrackPoints: return "ClearGpsTrackPoints";
  case Message::Type::ShowChoosePositionMark: return "ShowChoosePositionMark";
  case Message::Type::SetKineticScrollEnabled: return "SetKineticScrollEnabled";
  case Message::Type::BlockTapEvents: return "BlockTapEvents";
  case Message::Type::OnEnterForeground: return "OnEnterForeground";
  case Message::Type::SetAddNewPlaceMode: return "SetAddNewPlaceMode";
  case Message::Type::AllowAutoZoom: return "AllowAutoZoom";
  case Message::Type::RequestSymbolsSize: return "RequestSymbolsSize";
  case Message::Type::RecoverContextDependentResources: return "RecoverContextDependentResources";
  case Message::Type::SetVisibleViewport: return "SetVisibleViewport";
  case Message::Type::EnableTraffic: return "EnableTraffic";
  case Message::Type::FlushTrafficGeometry: return "FlushTrafficGeometry";
  case Message::Type::RegenerateTraffic: return "RegenerateTraffic";
  case Message::Type::UpdateTraffic: return "UpdateTraffic";
  case Message::Type::FlushTrafficData: return "FlushTrafficData";
  case Message::Type::ClearTrafficData: return "ClearTrafficData";
  case Message::Type::SetSimplifiedTrafficColors: return "SetSimplifiedTrafficColors";
  case Message::Type::DrapeApiAddLines: return "DrapeApiAddLines";
  case Message::Type::DrapeApiRemove: return "DrapeApiRemove";
  case Message::Type::DrapeApiFlush: return "DrapeApiFlush";
  case Message::Type::SetCustomFeatures: return "SetCustomFeatures";
  case Message::Type::RemoveCustomFeatures: return "RemoveCustomFeatures";
  case Message::Type::SetTrackedFeatures: return "SetTrackedFeatures";
  case Message::Type::SetPostprocessStaticTextures: return "SetPostprocessStaticTextures";
  case Message::Type::SetPosteffectEnabled: return "SetPosteffectEnabled";
  case Message::Type::RunFirstLaunchAnimation: return "RunFirstLaunchAnimation";
  case Message::Type::UpdateMetalines: return "UpdateMetalines";
  case Message::Type::PostUserEvent: return "PostUserEvent";
  case Message::Type::FinishTexturesInitialization: return "FinishTexturesInitialization";
  case Message::Type::CleanupTextures: return "CleanupTextures";
  case Message::Type::EnableDebugRectRendering: return "EnableDebugRectRendering";
  case Message::Type::EnableTransitScheme: return "EnableTransitScheme";
  case Message::Type::UpdateTransitScheme: return "UpdateTransitScheme";
  case Message::Type::ClearTransitSchemeData: return "ClearTransitSchemeData";
  case Message::Type::ClearAllTransitSchemeData: return "ClearAllTransitSchemeData";
  case Message::Type::RegenerateTransitScheme: return "RegenerateTransitScheme";
  case Message::Type::FlushTransitScheme: return "FlushTransitScheme";
  case Message::Type::ShowDebugInfo: return "ShowDebugInfo";
  case Message::Type::NotifyRenderThread: return "NotifyRenderThread";
  case Message::Type::NotifyGraphicsReady: return "NotifyGraphicsReady";
  case Message::Type::EnableIsolines: return "EnableIsolines";
  case Message::Type::OnEnterBackground: return "OnEnterBackground";
  case Message::Type::Arrow3dRecache: return "Arrow3dRecache";
  }
  ASSERT(false, ("Unknown message type."));
  return "Unknown type";
}
}  // namespace df
