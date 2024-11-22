#include "generator/mini_roundabout_transformer.hpp"

#include "indexer/classificator.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace generator
{
namespace
{
double constexpr kDefaultRadiusMeters = 5.0;

/// \brief Moves |it| on the |road| range one step from current |it| element.
bool MoveIterAwayFromRoundabout(feature::FeatureBuilder::PointSeq::iterator & it,
                                feature::FeatureBuilder::PointSeq const & road)
{
  bool middlePoint = false;
  if (it == road.begin())
  {
    ++it;
  }
  else if (it + 1 == road.end())
  {
    --it;
  }
  else
  {
    // Mini-roundabout is on the middle of the road so we need to insert 2 points to the roundabout
    // circle and create additional surrogate road.
    --it;
    middlePoint = true;
  }
  return middlePoint;
}

feature::FeatureBuilder::PointSeq::iterator GetIterOnRoad(m2::PointD const & point,
                                                          feature::FeatureBuilder::PointSeq & road)
{
  return std::find_if(road.begin(), road.end(), [&point](m2::PointD const & pointOnRoad)
  {
    return AlmostEqualAbs(pointOnRoad, point, kMwmPointAccuracy);
  });
}
} // namespace

MiniRoundaboutTransformer::MiniRoundaboutTransformer(std::vector<MiniRoundaboutInfo> const & data,
                                                     feature::AffiliationInterface const & affiliation)
  : MiniRoundaboutTransformer(data, affiliation, kDefaultRadiusMeters)
{
}

MiniRoundaboutTransformer::MiniRoundaboutTransformer(std::vector<MiniRoundaboutInfo> const & data,
                                                     feature::AffiliationInterface const & affiliation,
                                                     double radiusMeters)
  : m_roundabouts(data)
  , m_radiusMercator(mercator::MetersToMercator(radiusMeters))
  , m_affiliation(&affiliation)
{
}

/// \brief Sets |roadType| with one of |foundTypes| if it is more important.
/// (E.g. if roundabout connects motorway and tertiary ways, roundabout Feature will have motorway type).
bool UpdateRoadType(FeatureParams::Types const & foundTypes, uint32_t & roadType)
{
  // Highways are sorted from the most to least important.
  auto const & cl = classif();
  static std::array<uint32_t, 16> const kHighwayTypes = {
      cl.GetTypeByPath({"highway", "motorway"}),
      cl.GetTypeByPath({"highway", "motorway_link"}),
      cl.GetTypeByPath({"highway", "trunk"}),
      cl.GetTypeByPath({"highway", "trunk_link"}),
      cl.GetTypeByPath({"highway", "primary"}),
      cl.GetTypeByPath({"highway", "primary_link"}),
      cl.GetTypeByPath({"highway", "secondary"}),
      cl.GetTypeByPath({"highway", "secondary_link"}),
      cl.GetTypeByPath({"highway", "tertiary"}),
      cl.GetTypeByPath({"highway", "tertiary_link"}),
      cl.GetTypeByPath({"highway", "unclassified"}),
      cl.GetTypeByPath({"highway", "road"}),
      cl.GetTypeByPath({"highway", "residential"}),
      cl.GetTypeByPath({"highway", "living_street"}),
      cl.GetTypeByPath({"highway", "service"}),
      cl.GetTypeByPath({"highway", "track"}),
  };

  for (uint32_t t : foundTypes)
  {
    ftype::TruncValue(t, 2);
    auto const it = std::find(kHighwayTypes.begin(), kHighwayTypes.end(), t);
    if (it == kHighwayTypes.end())
      continue;

    auto const itPrev = std::find(kHighwayTypes.begin(), kHighwayTypes.end(), roadType);
    if (itPrev == kHighwayTypes.end() || itPrev > it)
      roadType = *it;

    return true;
  }

  return false;
}

feature::FeatureBuilder CreateFb(std::vector<m2::PointD> && way, uint64_t osmID)
{
  feature::FeatureBuilder fb;
  fb.SetOsmId(base::MakeOsmWay(osmID));
  fb.SetLinear();
  fb.AssignPoints(std::move(way));
  return fb;
}

feature::FeatureBuilder MiniRoundaboutTransformer::CreateRoundaboutFb(PointsT && way, uint32_t roadType)
{
  if (m_leftHandTraffic)
    std::reverse(way.begin(), way.end());

  way.push_back(way[0]);

  feature::FeatureBuilder fb = CreateFb(std::move(way), m_newWayId--);

  static uint32_t const roundaboutType = classif().GetTypeByPath({"junction", "roundabout"});
  fb.AddType(roundaboutType);
  static uint32_t const defaultRoadType = classif().GetTypeByPath({"highway", "tertiary"});
  fb.AddType(roadType == 0 ? defaultRoadType : roadType);
  static uint32_t const onewayType = classif().GetTypeByPath({"hwtag", "oneway"});
  fb.AddType(onewayType);

  return fb;
}

MiniRoundaboutTransformer::PointsT MiniRoundaboutTransformer::CreateSurrogateRoad(
    RoundaboutUnit const & roundaboutOnRoad, PointsT & roundaboutCircle,
    PointsT & road, PointsT::iterator & itPointUpd) const
{
  PointsT surrogateRoad(itPointUpd, road.end());
  auto itPointOnSurrogateRoad = surrogateRoad.begin();
  auto itPointSurrogateUpd = itPointOnSurrogateRoad;
  ++itPointOnSurrogateRoad;

  m2::PointD const nextPointOnSurrogateRoad = GetPointAtDistFromTarget(
      *itPointOnSurrogateRoad /* source */, roundaboutOnRoad.m_location /* target */,
      m_radiusMercator /* dist */);

  if (AlmostEqualAbs(nextPointOnSurrogateRoad, *itPointOnSurrogateRoad, kMwmPointAccuracy))
    return {};

  AddPointToCircle(roundaboutCircle, nextPointOnSurrogateRoad);
  *itPointSurrogateUpd = nextPointOnSurrogateRoad;
  road.erase(itPointUpd + 1, road.end());

  return surrogateRoad;
}

bool MiniRoundaboutTransformer::AddRoundaboutToRoad(RoundaboutUnit const & roundaboutOnRoad,
                                                    PointsT & roundaboutCircle, PointsT & road,
                                                    std::vector<feature::FeatureBuilder> & newRoads) const
{
  auto const roundaboutCenter = roundaboutOnRoad.m_location;
  auto itPointUpd = GetIterOnRoad(roundaboutCenter, road);

  CHECK(itPointUpd != road.end(), ());

  auto itPointNearRoundabout = itPointUpd;
  bool const isMiddlePoint = MoveIterAwayFromRoundabout(itPointNearRoundabout, road);
  m2::PointD const nextPointOnRoad =
      GetPointAtDistFromTarget(*itPointNearRoundabout /* source */, roundaboutCenter /* target */,
                               m_radiusMercator /* dist */);

  if (isMiddlePoint && !AlmostEqualAbs(nextPointOnRoad, *itPointNearRoundabout, kMwmPointAccuracy))
  {
    auto surrogateRoad = CreateSurrogateRoad(roundaboutOnRoad, roundaboutCircle, road, itPointUpd);
    if (surrogateRoad.size() < 2)
      return false;
    auto fbSurrogateRoad = CreateFb(std::move(surrogateRoad), roundaboutOnRoad.m_roadId);
    for (auto const & t : roundaboutOnRoad.m_roadTypes)
      fbSurrogateRoad.AddType(t);

    newRoads.push_back(std::move(fbSurrogateRoad));
  }

  AddPointToCircle(roundaboutCircle, nextPointOnRoad);
  *itPointUpd = nextPointOnRoad;
  return true;
}

void MiniRoundaboutTransformer::AddRoad(feature::FeatureBuilder && road)
{
  auto const id = road.GetMostGenericOsmId();
  CHECK(m_roads.emplace(id, std::move(road)).second, ());
}

void MiniRoundaboutTransformer::ProcessRoundabouts(std::function<void (feature::FeatureBuilder const &)> const & fn)
{
  // Some mini-roundabouts are mapped in the middle of the road. These roads should be split
  // in two parts on the opposite sides of the roundabout. New roads are saved in |fbsRoads|.
  std::vector<feature::FeatureBuilder> fbsRoads;
  size_t constexpr kReserveSize = 4*1024;
  fbsRoads.reserve(kReserveSize);

  size_t transformed = 0;
  for (auto const & rb : m_roundabouts)
  {
    bool allRoadsInOneMwm = true;

    // First of all collect Ways-Features and make sure that they are _transformable_. Do nothing otherwise.
    std::vector<std::pair<feature::FeatureBuilder *, uint64_t>> features;
    for (uint64_t const wayId : rb.m_ways)
    {
      base::GeoObjectId const geoWayId = base::MakeOsmWay(wayId);
      auto const it = m_roads.find(geoWayId);
      if (it == m_roads.end())
        continue;

      feature::FeatureBuilder & fb = it->second;

      // Transform only mini_roundabouts on roads contained in single mwm.
      // They will overlap and break a picture, otherwise.
      if (m_affiliation->GetAffiliations(fb).size() != 1)
      {
        LOG(LWARNING, ("Roundabout's connected way in many MWMs", geoWayId, rb.m_id));
        allRoadsInOneMwm = false;
        break;
      }
      else
        features.emplace_back(&fb, wayId);
    }
    if (!allRoadsInOneMwm)
      continue;

    m2::PointD const center = mercator::FromLatLon(rb.m_coord);
    PointsT circlePlain = PointToPolygon(center, m_radiusMercator);
    uint32_t roadType = 0;
    bool foundRoad = false;

    for (auto [feature, wayId] : features)
    {
      base::GeoObjectId const geoWayId = base::MakeOsmWay(wayId);
      auto road = feature->GetOuterGeometry();

      if (GetIterOnRoad(center, road) == road.end())
      {
        feature = nullptr;
        for (auto & rd : fbsRoads)
        {
          if (rd.GetMostGenericOsmId() == geoWayId)
          {
            road = rd.GetOuterGeometry();
            if (GetIterOnRoad(center, road) != road.end())
            {
              feature = &rd;
              break;
            }
          }
        }

        if (feature == nullptr)
        {
          LOG(LERROR, ("Road not found for mini_roundabout", rb.m_coord));
          continue;
        }
      }

      // Since we obtain |feature| pointer on element from |fbsRoads| above, we can't allow reallocation of this vector.
      // If this CHECK will fire someday, just increase kReserveSize constant.
      CHECK_LESS(fbsRoads.size(), kReserveSize, ());
      if (!AddRoundaboutToRoad({wayId, center, feature->GetTypes()}, circlePlain, road, fbsRoads))
        continue;

      feature->AssignPoints(std::move(road));
      if (!UpdateRoadType(feature->GetTypes(), roadType))
        LOG(LERROR, ("Unrecognized roundabout way type for", geoWayId));

      foundRoad = true;
    }

    if (!foundRoad)
      continue;

    fn(CreateRoundaboutFb(std::move(circlePlain), roadType));
    ++transformed;
  }

  LOG(LINFO, ("Transformed", transformed, "mini_roundabouts. Added", fbsRoads.size(), "surrogate roads."));

  for (auto const & fb : fbsRoads)
    fn(fb);
  for (auto const & fb : m_roads)
    fn(fb.second);
}

double DistanceOnPlain(m2::PointD const & a, m2::PointD const & b) { return a.Length(b); }

m2::PointD GetPointAtDistFromTarget(m2::PointD const & source, m2::PointD const & target,
                                    double dist)
{
  double const len = DistanceOnPlain(source, target);
  if (len < dist)
    return source;

  double const k = (len - dist) / dist;
  return (source + target * k) / (1.0 + k);
}

std::vector<m2::PointD> PointToPolygon(m2::PointD const & center, double radiusMercator,
                                       size_t verticesCount, double initAngleDeg)
{
  CHECK_GREATER(verticesCount, 2, ());
  CHECK_GREATER(radiusMercator, 0.0, ());

  std::vector<m2::PointD> vertices;
  vertices.reserve(verticesCount);

  double const kAngularPitch = 2 * math::pi / static_cast<double>(verticesCount);
  double angle = math::DegToRad(initAngleDeg);

  for (size_t i = 0; i < verticesCount; ++i)
  {
    vertices.emplace_back(center.x + radiusMercator * cos(angle),
                          center.y + radiusMercator * sin(angle));
    angle += kAngularPitch;
  }

  return vertices;
}

void AddPointToCircle(std::vector<m2::PointD> & circle, m2::PointD const & point)
{
  size_t iDist1 = 0;
  size_t iDist2 = 0;
  double dist1 = std::numeric_limits<double>::max();
  double dist2 = std::numeric_limits<double>::max();

  for (size_t i = 0; i < circle.size(); ++i)
  {
    double const dist = DistanceOnPlain(circle[i], point);
    if (dist < dist1)
    {
      dist2 = dist1;
      iDist2 = iDist1;

      dist1 = dist;
      iDist1 = i;
    }
    else if (dist < dist2)
    {
      dist2 = dist;
      iDist2 = i;
    }
  }

  if (iDist1 > iDist2)
    std::swap(iDist1, iDist2);

  if (iDist1 == 0 && iDist2 == circle.size() - 1)
    circle.push_back(point);
  else
    circle.insert(circle.begin() + iDist2, point);
}
}  // namespace generator
