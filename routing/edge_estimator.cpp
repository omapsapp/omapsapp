#include "routing/edge_estimator.hpp"

#include "routing/geometry.hpp"
#include "routing/latlon_with_altitude.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/traffic_stash.hpp"

#include "traffic/speed_groups.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/point_with_altitude.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <unordered_map>

namespace routing
{
using namespace std;
using namespace traffic;
using measurement_utils::KmphToMps;

namespace
{
geometry::Altitude constexpr kMountainSicknessAltitudeM = 2500;

double TimeBetweenSec(ms::LatLon const & from, ms::LatLon const & to, double speedMpS)
{
  ASSERT_GREATER(speedMpS, 0.0, ("from:", from, "to:", to));

  double const distanceM = ms::DistanceOnEarth(from, to);
  return distanceM / speedMpS;
}

double CalcTrafficFactor(SpeedGroup speedGroup)
{
  if (speedGroup == SpeedGroup::TempBlock)
  {
    // impossible driving factor
    return 1.0E4;
  }

  double const percentage = 0.01 * kSpeedGroupThresholdPercentage[static_cast<size_t>(speedGroup)];
  ASSERT_GREATER(percentage, 0.0, (speedGroup));
  return 1.0 / percentage;
}

double GetSpeedMpS(EdgeEstimator::Purpose purpose, Segment const & segment, RoadGeometry const & road)
{
  SpeedKMpH const & speed = road.GetSpeed(segment.IsForward());
  double const speedMpS = KmphToMps(purpose == EdgeEstimator::Purpose::Weight ? speed.m_weight : speed.m_eta);
  ASSERT_GREATER(speedMpS, 0.0, (segment));
  return speedMpS;
}

bool IsTransit(std::optional<HighwayType> type)
{
  return type && (type == HighwayType::RouteFerry || type == HighwayType::RouteShuttleTrain);
}

template <class CalcSpeed>
double CalcClimbSegment(EdgeEstimator::Purpose purpose, Segment const & segment,
                        RoadGeometry const & road, CalcSpeed && calcSpeed)
{
  double const distance = road.GetDistance(segment.GetSegmentIdx());
  double speedMpS = GetSpeedMpS(purpose, segment, road);

  static double constexpr kSmallDistanceM = 1;   // we have altitude threshold is 0.5m
  if (distance > kSmallDistanceM && !IsTransit(road.GetHighwayType()))
  {
    LatLonWithAltitude const & from = road.GetJunction(segment.GetPointId(false /* front */));
    LatLonWithAltitude const & to = road.GetJunction(segment.GetPointId(true /* front */));

    ASSERT(to.GetAltitude() != geometry::kInvalidAltitude && from.GetAltitude() != geometry::kInvalidAltitude, ());
    auto const altitudeDiff = to.GetAltitude() - from.GetAltitude();

    if (altitudeDiff != 0)
    {
      speedMpS = calcSpeed(speedMpS, altitudeDiff / distance, to.GetAltitude());
      ASSERT_GREATER(speedMpS, 0.0, (segment));
    }
  }

  return distance / speedMpS;
}
}  // namespace

double GetPedestrianClimbPenalty(EdgeEstimator::Purpose purpose, double tangent, geometry::Altitude altitudeM)
{
  double constexpr kMinPenalty = 1.0;
  // Descent penalty is less then the ascent penalty.
  double const impact = tangent >= 0.0 ? 1.0 : 0.35;

  if (altitudeM >= kMountainSicknessAltitudeM)
    return kMinPenalty + (10.0 + (altitudeM - kMountainSicknessAltitudeM) * 10.0 / 1500.0) * fabs(tangent) * impact;

  // Use magic constant from this table: https://en.wikipedia.org/wiki/Tobler's_hiking_function#Sample_values
  // Tobler's returns unusually big values for bigger tangent.
  // See Australia_Mountains_Downlhill test.
  if (purpose == EdgeEstimator::Purpose::Weight || fabs(tangent) > 1.19)
  {
    tangent = fabs(tangent);
    // Some thoughts about gradient and foot walking: https://gre-kow.livejournal.com/26916.html
    // 3cm diff with avg foot length 60cm is imperceptible (see Hungary_UseFootways).
    double constexpr kTangentThreshold = 3.0/60.0;
    if (tangent < kTangentThreshold)
      return kMinPenalty;

    // ETA coefficients are calculated in https://github.com/mapsme/omim-scripts/pull/21
    auto const penalty = purpose == EdgeEstimator::Purpose::Weight
                             ? 5.0 * tangent + 7.0 * tangent * tangent
                             : 3.01 * tangent + 3.54 * tangent * tangent;

    return kMinPenalty + penalty * impact;
  }
  else
  {
    // Use Tobler’s Hiking Function for ETA like more comprehensive. See France_Uphill_Downlhill test.
    // Why not in Weight? See Crimea_Altitude_Mountains test.
    // https://mtntactical.com/research/yet-calculating-movement-uneven-terrain/
    // Returns factor: W(0) / W(tangent).
    return exp(-3.5 * (0.05 - fabs(tangent + 0.05)));
  }
}

double GetBicycleClimbPenalty(EdgeEstimator::Purpose, double tangent, geometry::Altitude altitudeM)
{
  double constexpr kMinPenalty = 1.0;
  double const impact = tangent >= 0.0 ? 1.0 : 0.35;

  if (altitudeM >= kMountainSicknessAltitudeM)
    return kMinPenalty + 50.0 * fabs(tangent) * impact;

  // By VNG: This approach is strange at least because it always returns penalty > 1 (even for downhill)
  /*
  tangent = fabs(tangent);
  // ETA coefficients are calculated in https://github.com/mapsme/omim-scripts/pull/22
  auto const penalty = purpose == EdgeEstimator::Purpose::Weight
                           ? 10.0 * tangent + 26.0 * tangent * tangent
                           : 8.8 * tangent + 6.51 * tangent * tangent;

  return kMinPenalty + penalty * impact;
  */

  // https://web.tecnico.ulisboa.pt/~rosamfelix/gis/declives/SpeedSlopeFactor.html
  double const slope = tangent * 100;

  double factor;
  if (slope < -30)
    factor = 1.5;
  else if (slope < 0)
  {
    // Min factor (max speed) will be at slope = -13.
    factor = 1 + 2 * 0.7 / 13.0 * slope + 0.7 / 169 * slope * slope;
  }
  else if (slope <= 20)
    factor = 1 + slope * slope / 49;
  else
    factor = 10.0;
  return factor;
}

double GetCarClimbPenalty(EdgeEstimator::Purpose, double, geometry::Altitude)
{
  return 1.0;
}

// EdgeEstimator -----------------------------------------------------------------------------------
EdgeEstimator::EdgeEstimator(double maxWeightSpeedKMpH, SpeedKMpH const & offroadSpeedKMpH,
                             DataSource * /*dataSourcePtr*/, std::shared_ptr<NumMwmIds> /*numMwmIds*/)
  : m_maxWeightSpeedMpS(KmphToMps(maxWeightSpeedKMpH))
  , m_offroadSpeedKMpH(offroadSpeedKMpH)
  //, m_dataSourcePtr(dataSourcePtr)
  //, m_numMwmIds(numMwmIds)
{
  CHECK_GREATER(m_offroadSpeedKMpH.m_weight, 0.0, ());
  CHECK_GREATER(m_offroadSpeedKMpH.m_eta, 0.0, ());
  CHECK_GREATER_OR_EQUAL(m_maxWeightSpeedMpS, KmphToMps(m_offroadSpeedKMpH.m_weight), ());

  if (m_offroadSpeedKMpH.m_eta != kNotUsed)
    CHECK_GREATER_OR_EQUAL(m_maxWeightSpeedMpS, KmphToMps(m_offroadSpeedKMpH.m_eta), ());
}

double EdgeEstimator::CalcHeuristic(ms::LatLon const & from, ms::LatLon const & to) const
{
  // For the correct A*, we should use maximum _possible_ speed here, including:
  // default model, feature stored, unlimited autobahn, ferry or rail transit.
  return TimeBetweenSec(from, to, m_maxWeightSpeedMpS);
}

double EdgeEstimator::ComputeDefaultLeapWeightSpeed() const
{
  // 1.76 factor was computed as an average ratio of escape/enter speed to max MWM speed across all MWMs.
  //return m_maxWeightSpeedMpS / 1.76;

  // By VNG: Current m_maxWeightSpeedMpS is > 120 km/h, so estimating speed was > 60km/h
  // for start/end fake edges by straight line! I strongly believe that this is very! optimistic.
  // Set speed to 57.5km/h (16m/s):
  // - lower bound Russia_MoscowDesnogorsk (https://github.com/organicmaps/organicmaps/issues/1071)
  // - upper bound RussiaSmolenskRussiaMoscowTimeTest
  return 16.0;
}

/*
double EdgeEstimator::LoadLeapWeightSpeed(NumMwmId mwmId)
{
  double leapWeightSpeed = ComputeDefaultLeapWeightSpeed();

  if (m_dataSourcePtr)
  {
    MwmSet::MwmHandle handle =
        m_dataSourcePtr->GetMwmHandleByCountryFile(m_numMwmIds->GetFile(mwmId));
    if (!handle.IsAlive())
      MYTHROW(RoutingException, ("Mwm", m_numMwmIds->GetFile(mwmId), "cannot be loaded."));

    if (handle.GetInfo())
      leapWeightSpeed = handle.GetInfo()->GetRegionData().GetLeapWeightSpeed(leapWeightSpeed);
  }

  if (leapWeightSpeed > m_maxWeightSpeedMpS)
    leapWeightSpeed = m_maxWeightSpeedMpS;

  return leapWeightSpeed;
}
*/

double EdgeEstimator::GetLeapWeightSpeed(NumMwmId /*mwmId*/)
{
  double defaultSpeed = ComputeDefaultLeapWeightSpeed();

  /// @todo By VNG: We don't have LEAP_SPEEDS_FILE to assign RegionData::SetLeapWeightSpeed
  /// unique for each MWM, so this is useless now. And what about possible races here?
//  if (mwmId != kFakeNumMwmId)
//  {
//    auto [speedIt, inserted] = m_leapWeightSpeedMpS.emplace(mwmId, defaultSpeed);
//    if (inserted)
//      speedIt->second = LoadLeapWeightSpeed(mwmId);
//    return speedIt->second;
//  }

  return defaultSpeed;
}

double EdgeEstimator::CalcLeapWeight(ms::LatLon const & from, ms::LatLon const & to, NumMwmId mwmId)
{
  return TimeBetweenSec(from, to, GetLeapWeightSpeed(mwmId));
}

double EdgeEstimator::GetMaxWeightSpeedMpS() const { return m_maxWeightSpeedMpS; }

double EdgeEstimator::CalcOffroad(ms::LatLon const & from, ms::LatLon const & to,
                                  Purpose purpose) const
{
  auto const offroadSpeedKMpH =
      purpose == Purpose::Weight ? m_offroadSpeedKMpH.m_weight : m_offroadSpeedKMpH.m_eta;
  if (offroadSpeedKMpH == kNotUsed)
    return 0.0;

  return TimeBetweenSec(from, to, KmphToMps(offroadSpeedKMpH));
}

// PedestrianEstimator -----------------------------------------------------------------------------
class PedestrianEstimator final : public EdgeEstimator
{
public:
  PedestrianEstimator(double maxWeightSpeedKMpH, SpeedKMpH const & offroadSpeedKMpH)
    : EdgeEstimator(maxWeightSpeedKMpH, offroadSpeedKMpH)
  {
  }

