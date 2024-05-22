#pragma once

#include "transit/experimental/transit_types_experimental.hpp"
#include "transit/transit_types.hpp"

#include "coding/geometry_coding.hpp"
#include "coding/point_coding.hpp"
#include "coding/read_write_utils.hpp"
#include "coding/reader.hpp"
#include "coding/varint.hpp"
#include "coding/write_to_sink.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/newtype.hpp"

#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "3party/opening_hours/opening_hours.hpp"

namespace routing
{
namespace transit
{
// Note. For the time being double in transit section is used only for saving weight of edges (in seconds).
// Let us assume that it takes less than 10^7 seconds (115 days) to get from one station to a neighboring one.
double constexpr kMinDoubleAtTransitSection = kInvalidWeight;
double constexpr kMaxDoubleAtTransitSection = 10000000.0;
uint8_t constexpr kDoubleBits = 32;

template <typename Sink>
class Serializer
{
public:
  explicit Serializer(Sink & sink) : m_sink(sink) {}

  template <typename T>
  std::enable_if_t<(std::is_integral<T>::value || std::is_enum<T>::value) &&
                   !std::is_same<T, uint32_t>::value && !std::is_same<T, uint64_t>::value &&
                   !std::is_same<T, int32_t>::value && !std::is_same<T, int64_t>::value>
  operator()(T const & t, char const * /* name */ = nullptr)
  {
    WriteToSink(m_sink, t);
  }

  template <typename T>
  std::enable_if_t<std::is_same<T, uint32_t>::value || std::is_same<T, uint64_t>::value> operator()(
      T t, char const * /* name */ = nullptr) const
  {
    WriteVarUint(m_sink, t);
  }

  template <typename T>
  std::enable_if_t<std::is_same<T, int32_t>::value || std::is_same<T, int64_t>::value> operator()(
      T t, char const * name = nullptr) const
  {
    UNUSED_VALUE(name);
    WriteVarInt(m_sink, t);
  }

  template <typename T>
  std::enable_if_t<std::is_same<T, double>::value || std::is_same<T, float>::value> operator()(
      T d, char const * name = nullptr)
  {
    CHECK_GREATER_OR_EQUAL(d, kMinDoubleAtTransitSection, ());
    CHECK_LESS_OR_EQUAL(d, kMaxDoubleAtTransitSection, ());
    (*this)(DoubleToUint32(d, kMinDoubleAtTransitSection, kMaxDoubleAtTransitSection, kDoubleBits), name);
  }

  void operator()(std::string const & s, char const * /* name */ = nullptr)
  {
    rw::Write(m_sink, s);
  }

  void operator()(m2::PointD const & p, char const * /* name */ = nullptr)
  {
    WriteVarInt(m_sink, PointToInt64Obsolete(p, kPointCoordBits));
  }

  void operator()(std::vector<m2::PointD> const & vs, char const * /* name */ = nullptr)
  {
    WriteVarUint(m_sink, static_cast<uint64_t>(vs.size()));
    m2::PointU lastEncodedPoint(0, 0);
    for (auto const & p : vs)
    {
      m2::PointU const pointU = PointDToPointU(p, kPointCoordBits);
      WriteVarUint(m_sink, coding::EncodePointDeltaAsUint(pointU, lastEncodedPoint));
      lastEncodedPoint = pointU;
    }
  }

  void operator()(Edge::WrappedEdgeId const & id, char const * /* name */ = nullptr)
  {
    CHECK_GREATER_OR_EQUAL(id.Get(), m_lastWrappedEdgeId.Get(), ());
    WriteVarUint(m_sink, static_cast<uint64_t>(id.Get() - m_lastWrappedEdgeId.Get()));
    m_lastWrappedEdgeId = id;
  }

  void operator()(Stop::WrappedStopId const & id, char const * /* name */ = nullptr)
  {
    CHECK_GREATER_OR_EQUAL(id.Get(), m_lastWrappedStopId.Get(), ());
    WriteVarUint(m_sink, static_cast<uint64_t>(id.Get() - m_lastWrappedStopId.Get()));
    m_lastWrappedStopId = id;
  }

  void operator()(FeatureIdentifiers const & id, char const * name = nullptr)
  {
    if (id.IsSerializeFeatureIdOnly())
      (*this)(id.GetFeatureId(), name);
    else
      id.Visit(*this);
  }

  void operator()(::transit::experimental::IdBundle const & id, char const * name = nullptr)
  {
    if (id.SerializeFeatureIdOnly())
      (*this)(id.GetFeatureId(), name);
    else
      id.Visit(*this);
  }

  void operator()(osmoh::OpeningHours const & oh, char const * /* name */ = nullptr)
  {
    (*this)(ToString(oh));
  }

