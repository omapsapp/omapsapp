#include "indexer/scales.hpp"

#include "indexer/feature_algo.hpp"
#include "geometry/mercator.hpp"
#include "base/math.hpp"

#include <algorithm>

namespace scales
{
  static const int INITIAL_LEVEL = 1;

  int GetMinAllowableIn3dScale()
  {
    return std::min(16, std::min(GetNavigation3dScale(), GetPedestrianNavigation3dScale()));
  }

  double GetScaleLevelD(double ratio)
  {
    double const level = std::min(static_cast<double>(GetUpperScale()), std::log2(ratio) + INITIAL_LEVEL);
    return level < 0.0 ? 0.0 : level;
  }

  double GetScaleLevelD(m2::RectD const & r)
  {
    // TODO: fix scale factors for mercator projection
    double const dx = mercator::Bounds::kRangeX / r.SizeX();
    double const dy = mercator::Bounds::kRangeY / r.SizeY();

    // get the average ratio
    return GetScaleLevelD((dx + dy) / 2.0);
  }

  int GetScaleLevel(double ratio)
  {
    return math::SignedRound(GetScaleLevelD(ratio));
  }

  int GetScaleLevel(m2::RectD const & r)
  {
    return math::SignedRound(GetScaleLevelD(r));
  }

  namespace
  {
    double GetEpsilonImpl(long level, double pixelTolerance)
    {
      return mercator::Bounds::kRangeX * pixelTolerance / double(256L << level);
    }
  }

  double GetEpsilonForLevel(int level)
  {
    return GetEpsilonImpl(level, 7);
  }

  double GetEpsilonForSimplify(int level)
  {
    // Keep better geometries on highest zoom to allow scaling them deeper.
    // Effectively it leads to x26 precision difference from geom scale 2 to 3.
    // Keep crude geometries for all other zooms,
    // x4 precision difference from geom scale 0 to 1 and 1 to 2.
    if (level == GetUpperScale())
      return GetEpsilonImpl(level, 0.4);
    else
      return GetEpsilonImpl(level, 1.3);
  }

  double GetEpsilonForHousenumbers(int level)
  {
    // Leads to housenumbers either not fitting into buildings on zl16.
    ASSERT_LESS(level, GetPointHousenumbersScale(), ());
    return GetEpsilonImpl(level, 30);
  }

  bool IsGoodForLevel(int level, m2::RectD const & r)
  {
    ASSERT(level >= 0 && level <= GetUpperScale(), (level));
    // assume that feature is always visible in upper scale
    return (level == GetUpperScale() || std::max(r.SizeX(), r.SizeY()) > GetEpsilonForLevel(level));
  }

  bool IsGoodOutlineForLevel(int level, Points const & poly)
  {
    // Areas and closed lines have the same first and last points.
    // Hence the minimum number of outline points is 4.
    if (poly.size() < 4)
      return false;

    m2::RectD r;
    feature::CalcRect(poly, r);

    return IsGoodForLevel(level, r);
  }
} // namespace scales