  // EdgeEstimator overrides:
  double GetUTurnPenalty(Purpose /* purpose */) const override { return 0.0 /* seconds */; }
  double GetFerryLandingPenalty(Purpose purpose) const override
  {
    switch (purpose)
    {
    case Purpose::Weight: return 10 * 60;   // seconds
    case Purpose::ETA: return 8 * 60;       // seconds
    }
    UNREACHABLE();
  }

  double CalcSegmentWeight(Segment const & segment, RoadGeometry const & road, Purpose purpose) const override
  {
    return CalcClimbSegment(purpose, segment, road,
        [purpose](double speedMpS, double tangent, geometry::Altitude altitude)
        {
          return speedMpS / GetPedestrianClimbPenalty(purpose, tangent, altitude);
        });
  }
};

// BicycleEstimator --------------------------------------------------------------------------------
class BicycleEstimator final : public EdgeEstimator
{
public:
  BicycleEstimator(double maxWeightSpeedKMpH, SpeedKMpH const & offroadSpeedKMpH)
    : EdgeEstimator(maxWeightSpeedKMpH, offroadSpeedKMpH)
  {
  }

  // EdgeEstimator overrides:
  double GetUTurnPenalty(Purpose /* purpose */) const override { return 20.0 /* seconds */; }
  double GetFerryLandingPenalty(Purpose purpose) const override
  {
    switch (purpose)
    {
    case Purpose::Weight: return 10 * 60;   // seconds
    case Purpose::ETA: return 8 * 60;       // seconds
    }
    UNREACHABLE();
  }

