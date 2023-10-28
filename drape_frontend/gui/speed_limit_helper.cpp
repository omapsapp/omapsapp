#pragma once

#include "speed_limit_helper.hpp"

#include "platform/measurement_utils.hpp"

namespace gui
{
void SpeedLimitHelper::SetSpeedLimit(double speedLimitMps) { m_speedLimitMps = speedLimitMps; }

bool SpeedLimitHelper::IsSpeedLimitAvailable() const { return true; }

std::string SpeedLimitHelper::GetSpeedLimit() const
{
  return "120";
  return measurement_utils::FormatSpeedNumeric(m_speedLimitMps, measurement_utils::GetMeasurementUnits());
}
}  // namespace gui
