#pragma once

#include "base/small_map.hpp"

#include <optional>
#include <string>
#include <type_traits>

namespace routing
{
class RoutingOptions
{
public:
  enum Road : uint8_t
  {
    Usual    = 1u << 0,
    Toll     = 1u << 1,
    Motorway = 1u << 2,
    Ferry    = 1u << 3,
    Dirty    = 1u << 4,

    Max      = (1u << 4) + 1
  };

  using RoadType = std::underlying_type_t<Road>;

  RoutingOptions() = default;
  explicit RoutingOptions(RoadType mask) : m_options(mask) {}

  static RoutingOptions LoadCarOptionsFromSettings();
  static void SaveCarOptionsToSettings(RoutingOptions options);

  void Add(Road type);
  void Remove(Road type);
  bool Has(Road type) const;

  RoadType GetOptions() const { return m_options; }

private:
  RoadType m_options = 0;
};

class RoutingOptionsClassifier
{
public:
  RoutingOptionsClassifier();

  std::optional<RoutingOptions::Road> Get(uint32_t type) const;
  static RoutingOptionsClassifier const & Instance();

private:
  base::SmallMap<uint32_t, RoutingOptions::Road> m_data;
};

RoutingOptions::Road ChooseMainRoutingOptionRoad(RoutingOptions options, bool isCarRouter);

std::string DebugPrint(RoutingOptions const & routingOptions);
std::string DebugPrint(RoutingOptions::Road type);

/// Options guard for debugging/tests.
class RoutingOptionSetter
{
public:
  explicit RoutingOptionSetter(RoutingOptions::RoadType roadsMask);
  ~RoutingOptionSetter();

private:
  RoutingOptions m_saved;
};
}  // namespace routing
