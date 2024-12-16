#include "indexer/road_shields_parser.hpp"

#include "indexer/feature.hpp"
#include "indexer/ftypes_matcher.hpp"

#include "base/string_utils.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>
#include <utility>

/*
 * TODO : why all this parsing happens in the run-time? The most of it should be moved to the generator.
 * E.g. now the ref contains
 *    ee:national/8;e-road/E 67;ee:local/7841171
 * where the latter part is not being used at all.
 * The generator should produce just
 *    x8;yE 67
 * where x/y are one byte road shield type codes.
 */

namespace ftypes
{
namespace
{

uint32_t constexpr kMaxRoadShieldBytesSize = 8;

std::array<std::string, 2> const kFederalCode = {{"US", "FSR"}};

std::array<std::string, 61> const kStatesCode = {{
    "AL", "AK", "AZ", "AR", "CA", "CO", "CT", "DE", "DC", "FL", "GA", "HI", "ID", "IL", "IN",
    "IA", "KS", "KY", "LA", "ME", "MD", "MA", "MI", "MN", "MS", "MO", "MT", "NE", "NV", "NH",
    "NJ", "NM", "NY", "NC", "ND", "OH", "OK", "OR", "PA", "RI", "SC", "SD", "TN", "TX", "UT",
    "VT", "VA", "WA", "WV", "WI", "WY", "AS", "GU", "MP", "PR", "VI", "UM", "FM", "MH", "PW",

    "SR",   // common prefix for State Road
}};

std::array<std::string, 13> const kModifiers = {{"alt", "alternate", "bus", "business", "bypass",
                                                 "historic", "connector", "loop", "scenic", "spur",
                                                 "temporary", "toll", "truck"}};

// Shields based on a network tag in a route=road relation.
std::unordered_map<std::string, RoadShieldType> const kRoadNetworkShields = {
    // International road networks.
    {"e-road", RoadShieldType::Generic_Green},  // E 105
    {"asianhighway", RoadShieldType::Hidden},   // AH8. Blue, but usually not posted.
    // National and regional networks for some countries.
    {"ru:national", RoadShieldType::Generic_Blue},
    {"ru:regional", RoadShieldType::Generic_Blue},
    {"bg:national", RoadShieldType::Generic_Green},
    {"bg:regional", RoadShieldType::Generic_Blue},
    {"by:national", RoadShieldType::Generic_Red},
    // https://github.com/organicmaps/organicmaps/issues/3083
    //{"by:regional", RoadShieldType::Generic_Red},
    {"co:national", RoadShieldType::Generic_White},
    {"cz:national", RoadShieldType::Generic_Red},
    {"cz:regional", RoadShieldType::Generic_Blue},
    // Estonia parser produces more specific shield types, incl. Generic_Orange.
    //{"ee:national", RoadShieldType::Generic_Red},
    //{"ee:regional", RoadShieldType::Generic_White},
    {"in:ne", RoadShieldType::Generic_Blue},
    {"in:nh", RoadShieldType::Generic_Orange},
    {"in:sh", RoadShieldType::Generic_Green},
    {"fr:a-road", RoadShieldType::Generic_Red},
    {"jp:national", RoadShieldType::Generic_Blue},
    {"jp:regional", RoadShieldType::Generic_Blue},
    {"jp:prefectural", RoadShieldType::Generic_Blue},
    {"lt:national", RoadShieldType::Generic_Red},
    {"lt:regional", RoadShieldType::Generic_Blue},
    {"lv:national", RoadShieldType::Generic_Red},
    {"lv:regional", RoadShieldType::Generic_Blue},
    {"pl:national", RoadShieldType::Generic_Red},
    {"pl:regional", RoadShieldType::Generic_Orange},
    {"pl:local", RoadShieldType::Generic_White},
    {"tr:motorway", RoadShieldType::Generic_Orange},
    {"tr:road", RoadShieldType::Generic_Blue},
    {"tr:provincial", RoadShieldType::Generic_White},
    {"ua:national", RoadShieldType::Generic_Blue},
    {"ua:regional", RoadShieldType::Generic_Blue},
    {"ua:territorial", RoadShieldType::Generic_White},
    {"ua:local", RoadShieldType::Generic_White},
    {"za:national", RoadShieldType::Generic_White},
    {"za:regional", RoadShieldType::Generic_White},
    {"my:federal", RoadShieldType::Generic_Orange},
    // United States road networks.
    {"us:i", RoadShieldType::US_Interstate},
    {"us:us", RoadShieldType::US_Highway},
    {"us:sr", RoadShieldType::US_Highway},
    {"us:fsr", RoadShieldType::US_Highway},
};

class RoadShieldParser
{
public:
  explicit RoadShieldParser(std::string const & baseRoadNumber) : m_baseRoadNumber(baseRoadNumber) {}
  virtual ~RoadShieldParser() = default;
  virtual RoadShield ParseRoadShield(std::string_view rawText) const = 0;

