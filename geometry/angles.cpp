#include "geometry/angles.hpp"

namespace ang
{
double AngleIn2PI(double ang)
{
  double constexpr period = 2.0 * math::pi;
  ang = fmod(ang, period);
  if (ang < 0.0)
    ang += period;

  if (AlmostEqualULPs(period, ang))
    return 0.0;

  return ang;
}

double GetShortestDistance(double rad1, double rad2)
{
  double constexpr period = 2.0 * math::pi;
  rad1 = fmod(rad1, period);
  rad2 = fmod(rad2, period);

  double res = rad2 - rad1;
  if (fabs(res) > math::pi)
  {
    if (res < 0.0)
      res = period + res;
    else
      res = -period + res;
  }
  return res;
}

double GetMiddleAngle(double a1, double a2)
{
  double ang = (a1 + a2) / 2.0;

  if (fabs(a1 - a2) > math::pi)
  {
    if (ang > 0.0)
      ang -= math::pi;
    else
      ang += math::pi;
  }
  return ang;
}
}  // namespace ang
