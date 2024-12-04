#pragma once

#include "indexer/feature_decl.hpp"
#include "indexer/feature_meta.hpp"

#include "coding/reader.hpp"
#include "coding/string_utf8_multilang.hpp"
#include "coding/value_opt_string.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

struct FeatureParamsBase;
class FeatureType;

namespace feature
{
  enum HeaderMask
  {
    HEADER_MASK_TYPE = 7U,
    HEADER_MASK_HAS_NAME = 1U << 3,
    HEADER_MASK_HAS_LAYER = 1U << 4,
    HEADER_MASK_GEOMTYPE = 3U << 5,
    HEADER_MASK_HAS_ADDINFO = 1U << 7
  };

  enum class HeaderGeomType : uint8_t
  {
    /// Coding geometry feature type in 2 bits.
    Point = 0,         /// point feature (addinfo = rank)
    Line = 1U << 5,    /// linear feature (addinfo = ref)
    Area = 1U << 6,    /// area feature (addinfo = house)
    PointEx = 3U << 5  /// point feature (addinfo = house)
  };

  static constexpr int kMaxTypesCount = HEADER_MASK_TYPE + 1; // 8, because there should be no features with 0 types

  enum Layer : int8_t
  {
    LAYER_LOW = -10,
    LAYER_EMPTY = 0,
    LAYER_HIGH = 10
  };

  class TypesHolder
  {
  public:
    using Types = std::array<uint32_t, kMaxTypesCount>;

    TypesHolder() = default;
    explicit TypesHolder(GeomType geomType) : m_geomType(geomType) {}
    explicit TypesHolder(FeatureType & f);

    void Assign(uint32_t type)
    {
      m_types[0] = type;
      m_size = 1;
    }

    template <class IterT> void Assign(IterT beg, IterT end)
    {
      m_size = std::distance(beg, end);
      CHECK_LESS_OR_EQUAL(m_size, kMaxTypesCount, ());
      std::copy(beg, end, m_types.begin());
    }

    void Add(uint32_t type)
    {
      CHECK_LESS(m_size, kMaxTypesCount, ());
      m_types[m_size++] = type;
    }

    void SafeAdd(uint32_t type)
    {
      if (!Has(type))
      {
        if (m_size < kMaxTypesCount)
          Add(type);
        else
          LOG(LWARNING, ("Type could not be added, MaxTypesCount exceeded"));
      }
    }

    GeomType GetGeomType() const { return m_geomType; }

    size_t Size() const { return m_size; }
    bool Empty() const { return (m_size == 0); }

    uint32_t front() const
    {
      ASSERT(m_size > 0, ());
      return m_types[0];
    }
    auto begin() const { return m_types.cbegin(); }
    auto end() const { return m_types.cbegin() + m_size; }
    auto begin() { return m_types.begin(); }
    auto end() { return m_types.begin() + m_size; }

    /// Assume that m_types is already sorted by SortBySpec function.
    uint32_t GetBestType() const
    {
      // 0 - is an empty type.
      return (m_size > 0 ? m_types[0] : 0);
    }

    bool Has(uint32_t type) const { return base::IsExist(*this, type); }

    // More _natural_ way of checking than Has, including subclass types hierarchy.
    // "railway-station-subway" holder returns true for "railway-station" input.
    bool HasWithSubclass(uint32_t type) const;

    template <typename Fn>
    bool RemoveIf(Fn && fn)
    {
      size_t const oldSize = m_size;

      auto const e = std::remove_if(begin(), end(), std::forward<Fn>(fn));
      m_size = std::distance(begin(), e);

      return (m_size != oldSize);
    }

    void Remove(uint32_t type);

    /// Used in tests only to check UselessTypesChecker
    /// (which is used in the generator to discard least important types if max types count is exceeded).
    void SortByUseless();

    /// Sort types by it's specification (more detailed type goes first). Should be used in client app.
    void SortBySpec();

