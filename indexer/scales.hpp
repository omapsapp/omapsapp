#pragma once

#include "geometry/rect2d.hpp"
#include "geometry/point2d.hpp"


namespace scales
{
  constexpr int UPPER_STYLE_SCALE = 19;

  /// Upper scale for data generation and indexer buckets.
  constexpr int GetUpperScale() { return 17; }
  /// Upper scale according to drawing rules.
  constexpr int GetUpperStyleScale() { return UPPER_STYLE_SCALE; }
  /// Upper scales for World visible styles and indexer buckets.
  constexpr int GetUpperWorldScale() { return 9; }
  /// Upper scale level for countries.
  constexpr int GetUpperCountryScale() { return GetUpperWorldScale() + 1; }
  /// Upper scale for user comfort view (e.g. location zoom).
  constexpr int GetUpperComfortScale() { return UPPER_STYLE_SCALE - 2; }
  /// Default navigation mode scale.
  constexpr int GetNavigationScale() { return UPPER_STYLE_SCALE - 3; }
  /// Default pedestrian navigation mode scale.
  constexpr int GetPedestrianNavigationScale() { return UPPER_STYLE_SCALE - 2; }
  /// Default navigation 3d mode scale.
  constexpr int GetNavigation3dScale() { return UPPER_STYLE_SCALE - 2; }
  /// Default pedestrian navigation 3d mode scale.
  constexpr int GetPedestrianNavigation3dScale() { return UPPER_STYLE_SCALE - 2; }
  /// Default scale in adding-new-place mode.
  constexpr int GetAddNewPlaceScale() { return 18; }
  /// Lower scale when standalone point housenumbers (of building-address type) become visible.
  constexpr int GetPointHousenumbersScale() { return 18; }

  int GetMinAllowableIn3dScale();

  double GetScaleLevelD(double ratio);
  double GetScaleLevelD(m2::RectD const & r);
  int GetScaleLevel(double ratio);
  int GetScaleLevel(m2::RectD const & r);

  double GetEpsilonForLevel(int level);
  double GetEpsilonForSimplify(int level);
  double GetEpsilonForHousenumbers(int level);
  bool IsGoodForLevel(int level, m2::RectD const & r);

  using Points = std::vector<m2::PointD>;
  bool IsGoodOutlineForLevel(int level, Points const & poly);
}