  RoadShieldType FindNetworkShield(std::string network) const
  {
    // Special processing for US state highways, to not duplicate the table.
    if (network.size() == 5 && network.starts_with("US:"))
    {
      if (base::IsExist(kStatesCode, network.substr(3)))
        return RoadShieldType::Generic_White;
    }

    // Minimum length for the network tag is 4 (US:I).
    if (network.size() >= 4)
    {
      strings::AsciiToLower(network);

      // Cut off suffixes after a semicolon repeatedly, until we find a relevant shield.
      auto semicolonPos = network.size();
      while (semicolonPos != std::string::npos)
      {
        network.resize(semicolonPos);  // cut off the ":xxx" suffix
        auto const it = kRoadNetworkShields.find(network);
        if (it != kRoadNetworkShields.cend())
          return it->second;
        semicolonPos = network.rfind(':');
      }
    }
    return RoadShieldType::Default;
  }

  RoadShieldsSetT GetRoadShields() const
  {
    RoadShieldsSetT result, defaultShields;

    strings::Tokenize(m_baseRoadNumber, ";", [&](std::string_view rawText)
    {
      RoadShield shield;
      auto slashPos = rawText.find('/');
      if (slashPos == std::string::npos)
      {
        shield = ParseRoadShield(rawText);
      }
      else
      {
        shield = ParseRoadShield(rawText.substr(slashPos + 1));
        // TODO: use a network-based shield type override only if a parser couldn't make it
        // more specific than country's default shield type.
        // E.g. "94" is set to Generic_Orange by Estonia parser, but then
        // is overriden by "ee:national" => Generic_Red.
        // (can't override just RoadShieldType::Default, as e.g. Russia parser uses Generic_Blue as country default).
        if (shield.m_type != RoadShieldType::Hidden)
        {
          RoadShieldType const networkType = FindNetworkShield(std::string(rawText.substr(0, slashPos)));
          if (networkType != RoadShieldType::Default)
            shield.m_type = networkType;
        }
      }
      if (!shield.m_name.empty() && shield.m_type != RoadShieldType::Hidden)
      {
        if (shield.m_type != RoadShieldType::Default)
        {
          // Schedule deletion of a shield with the same text and default style, if present.
          defaultShields.emplace_back(RoadShieldType::Default, shield.m_name, shield.m_additionalText);
        }
        result.push_back(std::move(shield));
      }
    });

    result.erase_if([&defaultShields](RoadShield const & shield)
    {
      return std::find(defaultShields.begin(), defaultShields.end(), shield) != defaultShields.end();
    });

    return result;
  }

protected:
  std::string const m_baseRoadNumber;
};

class USRoadShieldParser : public RoadShieldParser
{
public:
  explicit USRoadShieldParser(std::string const & baseRoadNumber) : RoadShieldParser(baseRoadNumber) {}
  RoadShield ParseRoadShield(std::string_view rawText) const override
  {
    std::string shieldText(rawText);

    std::replace(shieldText.begin(), shieldText.end(), '-', ' ');
    auto const shieldParts = strings::Tokenize(shieldText, " ");

    // Process long road shield titles to skip invalid data.
    if (shieldText.size() > kMaxRoadShieldBytesSize)
    {
      std::string lowerShieldText = shieldText;
      strings::AsciiToLower(lowerShieldText);

      bool modifierFound = false;
      for (auto const & modifier : kModifiers)
      {
        if (lowerShieldText.find(modifier) != std::string::npos)
        {
          modifierFound = true;
          break;
        }
      }
      if (!modifierFound)
        return RoadShield();
    }

    if (shieldParts.size() <= 1)
      return RoadShield(RoadShieldType::Default, rawText);

    std::string_view const roadType = shieldParts[0];  // 'I' for interstates and kFederalCode/kStatesCode for highways.
    std::string roadNumber(shieldParts[1]);
    std::string additionalInfo;
    if (shieldParts.size() >= 3)
    {
      additionalInfo = shieldParts[2];
      // Process cases like "US Loop 16".
      if (!strings::IsASCIINumeric(shieldParts[1]) && strings::IsASCIINumeric(shieldParts[2]))
      {
        roadNumber = shieldParts[2];
        additionalInfo = shieldParts[1];
      }
    }

    if (roadType == "I")
      return RoadShield(RoadShieldType::US_Interstate, roadNumber, additionalInfo);

    if (base::IsExist(kFederalCode, shieldParts[0]))
      return RoadShield(RoadShieldType::US_Highway, roadNumber, additionalInfo);

    if (base::IsExist(kStatesCode, shieldParts[0]))
      return RoadShield(RoadShieldType::Generic_White, roadNumber, additionalInfo);

    return RoadShield(RoadShieldType::Default, rawText);
  }
};

class IndiaRoadShieldParser : public RoadShieldParser
{
public:
  explicit IndiaRoadShieldParser(std::string const & baseRoadNumber) : RoadShieldParser(baseRoadNumber) {}
  RoadShield ParseRoadShield(std::string_view rawText) const override
  {
    std::string shieldText(rawText);

    std::erase_if(shieldText, [](char c) { return c == '-' || ::isspace(c); });

    if (shieldText.size() <= 2)
      return RoadShield(RoadShieldType::Default, rawText);

    std::string_view roadType = std::string_view(shieldText).substr(0, 2);
    std::string_view roadNumber = std::string_view(shieldText).substr(2);

    if (roadType == "NE")
      return RoadShield(RoadShieldType::Generic_Blue, roadNumber);

    if (roadType == "NH")
      return RoadShield(RoadShieldType::Generic_Orange, roadNumber);

    if (roadType == "SH")
      return RoadShield(RoadShieldType::Generic_Green, roadNumber);

    return RoadShield(RoadShieldType::Default, rawText);
  }
};

class DefaultTypeRoadShieldParser : public RoadShieldParser
{
public:
  DefaultTypeRoadShieldParser(std::string const & baseRoadNumber,
                              RoadShieldType const & defaultType)
    : RoadShieldParser(baseRoadNumber), m_type(defaultType)
  {
  }