    bool Equals(TypesHolder const & other) const;

    std::vector<std::string> ToObjectNames() const;

  private:
    Types m_types = {};
    size_t m_size = 0;

    GeomType m_geomType = GeomType::Undefined;
  };

  std::string DebugPrint(TypesHolder const & holder);

  uint8_t CalculateHeader(size_t const typesCount, HeaderGeomType const headerGeomType,
                          FeatureParamsBase const & params);
}  // namespace feature

struct FeatureParamsBase
{
  StringUtf8Multilang name;
  StringNumericOptimal house;
  std::string ref;
  int8_t layer;
  uint8_t rank;

  FeatureParamsBase() : layer(feature::LAYER_EMPTY), rank(0) {}

  void MakeZero();
  bool SetDefaultNameIfEmpty(std::string const & s);

  bool operator == (FeatureParamsBase const & rhs) const;

  bool IsValid() const;
  std::string DebugString() const;

  /// @return true if feature doesn't have any drawable strings (names, houses, etc).
  bool IsEmptyNames() const;

  template <class Sink>
  void Write(Sink & sink, uint8_t header) const
  {
    using namespace feature;

    if (header & HEADER_MASK_HAS_NAME)
      name.Write(sink);

    if (header & HEADER_MASK_HAS_LAYER)
      WriteToSink(sink, layer);

    if (header & HEADER_MASK_HAS_ADDINFO)
    {
      auto const headerGeomType = static_cast<HeaderGeomType>(header & HEADER_MASK_GEOMTYPE);
      switch (headerGeomType)
      {
      case HeaderGeomType::Point:
        WriteToSink(sink, rank);
        break;
      case HeaderGeomType::Line:
        utils::WriteString(sink, ref);
        break;
      case HeaderGeomType::Area:
      case HeaderGeomType::PointEx:
        house.Write(sink);
        break;
      }
    }
  }

  template <class TSrc>
  void Read(TSrc & src, uint8_t header)
  {
    using namespace feature;

    if (header & HEADER_MASK_HAS_NAME)
      name.Read(src);

    if (header & HEADER_MASK_HAS_LAYER)
      layer = ReadPrimitiveFromSource<int8_t>(src);

    if (header & HEADER_MASK_HAS_ADDINFO)
    {
      auto const headerGeomType = static_cast<HeaderGeomType>(header & HEADER_MASK_GEOMTYPE);
      switch (headerGeomType)
      {
      case HeaderGeomType::Point:
        rank = ReadPrimitiveFromSource<uint8_t>(src);
        break;
      case HeaderGeomType::Line:
        utils::ReadString(src, ref);
        break;
      case HeaderGeomType::Area:
      case HeaderGeomType::PointEx:
        house.Read(src);
        break;
      }
    }
  }
};

class FeatureParams : public FeatureParamsBase
{
  static char const * kHNLogTag;

public:
  using Types = std::vector<uint32_t>;

  void ClearName();

  bool AddName(std::string_view lang, std::string_view s);

  static bool LooksLikeHouseNumber(std::string const & hn);
  void SetHouseNumberAndHouseName(std::string houseNumber, std::string houseName);
  bool AddHouseNumber(std::string houseNumber);

  void SetGeomType(feature::GeomType t);
  void SetGeomTypePointEx();
  feature::GeomType GetGeomType() const;

  void AddType(uint32_t t) { m_types.push_back(t); }

  /// Special function to replace a regular railway station type with
  /// the special subway type for the correspondent city.
  void SetRwSubwayType(char const * cityName);

  enum TypesResult { TYPES_GOOD, TYPES_EMPTY, TYPES_EXCEED_MAX };
  TypesResult FinishAddingTypesEx();
  bool FinishAddingTypes() { return FinishAddingTypesEx() != TYPES_EMPTY; }

  // For logging purpose.
  std::string PrintTypes();

