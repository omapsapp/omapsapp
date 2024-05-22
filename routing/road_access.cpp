#include "routing/road_access.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace routing
{

namespace
{
std::string const kNames[] = {"No", "Private", "Destination", "Yes", "Count"};

template <typename KV>
std::string DebugPrintKV(KV const & kvs, size_t /* maxKVToShow */)
{
  // Print all range for now.
  return DebugPrintSequence(kvs.begin(), kvs.end());
}
}  // namespace

/** RoadAccess --------------------------------------------------------------------------------------
 * @todo (bykoinako) It's a fast fix for release. The idea behind it is to remember the time of
 * creation RoadAccess instance and return it instead of getting time when m_currentTimeGetter() is
 * called. But it's not understandable now why m_currentTimeGetter() is called when
 * cross_mwm section is built.
 *
 * @todo (vng) Return back lazy time calculation.
 * It's called in cross_mwm_section via IndexGraph::CalculateEdgeWeight.
 * Fixed by setting custom time getter in BuildRoutingCrossMwmSection -> FillWeights.
 */
RoadAccess::RoadAccess() : m_currentTimeGetter([]() { return GetCurrentTimestamp(); }) {}

std::pair<RoadAccess::Type, RoadAccess::Confidence> RoadAccess::GetAccess(
    uint32_t featureId, RouteWeight const & weightToFeature) const
{
  return GetAccess(featureId, weightToFeature.GetWeight());
}

std::pair<RoadAccess::Type, RoadAccess::Confidence> RoadAccess::GetAccess(
    RoadPoint const & point, RouteWeight const & weightToPoint) const
{
  return GetAccess(point, weightToPoint.GetWeight());
}

std::pair<RoadAccess::Type, RoadAccess::Confidence> RoadAccess::GetAccess(
    uint32_t featureId, double weight) const
{
  auto const itConditional = m_wayToAccessConditional.find(featureId);
  if (itConditional != m_wayToAccessConditional.cend())
  {
    auto const time = m_currentTimeGetter();
    auto const & conditional = itConditional->second;
    for (auto const & access : conditional.GetAccesses())
    {
      auto const op = GetConfidenceForAccessConditional(time + weight, access.m_openingHours);
      if (op)
        return {access.m_type, *op};
    }
  }

  return GetAccessWithoutConditional(featureId);
}

std::pair<RoadAccess::Type, RoadAccess::Confidence> RoadAccess::GetAccess(
    RoadPoint const & point, double weight) const
{
  auto const itConditional = m_pointToAccessConditional.find(point);
  if (itConditional != m_pointToAccessConditional.cend())
  {
    auto const time = m_currentTimeGetter();
    auto const & conditional = itConditional->second;
    for (auto const & access : conditional.GetAccesses())
    {
      auto const op = GetConfidenceForAccessConditional(time + weight, access.m_openingHours);
      if (op)
        return {access.m_type, *op};
    }
  }

  return GetAccessWithoutConditional(point);
}

std::pair<RoadAccess::Type, RoadAccess::Confidence> RoadAccess::GetAccessWithoutConditional(
    uint32_t featureId) const
{
  // todo(@m) This may or may not be too slow. Consider profiling this and using
  // a Bloom filter or anything else that is faster than ska::flat_hash_map
  auto const it = m_wayToAccess.find(featureId);
  if (it != m_wayToAccess.cend())
    return {it->second, Confidence::Sure};

  return {Type::Yes, Confidence::Sure};
}

std::pair<RoadAccess::Type, RoadAccess::Confidence> RoadAccess::GetAccessWithoutConditional(
    RoadPoint const & point) const
{
  auto const it = m_pointToAccess.find(point);
  if (it != m_pointToAccess.cend())
    return {it->second, Confidence::Sure};

  return {Type::Yes, Confidence::Sure};
}

bool RoadAccess::operator==(RoadAccess const & rhs) const
{
  return m_wayToAccess == rhs.m_wayToAccess && m_pointToAccess == rhs.m_pointToAccess &&
         m_wayToAccessConditional == rhs.m_wayToAccessConditional &&
         m_pointToAccessConditional == rhs.m_pointToAccessConditional;
}

// static
std::optional<RoadAccess::Confidence> RoadAccess::GetConfidenceForAccessConditional(
    time_t momentInTime, osmoh::OpeningHours const & openingHours)
{
  auto const left = momentInTime - kConfidenceIntervalSeconds / 2;
  auto const right = momentInTime + kConfidenceIntervalSeconds / 2;

  auto const leftOpen = openingHours.IsOpen(left);
  auto const rightOpen = openingHours.IsOpen(right);

  if (!leftOpen && !rightOpen)
    return {};

  return leftOpen && rightOpen ? Confidence::Sure : Confidence::Maybe;
}

// Functions ---------------------------------------------------------------------------------------
time_t GetCurrentTimestamp()
{
  using system_clock = std::chrono::system_clock;
  return system_clock::to_time_t(system_clock::now());
}

std::string ToString(RoadAccess::Type type)
{
  if (type <= RoadAccess::Type::Count)
    return kNames[static_cast<size_t>(type)];
  CHECK(false, ("Bad road access type", static_cast<size_t>(type)));
  return {};
}

void FromString(std::string_view s, RoadAccess::Type & result)
{
  for (size_t i = 0; i <= static_cast<size_t>(RoadAccess::Type::Count); ++i)
  {
    if (s == kNames[i])
    {
      result = static_cast<RoadAccess::Type>(i);
      return;
    }
  }
  CHECK(false, ("Could not read RoadAccess from the string", s));
}

std::string DebugPrint(RoadAccess::Conditional const & conditional)
{
  std::ostringstream oss;
  oss << "Conditional { ";
  for (auto const & access : conditional.GetAccesses())
    oss << DebugPrint(access.m_type) << " @ (" << access.m_openingHours.GetRule() << "), ";
  oss << " }";
  return oss.str();
}

std::string DebugPrint(RoadAccess::Confidence confidence)
{
  switch (confidence)
  {
  case RoadAccess::Confidence::Maybe: return "Maybe";
  case RoadAccess::Confidence::Sure: return "Sure";
  }
  UNREACHABLE();
}

std::string DebugPrint(RoadAccess::Type type) { return ToString(type); }

std::string DebugPrint(RoadAccess const & r)
{
  size_t constexpr kMaxIdsToShow = 10;

  std::ostringstream oss;
  oss << "RoadAccess { WayToAccess " << DebugPrintKV(r.GetWayToAccess(), kMaxIdsToShow)
      << "; PointToAccess "<< DebugPrintKV(r.GetPointToAccess(), kMaxIdsToShow)
      << "; WayToAccessConditional "<< DebugPrintKV(r.GetWayToAccessConditional(), kMaxIdsToShow)
      << "; PointToAccessConditional " << DebugPrintKV(r.GetPointToAccessConditional(), kMaxIdsToShow)
      << " }";
  return oss.str();
}

}  // namespace routing
