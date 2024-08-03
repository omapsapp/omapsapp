#pragma once

#include "indexer/metadata_serdes.hpp"

#include "coding/reader.hpp"
#include "coding/string_utf8_multilang.hpp"

#include "base/stl_helpers.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace feature
{
class MetadataBase
{
public:
  bool Has(uint8_t type) const
  {
    auto const it = m_metadata.find(type);
    return it != m_metadata.end();
  }

  std::string_view Get(uint8_t type) const
  {
    std::string_view sv;
    auto const it = m_metadata.find(type);
    if (it != m_metadata.end())
    {
      sv = it->second;
      ASSERT(!sv.empty(), ());
    }
    return sv;
  }

  inline bool Empty() const { return m_metadata.empty(); }
  inline size_t Size() const { return m_metadata.size(); }

  template <class Sink>
  void SerializeForMwmTmp(Sink & sink) const
  {
    auto const sz = static_cast<uint32_t>(m_metadata.size());
    WriteVarUint(sink, sz);
    for (auto const & it : m_metadata)
    {
      WriteVarUint(sink, static_cast<uint32_t>(it.first));
      utils::WriteString(sink, it.second);
    }
  }

  template <class Source>
  void DeserializeFromMwmTmp(Source & src)
  {
    auto const sz = ReadVarUint<uint32_t>(src);
    for (size_t i = 0; i < sz; ++i)
    {
      auto const key = ReadVarUint<uint32_t>(src);
      utils::ReadString(src, m_metadata[key]);
    }
  }

  inline bool Equals(MetadataBase const & other) const
  {
    return m_metadata == other.m_metadata;
  }

  void Clear() { m_metadata.clear(); }

protected:
  friend bool indexer::MetadataDeserializer::Get(uint32_t id, MetadataBase & meta);

  std::string_view Set(uint8_t type, std::string value)
  {
    std::string_view sv;

    if (value.empty())
      m_metadata.erase(type);
    else
    {
      auto & res = m_metadata[type];
      res = std::move(value);
      sv = res;
    }

    return sv;
  }

  /// @todo Change uint8_t to appropriate type when FMD_COUNT reaches 256.
  std::map<uint8_t, std::string> m_metadata;
};

class Metadata : public MetadataBase
{
public:
  /// @note! Do not change values here.
  /// Add new types to the end of list, before FMD_COUNT.
  /// Add new types to the corresponding list in android/.../Metadata.java.
  /// For types parsed from OSM get corresponding OSM tag to MetadataTagProcessor::TypeFromString().
  enum EType : int8_t
  {
    // Used as id only, because cuisines are defined by classifier types now. Will be active in future.
    FMD_CUISINE = 1,
    FMD_OPEN_HOURS = 2,
    FMD_PHONE_NUMBER = 3,
    FMD_FAX_NUMBER = 4,
    FMD_STARS = 5,
    FMD_OPERATOR = 6,
    //FMD_URL = 7,        // Deprecated, use FMD_WEBSITE
    FMD_WEBSITE = 8,
    /// @todo We have meta and classifier type at the same type. It's ok now for search, but should be revised in future.
    FMD_INTERNET = 9,
    FMD_ELE = 10,
    FMD_TURN_LANES = 11,
    FMD_TURN_LANES_FORWARD = 12,
    FMD_TURN_LANES_BACKWARD = 13,
    FMD_EMAIL = 14,
    FMD_POSTCODE = 15,
    FMD_WIKIPEDIA = 16,
    FMD_DESCRIPTION = 17,
    FMD_FLATS = 18,
    FMD_HEIGHT = 19,
    FMD_MIN_HEIGHT = 20,
    FMD_DENOMINATION = 21,
    FMD_BUILDING_LEVELS = 22,
    FMD_TEST_ID = 23,
    FMD_CUSTOM_IDS = 24,
    FMD_PRICE_RATES = 25,
    FMD_RATINGS = 26,
    FMD_EXTERNAL_URI = 27,
    FMD_LEVEL = 28,
    FMD_AIRPORT_IATA = 29,
    FMD_BRAND = 30,
    // Duration of routes by ferries and other rare means of transportation.
    // The number of ferries having the duration key in OSM is low so we
    // store the parsed tag value in Metadata instead of building a separate section for it.
    // See https://wiki.openstreetmap.org/wiki/Key:duration
    FMD_DURATION = 31,
    FMD_CONTACT_FACEBOOK = 32,
    FMD_CONTACT_INSTAGRAM = 33,
    FMD_CONTACT_TWITTER = 34,
    FMD_CONTACT_VK = 35,
    FMD_CONTACT_LINE = 36,
    FMD_DESTINATION = 37,
    FMD_DESTINATION_REF = 38,
    FMD_JUNCTION_REF = 39,
    FMD_BUILDING_MIN_LEVEL = 40,
    FMD_WIKIMEDIA_COMMONS = 41,
    FMD_CAPACITY = 42,
    FMD_WHEELCHAIR = 43, // Value is runtime only, data is taken from the classificator types
    FMD_LOCAL_REF = 44,
    FMD_DRIVE_THROUGH = 45,
    FMD_WEBSITE_MENU = 46,
    FMD_CHECK_DATE = 47,
    FMD_COUNT
  };

  enum ESource : uint8_t { SRC_KAYAK = 0 };

  /// Used to normalize tags like "contact:phone", "phone" and "contact:mobile" to a common metadata enum value.
  static bool TypeFromString(std::string_view osmTagKey, EType & outType);

  template <class FnT> void ForEach(FnT && fn) const
  {
    for (auto const & e : m_metadata)
      fn(static_cast<Metadata::EType>(e.first), e.second);
  }

  bool Has(EType type) const { return MetadataBase::Has(static_cast<uint8_t>(type)); }
  std::string_view Get(EType type) const { return MetadataBase::Get(static_cast<uint8_t>(type)); }

  std::string_view Set(EType type, std::string value)
  {
    return MetadataBase::Set(static_cast<uint8_t>(type), std::move(value));
  }
  void Drop(EType type) { Set(type, std::string()); }

  static std::string ToWikiURL(std::string v);
  std::string GetWikiURL() const;
  static std::string ToWikimediaCommonsURL(std::string const & v);

  void ClearPOIAttribs();
};

class AddressData : public MetadataBase
{
public:
  enum class Type : uint8_t
  {
    Street, Place,
  };

  // Store single value only.
  void Set(Type type, std::string_view s)
  {
    Set(type, std::string(s));
  }
  void Set(Type type, std::string s)
  {
    if (!s.empty())
      MetadataBase::Set(base::Underlying(type), std::move(s));
  }

  void SetIfAbsent(Type type, std::string s)
  {
    uint8_t const ut = base::Underlying(type);
    if (!s.empty() && !Has(ut))
      MetadataBase::Set(ut, std::move(s));
  }

  std::string_view Get(Type type) const { return MetadataBase::Get(base::Underlying(type)); }
};

class RegionData : public MetadataBase
{
public:
  enum Type : int8_t
  {
    RD_LANGUAGES,        // list of written languages
    RD_DRIVING,          // left- or right-hand driving (letter 'l' or 'r')
    RD_TIMEZONE,         // UTC timezone offset, floating signed number of hours: -3, 4.5
    RD_ADDRESS_FORMAT,   // address format, re: mapzen
    RD_PHONE_FORMAT,     // list of strings in "+N NNN NN-NN-NN" format
    RD_POSTCODE_FORMAT,  // list of strings in "AAA ANN" format
    RD_PUBLIC_HOLIDAYS,  // fixed PH dates
    RD_ALLOW_HOUSENAMES, // 'y' if housenames are commonly used
    RD_LEAP_WEIGHT_SPEED // speed factor for leap weight computation
  };

  // Special values for month references in public holiday definitions.
  enum PHReference : int8_t
  {
    PH_EASTER = 20,
    PH_ORTHODOX_EASTER = 21,
    PH_VICTORIA_DAY = 22,
    PH_CANADA_DAY = 23
  };

  template <class Sink>
  void Serialize(Sink & sink) const
  {
    MetadataBase::SerializeForMwmTmp(sink);
  }

  template <class Source>
  void Deserialize(Source & src)
  {
    MetadataBase::DeserializeFromMwmTmp(src);
  }

  void Set(Type type, std::string const & s)
  {
    CHECK_NOT_EQUAL(type, Type::RD_LANGUAGES, ("Please use RegionData::SetLanguages method"));
    MetadataBase::Set(type, s);
  }

  void SetLanguages(std::vector<std::string> const & codes);
  void GetLanguages(std::vector<int8_t> & langs) const;
  bool HasLanguage(int8_t const lang) const;
  bool IsSingleLanguage(int8_t const lang) const;

  void AddPublicHoliday(int8_t month, int8_t offset);
  // No public holidays getters until we know what to do with these.

  void SetLeapWeightSpeed(double speedValue)
  {
    MetadataBase::Set(Type::RD_LEAP_WEIGHT_SPEED, std::to_string(speedValue));
  }

  /// @see EdgeEstimator::GetLeapWeightSpeed
//  double GetLeapWeightSpeed(double defaultValue) const
//  {
//    if (Has(Type::RD_LEAP_WEIGHT_SPEED))
//      return std::stod(Get(Type::RD_LEAP_WEIGHT_SPEED));
//    return defaultValue;
//  }
};

// Prints types in osm-friendly format.
std::string ToString(feature::Metadata::EType type);
inline std::string DebugPrint(feature::Metadata::EType type) { return ToString(type); }

std::string DebugPrint(feature::Metadata const & metadata);
std::string DebugPrint(feature::AddressData const & addressData);
}  // namespace feature
