#pragma once

#include <string>

namespace gui
{
class SpeedLimitHelper
{
public:
  void SetSpeedLimit(double speedLimitMps);

  bool IsSpeedLimitAvailable() const;
  std::string GetSpeedLimit() const;

private:
  double m_speedLimitMps;
};
}  // namespace gui