  void operator()(Edge const & e, char const * /* name */ = nullptr)
  {
    (*this)(e.m_stop1Id);
    (*this)(e.m_stop2Id);
    (*this)(e.m_weight);
    (*this)(e.m_lineId);
    // Note. |Edge::m_flags| is not filled fully after deserialization from json.
    // So it's necessary to fill it here.
    EdgeFlags const flags =
        GetEdgeFlags(e.GetTransfer(), e.GetStop1Id(), e.GetStop2Id(), e.GetShapeIds());
    (*this)(flags);

    if (flags.m_isShapeIdsEmpty || flags.m_isShapeIdsSame || flags.m_isShapeIdsReversed)
      return;

    if (flags.m_isShapeIdsSingle)
    {
      CHECK_EQUAL(e.GetShapeIds().size(), 1, ());
      (*this)(e.GetShapeIds()[0]);
      return;
    }

    (*this)(e.GetShapeIds());
  }

  void operator()(EdgeFlags const & f, char const * /* name */ = nullptr) { (*this)(f.GetFlags()); }

  template <typename T>
  void operator()(std::vector<T> const & vs, char const * /* name */ = nullptr)
  {
    WriteVarUint(m_sink, static_cast<uint64_t>(vs.size()));
    for (auto const & v : vs)
      (*this)(v);
  }

  template <class K, class V, class H>
  void operator()(std::unordered_map<K, V, H> const & container, char const * /* name */ = nullptr)
  {
    WriteVarUint(m_sink, static_cast<uint64_t>(container.size()));
    for (auto const & [key, val] : container)
    {
      (*this)(key);
      (*this)(val);
    }
  }

  template <class K, class V, class H>
  void operator()(std::map<K, V, H> const & container, char const * /* name */ = nullptr)
  {
    WriteVarUint(m_sink, static_cast<uint64_t>(container.size()));
    for (auto const & [key, val] : container)
    {
      (*this)(key);
      (*this)(val);
    }
  }

  template <typename T>
  std::enable_if_t<std::is_class<T>::value> operator()(T const & t,
                                                       char const * /* name */ = nullptr)
  {
    t.Visit(*this);
  }

private:
  Sink & m_sink;
  Edge::WrappedEdgeId m_lastWrappedEdgeId = {};
  Stop::WrappedStopId m_lastWrappedStopId = {};
};

template <typename Source>
class Deserializer
{
public:
  explicit Deserializer(Source & source) : m_source(source) {}

  template <typename T>
  std::enable_if_t<(std::is_integral<T>::value || std::is_enum<T>::value) &&
                   !std::is_same<T, uint32_t>::value && !std::is_same<T, uint64_t>::value &&
                   !std::is_same<T, int32_t>::value && !std::is_same<T, int64_t>::value>
  operator()(T & t, char const * /* name */ = nullptr)
  {
    ReadPrimitiveFromSource(m_source, t);
  }

  template <typename T>
  std::enable_if_t<std::is_same<T, uint32_t>::value || std::is_same<T, uint64_t>::value> operator()(
      T & t, char const * /* name */ = nullptr)
  {
    t = ReadVarUint<T>(m_source);
  }

  template <typename T>
  std::enable_if_t<std::is_same<T, int32_t>::value || std::is_same<T, int64_t>::value> operator()(
      T & t, char const * /* name */ = nullptr)
  {
    t = ReadVarInt<T>(m_source);
  }

  template <typename T>
  std::enable_if_t<std::is_same<T, double>::value || std::is_same<T, float>::value> operator()(
      T & d, char const * name = nullptr)
  {
    uint32_t ui;
    (*this)(ui, name);
    d = Uint32ToDouble(ui, kMinDoubleAtTransitSection, kMaxDoubleAtTransitSection, kDoubleBits);
  }

  void operator()(std::string & s, char const * /* name */ = nullptr)
  {
    rw::Read(m_source, s);
  }

  void operator()(m2::PointD & p, char const * /* name */ = nullptr)
  {
    p = Int64ToPointObsolete(ReadVarInt<int64_t>(m_source), kPointCoordBits);
  }

  void operator()(Edge::WrappedEdgeId & id, char const * /* name */ = nullptr)
  {
    id = m_lastWrappedEdgeId + Edge::WrappedEdgeId(ReadVarUint<uint64_t>(m_source));
    m_lastWrappedEdgeId = id;
  }

  void operator()(Stop::WrappedStopId & id, char const * /* name */ = nullptr)
  {
    id = m_lastWrappedStopId + Stop::WrappedStopId(ReadVarUint<uint64_t>(m_source));
    m_lastWrappedStopId = id;
  }

  void operator()(FeatureIdentifiers & id, char const * name = nullptr)
  {
    if (id.IsSerializeFeatureIdOnly())
    {
      FeatureId featureId;
      operator()(featureId, name);
      id.SetOsmId(kInvalidOsmId);
      id.SetFeatureId(featureId);
      return;
    }

    id.Visit(*this);
  }

