#include "generator/road_access_generator.hpp"

#include "generator/feature_builder.hpp"
#include "generator/routing_helpers.hpp"

#include "routing/road_access.hpp"
#include "routing/road_access_serialization.hpp"

#include "coding/file_writer.hpp"
#include "coding/files_container.hpp"
#include "coding/reader.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "defines.hpp"

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <unordered_map>
#include <utility>

#include "3party/opening_hours/opening_hours.hpp"


namespace routing_builder
{
using std::string, std::vector;

using TagMapping = RoadAccessTagProcessor::TagMapping;
using ConditionalTagsList = RoadAccessTagProcessor::ConditionalTagsList;

/// @name Got from: https://taginfo.openstreetmap.org/search?q=%3Aconditional
/// @{
vector<string> const kCarAccessConditionalTags = {
    "motor_vehicle:conditional", "motorcar:conditional", "vehicle:conditional",
};

vector<string> const kDefaultAccessConditionalTags = {
    "access:conditional", "locked:conditional"
};

vector<string> const kPedestrianAccessConditionalTags = {
    "foot:conditional"
};

vector<string> const kBycicleAccessConditionalTags = {
    "bicycle:conditional"
};
/// @}

// Some tags assume access:conditional in fact, but doesn't have it.
// For example if road is tagged as "winter_road = yes",
// for routing it is like: "access:conditional = no @ (Feb - Dec)"
std::map<OsmElement::Tag, string> kTagToAccessConditional = {
    {OsmElement::Tag("winter_road", "yes"), "no @ (Mar - Nov)"},
    {OsmElement::Tag("ice_road", "yes"), "no @ (Mar - Nov)"}
};

TagMapping const kMotorCarTagMapping = {
    {OsmElement::Tag("motorcar", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motorcar", "designated"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motorcar", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motorcar", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("motorcar", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("motorcar", "destination"), RoadAccess::Type::Destination},
    {OsmElement::Tag("motorcar", "permit"), RoadAccess::Type::Permit},
};

TagMapping const kMotorVehicleTagMapping = {
    {OsmElement::Tag("motor_vehicle", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motor_vehicle", "designated"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motor_vehicle", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motor_vehicle", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("motor_vehicle", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("motor_vehicle", "destination"), RoadAccess::Type::Destination},
    {OsmElement::Tag("motor_vehicle", "permit"), RoadAccess::Type::Permit},
};

TagMapping const kVehicleTagMapping = {
    {OsmElement::Tag("vehicle", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("vehicle", "designated"), RoadAccess::Type::Yes},
    {OsmElement::Tag("vehicle", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("vehicle", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("vehicle", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("vehicle", "destination"), RoadAccess::Type::Destination},
    {OsmElement::Tag("vehicle", "permit"), RoadAccess::Type::Permit},
};

TagMapping const kCarBarriersTagMapping = {
    {OsmElement::Tag("barrier", "block"), RoadAccess::Type::No},
    {OsmElement::Tag("barrier", "bollard"), RoadAccess::Type::No},
    {OsmElement::Tag("barrier", "cycle_barrier"), RoadAccess::Type::No},
    {OsmElement::Tag("barrier", "chain"), RoadAccess::Type::Private},
    {OsmElement::Tag("barrier", "gate"), RoadAccess::Type::Private},
    {OsmElement::Tag("barrier", "lift_gate"), RoadAccess::Type::Private},
    {OsmElement::Tag("barrier", "swing_gate"), RoadAccess::Type::Private},

    // TODO (@gmoryes) The types below should be added.
    //  {OsmElement::Tag("barrier", "log"), RoadAccess::Type::No},
    //  {OsmElement::Tag("barrier", "motorcycle_barrier"), RoadAccess::Type::No},
};

TagMapping const kPedestrianTagMapping = {
    {OsmElement::Tag("foot", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("foot", "designated"), RoadAccess::Type::Yes},
    {OsmElement::Tag("foot", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("foot", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("foot", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("foot", "destination"), RoadAccess::Type::Destination},
};

TagMapping const kBicycleTagMapping = {
    {OsmElement::Tag("bicycle", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("bicycle", "designated"), RoadAccess::Type::Yes},
    {OsmElement::Tag("bicycle", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("bicycle", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("bicycle", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("bicycle", "destination"), RoadAccess::Type::Destination},
};

TagMapping const kBicycleBarriersTagMapping = {
    {OsmElement::Tag("barrier", "gate"), RoadAccess::Type::Private},
    // TODO (@gmoryes) The types below should be added.
    //  {OsmElement::Tag("barrier", "kissing_gate"), RoadAccess::Type::Private},
};

// Allow everything to keep transit section empty. We'll use pedestrian section for
// transit + pedestrian combination.
// Empty mapping leads to default RoadAccess::Type::Yes access type for all roads.
TagMapping const kTransitTagMapping = {};

TagMapping const kDefaultTagMapping = {
    {OsmElement::Tag("access", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("access", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("access", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("access", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("access", "destination"), RoadAccess::Type::Destination},
    {OsmElement::Tag("access", "emergency"), RoadAccess::Type::No},
    {OsmElement::Tag("access", "military"), RoadAccess::Type::No},
    {OsmElement::Tag("access", "agricultural"), RoadAccess::Type::Private},
    {OsmElement::Tag("access", "forestry"), RoadAccess::Type::Private},
    {OsmElement::Tag("locked", "yes"), RoadAccess::Type::Locked},
};

// Removed secondary, tertiary from car list. Example https://www.openstreetmap.org/node/8169922700
std::set<OsmElement::Tag> const kHighwaysWhereIgnoreBarriersWithoutAccessCar = {
    {"highway", "motorway"},
    {"highway", "motorway_link"},
    {"highway", "trunk"},
    {"highway", "trunk_link"},
    {"highway", "primary"},
    {"highway", "primary_link"},
};

/// @todo Looks controversial for secondary/tertiary, but leave as-is for bicycle.
std::set<OsmElement::Tag> const kHighwaysWhereIgnoreBarriersWithoutAccessBicycle = {
    {"highway", "motorway"},
    {"highway", "motorway_link"},
    {"highway", "trunk"},
    {"highway", "trunk_link"},
    {"highway", "primary"},
    {"highway", "primary_link"},
    {"highway", "secondary"},
    {"highway", "secondary_link"},
    {"highway", "tertiary"},
    {"highway", "tertiary_link"},
    {"highway", "cycleway"},  // Bicycle barriers without access on cycleway are ignored :)
};

// motorway_junction blocks not only highway link, but main road also.
// Actually, tagging motorway_junction with access is not used, see https://overpass-turbo.eu/s/1d1t
// https://github.com/organicmaps/organicmaps/issues/1389
std::set<OsmElement::Tag> const kIgnoreAccess = {
    {"highway", "motorway_junction"}
};

std::set<OsmElement::Tag> const kHighwaysWhereIgnoreAccessDestination = {
    {"highway", "motorway"},
    {"highway", "motorway_link"},
    {"highway", "trunk"},
    {"highway", "trunk_link"},
    {"highway", "primary"},
    {"highway", "primary_link"},
};

auto const kEmptyAccess = RoadAccess::Type::Count;

// Access can be together with locked (not exclusive).
RoadAccess::Type GetAccessTypeFromMapping(OsmElement const & elem, TagMapping const * mapping)
{
  bool isLocked = false;
  for (auto const & tag : elem.m_tags)
  {
    auto const it = mapping->find(tag);
    if (it != mapping->cend())
    {
      if (it->second == RoadAccess::Type::Locked)
        isLocked = true;
      else
        return it->second;
    }
  }

  return (isLocked ? RoadAccess::Type::Locked : kEmptyAccess);
}

std::optional<std::pair<string, string>> GetTagValueConditionalAccess(
    OsmElement const & elem, vector<ConditionalTagsList> const & tagsList)
{
  if (tagsList.empty())
    return {};

  for (auto const & tags : tagsList)
  {
    for (auto const & tag : tags)
    {
      if (elem.HasTag(tag))
        return std::make_pair(tag, elem.GetTag(tag));
    }
  }

  for (auto const & [tag, access] : kTagToAccessConditional)
  {
    if (elem.HasTag(tag.m_key, tag.m_value))
      return std::make_pair(kDefaultAccessConditionalTags.front(), access);
  }

  return {};
}

// "motor_vehicle:conditional" -> "motor_vehicle"
// "access:conditional" -> "access"
// etc.
string GetVehicleTypeForAccessConditional(string const & accessConditionalTag)
{
  auto const pos = accessConditionalTag.find(':');
  CHECK_NOT_EQUAL(pos, string::npos, (accessConditionalTag));
  return accessConditionalTag.substr(0, pos);
}

// RoadAccessTagProcessor --------------------------------------------------------------------------
RoadAccessTagProcessor::RoadAccessTagProcessor(VehicleType vehicleType)
  : m_vehicleType(vehicleType)
{
  switch (vehicleType)
  {
  case VehicleType::Car:
    // Order is important here starting from most specific (motorcar) to generic (access).
    m_accessMappings.push_back(&kMotorCarTagMapping);
    m_accessMappings.push_back(&kMotorVehicleTagMapping);
    m_accessMappings.push_back(&kVehicleTagMapping);
    m_accessMappings.push_back(&kDefaultTagMapping);

    m_barrierMappings.push_back(&kCarBarriersTagMapping);
    m_highwaysToIgnoreWABarriers = &kHighwaysWhereIgnoreBarriersWithoutAccessCar;
    m_conditionalTagsVector.push_back(kCarAccessConditionalTags);
    m_conditionalTagsVector.push_back(kDefaultAccessConditionalTags);
    break;

  case VehicleType::Pedestrian:
    m_accessMappings.push_back(&kPedestrianTagMapping);
    m_accessMappings.push_back(&kDefaultTagMapping);
    m_conditionalTagsVector.push_back(kPedestrianAccessConditionalTags);
    m_conditionalTagsVector.push_back(kDefaultAccessConditionalTags);
    break;

  case VehicleType::Bicycle:
    m_accessMappings.push_back(&kBicycleTagMapping);
    m_accessMappings.push_back(&kVehicleTagMapping);
    m_accessMappings.push_back(&kDefaultTagMapping);
    m_barrierMappings.push_back(&kBicycleBarriersTagMapping);
    m_highwaysToIgnoreWABarriers = &kHighwaysWhereIgnoreBarriersWithoutAccessBicycle;
    m_conditionalTagsVector.push_back(kBycicleAccessConditionalTags);
    m_conditionalTagsVector.push_back(kDefaultAccessConditionalTags);
    break;

  case VehicleType::Transit:
    // Use kTransitTagMapping to keep transit section empty. We'll use pedestrian section for
    // transit + pedestrian combination.
    m_accessMappings.push_back(&kTransitTagMapping);
    break;

  case VehicleType::Count:
    CHECK(false, ("Bad vehicle type"));
    break;
  }
}

void RoadAccessTagProcessor::Process(OsmElement const & elem)
{
  auto const getAccessType = [&](vector<TagMapping const *> const & mapping)
  {
    bool isPermit = false;
    for (auto const tagMapping : mapping)
    {
      auto const accessType = GetAccessTypeFromMapping(elem, tagMapping);
      if (accessType != kEmptyAccess)
      {
        switch (accessType)
        {
        case RoadAccess::Type::Permit:
          isPermit = true;
          break;
        case RoadAccess::Type::Locked:
          return RoadAccess::Type::Private;
        default:
          // Patch: (permit + access=no) -> private.
          if (accessType == RoadAccess::Type::No && isPermit)
            return RoadAccess::Type::Private;
          return accessType;
        }
      }
    }

    return kEmptyAccess;
  };

  auto op = getAccessType(m_accessMappings);
  if (op != kEmptyAccess)
  {
    if (op == RoadAccess::Type::Yes)
      return;

    if (op == RoadAccess::Type::Destination)
    {
      for (auto const & tag : elem.m_tags)
      {
        if (kHighwaysWhereIgnoreAccessDestination.count(tag))
          return;
      }
    }

    if (elem.IsNode())
    {
      // OSM mapping workaround.
      // https://github.com/organicmaps/organicmaps/issues/4837
      if (op == RoadAccess::Type::No && m_vehicleType == VehicleType::Bicycle && elem.GetTag("highway") == "crossing")
      {
        // Skip this "barrier". It blocks cycling on main road via this kind of crossings.
        // Example here: https://overpass-turbo.eu/s/1sSx

        /// @todo I suppose that we should allow _main_ road, but block real _crossing_ Way via this Node.
        /// But I have no idea how we can easily implement it right now ...
      }
      else
        m_barriersWithAccessTag.Add(elem.m_id, {elem.m_lat, elem.m_lon}, op);
    }
    else if (elem.IsWay())
      m_wayToAccess.Add(elem.m_id, op);
    return;
  }

  if (!elem.IsNode())
    return;

  // Apply barrier tags if we have no {vehicle = ...}, {access = ...} etc.
  op = getAccessType(m_barrierMappings);
  if (op != kEmptyAccess)
    m_barriersWithoutAccessTag.Add(elem.m_id, {elem.m_lat, elem.m_lon}, op);
}

void RoadAccessTagProcessor::ProcessConditional(OsmElement const & elem)
{
  if (!elem.IsWay())
    return;

  auto op = GetTagValueConditionalAccess(elem, m_conditionalTagsVector);
  if (!op)
    return;

  auto const & parser = AccessConditionalTagParser::Instance();
  for (auto & access : parser.ParseAccessConditionalTag(op->first, op->second))
  {
    if (access.m_accessType != kEmptyAccess)
      m_wayToAccessConditional.FindOrInsert(elem.m_id)->emplace_back(std::move(access));
  }
}

void RoadAccessTagProcessor::SetIgnoreBarriers(OsmElement const & elem)
{
  if (!m_highwaysToIgnoreWABarriers)
    return;

  for (auto const & tag : elem.m_tags)
  {
    if (m_highwaysToIgnoreWABarriers->count(tag) > 0)
    {
      m_ignoreWABarriers.push_back(true);
      return;
    }
  }

  m_ignoreWABarriers.push_back(false);
}

bool RoadAccessTagProcessor::IsIgnoreBarriers(size_t wayIdx) const
{
  if (m_highwaysToIgnoreWABarriers)
  {
    CHECK_LESS(wayIdx, m_ignoreWABarriers.size(), ());
    return m_ignoreWABarriers[wayIdx];
  }
  return false;
}

void RoadAccessTagProcessor::MergeInto(RoadAccessTagProcessor & processor) const
{
  CHECK_EQUAL(m_vehicleType, processor.m_vehicleType, ());

  m_barriersWithAccessTag.MergeInto(processor.m_barriersWithAccessTag);
  m_barriersWithoutAccessTag.MergeInto(processor.m_barriersWithoutAccessTag);
  m_wayToAccess.MergeInto(processor.m_wayToAccess);
  m_wayToAccessConditional.MergeInto(processor.m_wayToAccessConditional);

  processor.m_ignoreWABarriers.insert(processor.m_ignoreWABarriers.end(),
                                      m_ignoreWABarriers.begin(), m_ignoreWABarriers.end());
}

// RoadAccessCollector ------------------------------------------------------------
RoadAccessCollector::RoadAccessCollector(string const & filename, IDRInterfacePtr cache)
  : generator::CollectorInterface(filename), m_cache(std::move(cache))
{
  for (uint8_t i = 0; i < static_cast<uint8_t>(VehicleType::Count); ++i)
    m_tagProcessors.emplace_back(static_cast<VehicleType>(i));
}

std::shared_ptr<generator::CollectorInterface> RoadAccessCollector::Clone(IDRInterfacePtr const & cache) const
{
  return std::make_shared<RoadAccessCollector>(GetFilename(), cache);
}

void RoadAccessCollector::CollectFeature(feature::FeatureBuilder const & fb, OsmElement const & elem)
{
  /// @todo Note that here we take into account only classifier recognized barriers (valid FeatureBuilder).
  /// Can't say for sure is it good or not, but as it is.

  for (auto const & tag : elem.m_tags)
  {
    if (kIgnoreAccess.count(tag))
      return;
  }

  for (auto & p : m_tagProcessors)
  {
    p.Process(elem);
    p.ProcessConditional(elem);
  }

  if (routing::IsRoadWay(fb))
  {
    m_roads.AddWay(elem);

    for (auto & p : m_tagProcessors)
      p.SetIgnoreBarriers(elem);
  }
}

/// @see correspondent ReadRoadAccess.
void RoadAccessCollector::Save()
{
  auto fileName = GetFilename();

  LOG(LINFO, ("Saving road access values to", fileName));
  FileWriter writer(fileName);

  for (auto & p : m_tagProcessors)
  {
    p.m_wayToAccess.Serialize(writer);
    p.m_wayToAccessConditional.Serialize(writer);
  }

  generator::SizeWriter<uint64_t> sizeWriter;
  uint64_t count = 0;
  sizeWriter.Reserve(writer);

  m_roads.ForEachWayWithIndex([&](uint64_t wayID, std::vector<uint64_t> const & nodes, size_t idx)
  {
    for (auto & p : m_tagProcessors)
    {
      for (uint32_t nodeIdx = 0; nodeIdx < nodes.size(); ++nodeIdx)
      {
        uint64_t const nodeID = nodes[nodeIdx];
        auto const * entry = p.m_barriersWithAccessTag.Find(nodeID);
        if (entry == nullptr)
        {
          entry = p.m_barriersWithoutAccessTag.Find(nodeID);
          if (entry == nullptr)
            continue;

          if (p.IsIgnoreBarriers(idx))
          {
            // Dump only bicycle profile with the most wide barriers set to ignore.
            if (p.m_vehicleType == VehicleType::Bicycle)
              LOG(LWARNING, ("Node barrier without access:", nodeID));
            continue;
          }
        }

        ++count;
        // Write vehicle type for each entry to avoid multiple roads iterating.
        WriteToSink(writer, static_cast<uint8_t>(p.m_vehicleType));
        entry->Write(writer, wayID, nodeIdx);
      }
    }
  }, m_cache);

  sizeWriter.Write(writer, count);
}

void RoadAccessCollector::MergeInto(RoadAccessCollector & collector) const
{
  auto & dest = collector.m_tagProcessors;
  CHECK_EQUAL(m_tagProcessors.size(), dest.size(), ());

  for (size_t i = 0; i < dest.size(); ++i)
    m_tagProcessors[i].MergeInto(dest[i]);

  m_roads.MergeInto(collector.m_roads);
}

/// @see correspondent RoadAccessCollector::Save.
void ReadRoadAccess(string const & roadAccessPath, routing::OsmWay2FeaturePoint & way2feature,
                    RoadAccessByVehicleType & roadAccessByVehicleType)
{
  FileReader reader(roadAccessPath);
  ReaderSource src(reader);

  uint8_t constexpr vehiclesCount = static_cast<uint8_t>(VehicleType::Count);

  for (uint8_t vehicleType = 0; vehicleType < vehiclesCount; ++vehicleType)
  {
    RoadAccess::WayToAccess wayAccess;
    RoadAccess::WayToAccessConditional wayAccessConditional;

    generator::WaysMapper<RoadAccess::Type>::Deserialize(src, [&](uint64_t id, RoadAccess::Type ra)
    {
      way2feature.ForEachFeature(id, [&](uint32_t featureID)
      {
        auto const res = wayAccess.emplace(featureID, ra);
        if (!res.second && res.first->second != ra)
        {
          LOG(LWARNING, ("Duplicate road access info for OSM way", id, "vehicle:", vehicleType,
                         "access is:", res.first->second, "tried:", ra));
        }
      });
    });

    generator::WaysMapper<ConditionalRAVectorT>::Deserialize(src, [&](uint64_t id, ConditionalRAVectorT const & vec)
    {
      way2feature.ForEachFeature(id, [&](uint32_t featureID)
      {
        auto const res = wayAccessConditional.insert({featureID, {}});
        if (!res.second)
        {
          LOG(LWARNING, ("Duplicate conditional road access info for OSM way", id, "vehicle:", vehicleType,
                         "access is:", res.first->second, "tried:", vec));
          return;
        }

        auto & conditional = res.first->second;
        for (auto const & e : vec)
        {
          osmoh::OpeningHours oh(e.m_openingHours);
          if (oh.IsValid())
            conditional.Insert(e.m_accessType, std::move(oh));
        }

        if (conditional.IsEmpty())
        {
          LOG(LWARNING, ("Invalid conditional access:", vec));
          wayAccessConditional.erase(res.first);
        }
      });
    });

    roadAccessByVehicleType[vehicleType].SetWayAccess(std::move(wayAccess), std::move(wayAccessConditional));
  }

  RoadAccess::PointToAccess pointAccess[vehiclesCount];

  uint64_t count = ReadPrimitiveFromSource<uint64_t>(src);
  while (count-- > 0)
  {
    generator::WayNodesMapper<RoadAccess::Type>::Entry entry;

    uint8_t const vehicleType = ReadPrimitiveFromSource<uint8_t>(src);
    uint64_t wayID;
    uint32_t candidateIdx;
    entry.Read(src, wayID, candidateIdx);

    way2feature.ForEachNodeIdx(wayID, candidateIdx, entry.m_coord, [&](uint32_t featureID, uint32_t nodeIdx)
    {
      auto const res = pointAccess[vehicleType].insert({{featureID, nodeIdx}, entry.m_t});
      if (!res.second && res.first->second != entry.m_t)
      {
        LOG(LWARNING, ("Duplicate road access info for OSM way", wayID, "vehicle:", vehicleType,
                       "access is:", res.first->second, "tried:", entry.m_t));
      }
    });
  }

  /// @todo We don't have point conditional access now.
  for (uint8_t vehicleType = 0; vehicleType < vehiclesCount; ++vehicleType)
    roadAccessByVehicleType[vehicleType].SetPointAccess(std::move(pointAccess[vehicleType]), {});
}

// AccessConditionalTagParser ----------------------------------------------------------------------
// static
AccessConditionalTagParser const & AccessConditionalTagParser::Instance()
{
  static AccessConditionalTagParser instance;
  return instance;
}

AccessConditionalTagParser::AccessConditionalTagParser()
{
  // Order is important here starting from most specific (motorcar) to generic (access).
  m_vehiclesToRoadAccess.push_back(kMotorCarTagMapping);
  m_vehiclesToRoadAccess.push_back(kMotorVehicleTagMapping);
  m_vehiclesToRoadAccess.push_back(kVehicleTagMapping);
  m_vehiclesToRoadAccess.push_back(kPedestrianTagMapping);
  m_vehiclesToRoadAccess.push_back(kBicycleTagMapping);
  m_vehiclesToRoadAccess.push_back(kDefaultTagMapping);
}

vector<AccessConditional> AccessConditionalTagParser::ParseAccessConditionalTag(
    string const & tag, string const & value) const
{
  size_t pos = 0;

  string const vehicleType = GetVehicleTypeForAccessConditional(tag);
  vector<AccessConditional> accessConditionals;
  while (pos < value.size())
  {
    AccessConditional access;
    auto accessOp = ReadUntilSymbol(value, pos, '@');
    if (!accessOp)
      break;

    string accessString;
    tie(pos, accessString) = *accessOp;
    ++pos;  // skip '@'
    strings::Trim(accessString);

    access.m_accessType = GetAccessByVehicleAndStringValue(vehicleType, accessString);

    auto substrOp = ReadUntilSymbol(value, pos, ';');
    if (!substrOp)
    {
      auto openingHours = std::string(value.begin() + pos, value.end());
      access.m_openingHours = TrimAndDropAroundParentheses(std::move(openingHours));
      pos = value.size();
    }
    else
    {
      auto [newPos, _] = *substrOp;
      // We cannot distinguish these two situations:
      //   1) no @ Mo-Fr ; yes @ Sa-Su
      //   2) no @ Mo-Fr ; Sa 10:00-19:00
      auto nextAccessOp = ReadUntilSymbol(value, newPos, '@');
      if (nextAccessOp)
      {
        // This is 1) case.
        auto openingHours = std::string(value.begin() + pos, value.begin() + newPos);
        access.m_openingHours = TrimAndDropAroundParentheses(std::move(openingHours));
        pos = newPos;
        ++pos;  // skip ';'
      }
      else
      {
        // This is 2) case.
        auto openingHours = std::string(value.begin() + pos, value.end());
        access.m_openingHours = TrimAndDropAroundParentheses(std::move(openingHours));
        pos = value.size();
      }
    }

    if (!access.m_openingHours.empty())
      accessConditionals.emplace_back(std::move(access));
    else
      LOG(LWARNING, ("Error parsing OH for:", tag, value));
  }

  return accessConditionals;
}

// static
std::optional<std::pair<size_t, string>>
AccessConditionalTagParser::ReadUntilSymbol(string const & input, size_t startPos, char symbol)
{
  string result;
  while (startPos < input.size() && input[startPos] != symbol)
  {
    result += input[startPos];
    ++startPos;
  }

  if (input[startPos] == symbol)
    return std::make_pair(startPos, result);

  return {};
}

RoadAccess::Type AccessConditionalTagParser::GetAccessByVehicleAndStringValue(
    string const & vehicleFromTag, string const & stringAccessValue) const
{
  bool isPermit = false;
  for (auto const & vehicleToAccess : m_vehiclesToRoadAccess)
  {
    auto const it = vehicleToAccess.find({vehicleFromTag, stringAccessValue});
    if (it != vehicleToAccess.end())
    {
      switch (it->second)
      {
      case RoadAccess::Type::Permit:
        isPermit = true;
        break;
      case RoadAccess::Type::Locked:
        return RoadAccess::Type::Private;
      default:
        // Patch: (permit + access=no) -> private.
        if (it->second == RoadAccess::Type::No && isPermit)
          return RoadAccess::Type::Private;
        return it->second;
      }
    }
  }

  return kEmptyAccess;
}

// static
string AccessConditionalTagParser::TrimAndDropAroundParentheses(string input)
{
  strings::Trim(input);

  if (!input.empty() && input.back() == ';')
    input.pop_back();

  if (input.size() >= 2 && input.front() == '(' && input.back() == ')')
  {
    input.erase(input.begin());
    input.pop_back();
  }

  return input;
}

// Functions ------------------------------------------------------------------
bool BuildRoadAccessInfo(string const & dataFilePath, string const & roadAccessPath,
                         routing::OsmWay2FeaturePoint & way2feature)
{
  LOG(LINFO, ("Generating road access info for", dataFilePath));

  try
  {
    RoadAccessByVehicleType roadAccessByVehicleType;
    ReadRoadAccess(roadAccessPath, way2feature, roadAccessByVehicleType);

    FilesContainerW cont(dataFilePath, FileWriter::OP_WRITE_EXISTING);
    auto writer = cont.GetWriter(ROAD_ACCESS_FILE_TAG);

    routing::RoadAccessSerializer::Serialize(*writer, roadAccessByVehicleType);
  }
  catch (RootException const & ex)
  {
    LOG(LWARNING, ("No road access created:", ex.Msg()));
    return false;
  }

  return true;
}
}  // namespace routing_builder
