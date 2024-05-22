#include "generator/srtm_parser.hpp"

#include "routing_common/bicycle_model.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_processor.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"
#include "geometry/point_with_altitude.hpp"

#include "base/checked_cast.hpp"
#include "base/logging.hpp"
#include "base/math.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>

#include <gflags/gflags.h>

DEFINE_string(srtm_dir_path, "", "Path to directory with SRTM files");
DEFINE_string(mwm_file_path, "", "Path to an mwm file.");

namespace feature_segments_checker
{
using namespace feature;

routing::BicycleModel const & GetBicycleModel()
{
  static routing::BicycleModel const instance;
  return instance;
}

int32_t Coord2RoughCoord(double d)
{
  int32_t constexpr kFactor = 100000;
  return static_cast<int32_t>(d * kFactor);
}

struct RoughPoint
{
  explicit RoughPoint(m2::PointD const & point)
    : x(Coord2RoughCoord(point.x)), y(Coord2RoughCoord(point.y))
  {
  }

  int32_t x;
  int32_t y;
};

bool operator< (RoughPoint const & l, RoughPoint const & r)
{
  if (l.x != r.x)
    return l.x < r.x;
  return l.y < r.y;
}

template <typename Cont>
void PrintCont(Cont const & cont, std::string const & title, std::string const & msgText1,
               std::string const & msgText2)
{
  std::cout << std::endl << title << std::endl;
  for (auto const & a : cont)
    std::cout << a.second << msgText1 << a.first << msgText2 << std::endl;
}

template <typename Cont>
void WriteCSV(Cont const & cont, std::string const & fileName)
{
  std::ofstream fout(fileName);
  for (auto const & a : cont)
    fout << a.first << "," << a.second << std::endl;
}

/// \returns y = k * x + b. It's the expected altitude in meters.
double GetY(double k, double b, double x) { return k * x + b; }

/// \brief Calculates factors |k| and |b| of a line using linear least squares method.
/// \returns false in case of error (e.g. if the line is parallel to the vertical axis)
/// and true otherwise.
bool LinearLeastSquaresFactors(std::vector<double> const & xs, std::vector<double> const & ys, double & k,
                               double & b)
{
  double constexpr kEpsilon = 1e-6;
  size_t const n = xs.size();
  double mx = 0, my = 0, mxy = 0, mx2 = 0;
  for (size_t i = 0; i < n; ++i)
  {
    mx += xs[i] / n;
    my += ys[i] / n;
    mxy += xs[i] * ys[i] / n;
    mx2 += xs[i] * xs[i] / n;
  }

  if (base::AlmostEqualAbs(mx * mx, mx2, kEpsilon))
    return false;

  k = (my * mx - mxy) / (mx * mx - mx2);
  b = my - k * mx;
  return true;
}

class Processor
{
public:
  generator::SrtmTileManager & m_srtmManager;
  std::set<RoughPoint> m_uniqueRoadPoints;
  /// Key is an altitude difference for a feature in meters. If a feature goes up the key is greater then 0.
  /// Value is a number of features.
  std::map<geometry::Altitude, uint32_t> m_altitudeDiffs;
  /// Key is a length of a feature in meters. Value is a number of features.
  std::map<uint32_t, uint32_t> m_featureLength;
  /// Key is a length of a feature segment in meters. Value is a segment counter.
  std::map<uint32_t, uint32_t> m_segLength;
  /// Key is difference between two values:
  /// 1. how many meters it's necessary to go up following the feature. If feature wavy
  ///   calculates only raise meters.
  /// 2. the difference in altitude between end and start of features.
  /// Value is a segment counter.
  std::map<int32_t, uint32_t> m_featureWave;
  /// Key is number of meters which is necessary to go up following the feature.
  /// Value is a number of features.
  std::map<int32_t, uint32_t> m_featureUp;
  /// Key is number of meters which is necessary to go down following the feature.
  /// Value is a number of features.
  std::map<int32_t, uint32_t> m_featureDown;
  /// Key is number of meters. It shows altitude deviation of intermediate feature points
  /// from linear model.
  /// Value is a number of features.
  std::map<geometry::Altitude, uint32_t> m_diffFromLinear;
  /// Key is number of meters. It shows altitude deviation of intermediate feature points
  /// from line calculated base on least squares method for all feature points.
  /// Value is a number of features.
  std::map<geometry::Altitude, uint32_t> m_leastSquaresDiff;
  /// Number of features for GetBicycleModel().IsRoad(feature) == true.
  uint32_t m_roadCount;
  /// Number of features for empty features with GetBicycleModel().IsRoad(feature).
  uint32_t m_emptyRoadCount;
  /// Point counter for feature where GetBicycleModel().IsRoad(feature) == true.
  uint32_t m_roadPointCount;
  /// Number of features for GetBicycleModel().IsRoad(feature) != true.
  uint32_t m_notRoadCount;
  geometry::Altitude m_minAltitude = geometry::kInvalidAltitude;
  geometry::Altitude m_maxAltitude = geometry::kInvalidAltitude;

