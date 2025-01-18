package app.organicmaps.routing;

import androidx.annotation.NonNull;

import app.organicmaps.sdk.routing.RouteMarkType;

public interface RoutingBottomMenuListener
{
  void onUseMyPositionAsStart();
  void onSearchRoutePoint(@NonNull RouteMarkType type);
  void onRoutingStart();
}
