#include "routing/routing_settings.hpp"

#include "routing/routing_helpers.hpp"

#include "base/assert.hpp"

namespace routing
{
// RoutingSettings ---------------------------------------------------------------------------------
RoutingSettings::RoutingSettings(bool useDirectionForRouteBuilding, bool matchRoute,
                                 bool soundDirection, double matchingThresholdM,
                                 bool showTurnAfterNext, double minSpeedForRouteRebuildMpS,
                                 double finishToleranceM, size_t maxOutgoingPointsCount,
                                 double minOutgoingDistMeters, size_t maxIngoingPointsCount,
                                 double minIngoingDistMeters, size_t notSoCloseMaxPointsCount,
                                 double notSoCloseMaxDistMeters)

  : m_useDirectionForRouteBuilding(useDirectionForRouteBuilding)
  , m_matchRoute(matchRoute)
  , m_soundDirection(soundDirection)
  , m_matchingThresholdM(matchingThresholdM)
  , m_showTurnAfterNext(showTurnAfterNext)
  , m_minSpeedForRouteRebuildMpS(minSpeedForRouteRebuildMpS)
  , m_finishToleranceM(finishToleranceM)
  , m_maxOutgoingPointsCount(maxOutgoingPointsCount)
  , m_minOutgoingDistMeters(minOutgoingDistMeters)
  , m_maxIngoingPointsCount(maxIngoingPointsCount)
  , m_minIngoingDistMeters(minIngoingDistMeters)
  , m_notSoCloseMaxPointsCount(notSoCloseMaxPointsCount)
  , m_notSoCloseMaxDistMeters(notSoCloseMaxDistMeters)
{
}

RoutingSettings GetRoutingSettings(VehicleType vehicleType)
{
  switch (vehicleType)
  {
  case VehicleType::Pedestrian:
    return {false /* useDirectionForRouteBuilding */,
            false /* m_matchRoute */,
            true /* m_soundDirection */,
            20.0 /* m_matchingThresholdM */,
            false /* m_showTurnAfterNext */,
            -1 /* m_minSpeedForRouteRebuildMpS */,
            15.0 /* m_finishToleranceM */,
            6 /* m_maxOutgoingPointsCount */,
            5.0 /* m_minOutgoingDistMeters */,
            2 /* m_maxIngoingPointsCount */,
            4.0 /* m_minIngoingDistMeters */,
            3 /* m_notSoCloseMaxPointsCount */,
            25.0 /* m_notSoCloseMaxDistMeters */};
  case VehicleType::Transit:
    return {false /* useDirectionForRouteBuilding */,
            true /* m_matchRoute */,
            false /* m_soundDirection */,
            40.0 /* m_matchingThresholdM */,
            false /* m_showTurnAfterNext */,
            -1 /* m_minSpeedForRouteRebuildMpS */,
            15.0 /* m_finishToleranceM */,
            6 /* m_maxOutgoingPointsCount */,
            5.0 /* m_minOutgoingDistMeters */,
            2 /* m_maxIngoingPointsCount */,
            4.0 /* m_minIngoingDistMeters */,
            3 /* m_notSoCloseMaxPointsCount */,
            25.0 /* m_notSoCloseMaxDistMeters */};
  case VehicleType::Bicycle:
    return {false /* useDirectionForRouteBuilding */,
            true /* m_matchRoute */,
            true /* m_soundDirection */,
            30.0 /* m_matchingThresholdM */,
            false /* m_showTurnAfterNext */,
            -1 /* m_minSpeedForRouteRebuildMpS */,
            15.0 /* m_finishToleranceM */,
            9 /* m_maxOutgoingPointsCount */,
            10.0 /* m_minOutgoingDistMeters */,
            2 /* m_maxIngoingPointsCount */,
            10.0 /* m_minIngoingDistMeters */,
            3 /* m_notSoCloseMaxPointsCount */,
            25.0 /* m_notSoCloseMaxDistMeters */};
  case VehicleType::Car:
    return {true /* useDirectionForRouteBuilding */,
            true /* m_matchRoute */,
            true /* m_soundDirection */,
            50.0 /* m_matchingThresholdM */,
            true /* m_showTurnAfterNext */,
            measurement_utils::KmphToMps(3.0) /* m_minSpeedForRouteRebuildMpS */,
            30.0 /* m_finishToleranceM */,
            9 /* m_maxOutgoingPointsCount */,
            120.0 /* m_minOutgoingDistMeters */,
            2 /* m_maxIngoingPointsCount */,
            100.0 /* m_minIngoingDistMeters */,
            3 /* m_notSoCloseMaxPointsCount */,
            30.0 /* m_notSoCloseMaxDistMeters */};
  case VehicleType::Count:
    CHECK(false, ("Can't create GetRoutingSettings for", vehicleType));
  }
  UNREACHABLE();
}
}  // namespace routing