  explicit Processor(generator::SrtmTileManager & manager)
    : m_srtmManager(manager)
    , m_roadCount(0)
    , m_emptyRoadCount(0)
    , m_roadPointCount(0)
    , m_notRoadCount(0)
  {
  }

  void operator()(FeatureType & f, uint32_t const & /* id */)
  {
    f.ParseHeader2();
    if (!GetBicycleModel().IsRoad(feature::TypesHolder(f)))
    {
      ++m_notRoadCount;
      return;
    }

    f.ParseGeometry(FeatureType::BEST_GEOMETRY);
    uint32_t const numPoints = base::asserted_cast<uint32_t>(f.GetPointsCount());
    if (numPoints == 0)
    {
      ++m_emptyRoadCount;
      return;
    }

    ++m_roadCount;

    m_roadPointCount += numPoints;

    for (uint32_t i = 0; i < numPoints; ++i)
      m_uniqueRoadPoints.insert(RoughPoint(f.GetPoint(i)));

    // Preparing feature altitude and length.
    geometry::Altitudes pointAltitudes(numPoints);
    std::vector<double> pointDists(numPoints);
    double distFromStartMeters = 0;
    for (uint32_t i = 0; i < numPoints; ++i)
    {
      // Feature segment altitude.
      geometry::Altitude altitude = m_srtmManager.GetHeight(mercator::ToLatLon(f.GetPoint(i)));
      pointAltitudes[i] = altitude == geometry::kInvalidAltitude ? 0 : altitude;
      if (i == 0)
      {
        pointDists[i] = 0;
        continue;
      }
      // Feature segment length.
      double const segmentLengthMeters =
          mercator::DistanceOnEarth(f.GetPoint(i - 1), f.GetPoint(i));
      distFromStartMeters += segmentLengthMeters;
      pointDists[i] = distFromStartMeters;
    }

    // Min and max altitudes.
    for (auto const a : pointAltitudes)
    {
      if (m_minAltitude == geometry::kInvalidAltitude || a < m_minAltitude)
        m_minAltitude = a;
      if (m_maxAltitude == geometry::kInvalidAltitude || a > m_maxAltitude)
        m_maxAltitude = a;
    }

    // Feature length and feature segment length.
    double realFeatureLengthMeters = 0.0;
    for (uint32_t i = 0; i + 1 < numPoints; ++i)
    {
      // Feature segment length.
      double const realSegmentLengthMeters = pointDists[i + 1] - pointDists[i];
      m_segLength[static_cast<uint32_t>(floor(realSegmentLengthMeters))]++;

      // Feature length.
      realFeatureLengthMeters += realSegmentLengthMeters;
    }
    m_featureLength[static_cast<uint32_t>(realFeatureLengthMeters)]++;

    // Feature altitude difference.
    geometry::Altitude const startAltitude = pointAltitudes[0];
    geometry::Altitude const endAltitude = pointAltitudes[numPoints - 1];
    int16_t const altitudeDiff = endAltitude - startAltitude;
    m_altitudeDiffs[altitudeDiff]++;

    // Wave feature factor. Climb minus altitude difference.
    int32_t climb = 0;
    int32_t up = 0;
    int32_t down = 0;
    for (uint32_t i = 0; i + 1 < numPoints; ++i)
    {
      auto const segAltDiff = pointAltitudes[i + 1] - pointAltitudes[i];
      if (altitudeDiff >= 0)
      {
        // If the feature goes up generaly calculates how many meters it's necessary
        // to go up following the feature.
        if (segAltDiff > 0)
          climb += segAltDiff;
      }
      else
      {
        // If the feature goes down generaly calculates how many meters it's necessary
        // to go down following the feature.
        if (segAltDiff < 0)
          climb += segAltDiff;
      }

      if (segAltDiff >= 0)
        up += segAltDiff;
      else
        down += segAltDiff;
    }
    // Estimates monotony of the feature. If climb == altitudeDiff it's monotonous.
    // If not it's wavy. The more abs(climb - altitudeDiff) the more wavy the feature.
    int32_t const waveFactor = climb - altitudeDiff;
    m_featureWave[waveFactor]++;
    m_featureUp[up]++;
    m_featureDown[down]++;

    // Altitude deviation of internal feature points from linear model.
    if (realFeatureLengthMeters == 0.0)
      return;

    double const k = (endAltitude - startAltitude) / realFeatureLengthMeters;
    for (uint32_t i = 1; i + 1 < numPoints; ++i)
    {
      int32_t const deviation =
          static_cast<geometry::Altitude>(GetY(k, startAltitude, pointDists[i])) -
          pointAltitudes[i];
      m_diffFromLinear[deviation]++;
    }

    // Linear least squares for feature points.
    {
      double k;
      double b;
      std::vector<double> const pointAltitudesMeters(pointAltitudes.begin(), pointAltitudes.end());
      if (!LinearLeastSquaresFactors(pointDists, pointAltitudesMeters, k, b))
        return;

      for (uint32_t i = 0; i < numPoints; ++i)
      {
        geometry::Altitude const deviation =
            static_cast<geometry::Altitude>(GetY(k, b, pointDists[i])) - pointAltitudes[i];
        m_leastSquaresDiff[deviation]++;
      }
    }
  }
};

double CalculateEntropy(std::map<geometry::Altitude, uint32_t> const & diffFromLinear)
{
  uint32_t innerPointCount = 0;
  for (auto const & f : diffFromLinear)
    innerPointCount += f.second;

  if (innerPointCount == 0)
    return 0.0;

  double entropy = 0;
  for (auto const & f : diffFromLinear)
  {
    double const p = static_cast<double>(f.second) / innerPointCount;
    entropy += -p * log2(p);
  }
  return entropy;
}
}  // namespace feature_segments_checker

