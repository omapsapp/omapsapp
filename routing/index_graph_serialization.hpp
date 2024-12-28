#pragma once

#include "routing/coding.hpp"
#include "routing/index_graph.hpp"
#include "routing/joint.hpp"
#include "routing/routing_exceptions.hpp"
#include "routing/vehicle_mask.hpp"

#include "coding/bit_streams.hpp"
#include "coding/reader.hpp"
#include "coding/write_to_sink.hpp"

#include "base/checked_cast.hpp"

#include <limits>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace routing
{
class IndexGraphSerializer final
{
public:
  IndexGraphSerializer() = delete;

  template <class Sink>
  static void Serialize(IndexGraph const & graph,
                        std::unordered_map<uint32_t, VehicleMask> const & masks, Sink & sink)
  {
    Header header(graph);
    JointIdEncoder jointEncoder;

    std::vector<SectionSerializer> serializers;
    PrepareSectionSerializers(graph, masks, serializers);

    for (SectionSerializer & serializer : serializers)
    {
      Joint::Id const begin = jointEncoder.GetCount();
      serializer.PreSerialize(graph, masks, jointEncoder);
      header.AddSection({
          serializer.GetBufferSize(), static_cast<uint32_t>(serializer.GetNumRoads()), begin,
          jointEncoder.GetCount(), serializer.GetMask(),
      });
    }

    header.Serialize(sink);
    for (SectionSerializer & section : serializers)
      section.Flush(sink);
  }

  template <class Source>
  static void Deserialize(IndexGraph & graph, Source & src, VehicleMask requiredMask)
  {
    Header header;
    header.Deserialize(src);

    JointsFilter jointsFilter(graph, header.GetNumJoints());

    for (uint32_t i = 0; i < header.GetNumSections(); ++i)
    {
      Section const & section = header.GetSection(i);
      VehicleMask const mask = section.GetMask();

      if (!(mask & requiredMask))
      {
        src.Skip(section.GetSize());
        continue;
      }

      JointIdDecoder jointIdDecoder(section.GetBeginJointId());
      BitReader<Source> reader(src);
      uint64_t const expectedEndPos = src.Pos() + section.GetSize();

      // -1 for uint32_t is some confusing, but it allows process first iteration in the common way.
      // Delta coder can't write 0, so init prevFeatureId = -1 in case of first featureId == 0.
      // It works because uint32_t is residual ring type.
      uint32_t featureId = -1;
      for (uint32_t i = 0; i < section.GetNumRoads(); ++i)
      {
        uint32_t const featureDelta = ReadGamma<uint32_t>(reader);
        featureId += featureDelta;

        uint32_t const jointsNumber = ConvertJointsNumber(ReadGamma<uint32_t>(reader));

        // See comment above about -1.
        uint32_t pointId = -1;

        for (uint32_t j = 0; j < jointsNumber; ++j)
        {
          auto const pointDelta = ReadGamma<uint32_t>(reader);
          pointId += pointDelta;
          Joint::Id const jointId = jointIdDecoder.Read(reader);
          if (jointId >= section.GetEndJointId())
          {
            MYTHROW(CorruptedDataException,
                    ("Invalid jointId =", jointId, ", end =", section.GetEndJointId(), ", mask =",
                     mask, ", pointId =", pointId, ", featureId =", featureId));
          }

          jointsFilter.Push(jointId, RoadPoint(featureId, pointId));
        }
      }

      if (jointIdDecoder.GetCount() != section.GetEndJointId())
      {
        MYTHROW(CorruptedDataException,
                ("Invalid decoder count =", jointIdDecoder.GetCount(), ", expected =",
                 section.GetEndJointId(), ", mask =", mask));
      }

      if (src.Pos() != expectedEndPos)
      {
        MYTHROW(CorruptedDataException,
                ("Wrong position", src.Pos(), "after decoding section", mask, "expected",
                 expectedEndPos, "section size =", section.GetSize()));
      }
    }

    graph.Build(jointsFilter.GetCount());
  }

  template <class Source>
  static uint32_t DeserializeNumRoads(Source & src, VehicleMask requiredMask)
  {
    Header header;
    header.Deserialize(src);

    uint32_t numRoads = 0;
    for (uint32_t i = 0; i < header.GetNumSections(); ++i)
    {
      Section const & section = header.GetSection(i);
      VehicleMask const mask = section.GetMask();

      if (mask & requiredMask)
        numRoads += section.GetNumRoads();

      src.Skip(section.GetSize());
    }

    return numRoads;
  }

private:
  static uint8_t constexpr kLastVersion = 0;
  static uint8_t constexpr kNewJointIdBit = 0;
  static uint8_t constexpr kRepeatJointIdBit = 1;

  class Section final
  {
  public:
    Section() = default;

    Section(uint64_t size, uint32_t numRoads, Joint::Id beginJointId, Joint::Id endJointId,
            VehicleMask mask)
      : m_size(size)
      , m_numRoads(numRoads)
      , m_beginJointId(beginJointId)
      , m_endJointId(endJointId)
      , m_mask(mask)
    {
    }

    template <class Sink>
    void Serialize(Sink & sink) const
    {
      WriteToSink(sink, m_size);
      WriteToSink(sink, m_numRoads);
      WriteToSink(sink, m_beginJointId);
      WriteToSink(sink, m_endJointId);
      WriteToSink(sink, m_mask);
    }

    template <class Source>
    void Deserialize(Source & src)
    {
      m_size = ReadPrimitiveFromSource<decltype(m_size)>(src);
      m_numRoads = ReadPrimitiveFromSource<decltype(m_numRoads)>(src);
      m_beginJointId = ReadPrimitiveFromSource<decltype(m_beginJointId)>(src);
      m_endJointId = ReadPrimitiveFromSource<decltype(m_endJointId)>(src);
      m_mask = ReadPrimitiveFromSource<decltype(m_mask)>(src);
    }

    uint64_t GetSize() const { return m_size; }
    uint32_t GetNumRoads() const { return m_numRoads; }
    Joint::Id GetBeginJointId() const { return m_beginJointId; }
    Joint::Id GetEndJointId() const { return m_endJointId; }
    VehicleMask GetMask() const { return m_mask; }

  private:
    uint64_t m_size = 0;
    uint32_t m_numRoads = 0;
    Joint::Id m_beginJointId = Joint::kInvalidId;
    Joint::Id m_endJointId = Joint::kInvalidId;
    VehicleMask m_mask = 0;
  };

  class Header final
  {
  public:
    Header() = default;

    explicit Header(IndexGraph const & graph)
      : m_numRoads(graph.GetNumRoads()), m_numJoints(graph.GetNumJoints())
    {
    }

    template <class Sink>
    void Serialize(Sink & sink) const
    {
      WriteToSink(sink, m_version);
      WriteToSink(sink, m_numRoads);
      WriteToSink(sink, m_numJoints);

      WriteToSink(sink, static_cast<uint32_t>(m_sections.size()));
      for (Section const & section : m_sections)
        section.Serialize(sink);
    }

    template <class Source>
    void Deserialize(Source & src)
    {
      m_version = ReadPrimitiveFromSource<decltype(m_version)>(src);
      if (m_version != kLastVersion)
      {
        MYTHROW(CorruptedDataException,
                ("Unknown index graph version ", int(m_version), ", current version ", int(kLastVersion)));
      }

      m_numRoads = ReadPrimitiveFromSource<decltype(m_numRoads)>(src);
      m_numJoints = ReadPrimitiveFromSource<decltype(m_numJoints)>(src);

      auto const sectionsSize = ReadPrimitiveFromSource<uint32_t>(src);
      m_sections.resize(sectionsSize);
      for (Section & section : m_sections)
        section.Deserialize(src);
    }

    uint32_t GetNumRoads() const { return m_numRoads; }
    Joint::Id GetNumJoints() const { return m_numJoints; }
    uint32_t GetNumSections() const { return base::asserted_cast<uint32_t>(m_sections.size()); }

    Section const & GetSection(size_t index) const
    {
      CHECK_LESS(index, m_sections.size(), ());
      return m_sections[index];
    }

    void AddSection(Section const & section) { m_sections.push_back(section); }

  private:
    uint8_t m_version = kLastVersion;
    uint32_t m_numRoads = 0;
    Joint::Id m_numJoints = 0;
    std::vector<Section> m_sections;
  };

  class JointIdEncoder final
  {
  public:
    template <class Sink>
    void Write(Joint::Id originalId, BitWriter<Sink> & writer)
    {
      auto const & it = m_convertedIds.find(originalId);
      if (it != m_convertedIds.end())
      {
        Joint::Id const convertedId = it->second;
        CHECK_LESS(convertedId, m_count,
                   ("Duplicate joint id should be less than count, convertedId =", convertedId,
                    ", count =", m_count, ", originalId =", originalId));
        writer.Write(kRepeatJointIdBit, 1);
        // m_count - convertedId is less than convertedId in general.
        // So write this delta to save file space.
        WriteDelta(writer, m_count - convertedId);
        return;
      }

      m_convertedIds[originalId] = m_count;
      writer.Write(kNewJointIdBit, 1);
      ++m_count;
    }

    Joint::Id GetCount() const { return m_count; }

  private:
    Joint::Id m_count = 0;
    std::unordered_map<Joint::Id, Joint::Id> m_convertedIds;
  };

  class JointIdDecoder final
  {
  public:
    // Joint::Id count is incrementing along entire file.
    // But deserializer skips some sections, therefore we should recover counter at each section
    // start.
    JointIdDecoder(Joint::Id startId) : m_count(startId) {}

    template <class Source>
    Joint::Id Read(BitReader<Source> & reader)
    {
      uint8_t const bit = reader.Read(1);
      if (bit == kRepeatJointIdBit)
      {
        auto const delta = ReadDelta<uint32_t>(reader);
        if (delta > m_count)
          MYTHROW(CorruptedDataException, ("Joint id delta", delta, "> count =", m_count));

        return m_count - delta;
      }

      return m_count++;
    }

    Joint::Id GetCount() const { return m_count; }

  private:
    Joint::Id m_count;
  };

  // JointsFilter has two goals:
  //
  // 1. Deserialization skips some sections.
  //    Therefore one skips some joint ids from a continuous series of numbers [0, numJoints).
  //    JointsFilter converts loaded joint ids to a new continuous series of numbers [0,
  //    numLoadedJoints).
  //
  // 2. Some joints connect roads from different models.
  //    E.g. 2 roads joint: all vehicles road + pedestrian road.
  //    If one loads car roads only, pedestrian roads should be skipped,
  //    so joint would have only point from all vehicles road.
  //    JointsFilter filters such redundant joints.
  class JointsFilter final
  {
  public:
    JointsFilter(IndexGraph & graph, Joint::Id numJoints) : m_graph(graph)
    {
      m_entries.assign(numJoints, {kEmptyEntry, {0}});
    }

    void Push(Joint::Id jointIdInFile, RoadPoint const & rp);
    Joint::Id GetCount() const { return m_count; }

  private:
    static auto constexpr kEmptyEntry = std::numeric_limits<uint32_t>::max();
    static auto constexpr kPushedEntry = std::numeric_limits<uint32_t>::max() - 1;

    // Joints number is large.
    // Therefore point id and joint id are stored in same space to save some memory.
    union Point {
      uint32_t pointId;
      Joint::Id jointId;
    };

    static_assert(std::is_integral<Joint::Id>::value, "Joint::Id should be integral type");

    IndexGraph & m_graph;
    Joint::Id m_count = 0;
    std::vector<std::pair<uint32_t, Point>> m_entries;
  };

  class SectionSerializer final
  {
  public:
    explicit SectionSerializer(VehicleMask mask) : m_mask(mask) {}

    size_t GetBufferSize() const { return m_buffer.size(); }
    VehicleMask GetMask() const { return m_mask; }
    size_t GetNumRoads() const { return m_featureIds.size(); }

    void AddRoad(uint32_t featureId) { m_featureIds.push_back(featureId); }
    void SortRoads() { sort(m_featureIds.begin(), m_featureIds.end()); }
    void PreSerialize(IndexGraph const & graph, std::unordered_map<uint32_t, VehicleMask> const & masks,
                      JointIdEncoder & jointEncoder);

    template <class Sink>
    void Flush(Sink & sink)
    {
      sink.Write(m_buffer.data(), m_buffer.size());
      m_buffer.clear();
    }

  private:
    VehicleMask const m_mask;
    std::vector<uint32_t> m_featureIds;
    std::vector<uint8_t> m_buffer;
  };

  static VehicleMask GetRoadMask(std::unordered_map<uint32_t, VehicleMask> const & masks,
                                 uint32_t featureId);
  static uint32_t ConvertJointsNumber(uint32_t jointsNumber);
  static void PrepareSectionSerializers(IndexGraph const & graph,
                                        std::unordered_map<uint32_t, VehicleMask> const & masks,
                                        std::vector<SectionSerializer> & sections);
};
}  // namespace routing