  RoadShield ParseRoadShield(std::string_view rawText) const override
  {
    if (rawText.size() > kMaxRoadShieldBytesSize)
      return RoadShield();

    return RoadShield(m_type, rawText);
  }

private:
  RoadShieldType const m_type;
};

// Matches by a list of given substrings.
// If several substrings are present, then the leftmost wins.
class SimpleRoadShieldParser : public RoadShieldParser
{
public:
  struct Entry
  {
    Entry() = default;
    Entry(std::string_view name, RoadShieldType type)
      : m_name(name), m_type(type)
    {
    }

    std::string_view m_name;
    RoadShieldType m_type = RoadShieldType::Default;
  };

  using ShieldTypes = buffer_vector<Entry, 8>;

  SimpleRoadShieldParser(std::string const & baseRoadNumber, ShieldTypes && types,
                         RoadShieldType defaultType = RoadShieldType::Default)
    : RoadShieldParser(baseRoadNumber), m_types(std::move(types)), m_defaultType(defaultType)
  {
  }

  RoadShield ParseRoadShield(std::string_view rawText) const override
  {
    if (rawText.size() > kMaxRoadShieldBytesSize)
      return RoadShield();

    size_t idx = std::numeric_limits<size_t>::max();
    RoadShieldType type = m_defaultType;
    for (auto const & p : m_types)
    {
      auto const i = rawText.find(p.m_name);
      if (i != std::string::npos && i < idx)
      {
        type = p.m_type;
        idx = i;
      }
    }

    return { type, rawText };
  }

private:
  ShieldTypes const m_types;
  RoadShieldType const m_defaultType;
};

uint16_t constexpr kAnyHigherRoadNumber = std::numeric_limits<uint16_t>::max();

// Matches by a list of numeric ranges (a first matching range is used).
// Non-numbers and numbers out of any range are matched to RoadShieldType::Default.
// Use kAnyHigherRoadNumber to match any number higher than a specified lower bound.
class NumericRoadShieldParser : public RoadShieldParser
{
public:
  struct Entry
  {
    Entry() = default;
    Entry(uint16_t low, uint16_t high, RoadShieldType type)
      : m_low(low), m_high(high), m_type(type)
    {
    }