  void SetType(uint32_t t);
  bool PopAnyType(uint32_t & t);
  bool PopExactType(uint32_t t);
  bool IsTypeExist(uint32_t t) const;
  bool IsTypeExist(uint32_t comp, uint8_t level) const;

  /// @return Type that matches |comp| with |level| in classificator hierarchy.
  ///   ftype::GetEmptyValue() if type not found.
  uint32_t FindType(uint32_t comp, uint8_t level) const;

  bool IsValid() const;

  uint8_t GetHeader() const;

  template <class Sink>
  void Write(Sink & sink) const
  {
    uint8_t const header = GetHeader();
    WriteToSink(sink, header);

    for (size_t i = 0; i < m_types.size(); ++i)
      WriteVarUint(sink, GetIndexForType(m_types[i]));

    Base::Write(sink, header);
  }

  template <class Source>
  void Read(Source & src)
  {
    using namespace feature;

    uint8_t const header = ReadPrimitiveFromSource<uint8_t>(src);
    m_geomType = static_cast<HeaderGeomType>(header & HEADER_MASK_GEOMTYPE);

    size_t const count = (header & HEADER_MASK_TYPE) + 1;
    for (size_t i = 0; i < count; ++i)
      m_types.push_back(GetTypeForIndex(ReadVarUint<uint32_t>(src)));

    Base::Read(src, header);
  }

  /// @todo Make protected and update EditableMapObject code.
  Types m_types;

  friend std::string DebugPrint(FeatureParams const & p);

private:
  using Base = FeatureParamsBase;

  feature::HeaderGeomType GetHeaderGeomType() const;
  static uint32_t GetIndexForType(uint32_t t);
  static uint32_t GetTypeForIndex(uint32_t i);

  std::optional<feature::HeaderGeomType> m_geomType;
};

/// @todo Take out into generator library.
class FeatureBuilderParams : public FeatureParams
{
public:
  /// Assign parameters except geometry type.
  void SetParams(FeatureBuilderParams const & rhs)
  {
    FeatureParamsBase::operator=(rhs);

    m_types = rhs.m_types;
    m_addrTags = rhs.m_addrTags;
    m_metadata = rhs.m_metadata;
    m_reversedGeometry = rhs.m_reversedGeometry;
  }

  void MakeZero()
  {
    m_metadata.Clear();
    m_addrTags.Clear();
    FeatureParams::MakeZero();
  }

  /// @name Used to store address to temporary TEMP_ADDR_EXTENSION file.
  /// @{
  void SetAddress(feature::AddressData && addr) { m_addrTags = std::move(addr); }

  void SetStreet(std::string s);
  std::string_view GetStreet() const;

  template <class TSink> void SerializeAddress(TSink & sink) const
  {
    m_addrTags.SerializeForMwmTmp(sink);
  }
  /// @}

  void SetPostcode(std::string s);
  std::string_view GetPostcode() const;

  feature::Metadata const & GetMetadata() const { return m_metadata; }
  feature::Metadata & GetMetadata() { return m_metadata; }

  template <class Sink>
  void Write(Sink & sink) const
  {
    FeatureParams::Write(sink);
    m_metadata.SerializeForMwmTmp(sink);
    m_addrTags.SerializeForMwmTmp(sink);
  }

  template <class Source>
  void Read(Source & src)
  {
    FeatureParams::Read(src);
    m_metadata.DeserializeFromMwmTmp(src);
    m_addrTags.DeserializeFromMwmTmp(src);
  }

  bool GetReversedGeometry() const { return m_reversedGeometry; }
  void SetReversedGeometry(bool reversedGeometry) { m_reversedGeometry = reversedGeometry; }

  /// @return true If any inconsistency was found here.
  bool RemoveInconsistentTypes();

  void ClearPOIAttribs();

  friend std::string DebugPrint(FeatureBuilderParams const & p);

private:
  bool m_reversedGeometry = false;
  feature::Metadata m_metadata;
  feature::AddressData m_addrTags;
};
