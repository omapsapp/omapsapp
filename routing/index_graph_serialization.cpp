#include "routing/index_graph_serialization.hpp"

namespace routing
{
// static
uint8_t constexpr IndexGraphSerializer::kLastVersion;
uint32_t constexpr IndexGraphSerializer::JointsFilter::kEmptyEntry;
uint32_t constexpr IndexGraphSerializer::JointsFilter::kPushedEntry;

// IndexGraphSerializer::SectionSerializer ---------------------------------------------------------
void IndexGraphSerializer::SectionSerializer::PreSerialize(IndexGraph const & graph,
                                                           std::unordered_map<uint32_t, VehicleMask> const &,
                                                           JointIdEncoder & jointEncoder)
{
  m_buffer.clear();
  MemWriter<std::vector<uint8_t>> memWriter(m_buffer);
  BitWriter<MemWriter<std::vector<uint8_t>>> writer(memWriter);

  // -1 for uint32_t is some confusing, but it allows process first iteration in the common way.
  // Gamma coder can't write 0, so init prevFeatureId = -1 in case of first featureId == 0.
  // It works because uint32_t is residual ring type.
  uint32_t prevFeatureId = -1;

  for (uint32_t const featureId : m_featureIds)
  {
    RoadJointIds const & road = graph.GetRoad(featureId);
    WriteGamma(writer, featureId - prevFeatureId);
    WriteGamma(writer, ConvertJointsNumber(road.GetJointsNumber()));

    uint32_t prevPointId = -1;
    road.ForEachJoint([&](uint32_t pointId, Joint::Id jointId) {
      WriteGamma(writer, pointId - prevPointId);
      jointEncoder.Write(jointId, writer);
      prevPointId = pointId;
    });

    prevFeatureId = featureId;
  }
}

// IndexGraphSerializer::JointsFilter --------------------------------------------------------------
void IndexGraphSerializer::JointsFilter::Push(Joint::Id jointIdInFile, RoadPoint const & rp)
{
  CHECK_LESS(jointIdInFile, m_entries.size(), ());

  auto & entry = m_entries[jointIdInFile];
  switch (entry.first)
  {
  case kEmptyEntry:
    // Keep RoadPoint here until second RoadPoint with same Joint::Id comes.
    // If second point does not come, it is redundant Joint that should be filtered.
    entry.first = rp.GetFeatureId();
    entry.second.pointId = rp.GetPointId();
    break;
  case kPushedEntry: m_graph.PushFromSerializer(entry.second.jointId, rp); break;
  default:
    m_graph.PushFromSerializer(m_count,
                               RoadPoint(entry.first /* featureId */, entry.second.pointId));
    m_graph.PushFromSerializer(m_count, rp);
    entry.first = kPushedEntry;
    entry.second.jointId = m_count;
    ++m_count;
  }
}

// IndexGraphSerializer ----------------------------------------------------------------------------
// static
VehicleMask IndexGraphSerializer::GetRoadMask(std::unordered_map<uint32_t, VehicleMask> const & masks,
                                              uint32_t featureId)
{
  auto const & it = masks.find(featureId);
  CHECK(it != masks.cend(), ("Can't find vehicle mask for feature", featureId));
  return it->second;
}

// Gamma Coder encodes value 1 with 1 bit, value 2 with 3 bits.
// The vast majority of roads contains 2 joints: endpoints connections.
// Swap values 1, 2 to encode most of joints numbers with 1 bit.
//
// static
uint32_t IndexGraphSerializer::ConvertJointsNumber(uint32_t jointsNumber)
{
  switch (jointsNumber)
  {
  case 1: return 2;
  case 2: return 1;
  default: return jointsNumber;
  }
}

// static
void IndexGraphSerializer::PrepareSectionSerializers(
    IndexGraph const & graph, std::unordered_map<uint32_t, VehicleMask> const & masks,
    std::vector<SectionSerializer> & serializers)
{
  size_t maskToIndex[kNumVehicleMasks] = {};
  // Car routing is most used routing: put car sections first.
  // It would be a bit faster read them for car model.
  for (size_t step = 0; step < 2; ++step)
  {
    for (VehicleMask mask = 1; mask < kNumVehicleMasks; ++mask)
    {
      bool const hasCar = (mask & kCarMask) != 0;
      if ((step == 0) == hasCar)
      {
        CHECK_EQUAL(maskToIndex[mask], 0, ("Mask", mask, "already has serializer"));
        maskToIndex[mask] = serializers.size();
        serializers.emplace_back(mask /* SectionSerializer(mask) */);
      }
    }
  }

  graph.ForEachRoad([&](uint32_t featureId, RoadJointIds const & /* road */) {
    VehicleMask const mask = GetRoadMask(masks, featureId);
    SectionSerializer & serializer = serializers[maskToIndex[mask]];
    CHECK_EQUAL(serializer.GetMask(), mask, ());
    serializer.AddRoad(featureId);
  });

  for (SectionSerializer & serializer : serializers)
    serializer.SortRoads();
}
}  // namespace routing
