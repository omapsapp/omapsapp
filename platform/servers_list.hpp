#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace downloader
{
// Dynamic configuration from MetaServer.
struct MetaConfig
{
  using ServersList = std::vector<std::string>;
  ServersList m_serversList;
  using SettingsMap = std::map<std::string, std::string>;
  SettingsMap m_settings;
  std::string m_productsConfig;
};

std::optional<MetaConfig> ParseMetaConfig(std::string const & jsonStr);
}  // namespace downloader