  double CalcSegmentWeight(Segment const & segment, RoadGeometry const & road, Purpose purpose) const override
  {
    return CalcClimbSegment(purpose, segment, road,
        [purpose, this](double speedMpS, double tangent, geometry::Altitude altitude)
        {
          auto const factor = GetBicycleClimbPenalty(purpose, tangent, altitude);
          ASSERT_GREATER(factor, 0.0, ());

          /// @todo Take out "bad" bicycle road (path, track, footway, ...) check into BicycleModel?
          static double constexpr badBicycleRoadSpeed = KmphToMps(9);
          if (speedMpS <= badBicycleRoadSpeed)
          {
            if (factor > 1)
              speedMpS /= factor;
          }
          else
          {
            if (factor > 1)
            {
              // Calculate uphill speed according to the average bicycle speed, because "good-roads" like
              // residential, secondary, cycleway are "equal-low-speed" uphill and road type doesn't matter.
              static double constexpr avgBicycleSpeed = KmphToMps(20);
              double const upperBound = avgBicycleSpeed / factor;
              if (speedMpS > upperBound)
              {
                // Add small weight to distinguish roads by class (10 is a max factor value).
                speedMpS = upperBound + (purpose == Purpose::Weight ? speedMpS / (10 * avgBicycleSpeed) : 0);
              }
            }
            else
              speedMpS /= factor;
          }

          return std::min(speedMpS, GetMaxWeightSpeedMpS());
        });
  }
};

// CarEstimator ------------------------------------------------------------------------------------
class CarEstimator final : public EdgeEstimator
{
public:
  CarEstimator(DataSource * dataSourcePtr, std::shared_ptr<NumMwmIds> numMwmIds,
               shared_ptr<TrafficStash> trafficStash, double maxWeightSpeedKMpH,
               SpeedKMpH const & offroadSpeedKMpH)
    : EdgeEstimator(maxWeightSpeedKMpH, offroadSpeedKMpH, dataSourcePtr, numMwmIds)
    , m_trafficStash(std::move(trafficStash))
  {
  }