    uint16_t m_low, m_high;
    RoadShieldType m_type = RoadShieldType::Default;
  };

  // A map of {lower_bound, higher_bound} -> RoadShieldType.
  using ShieldTypes = buffer_vector<Entry, 8>;

  NumericRoadShieldParser(std::string const & baseRoadNumber, ShieldTypes && types)
    : RoadShieldParser(baseRoadNumber), m_types(std::move(types))
  {
  }

  RoadShield ParseRoadShield(std::string_view rawText) const override
  {
    if (rawText.size() > kMaxRoadShieldBytesSize)
      return RoadShield();

    uint64_t ref;
    if (strings::to_uint(rawText, ref))
    {
      for (auto const & p : m_types)
      {
        if (p.m_low <= ref && (ref <= p.m_high || p.m_high == kAnyHigherRoadNumber))
          return RoadShield(p.m_type, rawText);
      }
    }

    return RoadShield(RoadShieldType::Default, rawText);
  }

private:
  ShieldTypes const m_types;
};

class SimpleUnicodeRoadShieldParser : public RoadShieldParser
{
public:
  struct Entry
  {
    Entry() = default;
    Entry(std::string_view simpleName, std::string_view unicodeName, RoadShieldType type)
      : m_simpleName(simpleName), m_unicodeName(unicodeName), m_type(type)
    {
      ASSERT_NOT_EQUAL(simpleName, unicodeName, ());
      ASSERT_LESS_OR_EQUAL(simpleName.size(), unicodeName.size(), ());
    }

    std::string_view m_simpleName;
    std::string_view m_unicodeName;
    RoadShieldType m_type = RoadShieldType::Default;
  };

  using ShieldTypes = buffer_vector<Entry, 8>;

  SimpleUnicodeRoadShieldParser(std::string const & baseRoadNumber, ShieldTypes && types,
                                RoadShieldType defaultType = RoadShieldType::Default)
    : RoadShieldParser(baseRoadNumber), m_types(std::move(types)), m_defaultType(defaultType)
  {
  }