int main(int argc, char ** argv)
{
  using namespace feature_segments_checker;

  gflags::SetUsageMessage("This tool extracts some staticstics about features and its altitudes.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  LOG(LINFO, ("srtm_dir_path =", FLAGS_srtm_dir_path));
  LOG(LINFO, ("mwm_file_path =", FLAGS_mwm_file_path));

  classificator::Load();
  generator::SrtmTileManager manager(FLAGS_srtm_dir_path);

  Processor processor(manager);
  ForEachFeature(FLAGS_mwm_file_path, processor);

  PrintCont(processor.m_altitudeDiffs, "Altitude difference between start and end of features.",
            " feature(s) with altitude difference ", " meter(s)");
  WriteCSV(processor.m_altitudeDiffs, "altitude_difference.csv");

  PrintCont(processor.m_featureLength, "Feature length.", " feature(s) with length ", " meter(s)");
  WriteCSV(processor.m_featureLength, "feature_length.csv");

  PrintCont(processor.m_segLength, "Feature segment length.", " segment(s) with length ",
            " meter(s)");

  PrintCont(processor.m_featureWave, "Wave factor", " feature(s) with wave factor ", "");
  WriteCSV(processor.m_featureWave, "feature_wave.csv");

  PrintCont(processor.m_featureUp, "Feature go up in meters", " feature(s) go up ", " meter(s)");
  WriteCSV(processor.m_featureUp, "feature_up.csv");

  PrintCont(processor.m_featureDown, "Feature go down in meters", " feature(s) go down ", " meter(s)");
  WriteCSV(processor.m_featureDown, "feature_down.csv");

  PrintCont(processor.m_diffFromLinear, "Altitude deviation of internal feature points from linear model.",
            " internal feature point(s) deviate from linear model with ", " meter(s)");

  PrintCont(processor.m_leastSquaresDiff,
            "Altitude deviation of feature points from least squares line.",
            " internal feature point(s) deviate from linear model with ", " meter(s)");

  using std::cout, std::endl;
  cout << endl << FLAGS_mwm_file_path << endl;
  cout << "Road feature count = " << processor.m_roadCount << endl;
  cout << "Empty road feature count = " << processor.m_emptyRoadCount << endl;
  cout << "Unique road points count = " << processor.m_uniqueRoadPoints.size() << endl;
  cout << "All road point count = " << processor.m_roadPointCount << endl;
  cout << "Not road feature count = " << processor.m_notRoadCount << endl;
  cout << "Entropy for altitude deviation of internal feature points from linear model = "
       << CalculateEntropy(processor.m_diffFromLinear) << endl;
  cout << "Entropy for altitude deviation of feature points from least squares line = "
       << CalculateEntropy(processor.m_leastSquaresDiff) << endl;
  cout << "Min altitude of the mwm = " << processor.m_minAltitude << endl;
  cout << "Max altitude of the mwm = " << processor.m_maxAltitude << endl;
  return 0;
}
