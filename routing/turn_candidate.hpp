#pragma once

#include "routing/segment.hpp"
#include "routing/turns.hpp"

#include "base/math.hpp"

#include <vector>

namespace ftypes
{
enum class HighwayClass;
}

namespace routing
{
namespace turns
{
/// \brief The TurnCandidate struct contains information about possible ways from a junction.
struct TurnCandidate
{

  /// |m_angle| is the angle of the turn in degrees. It means the angle is 180 minus
  /// the angle between the current edge and the edge of the candidate. A counterclockwise rotation.
  /// The current edge is an edge which belongs to the route and is located before the junction.
  /// |m_angle| belongs to the range [-180; 180];
  double m_angle;

  /// |m_segment| is the first segment of a possible way from the junction.
  Segment m_segment;

  /// \brief |highwayClass| field for the road class caching. Because feature reading is
  /// a time-consuming function.
  ftypes::HighwayClass m_highwayClass;

  /// If |isLink| is true then the turn candidate is a link.
  bool m_isLink;

  TurnCandidate(double angle, Segment const & segment, ftypes::HighwayClass c, bool isLink)
    : m_angle(angle), m_segment(segment), m_highwayClass(c), m_isLink(isLink)
  {
  }

  bool IsAlmostEqual(TurnCandidate const & rhs) const
  {
    double constexpr kEpsilon = 0.01;
    return AlmostEqualAbs(m_angle, rhs.m_angle, kEpsilon) && m_segment == rhs.m_segment &&
           m_highwayClass == rhs.m_highwayClass && m_isLink == rhs.m_isLink;
  }
};

inline bool IsAlmostEqual(std::vector<TurnCandidate> const & lhs, std::vector<TurnCandidate> const & rhs)
{
  if (lhs.size() != rhs.size())
    return false;

  for (size_t i = 0; i < lhs.size(); ++i)
  {
    if (!lhs[i].IsAlmostEqual(rhs[i]))
      return false;
  }
  return true;
}

struct TurnCandidates
{
  std::vector<TurnCandidate> candidates;
  bool isCandidatesAngleValid;

  explicit TurnCandidates(bool angleValid = true) : isCandidatesAngleValid(angleValid) {}

  bool IsAlmostEqual(TurnCandidates const & rhs) const
  {
    return turns::IsAlmostEqual(candidates, rhs.candidates) &&
           isCandidatesAngleValid == rhs.isCandidatesAngleValid;
  }
};
}  // namespace routing
}  // namespace turns
