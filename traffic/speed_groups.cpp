#include "traffic/speed_groups.hpp"

#include "base/math.hpp"

namespace traffic
{
uint32_t const kSpeedGroupThresholdPercentage[] = {8, 16, 33, 58, 83, 100, 100, 100};

SpeedGroup GetSpeedGroupByPercentage(double p)
{
  p = math::Clamp(p, 0.0, 100.0);
  SpeedGroup res = SpeedGroup::Unknown;
  for (int i = static_cast<int>(SpeedGroup::Count) - 1; i >= 0; --i)
  {
    if (p <= kSpeedGroupThresholdPercentage[i])
      res = static_cast<SpeedGroup>(i);
  }
  return res;
}

std::string DebugPrint(SpeedGroup const & group)
{
  switch (group)
  {
  case SpeedGroup::G0: return "G0";
  case SpeedGroup::G1: return "G1";
  case SpeedGroup::G2: return "G2";
  case SpeedGroup::G3: return "G3";
  case SpeedGroup::G4: return "G4";
  case SpeedGroup::G5: return "G5";
  case SpeedGroup::TempBlock: return "TempBlock";
  case SpeedGroup::Unknown: return "Unknown";
  case SpeedGroup::Count: return "Count";
  }
  UNREACHABLE();
}
}  // namespace traffic