  RoadShield ParseRoadShield(std::string_view rawText) const override
  {
    uint32_t constexpr kMaxRoadShieldSymbolsSize = 4 * kMaxRoadShieldBytesSize;

    if (rawText.size() > kMaxRoadShieldSymbolsSize)
      return RoadShield();

    for (auto const & p : m_types)
    {
      if (rawText.find(p.m_simpleName) != std::string::npos)
        return RoadShield(p.m_type, rawText);

      if (rawText.find(p.m_unicodeName) != std::string::npos)
        return RoadShield(p.m_type, rawText);
    }

    return RoadShield(m_defaultType, rawText);
  }

private:
  ShieldTypes const m_types;
  RoadShieldType const m_defaultType;
};

// Implementations of "ref" parses for some countries.

class RussiaRoadShieldParser : public DefaultTypeRoadShieldParser
{
public:
  explicit RussiaRoadShieldParser(std::string const & baseRoadNumber)
    : DefaultTypeRoadShieldParser(baseRoadNumber, RoadShieldType::Generic_Blue)
  {
  }
};

class SpainRoadShieldParser : public DefaultTypeRoadShieldParser
{
public:
  explicit SpainRoadShieldParser(std::string const & baseRoadNumber)
    : DefaultTypeRoadShieldParser(baseRoadNumber, RoadShieldType::Generic_Blue)
  {
  }
};

class UKRoadShieldParser : public SimpleRoadShieldParser
{
public:
  explicit UKRoadShieldParser(std::string const & baseRoadNumber)
    : SimpleRoadShieldParser(baseRoadNumber, {{"M", RoadShieldType::Generic_Blue},
                                              {"A", RoadShieldType::UK_Highway}})
  {
  }
};

class FranceRoadShieldParser : public SimpleRoadShieldParser
{
public:
  explicit FranceRoadShieldParser(std::string const & baseRoadNumber)
    : SimpleRoadShieldParser(baseRoadNumber, {{"A", RoadShieldType::Generic_Red},
                                              {"N", RoadShieldType::Generic_Red},
                                              {"E", RoadShieldType::Generic_Green},
                                              {"D", RoadShieldType::Generic_Orange},
                                              {"M", RoadShieldType::Generic_Blue}})
  {
  }
};

class GermanyRoadShieldParser : public SimpleRoadShieldParser
{
public:
  explicit GermanyRoadShieldParser(std::string const & baseRoadNumber)
    : SimpleRoadShieldParser(baseRoadNumber, {{"A", RoadShieldType::Generic_Blue},
                                              {"B", RoadShieldType::Generic_Orange},
                                              {"L", RoadShieldType::Generic_White},
                                              {"K", RoadShieldType::Generic_White}})
  {
  }
};

class UkraineRoadShieldParser : public SimpleUnicodeRoadShieldParser
{
public:
  // The second parameter in the constructor is a cyrillic symbol.
  explicit UkraineRoadShieldParser(std::string const & baseRoadNumber)
    : SimpleUnicodeRoadShieldParser(baseRoadNumber,
                                    {{"M", "М", RoadShieldType::Generic_Blue},
                                     {"H", "Н", RoadShieldType::Generic_Blue},
                                     {"P", "Р", RoadShieldType::Generic_Blue},
                                     {"E", "Е", RoadShieldType::Generic_Green}})
  {
  }
};

class BelarusRoadShieldParser : public SimpleUnicodeRoadShieldParser
{
public:
  // The second parameter in the constructor is a cyrillic symbol.
  explicit BelarusRoadShieldParser(std::string const & baseRoadNumber)
    : SimpleUnicodeRoadShieldParser(baseRoadNumber,
                                    {{"M", "М", RoadShieldType::Generic_Red},
                                     {"P", "Р", RoadShieldType::Generic_Red},
                                     {"E", "Е", RoadShieldType::Generic_Green}})
  {
  }
};

class LatviaRoadShieldParser : public SimpleRoadShieldParser
{
public:
  explicit LatviaRoadShieldParser(std::string const & baseRoadNumber)
    : SimpleRoadShieldParser(baseRoadNumber, {{"A", RoadShieldType::Generic_Red},
                                              {"E", RoadShieldType::Generic_Green},
                                              {"P", RoadShieldType::Generic_Blue}})
  {
  }
};

class NetherlandsRoadShieldParser : public SimpleRoadShieldParser
{
public:
  explicit NetherlandsRoadShieldParser(std::string const & baseRoadNumber)
    : SimpleRoadShieldParser(baseRoadNumber, {{"A", RoadShieldType::Generic_Red},
                                              {"E", RoadShieldType::Generic_Green},
                                              {"N", RoadShieldType::Generic_Orange}})
  {
  }
};

class FinlandRoadShieldParser : public NumericRoadShieldParser
{
public:
  explicit FinlandRoadShieldParser(std::string const & baseRoadNumber)
    : NumericRoadShieldParser(baseRoadNumber, {{1, 30, RoadShieldType::Generic_Red},
                                               {40, 99, RoadShieldType::Generic_Orange},
                                               {100, 999, RoadShieldType::Generic_White},
                                               {1000, 9999, RoadShieldType::Generic_Blue},
                                               {10000, kAnyHigherRoadNumber, RoadShieldType::Hidden}})
  {
  }
};

class EstoniaRoadShieldParser : public NumericRoadShieldParser
{
public:
  explicit EstoniaRoadShieldParser(std::string const & baseRoadNumber)
    : NumericRoadShieldParser(baseRoadNumber, {{1, 11, RoadShieldType::Generic_Red},
                                               {12, 91, RoadShieldType::Generic_Orange},
                                               {92, 92, RoadShieldType::Generic_Red},
                                               {93, 95, RoadShieldType::Generic_Orange},
                                               {96, 999, RoadShieldType::Generic_White},
                                               {1000, kAnyHigherRoadNumber, RoadShieldType::Hidden}})
  {
  }
};

class MalaysiaRoadShieldParser : public SimpleRoadShieldParser
{
public:
  explicit MalaysiaRoadShieldParser(std::string const & baseRoadNumber)
    : SimpleRoadShieldParser(baseRoadNumber, {{"AH", RoadShieldType::Generic_Blue},
                                              {"E", RoadShieldType::Generic_Blue}},
                                              RoadShieldType::Generic_Orange)
  {
  }
};

class CyprusRoadShieldParser : public SimpleRoadShieldParser
{
public:
  explicit CyprusRoadShieldParser(std::string const & baseRoadNumber)
    : SimpleRoadShieldParser(baseRoadNumber, {// North Cyprus.
                                              {"D.", RoadShieldType::Generic_Blue},   // White font.
                                              {"GM.", RoadShieldType::Generic_White}, // Blue font.
                                              {"GZ.", RoadShieldType::Generic_White}, // Blue font.
                                              {"GR.", RoadShieldType::Generic_White}, // Blue font.
                                              {"LF.", RoadShieldType::Generic_White}, // Blue font.
                                              {"İK.", RoadShieldType::Generic_White}, // Blue font.
                                              // South Cyprus.
                                              {"A", RoadShieldType::Generic_Green},   // Yellow font. Hexagon.
                                              {"B", RoadShieldType::Generic_Blue},    // Yellow font.
                                              {"E", RoadShieldType::Generic_Blue},    // Yellow font.
                                              {"F", RoadShieldType::Generic_Blue},    // Yellow font.
                                              {"U", RoadShieldType::Generic_Blue}})   // Yellow font.
  {
  }
};

class TurkeyRoadShieldParser : public SimpleRoadShieldParser
//TODO: Implement provincial road shields, formatted as (province number, between 01-81)-(two digit number)
{
public:
  explicit TurkeyRoadShieldParser(std::string const & baseRoadNumber)
    : SimpleRoadShieldParser(baseRoadNumber, {
                                              {"O-", RoadShieldType::Generic_Orange}, // Black font. Hexagon.
                                              {"D.", RoadShieldType::Generic_Blue}, // White font.
                                              {"D-", RoadShieldType::Generic_Blue}, // White font.
                                              {"D", RoadShieldType::Generic_Blue}, // White font.
                                              {"D ", RoadShieldType::Generic_Blue}}) // White font.
  {
  }
};

class MexicoRoadShieldParser : public RoadShieldParser
{
public:
  explicit MexicoRoadShieldParser(std::string const & baseRoadNumber)
    : RoadShieldParser(baseRoadNumber)
  {}

