#pragma once

#include "map/power_management/power_management_schemas.hpp"

#include "platform/battery_tracker.hpp"

#include "base/visitor.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace power_management
{
// Note: this class is NOT thread-safe.
class PowerManager : public platform::BatteryLevelTracker::Subscriber
{
public:
  class Subscriber
  {
  public:
    virtual void OnPowerFacilityChanged(Facility const facility, bool enabled) = 0;
    virtual void OnPowerSchemeChanged(Scheme const actualScheme) = 0;
  protected:
    virtual ~Subscriber() = default;
  };

  static std::string GetConfigPath();

  void Load();
  // Set some facility state manually, it turns current scheme to Scheme::None.
  void SetFacility(Facility const facility, bool enabled);
  void SetScheme(Scheme const scheme);
  bool IsFacilityEnabled(Facility const facility) const;
  FacilitiesState const & GetFacilities() const;
  Scheme const & GetScheme() const;

  // BatteryLevelTracker::Subscriber overrides:
  void OnBatteryLevelReceived(uint8_t level) override;

  void Subscribe(Subscriber * subscriber);
  void UnsubscribeAll();
  void Init();

private:
  struct Config
  {
    DECLARE_VISITOR(visitor(m_facilities, "current_state"), visitor(m_scheme, "scheme"))

    Config() { m_facilities.fill(true); }

    FacilitiesState m_facilities;
    Scheme m_scheme = Scheme::Normal;
  };

  bool Save();

  std::vector<Subscriber *> m_subscribers;

  Config m_config;
};
}  // namespace power_management
