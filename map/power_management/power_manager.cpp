#include "map/power_management/power_manager.hpp"

#include "platform/platform.hpp"

#include "coding/file_reader.hpp"
#include "coding/file_writer.hpp"
#include "coding/internal/file_data.hpp"
#include "coding/serdes_json.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"

using namespace power_management;

namespace
{
using Subscribers = std::vector<PowerManager::Subscriber *>;

void NotifySubscribers(Subscribers & subscribers, Scheme const scheme)
{
  for (auto & subscriber : subscribers)
    subscriber->OnPowerSchemeChanged(scheme);
}

void NotifySubscribers(Subscribers & subscribers, Facility const facility, bool enabled)
{
  for (auto & subscriber : subscribers)
    subscriber->OnPowerFacilityChanged(facility, enabled);
}
}  // namespace

namespace power_management
{
// static
std::string PowerManager::GetConfigPath()
{
  return base::JoinPath(GetPlatform().SettingsDir(), "power_manager_config");
}

void PowerManager::Load()
{
  try
  {
    FileReader reader(GetConfigPath());
    NonOwningReaderSource source(reader);

    coding::DeserializerJson des(source);
    Config result;
    des(result);

    m_config = result;

    if (m_config.m_scheme == Scheme::Auto)
      GetPlatform().GetBatteryTracker().Subscribe(this);

    for (size_t i = 0; i < m_config.m_facilities.size(); ++i)
    {
      NotifySubscribers(m_subscribers, static_cast<Facility>(i), m_config.m_facilities[i]);
    }

    NotifySubscribers(m_subscribers, m_config.m_scheme);
    return;
  }
  catch (base::Json::Exception & ex)
  {
    LOG(LERROR, ("Cannot deserialize power manager data from file. Exception:", ex.Msg()));
  }
  catch (FileReader::Exception const & ex)
  {
    LOG(LWARNING, ("Cannot read power manager config file. Exception:", ex.Msg()));
  }

  // Reset to default state.
  m_config = {};
}

void PowerManager::SetFacility(Facility const facility, bool enabled)
{
  CHECK_NOT_EQUAL(facility, Facility::Count, ());

  if (m_config.m_facilities[static_cast<size_t>(facility)] == enabled)
    return;

  m_config.m_facilities[static_cast<size_t>(facility)] = enabled;

  auto const isSchemeChanged = m_config.m_scheme != Scheme::None;

  if (m_config.m_scheme == Scheme::Auto)
    GetPlatform().GetBatteryTracker().Unsubscribe(this);

  m_config.m_scheme = Scheme::None;

  if (!Save())
    return;

  NotifySubscribers(m_subscribers, facility, enabled);

  if (isSchemeChanged)
    NotifySubscribers(m_subscribers, m_config.m_scheme);
}

void PowerManager::SetScheme(Scheme const scheme)
{
  if (m_config.m_scheme == scheme)
    return;

  m_config.m_scheme = scheme;

  if (m_config.m_scheme == Scheme::Auto)
    GetPlatform().GetBatteryTracker().Subscribe(this);
  else
    GetPlatform().GetBatteryTracker().Unsubscribe(this);

  if (m_config.m_scheme == Scheme::None || m_config.m_scheme == Scheme::Auto)
  {
    if (!Save())
      return;

    NotifySubscribers(m_subscribers, m_config.m_scheme);
    return;
  }

  auto actualState = GetFacilitiesState(scheme);

  std::swap(m_config.m_facilities, actualState);

  if (!Save())
    return;

  NotifySubscribers(m_subscribers, m_config.m_scheme);

  for (size_t i = 0; i < actualState.size(); ++i)
  {
    if (m_config.m_facilities[i] != actualState[i])
      NotifySubscribers(m_subscribers, static_cast<Facility>(i), m_config.m_facilities[i]);
  }
}

bool PowerManager::IsFacilityEnabled(Facility const facility) const
{
  CHECK_NOT_EQUAL(facility, Facility::Count, ());

  return m_config.m_facilities[static_cast<size_t>(facility)];
}

FacilitiesState const & PowerManager::GetFacilities() const
{
  return m_config.m_facilities;
}

Scheme const & PowerManager::GetScheme() const
{
  return m_config.m_scheme;
}

void PowerManager::OnBatteryLevelReceived(uint8_t level)
{
  CHECK_LESS_OR_EQUAL(level, 100, ());

  if (m_config.m_scheme != Scheme::Auto)
    return;

  AutoScheme actualScheme = AutoScheme::Normal;
  if (level < 20)
    actualScheme = AutoScheme::EconomyMaximum;
  else if (level < 30)
    actualScheme = AutoScheme::EconomyMedium;

  auto actualState = GetFacilitiesState(actualScheme);

  if (m_config.m_facilities == actualState)
    return;

  std::swap(m_config.m_facilities, actualState);

  if (!Save())
    return;

  for (size_t i = 0; i < actualState.size(); ++i)
  {
    if (m_config.m_facilities[i] != actualState[i])
      NotifySubscribers(m_subscribers, static_cast<Facility>(i), m_config.m_facilities[i]);
  }
}

void PowerManager::Subscribe(Subscriber * subscriber)
{
  ASSERT(subscriber, ());
  m_subscribers.push_back(subscriber);
}

void PowerManager::UnsubscribeAll()
{
  m_subscribers.clear();
}

bool PowerManager::Save()
{
  auto const result =
      base::WriteToTempAndRenameToFile(GetConfigPath(), [this](std::string const & fileName) {
        try
        {
          FileWriter writer(fileName);
          coding::SerializerJson<FileWriter> ser(writer);
          ser(m_config);
          return true;
        }
        catch (base::Json::Exception & ex)
        {
          LOG(LERROR, ("Cannot serialize power manager data into file. Exception:", ex.Msg()));
        }
        catch (FileWriter::Exception const & ex)
        {
          LOG(LERROR, ("Cannot write power manager file. Exception:", ex.Msg()));
        }

        return false;
      });

  if (result)
    return true;

  // Try to load last correct state and notify subscribers.
  Load();
  return false;
}

void PowerManager::Init()
{
  FILE* fp = std::fopen(GetConfigPath().c_str(), "r");
  if (fp)
    std::fclose(fp);
  else
    Save();
}
}  // namespace power_management
