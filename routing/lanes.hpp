#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace routing
{
class RouteSegment;

namespace turns
{
enum class CarDirection;
}
}  // namespace routing

namespace routing::turns::lanes
{
/**
 * \brief This union represents all possible lane turns according to
 * \link https://wiki.openstreetmap.org/wiki/Key:turn \endlink
 */
union LaneWays
{
  /**
   * \warning Do not change the order of these variables.
   */
  struct
  {
    // clang-format off
    std::uint16_t left           : 1;
    std::uint16_t slightLeft     : 1;
    std::uint16_t sharpLeft      : 1;
    std::uint16_t through        : 1;
    std::uint16_t right          : 1;
    std::uint16_t slightRight    : 1;
    std::uint16_t sharpRight     : 1;
    std::uint16_t reverse        : 1;
    std::uint16_t mergeToLeft    : 1;
    std::uint16_t mergeToRight   : 1;
    std::uint16_t slideLeft      : 1;
    std::uint16_t slideRight     : 1;
    std::uint16_t nextRight      : 1;
    std::uint16_t unused         : 3;
    // clang-format on
  } turns;
  std::uint16_t data = 0;

  bool operator==(LaneWays const other) const { return data == other.data; }
};

struct SingleLaneInfo
{
  LaneWays m_laneWays;
  LaneWays m_recommendedLaneWays;

  bool operator==(SingleLaneInfo const & other) const
  {
    return m_laneWays == other.m_laneWays && m_recommendedLaneWays == other.m_recommendedLaneWays;
  }
};

using LanesInfo = std::vector<SingleLaneInfo>;

/*!
 * \brief Selects lanes which are recommended for an end user.
 */
void SelectRecommendedLanes(std::vector<RouteSegment> & routeSegments);

/**
 * \brief Parse lane information which comes from @lanesString
 * \param lanesString lane information. Example 0b01001|0|0b1000. \see \union LaneWays
 * \param lanes the result of parsing.
 * \return true if @lanesString parsed successfully, false otherwise.
 * \note if @lanesString is empty returns false.
 */
bool ParseLanes(std::string const & lanesString, LanesInfo & lanes);

std::string DebugPrint(LaneWays laneWays);
std::string DebugPrint(SingleLaneInfo const & singleLaneInfo);
std::string DebugPrint(LanesInfo const & lanesInfo);
}  // namespace routing::turns::lanes
