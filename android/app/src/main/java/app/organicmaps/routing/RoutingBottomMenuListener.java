package app.organicmaps.routing;

import app.organicmaps.sdk.routing.RoutePointInfo;

public interface RoutingBottomMenuListener
{
  void onUseMyPositionAsStart();
  void onSearchRoutePoint(@RoutePointInfo.RouteMarkType int type);
  void onRoutingStart();
}
