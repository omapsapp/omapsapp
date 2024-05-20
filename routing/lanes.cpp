#include "lanes.hpp"

#include "base/string_utils.hpp"

using routing::turns::CarDirection;

// TODO: remove before merge
namespace
{
/// @warning The order of these values must be synchronized with @union LaneWays
const std::unordered_map<std::string_view, std::uint8_t> laneWayStrToBitIdx = {
  // clang-format off
  {"left",             0},
  {"slight_left",      1},
  {"sharp_left",       2},
  {"through",          3},
  {"right",            4},
  {"slight_right",     5},
  {"sharp_right",      6},
  {"reverse",          7},
  {"merge_to_left",    8},
  {"merge_to_right",   9},
  {"slide_left",      10},
  {"slide_right",     11},
  {"next_right",      12},
  // clang-format on
};

std::string ToString(std::vector<std::uint16_t> const & lanes)
{
  std::ostringstream os;
  bool first = true;
  for (auto const & laneWays : lanes)
  {
    if (!first)
      os << "|";
    os << std::to_string(laneWays);
    first = false;
  }
  return os.str();
}
}  // namespace

namespace generator
{
std::string ValidateAndFormat(std::string value)
{
  if (value.empty())
    return "";

  strings::AsciiToLower(value);
  base::EraseIf(value, [](char const c) { return isspace(c); });

  std::vector<std::uint16_t> lanes;
  for (auto const lanesStr : strings::Tokenize<std::string_view, true>(value, "|"))
  {
    lanes.emplace_back(0);
    auto & laneWays = lanes.back();
    for (auto const laneWayStr : strings::Tokenize<std::string_view, true>(lanesStr, ";"))
    {
      if (laneWayStrToBitIdx.count(laneWayStr))
        laneWays |= 1 << laneWayStrToBitIdx.at(laneWayStr);
    }
  }
  return ToString(lanes);
}
}
// TODO: Remove before merge

namespace
{
using CarDirectionToLaneWays = std::unordered_map<CarDirection, std::uint16_t>;

const CarDirectionToLaneWays carDirectionToLaneWays{
    // clang-format off
    {CarDirection::TurnLeft,           0b00000001},  // Left
    {CarDirection::TurnSlightLeft,     0b00000010},  // SlightLeft
    {CarDirection::ExitHighwayToLeft,  0b00000010},  // SlightLeft
    {CarDirection::TurnSharpLeft,      0b00000100},  // SharpLeft
    {CarDirection::GoStraight,         0b00001000},  // Through
    {CarDirection::TurnRight,          0b00010000},  // Right
    {CarDirection::TurnSlightRight,    0b00100000},  // SlightRight
    {CarDirection::ExitHighwayToRight, 0b00100000},  // SlightRight
    {CarDirection::TurnSharpRight,     0b01000000},  // SharpRight
    {CarDirection::UTurnLeft,          0b10000000},  // Reverse
    {CarDirection::UTurnRight,         0b10000000},  // Reverse
    // clang-format on
};

const CarDirectionToLaneWays carDirectionToLaneWaysApproximate{
    // clang-format off
    {CarDirection::TurnLeft,           0b0000000000111},  // Left, SlightLeft, SharpLeft
    {CarDirection::TurnSlightLeft,     0b0000000001011},  // Left, SlightLeft, Through
    {CarDirection::ExitHighwayToLeft,  0b0000000000011},  // Left, SlightLeft
    {CarDirection::TurnSharpLeft,      0b0000000000101},  // Left, SharpLeft
    {CarDirection::GoStraight,         0b0111100101010},  // SlightLeft, Through, SlightRight, MergeToLeft, MergeToRight, SlideLeft, SlideRight
    {CarDirection::TurnRight,          0b1000001110000},  // Right, SlightRight, SharpRight, NextRight
    {CarDirection::TurnSlightRight,    0b0000000111000},  // Through, Right, SlightRight
    {CarDirection::ExitHighwayToRight, 0b0000000110000},  // Right, SlightRight
    {CarDirection::TurnSharpRight,     0b0000001010000},  // Right, SharpRight
    {CarDirection::UTurnLeft,          0b0000010000000},  // Reverse
    {CarDirection::UTurnRight,         0b0000010000000},  // Reverse
    // clang-format on
};
}  // namespace