  RoadShield ParseRoadShield(std::string_view rawText) const override
  {
    std::string shieldText(rawText);

    std::replace(shieldText.begin(), shieldText.end(), '-', ' ');
    auto const shieldParts = strings::Tokenize(shieldText, " ");

    if (shieldText.size() > kMaxRoadShieldBytesSize)
      return {};

    if (shieldParts.size() <= 1)
      return RoadShield(RoadShieldType::Default, rawText);

    std::string roadNumber(shieldParts[1]);
    std::string additionalInfo;
    if (shieldParts.size() >= 3)
    {
      additionalInfo = shieldParts[2];
      if (!strings::IsASCIINumeric(shieldParts[1]) && strings::IsASCIINumeric(shieldParts[2]))
      {
        roadNumber = shieldParts[2];
        additionalInfo = shieldParts[1];
      }
    }

    // Remove possible leading zero.
    if (strings::IsASCIINumeric(roadNumber) && roadNumber[0] == '0')
      roadNumber.erase(0);

    if (shieldParts[0] == "MEX")
      return RoadShield(RoadShieldType::Default, roadNumber, additionalInfo);

    return RoadShield(RoadShieldType::Default, rawText);
  }
};
}  // namespace

RoadShieldsSetT GetRoadShields(FeatureType & f)
{
  auto const & ref = f.GetRef();
  if (ref.empty())
    return {};

  // Find out country name.
  std::string mwmName = f.GetID().GetMwmName();
  ASSERT(!mwmName.empty(), (f.GetID()));

  auto const underlinePos = mwmName.find('_');
  if (underlinePos != std::string::npos)
    mwmName = mwmName.substr(0, underlinePos);

  return GetRoadShields(mwmName, ref);
}

RoadShieldsSetT GetRoadShields(std::string const & mwmName, std::string const & roadNumber)
{
  if (mwmName == "US")
    return USRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "UK")
    return UKRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "India")
    return IndiaRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Russia")
    return RussiaRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "France")
    return FranceRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Germany")
    return GermanyRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Spain")
    return SpainRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Ukraine")
    return UkraineRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Belarus")
    return BelarusRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Latvia")
    return LatviaRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Netherlands")
    return NetherlandsRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Finland")
    return FinlandRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Estonia")
    return EstoniaRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Malaysia")
    return MalaysiaRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Mexico")
    return MexicoRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Cyprus")
    return CyprusRoadShieldParser(roadNumber).GetRoadShields();
  if (mwmName == "Turkey")
    return TurkeyRoadShieldParser(roadNumber).GetRoadShields();

  return SimpleRoadShieldParser(roadNumber, SimpleRoadShieldParser::ShieldTypes()).GetRoadShields();
}

