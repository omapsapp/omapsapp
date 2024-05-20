#pragma once

#include "routing/directions_engine.hpp"

#include "routing_common/num_mwm_id.hpp"
#include "routing/route.hpp"

#include <map>
#include <memory>
#include <vector>

namespace routing
{

class CarDirectionsEngine : public DirectionsEngine
{
public:
  CarDirectionsEngine(MwmDataSource & dataSource, std::shared_ptr<NumMwmIds> numMwmIds);

protected:
  virtual size_t GetTurnDirection(turns::IRoutingResult const & result, size_t const outgoingSegmentIndex,
                                  NumMwmIds const & numMwmIds,
                                  RoutingSettings const & vehicleSettings, turns::TurnItem & turn);
  virtual void FixupTurns(std::vector<RouteSegment> & routeSegments);
};

void FixupCarTurns(std::vector<RouteSegment> & routeSegments);

/*!
 * \brief Finds an U-turn that starts from master segment and returns how many segments it lasts.
 * \returns an index in |segments| that has the opposite direction with master segment
 * (|segments[currentSegment - 1]|) and 0 if there is no UTurn.
 * \warning |currentSegment| must be greater than 0.
 */
size_t CheckUTurnOnRoute(turns::IRoutingResult const & result, size_t const outgoingSegmentIndex,
                         NumMwmIds const & numMwmIds, RoutingSettings const & vehicleSettings,
                         turns::TurnItem & turn);

/*!
 * \brief Calculates a turn instruction if the ingoing edge or (and) the outgoing edge belongs to a
 * roundabout.
 * \return Returns one of the following results:
 * - TurnDirection::EnterRoundAbout if the ingoing edge does not belong to a roundabout
 *   and the outgoing edge belongs to a roundabout.
 * - TurnDirection::StayOnRoundAbout if the ingoing edge and the outgoing edge belong to a
 * roundabout
 *   and there is a reasonalbe way to leave the junction besides the outgoing edge.
 *   This function does not return TurnDirection::StayOnRoundAbout for small ways to leave the
 * roundabout.
 * - TurnDirection::NoTurn if the ingoing edge and the outgoing edge belong to a roundabout
 *   (a) and there is a single way (outgoing edge) to leave the junction.
 *   (b) and there is a way(s) besides outgoing edge to leave the junction (the roundabout)
 *       but it is (they are) relevantly small.
 */
turns::CarDirection GetRoundaboutDirectionBasic(bool isIngoingEdgeRoundabout, bool isOutgoingEdgeRoundabout,
                                                bool isMultiTurnJunction, bool keepTurnByHighwayClass);

}  // namespace routing