namespace routing::turns::lanes
{
namespace
{
bool FixupLaneSet(CarDirection const turn, LanesInfo & lanes, CarDirectionToLaneWays const & mapping)
{
  bool isLaneConformed = false;
  for (auto & [laneWays, recommendedLaneWays] : lanes)
  {
    if (mapping.count(turn))
    {
      recommendedLaneWays.data = laneWays.data & mapping.at(turn);
      if (recommendedLaneWays.data != 0)
        isLaneConformed = true;
    }
  }
  return isLaneConformed;
}

template <typename It>
bool SelectFirstUnrestrictedLane(bool direction, It lanesBegin, It lanesEnd)
{
  const It firstUnrestricted = find_if(lanesBegin, lanesEnd, [](auto const & it) { return it.m_laneWays.data == 0; });
  if (firstUnrestricted == lanesEnd)
    return false;

  if (direction)
  {
    firstUnrestricted->m_laneWays.turns.left = 1;
    firstUnrestricted->m_recommendedLaneWays.turns.left = 1;
  }
  else
  {
    firstUnrestricted->m_laneWays.turns.right = 1;
    firstUnrestricted->m_recommendedLaneWays.turns.right = 1;
  }
  return true;
}

bool SelectUnrestrictedLane(CarDirection const turn, LanesInfo & lanes)
{
  if (IsTurnMadeFromLeft(turn))
    return SelectFirstUnrestrictedLane(true, lanes.begin(), lanes.end());
  if (IsTurnMadeFromRight(turn))
    return SelectFirstUnrestrictedLane(false, lanes.rbegin(), lanes.rend());
  return false;
}
}  // namespace

void SelectRecommendedLanes(vector<RouteSegment> & routeSegments)
{
  for (auto & segment : routeSegments)
  {
    auto & t = segment.GetTurn();
    if (t.IsTurnNone() || t.m_lanes.empty())
      continue;
    auto & lanes = segment.GetTurnLanes();
    // Checking if there are elements in lanes which correspond with the turn exactly.
    // If so fixing up all the elements in lanes which correspond with the turn.
    if (FixupLaneSet(t.m_turn, lanes, carDirectionToLaneWays))
      continue;
    // If not checking if there are elements in lanes which corresponds with the turn
    // approximately. If so fixing up all these elements.
    if (FixupLaneSet(t.m_turn, lanes, carDirectionToLaneWaysApproximate))
      continue;
    // If not, check if there is an unrestricted lane which could correspond to the
    // turn. If so, fix up that lane.
    if (SelectUnrestrictedLane(t.m_turn, lanes))
      continue;
    // Otherwise, we don't have lane recommendations for the user, so we don't
    // want to send the lane data any further.
    segment.ClearTurnLanes();
  }
}

bool ParseLanes(std::string const & lanesString, LanesInfo & lanes)
{
  if (lanesString.empty())
    return false;
  lanes.clear();

  // TODO: Complete solution requires maps regeneration. This is a workaround for testing. Remove before merge.
  std::string const & lanesStr = generator::ValidateAndFormat(lanesString);

  for (auto const laneWaysStr : strings::Tokenize<std::string_view, true>(lanesStr, "|"))
  {
    SingleLaneInfo info;
    // TODO: Fix std::string(laneWaysStr).c_str()
    VERIFY(strings::to_uint16(std::string(laneWaysStr).c_str(), info.m_laneWays.data), ());
    ASSERT(info.m_laneWays.turns.unused == 0, ());
    lanes.push_back(info);
  }
  return true;
}

std::string DebugPrint(LaneWays const laneWays)
{
  std::ostringstream out;
  if (laneWays.data == 0)
  {
    out << "LaneWays[none]";
    return out.str();
  }

  // clang-format off
  out << "LaneWays["
      << "Left: " << laneWays.turns.left
      << ", SlightLeft: " << laneWays.turns.slightLeft
      << ", SharpLeft: " << laneWays.turns.sharpLeft
      << ", Through: " << laneWays.turns.through
      << ", Right: " << laneWays.turns.right
      << ", SlightRight: " << laneWays.turns.slightRight
      << ", SharpRight: " << laneWays.turns.sharpRight
      << ", Reverse: " << laneWays.turns.reverse
      << ", MergeToLeft: " << laneWays.turns.mergeToLeft
      << ", MergeToRight: " << laneWays.turns.mergeToRight
      << ", SlideLeft: " << laneWays.turns.slideLeft
      << ", SlideRight: " << laneWays.turns.slideRight
      << ", NextRight: " << laneWays.turns.nextRight
      << ", UnusedBits: " << laneWays.turns.unused
      << "]";
  // clang-format on
  return out.str();
}

std::string DebugPrint(SingleLaneInfo const & singleLaneInfo)
{
  std::stringstream out;
  out << "SingleLaneInfo[m_laneWays: " << DebugPrint(singleLaneInfo.m_laneWays)
      << ", m_recommendedLaneWays: " << DebugPrint(singleLaneInfo.m_recommendedLaneWays) << "]";
  return out.str();
}

std::string DebugPrint(LanesInfo const & lanesInfo)
{
  std::stringstream out;
  out << "LanesInfo[";
  for (auto const & laneInfo : lanesInfo)
    out << DebugPrint(laneInfo) << ", ";
  out << "]";
  return out.str();
}
}  // namespace routing::turns::lanes
