#include "turn_lanes_metadata_processor.hpp"

#include "base/string_utils.hpp"

#include <unordered_map>
#include <vector>

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
std::string TurnLanesMetadataProcessor::ValidateAndFormat(std::string value)
{
  if (value.empty())
    return "";

  strings::AsciiToLower(value);
  base::EraseIf(value, [](char const c) { return isspace(c); });

  std::vector<std::uint16_t> lanes;
  for (auto const lanesStr : strings::Tokenize(value, "|"))
  {
    lanes.emplace_back(0);
    auto & laneWays = lanes.back();
    for (auto const laneWayStr : strings::Tokenize(lanesStr, ";"))
    {
      if (laneWayStrToBitIdx.count(laneWayStr))
        laneWays |= 1 << laneWayStrToBitIdx.at(laneWayStr);
    }
  }
  return ToString(lanes);
}
}  // namespace generator