  // EdgeEstimator overrides:
  double CalcSegmentWeight(Segment const & segment, RoadGeometry const & road, Purpose purpose) const override;
  double GetUTurnPenalty(Purpose /* purpose */) const override
  {
    // Adds 2 minutes penalty for U-turn. The value is quite arbitrary
    // and needs to be properly selected after a number of real-world
    // experiments.
    return 2 * 60;  // seconds
  }

  double GetFerryLandingPenalty(Purpose purpose) const override
  {
    switch (purpose)
    {
    case Purpose::Weight: return 20 * 60;   // seconds
    case Purpose::ETA: return 20 * 60;      // seconds
    }
    UNREACHABLE();
  }

private:
  shared_ptr<TrafficStash> m_trafficStash;
};

double CarEstimator::CalcSegmentWeight(Segment const & segment, RoadGeometry const & road, Purpose purpose) const
{
  double result = road.GetDistance(segment.GetSegmentIdx()) / GetSpeedMpS(purpose, segment, road);

  if (m_trafficStash)
  {
    SpeedGroup const speedGroup = m_trafficStash->GetSpeedGroup(segment);
    ASSERT_LESS(speedGroup, SpeedGroup::Count, ());
    double const trafficFactor = CalcTrafficFactor(speedGroup);
    result *= trafficFactor;
    if (speedGroup != SpeedGroup::Unknown && speedGroup != SpeedGroup::G5)
    {
      // Current time estimation are too optimistic.
      // Need more accurate tuning: traffic lights, traffic jams, road models and so on.
      // Add some penalty to make estimation more realistic.
      /// @todo Make accurate tuning, remove penalty.
      result *= 1.8;
    }
  }

  return result;
}

// EdgeEstimator -----------------------------------------------------------------------------------
// static
shared_ptr<EdgeEstimator> EdgeEstimator::Create(VehicleType vehicleType, double maxWeighSpeedKMpH,
                                                SpeedKMpH const & offroadSpeedKMpH,
                                                shared_ptr<TrafficStash> trafficStash,
                                                DataSource * dataSourcePtr,
                                                std::shared_ptr<NumMwmIds> numMwmIds)
{
  switch (vehicleType)
  {
  case VehicleType::Pedestrian:
  case VehicleType::Transit:
    return make_shared<PedestrianEstimator>(maxWeighSpeedKMpH, offroadSpeedKMpH);
  case VehicleType::Bicycle:
    return make_shared<BicycleEstimator>(maxWeighSpeedKMpH, offroadSpeedKMpH);
  case VehicleType::Car:
    return make_shared<CarEstimator>(dataSourcePtr, numMwmIds, trafficStash, maxWeighSpeedKMpH,
                                     offroadSpeedKMpH);
  case VehicleType::Count:
    CHECK(false, ("Can't create EdgeEstimator for", vehicleType));
    return nullptr;
  }
  UNREACHABLE();
}

// static
shared_ptr<EdgeEstimator> EdgeEstimator::Create(VehicleType vehicleType,
                                                VehicleModelInterface const & vehicleModel,
                                                shared_ptr<TrafficStash> trafficStash,
                                                DataSource * dataSourcePtr,
                                                std::shared_ptr<NumMwmIds> numMwmIds)
{
  return Create(vehicleType, vehicleModel.GetMaxWeightSpeed(), vehicleModel.GetOffroadSpeed(),
                trafficStash, dataSourcePtr, numMwmIds);
}
}  // namespace routing
