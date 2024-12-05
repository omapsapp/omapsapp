#include "geometry/oblate_spheroid.hpp"
#include "geometry/distance_on_sphere.hpp"
#include "geometry/point2d.hpp"

#include "base/math.hpp"

#include <cfloat>
#include <cmath>
#include <limits>

namespace oblate_spheroid
{
// Length of semi-major axis of the spheroid WGS-84.
static double constexpr kA = 6378137.0;

// Flattening of the spheroid.
static double constexpr kF = 1.0 / 298.257223563;

// Length of semi-minor axis of the spheroid ~ 6356752.31424.
static double constexpr kB = (1.0 - kF) * kA;

// Desired degree of accuracy for convergence of Vincenty's formulae.
static double constexpr kEps = 1e-10;

// Maximum iterations of distance evaluation.
static int constexpr kIterations = 10;

/// \brief Calculates latitude on the auxiliary sphere for |angleRad| latitude on a spheroid.
static double ReducedLatitude(double angleRad) { return std::atan((1.0 - kF) * std::tan(angleRad)); }

double GetDistance(ms::LatLon const & point1, ms::LatLon const & point2)
{
  using namespace base;
  using namespace std;
  using math::DegToRad, math::Pow2;

  m2::PointD const p1 = {DegToRad(point1.m_lon), DegToRad(point1.m_lat)};
  m2::PointD const p2 = {DegToRad(point2.m_lon), DegToRad(point2.m_lat)};
  double const U1 = ReducedLatitude(p1.y);
  double const U2 = ReducedLatitude(p2.y);
  double const sinU1 = sin(U1);
  double const cosU1 = cos(U1);
  double const sinU2 = sin(U2);
  double const cosU2 = cos(U2);

  // Difference in longitude of two points.
  double const L = p2.x - p1.x;
  // Difference in longitude on the auxiliary sphere.
  double lambda = L;
  double lambdaPrev = std::numeric_limits<double>::max();
  int iterations = kIterations;
  double sinSigma = 1.;
  double cosSigma = 0.;
  double sigma = 0.;
  double cosAlphaSquare = 0.;
  double cosDoubleSigmaMid = 0.;
  double cosDoubleSigmaMidSquare = 0.;

  while (iterations-- > 0 && !AlmostEqualAbs(lambda, lambdaPrev, kEps))
  {
    sinSigma = sqrt(Pow2(cosU2 * sin(lambda)) + Pow2(cosU1 * sinU2 - sinU1 * cosU2 * cos(lambda)));
    cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cos(lambda);
    sigma = atan2(sinSigma, cosSigma);
    double const sinAlpha = cosU1 * cosU2 * sin(lambda) / sinSigma;
    cosAlphaSquare = 1 - Pow2(sinAlpha);

    // Cosine of SigmaMid - angular separation between the midpoint of the line and the equator.
    if (fabs(cosAlphaSquare) < DBL_EPSILON)
      cosDoubleSigmaMid = 0;
    else
      cosDoubleSigmaMid = cos(sigma) - 2 * sinU1 * sinU2 / cosAlphaSquare;
    cosDoubleSigmaMidSquare = Pow2(cosDoubleSigmaMid);

    double const C = (kF / 16.0) * cosAlphaSquare * (4.0 + kF * (4.0 - 3.0 * cosAlphaSquare));

    lambdaPrev = lambda;
    lambda = L + (1 - C) * kF * sinAlpha *
                     (sigma +
                      C * sinSigma *
                          (cosDoubleSigmaMid + C * cosSigma * (-1 + 2 * cosDoubleSigmaMidSquare)));
  }

  // Fallback solution.
  if (!AlmostEqualAbs(lambda, lambdaPrev, kEps))
    return DistanceOnEarth(point1, point2);

  double constexpr aSquare = kA * kA;
  double constexpr bSquare = kB * kB;

  double const uSquare = cosAlphaSquare * (aSquare - bSquare) / bSquare;

  double const A = 1.0 + (uSquare / 16384.0) *
                       (4096.0 + uSquare * (-768.0 + uSquare * (320.0 - 175.0 * uSquare)));

  double const B = (uSquare / 1024.0) * (256.0 + uSquare * (-128.0 + uSquare * (74.0 - 47 * uSquare)));

  double const deltaSigma =
      B * sinSigma *
      (cosDoubleSigmaMid + 0.25 * B *
                               (cosSigma * (-1.0 + 2.0 * cosDoubleSigmaMidSquare) -
                                (B / 6.0) * cosDoubleSigmaMid * (-3.0 + 4.0 * Pow2(sinSigma)) *
                                    (-3.0 + 4 * cosDoubleSigmaMidSquare)));

  return kB * A * (sigma - deltaSigma);
}
} // namespace oblate_spheroid
