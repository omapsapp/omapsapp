#include "junction_visitor.hpp"

#include "routing/joint_segment.hpp"
#include "routing/route_weight.hpp"

#include "base/logging.hpp"

namespace routing
{

#ifdef DEBUG
// Leaps algorithm.
void DebugRoutingState(Segment const &, std::optional<Segment> const &, RouteWeight const &, RouteWeight const &)
{
  // 1. Dump current processing vertex.
//  std::cout << DebugPrint(vertex);
//  std::cout << std::setprecision(8) << "; H = " << heuristic << "; D = " << distance;

  // 2. Dump parent vertex.
//  std::cout << "; P = " << (parent ? DebugPrint(*parent) : std::string("NO"));

//  std::cout << std::endl;
}

// Joints algorithm.
void DebugRoutingState(JointSegment const &, std::optional<JointSegment> const &, RouteWeight const &,
                       RouteWeight const &)
{
  // 1. Dump current processing vertex.
//  std::cout << DebugPrint(vertex);
//  std::cout << std::setprecision(8) << "; H = " << heuristic << "; D = " << distance;

  // 2. Dump parent vertex.
//  std::cout << "; P = " << (parent ? DebugPrint(*parent) : std::string("NO"));

//  std::cout << std::endl;

  // 3. Set breakpoint on a specific vertex.
//  if (vertex.GetMwmId() == 400 && vertex.GetFeatureId() == 2412 &&
//      vertex.GetEndSegmentId() == 0)
//  {
//    int noop = 0;
//  }

  // 4. Set breakpoint when MWM was changed.
//  if (parent)
//  {
//    auto const & p = *parent;
//    if (!p.IsFake() && !vertex.IsFake() && p.GetMwmId() != vertex.GetMwmId())
//      int noop = 0;
//  }
}
#endif

} // namespace routing
