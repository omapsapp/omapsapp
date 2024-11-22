#include "generator/restriction_collector.hpp"

#include "generator/routing_helpers.hpp"

#include "routing/restriction_loader.hpp"
#include "routing/road_graph.hpp"

#include "routing_common/car_model.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"
#include "base/geo_object_id.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace routing_builder
{
using namespace routing;

char const kNo[] = "No";
char const kOnly[] = "Only";
char const kNoUTurn[] = "NoUTurn";
char const kOnlyUTurn[] = "OnlyUTurn";

template <class TokenizerT> bool ParseLineOfWayIds(TokenizerT & iter, std::vector<base::GeoObjectId> & numbers)
{
  uint64_t number = 0;
  for (; iter; ++iter)
  {
    if (!strings::to_uint(*iter, number))
      return false;
    numbers.push_back(base::MakeOsmWay(number));
  }
  return true;
}

m2::PointD constexpr RestrictionCollector::kNoCoords;

RestrictionCollector::RestrictionCollector(std::string const & osmIdsToFeatureIdPath, IndexGraph & graph)
  : m_indexGraph(graph)
{
  ParseWaysOsmIdToFeatureIdMapping(osmIdsToFeatureIdPath, m_osmIdToFeatureIds);
}

bool RestrictionCollector::Process(std::string const & restrictionPath)
{
  SCOPE_GUARD(clean, [this]() {
    m_osmIdToFeatureIds.clear();
    m_restrictions.clear();
  });

  if (!ParseRestrictions(restrictionPath))
  {
    LOG(LWARNING, ("An error happened while parsing restrictions from file:",  restrictionPath));
    return false;
  }

  clean.release();

  base::SortUnique(m_restrictions);

  LOG(LDEBUG, ("Number of loaded restrictions:", m_restrictions.size()));
  return true;
}

bool RestrictionCollector::ParseRestrictions(std::string const & path)
{
  std::ifstream stream(path);
  if (stream.fail())
    return false;

  std::string line;
  while (std::getline(stream, line))
  {
    strings::SimpleTokenizer iter(line, ", \t\r\n");
    if (!iter)  // the line is empty
      return false;

    Restriction::Type restrictionType;
    auto viaType = RestrictionWriter::ViaType::Count;
    FromString(*iter, restrictionType);
    ++iter;

    FromString(*iter, viaType);
    ++iter;

    m2::PointD coords = kNoCoords;
    if (viaType == RestrictionWriter::ViaType::Node)
    {
      FromString(*iter, coords.x);
      ++iter;
      FromString(*iter, coords.y);
      ++iter;
    }

    std::vector<base::GeoObjectId> osmIds;
    if (!ParseLineOfWayIds(iter, osmIds))
    {
      LOG(LWARNING, ("Cannot parse osm ids from", path));
      return false;
    }

    if (viaType == RestrictionWriter::ViaType::Node)
      CHECK_EQUAL(osmIds.size(), 2, ("Only |from| and |to| osmId."));

    CHECK_NOT_EQUAL(viaType, RestrictionWriter::ViaType::Count, ());
    AddRestriction(coords, restrictionType, osmIds);
  }
  return true;
}

Joint::Id RestrictionCollector::GetFirstCommonJoint(uint32_t firstFeatureId,
                                                    uint32_t secondFeatureId) const
{
  uint32_t const firstLen = m_indexGraph.GetRoadGeometry(firstFeatureId).GetPointsCount();
  uint32_t const secondLen = m_indexGraph.GetRoadGeometry(secondFeatureId).GetPointsCount();

  auto const firstRoad = m_indexGraph.GetRoad(firstFeatureId);
  auto const secondRoad = m_indexGraph.GetRoad(secondFeatureId);

  std::unordered_set<Joint::Id> used;
  for (uint32_t i = 0; i < firstLen; ++i)
  {
    if (firstRoad.GetJointId(i) != Joint::kInvalidId)
      used.emplace(firstRoad.GetJointId(i));
  }

  for (uint32_t i = 0; i < secondLen; ++i)
  {
    if (used.count(secondRoad.GetJointId(i)) != 0)
      return secondRoad.GetJointId(i);
  }

  return Joint::kInvalidId;
}

bool RestrictionCollector::FeatureHasPointWithCoords(uint32_t featureId,
                                                     m2::PointD const & coords) const
{
  auto const & roadGeometry = m_indexGraph.GetRoadGeometry(featureId);
  uint32_t const pointsCount = roadGeometry.GetPointsCount();
  for (uint32_t i = 0; i < pointsCount; ++i)
  {
    static double constexpr kEps = 1e-5;
    if (AlmostEqualAbs(mercator::FromLatLon(roadGeometry.GetPoint(i)), coords, kEps))
      return true;
  }

  return false;
}

bool RestrictionCollector::FeaturesAreCross(m2::PointD const & coords,
                                            uint32_t prev, uint32_t cur) const
{
  if (coords == kNoCoords)
    return GetFirstCommonJoint(prev, cur) != Joint::kInvalidId;

  return FeatureHasPointWithCoords(prev, coords) && FeatureHasPointWithCoords(cur, coords);
}

Restriction::Type ConvertUTurnToSimpleRestriction(Restriction::Type type)
{
  CHECK(IsUTurnType(type), ());

  // Some people create no_u_turn not in the way we expect.
  // For example: https://www.openstreetmap.org/relation/9511182
  //              https://www.openstreetmap.org/relation/2532732
  //              https://www.openstreetmap.org/relation/7606104
  // OsmId of |from| member is differ from |to| member.
  // So we "convert" such no_u_turn to any no_* restriction.
  // And we do the same thing with only_u_turn.
  return type == Restriction::Type::NoUTurn ? Restriction::Type::No
                                            : Restriction::Type::Only;
}

void ConvertToUTurnIfPossible(Restriction::Type & type, m2::PointD const & coords,
                              std::vector<uint32_t> const & featureIds)
{
  if (IsUTurnType(type))
    return;

  // Some people create u_turn not in the way we expect.
  // For example:
  // At 31.05.2019 there is no_left_turn by url:
  // https://www.openstreetmap.org/relation/1431150
  // with the same |from| and |to| member with node as |via|):
  //
  // So we "convert" such relations to no_u_turn or only_u_turn restrictions.
  if (featureIds.size() == 2 &&
      featureIds.front() == featureIds.back() &&
      coords != RestrictionCollector::kNoCoords)
  {
    type = type == Restriction::Type::No ? Restriction::Type::NoUTurn
                                         : Restriction::Type::OnlyUTurn;
  }
}

bool RestrictionCollector::CheckAndProcessUTurn(Restriction::Type & restrictionType,
                                                m2::PointD const & coords,
                                                std::vector<uint32_t> & featureIds) const
{
  CHECK(IsUTurnType(restrictionType), ());

  // UTurn with via as way.
  if (coords == kNoCoords)
  {
    // featureIds = {from, via_1, ..., via_N, to}, size must be greater or equal 3.
    CHECK_GREATER_OR_EQUAL(featureIds.size(), 3, ());
    restrictionType = ConvertUTurnToSimpleRestriction(restrictionType);
    return true;
  }

  // Only |from| and |to| member must be here, because via is node.
  if (featureIds.size() != 2)
    return false;

  // Typical no_u_turn with node as via, for example:
  // {
  //  from(featureId = 123),
  //  via (node at feature 123 = 2),
  //  to(featureId = 123)
  // }
  if (featureIds[0] == featureIds[1])
  {
    featureIds.pop_back();
    CHECK_EQUAL(featureIds.size(), 1, ());

    uint32_t & featureId = featureIds.back();

    auto const & road = m_indexGraph.GetRoadGeometry(featureId);
    // Can not do UTurn from feature to the same feature if it is one way.
    if (road.IsOneWay())
      return false;

    uint32_t const n = road.GetPointsCount();

    // According to the wiki: via must be at the end or at the beginning of feature.
    // https://wiki.openstreetmap.org/wiki/Relation:restriction
    static auto constexpr kEps = 1e-5;
    bool const viaIsFirstNode =
        AlmostEqualAbs(coords, mercator::FromLatLon(road.GetPoint(0)), kEps);
    bool const viaIsLastNode =
        AlmostEqualAbs(coords, mercator::FromLatLon(road.GetPoint(n - 1)), kEps);

    if (viaIsFirstNode)
    {
      featureId |= RestrictionSerializer::kUTurnAtTheBeginMask;
    }
    else if (viaIsLastNode)
    {
      CHECK_EQUAL(featureId & RestrictionSerializer::kUTurnAtTheBeginMask, 0,
                  ("The first bit of featureId must be zero, too big value "
                   "of featureId, invariant violated."));
    }

    return viaIsFirstNode || viaIsLastNode;
  }

  restrictionType = ConvertUTurnToSimpleRestriction(restrictionType);
  return true;
}

bool RestrictionCollector::IsRestrictionValid(Restriction::Type & restrictionType,
                                              m2::PointD const & coords,
                                              std::vector<uint32_t> & featureIds) const
{
  if (featureIds.empty() || !m_indexGraph.IsRoad(featureIds[0]))
    return false;

  for (size_t i = 1; i < featureIds.size(); ++i)
  {
    auto prev = featureIds[i - 1];
    auto cur = featureIds[i];
    if (!m_indexGraph.IsRoad(cur))
      return false;

    if (!FeaturesAreCross(coords, prev, cur))
      return false;
  }

  ConvertToUTurnIfPossible(restrictionType, coords, featureIds);

  if (!IsUTurnType(restrictionType))
    return true;

  return CheckAndProcessUTurn(restrictionType, coords, featureIds);
}

bool RestrictionCollector::AddRestriction(m2::PointD const & coords,
                                          Restriction::Type restrictionType,
                                          std::vector<base::GeoObjectId> const & osmIds)
{
  std::vector<uint32_t> featureIds(osmIds.size());
  for (size_t i = 0; i < osmIds.size(); ++i)
  {
    auto const result = m_osmIdToFeatureIds.find(osmIds[i]);
    if (result == m_osmIdToFeatureIds.cend())
    {
      // It happens near mwm border when one of a restriction lines is not included in mwm
      // but the restriction is included.
      return false;
    }

    // Only one feature id is found for |osmIds[i]|.
    // @TODO(bykoianko) All the feature ids should be used instead of |result->second.front()|.
    CHECK(!result->second.empty(), ());
    featureIds[i] = result->second.front();
  }

  if (!IsRestrictionValid(restrictionType, coords, featureIds))
    return false;

  m_restrictions.emplace_back(restrictionType, featureIds);
  return true;
}

void RestrictionCollector::AddFeatureId(uint32_t featureId, base::GeoObjectId osmId)
{
  ::routing::AddFeatureId(osmId, featureId, m_osmIdToFeatureIds);
}

void FromString(std::string_view str, Restriction::Type & type)
{
  if (str == kNo)
  {
    type = Restriction::Type::No;
    return;
  }

  if (str == kOnly)
  {
    type = Restriction::Type::Only;
    return;
  }

  if (str == kNoUTurn)
  {
    type = Restriction::Type::NoUTurn;
    return;
  }

  if (str == kOnlyUTurn)
  {
    type = Restriction::Type::OnlyUTurn;
    return;
  }

  CHECK(false,
        ("Invalid line:", str, "expected:", kNo, "or", kOnly, "or", kNoUTurn, "or", kOnlyUTurn));
  UNREACHABLE();
}

void FromString(std::string_view str, RestrictionWriter::ViaType & type)
{
  if (str == RestrictionWriter::kNodeString)
  {
    type = RestrictionWriter::ViaType::Node;
    return;
  }

  if (str == RestrictionWriter::kWayString)
  {
    type = RestrictionWriter::ViaType::Way;
    return;
  }

  CHECK(false, ("Invalid line:", str, "expected:", RestrictionWriter::kNodeString,
                "or", RestrictionWriter::kWayString));
}

void FromString(std::string_view str, double & number)
{
  CHECK(strings::to_double(str, number), ());
}
}  // namespace routing_builder