RoadShieldsSetT GetRoadShields(std::string const & rawRoadNumber)
{
  if (rawRoadNumber.empty())
    return {};

  return SimpleRoadShieldParser(rawRoadNumber, SimpleRoadShieldParser::ShieldTypes()).GetRoadShields();
}

std::vector<std::string> GetRoadShieldsNames(FeatureType & ft)
{
  std::vector<std::string> names;
  auto const & ref = ft.GetRef();
  if (!ref.empty() && IsStreetOrSquareChecker::Instance()(ft))
  {
    for (auto && shield : GetRoadShields(ref))
      names.push_back(std::move(shield.m_name));
  }
  return names;
}

std::string DebugPrint(RoadShieldType shieldType)
{
  using ftypes::RoadShieldType;
  switch (shieldType)
  {
  case RoadShieldType::Default: return "default";
  case RoadShieldType::Generic_White: return "white";
  case RoadShieldType::Generic_Blue: return "blue";
  case RoadShieldType::Generic_Green: return "green";
  case RoadShieldType::Generic_Orange: return "orange";
  case RoadShieldType::Generic_Red: return "red";
  case RoadShieldType::US_Interstate: return "US interstate";
  case RoadShieldType::US_Highway: return "US highway";
  case RoadShieldType::UK_Highway: return "UK highway";
  case RoadShieldType::Hidden: return "hidden";
  case RoadShieldType::Count: CHECK(false, ("RoadShieldType::Count is not to be used as a type"));
  }
  return std::string();
}

std::string DebugPrint(RoadShield const & shield)
{
  return DebugPrint(shield.m_type) + "/" + shield.m_name +
         (shield.m_additionalText.empty() ? "" : " (" + shield.m_additionalText + ")");
}
}  // namespace ftypes
