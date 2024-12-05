#include "distance.hpp"

#include "platform/locale.hpp"
#include "platform/localization.hpp"
#include "platform/measurement_utils.hpp"

#include "base/assert.hpp"

namespace platform
{
using namespace measurement_utils;

namespace
{
Distance MetersTo(double distance, Distance::Units units)
{
  switch (units)
  {
  case Distance::Units::Meters:
    return Distance(distance);
  case Distance::Units::Kilometers:
    return {distance / 1000, Distance::Units::Kilometers};
  case Distance::Units::Feet:
    return {MetersToFeet(distance), Distance::Units::Feet};
  case Distance::Units::Miles:
    return {MetersToMiles(distance), Distance::Units::Miles};
  default: UNREACHABLE();
  }
}

Distance KilometersTo(double distance, Distance::Units units)
{
  return MetersTo(distance * 1000, units);
}

Distance FeetTo(double distance, Distance::Units units)
{
  switch (units)
  {
  case Distance::Units::Meters:
    return {FeetToMeters(distance), Distance::Units::Meters};
  case Distance::Units::Kilometers:
    return {FeetToMeters(distance) / 1000, Distance::Units::Kilometers};
  case Distance::Units::Miles:
    return {FeetToMiles(distance), Distance::Units::Miles};
  default: UNREACHABLE();
  }
}

Distance MilesTo(double distance, Distance::Units units)
{
  switch (units)
  {
  case Distance::Units::Meters:
    return {MilesToMeters(distance), Distance::Units::Meters};
  case Distance::Units::Kilometers:
    return {MilesToMeters(distance) / 1000, Distance::Units::Kilometers};
  case Distance::Units::Feet:
    return {MilesToFeet(distance), Distance::Units::Feet};
  default: UNREACHABLE();
  }
}

double WithPrecision(double value, uint8_t precision)
{
  if (precision == 0)
    return std::round(value);

  double const factor = math::PowUint(10.0, precision);
  return std::round(value * factor) / factor;
}
}  // namespace

Distance::Distance() : Distance(-1.0) {}

Distance::Distance(double distanceInMeters) : Distance(distanceInMeters, Units::Meters) {}

Distance::Distance(double distance, platform::Distance::Units units) : m_distance(distance), m_units(units) {}

Distance Distance::CreateFormatted(double distanceInMeters)
{
  return Distance(distanceInMeters).ToPlatformUnitsFormatted();
}

std::string Distance::FormatAltitude(double meters)
{
  Distance elevation = Distance(fabs(meters)).To(
      GetMeasurementUnits() == measurement_utils::Units::Metric ? Units::Meters : Units::Feet);

  ASSERT(elevation.IsLowUnits(), ());
  elevation.m_distance = WithPrecision(elevation.m_distance, 0);

  auto res = elevation.ToString();
  return meters < 0 ? "-" + res : res;
}

bool Distance::IsValid() const { return m_distance >= 0.0; }

bool Distance::IsLowUnits() const { return m_units == Units::Meters || m_units == Units::Feet; }

bool Distance::IsHighUnits() const { return !IsLowUnits(); }

Distance Distance::To(Units units) const
{
  if (m_units == units)
    return *this;

  /// @todo These double switches can be replaced with 4x4 factors matrix.
  switch (m_units)
  {
  case Units::Meters: return MetersTo(m_distance, units);
  case Units::Kilometers: return KilometersTo(m_distance, units);
  case Units::Feet: return FeetTo(m_distance, units);
  case Units::Miles: return MilesTo(m_distance, units);
  default: UNREACHABLE();
  }
}

Distance Distance::ToPlatformUnitsFormatted() const
{
  return To(GetMeasurementUnits() == measurement_utils::Units::Metric ? Units::Meters : Units::Feet)
      .GetFormattedDistance();
}

double Distance::GetDistance() const
{
  return m_distance;
}

Distance::Units Distance::GetUnits() const { return m_units; }

std::string Distance::GetDistanceString() const
{
  if (!IsValid())
    return "";

  // Default precision is 0 (no decimals).
  int precision = 0;

  // Set 1 decimal precision for high distances (km, miles) lower than 10.0 (9.5, 7.0,...).
  if (m_distance < 10.0 && IsHighUnits())
    precision = 1;

  return ToStringPrecision(m_distance, precision);
}

std::string Distance::GetUnitsString() const
{
  switch (m_units)
  {
  case Units::Meters: return GetLocalizedString("m");
  case Units::Kilometers: return GetLocalizedString("km");
  case Units::Feet: return GetLocalizedString("ft");
  case Units::Miles: return GetLocalizedString("mi");
  default: UNREACHABLE();
  }
}

Distance Distance::GetFormattedDistance() const
{
  ASSERT(IsValid(), ());

  // To low units.
  Distance res;
  if (m_units == Units::Kilometers)
    res = To(Units::Meters);
  else if (m_units == Units::Miles)
    res = To(Units::Feet);
  else
    res = *this;

  double lowRound = std::round(res.m_distance);
  // Round distances over 100 units to 10 units, e.g. 112 -> 110, 998 -> 1000
  if (lowRound > 100)
    lowRound = std::round(lowRound / 10) * 10;

  // Use high units for distances of 1000 units and over,
  // e.g. 1000m -> 1.0km, 1290m -> 1.3km, 1000ft -> 0.2mi
  if (lowRound >= 1000.0)
  {
    // To high units.
    res = res.To(res.m_units == Units::Meters ? Units::Kilometers : Units::Miles);

    // For distances of 10.0 high units and over round to a whole number, e.g. 9.98 -> 10, 10.9 -> 11
    uint8_t const precision = (std::round(res.m_distance * 10) / 10 >= 10.0) ? 0 : 1;
    return { WithPrecision(res.m_distance, precision), res.m_units };
  }

  res.m_distance = lowRound;
  return res;
}

std::string Distance::ToString() const
{
  if (!IsValid())
    return "";

  return GetDistanceString() + kNarrowNonBreakingSpace + GetUnitsString();
}

std::string DebugPrint(Distance::Units units)
{
  switch (units)
  {
  case Distance::Units::Meters: return "m";
  case Distance::Units::Kilometers: return "km";
  case Distance::Units::Feet: return "ft";
  case Distance::Units::Miles: return "mi";
  default: UNREACHABLE();
  }
}

}  // namespace platform