  void operator()(::transit::experimental::IdBundle & idBundle, char const * name = nullptr)
  {
    if (idBundle.SerializeFeatureIdOnly())
    {
      FeatureId featureId;
      operator()(featureId, name);
      idBundle.SetFeatureId(featureId);
      idBundle.SetOsmId(kInvalidOsmId);
      return;
    }

    idBundle.Visit(*this);
  }

  void operator()(Edge & e, char const * /* name */ = nullptr)
  {
    (*this)(e.m_stop1Id);
    (*this)(e.m_stop2Id);
    (*this)(e.m_weight);
    (*this)(e.m_lineId);
    (*this)(e.m_flags);

    e.m_shapeIds.clear();
    if (e.m_flags.m_isShapeIdsEmpty)
      return;

    if (e.m_flags.m_isShapeIdsSame)
    {
      e.m_shapeIds.emplace_back(e.GetStop1Id(), e.GetStop2Id());
      return;
    }

    if (e.m_flags.m_isShapeIdsReversed)
    {
      e.m_shapeIds.emplace_back(e.GetStop2Id(), e.GetStop1Id());
      return;
    }

    if (e.m_flags.m_isShapeIdsSingle)
    {
      e.m_shapeIds.resize(1 /* single shape id */);
      (*this)(e.m_shapeIds.back());
      return;
    }

    (*this)(e.m_shapeIds);
  }

  void operator()(EdgeFlags & f, char const * /* name */ = nullptr)
  {
    uint8_t flags = 0;
    (*this)(flags);
    f.SetFlags(flags);
  }

  void operator()(std::vector<m2::PointD> & vs, char const * /* name */ = nullptr)
  {
    auto const size = ReadVarUint<uint64_t>(m_source);
    m2::PointU lastDecodedPoint(0, 0);
    vs.resize(size);
    for (auto & p : vs)
    {
      m2::PointU const pointU = coding::DecodePointDeltaFromUint(
          ReadVarUint<uint64_t>(m_source), lastDecodedPoint);
      p = PointUToPointD(pointU, kPointCoordBits);
      lastDecodedPoint = pointU;
    }
  }

  template <typename T>
  void operator()(std::vector<T> & vs, char const * /* name */ = nullptr)
  {
    auto const size = ReadVarUint<uint64_t>(m_source);
    vs.resize(size);
    for (auto & v : vs)
      (*this)(v);
  }

  template <class K, class V, class H>
  void operator()(std::unordered_map<K, V, H> & container, char const * /* name */ = nullptr)
  {
    auto const size = static_cast<size_t>(ReadVarUint<uint64_t>(m_source));
    for (size_t i = 0; i < size; ++i)
    {
      K key;
      V val;

      (*this)(key);
      (*this)(val);

      container.emplace(key, val);
    }
  }

  template <class K, class V, class H>
  void operator()(std::map<K, V, H> & container, char const * /* name */ = nullptr)
  {
    auto const size = static_cast<size_t>(ReadVarUint<uint64_t>(m_source));
    for (size_t i = 0; i < size; ++i)
    {
      K key;
      V val;

      (*this)(key);
      (*this)(val);

      container.emplace(key, val);
    }
  }

  void operator()(osmoh::OpeningHours & oh, char const * /* name */ = nullptr)
  {
    std::string ohStr;
    (*this)(ohStr);
    oh = osmoh::OpeningHours(ohStr);
  }

  template <typename T>
  std::enable_if_t<std::is_class<T>::value> operator()(T & t, char const * /* name */ = nullptr)
  {
    t.Visit(*this);
  }

private:
  Source & m_source;
  Edge::WrappedEdgeId m_lastWrappedEdgeId = {};
  Stop::WrappedStopId m_lastWrappedStopId = {};
};

template <typename Sink>
class FixedSizeSerializer
{
public:
  explicit FixedSizeSerializer(Sink & sink) : m_sink(sink) {}

  template <typename T>
  std::enable_if_t<std::is_integral<T>::value || std::is_enum<T>::value, void> operator()(
      T const & t, char const * /* name */ = nullptr)
  {
    WriteToSink(m_sink, t);
  }

  void operator()(TransitHeader const & header) { header.Visit(*this); }

  void operator()(::transit::experimental::TransitHeader const & headerExperimental)
  {
    headerExperimental.Visit(*this);
  }

private:
  Sink & m_sink;
};

template <typename Source>
class FixedSizeDeserializer
{
public:
  explicit FixedSizeDeserializer(Source & source) : m_source(source) {}

  template <typename T>
  std::enable_if_t<std::is_integral<T>::value || std::is_enum<T>::value, void> operator()(
      T & t, char const * /* name */ = nullptr)
  {
    ReadPrimitiveFromSource(m_source, t);
  }

  void operator()(TransitHeader & header) { header.Visit(*this); }

  void operator()(::transit::experimental::TransitHeader & headerExperimental)
  {
    headerExperimental.Visit(*this);
  }

private:
  Source & m_source;
};
}  // namespace transit
}  // namespace routing
