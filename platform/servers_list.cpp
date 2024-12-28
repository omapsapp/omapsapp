#include "platform/servers_list.hpp"

#include "platform/http_request.hpp"
#include "platform/platform.hpp"

#include "base/logging.hpp"
#include "base/assert.hpp"

#include "cppjansson/cppjansson.hpp"

namespace downloader
{

std::optional<MetaConfig> ParseMetaConfig(std::string const & jsonStr)
{
  char const kSettings[] = "settings";
  char const kServers[] = "servers";
  char const kProductsConfig[] = "productsConfig";

  MetaConfig outMetaConfig;
  try
  {
    const base::Json root(jsonStr.c_str());
    const json_t * servers;
    if (json_is_object(root.get()))
    {
      // New format:
      // {
      //   "servers": ["http://url1", "http://url2", "http://url3"],
      //   "settings": {
      //      "key1": "value1",
      //      "key2": "http://url"
      //    }
      // }

      json_t * settings = json_object_get(root.get(), kSettings);
      const char * key;
      const json_t * value;
      json_object_foreach(settings, key, value)
      {
        const char * valueStr = json_string_value(value);
        if (key && value)
          outMetaConfig.m_settings[key] = valueStr;
      }

      servers = json_object_get(root.get(), kServers);

      auto const productsConfig = json_object_get(root.get(), kProductsConfig);
      if (productsConfig)
        outMetaConfig.m_productsConfig = json_dumps(productsConfig, JSON_ENCODE_ANY);
      else
        LOG(LINFO, ("No ProductsConfig in meta configuration"));
    }
    else
    {
      // Old format:
      // ["http://url1", "http://url2", "http://url3"]
      servers = root.get();
    }

    for (size_t i = 0; i < json_array_size(servers); ++i)
    {
      char const * url = json_string_value(json_array_get(servers, i));
      if (url)
        outMetaConfig.m_serversList.push_back(url);
    }
  }
  catch (base::Json::Exception const & ex)
  {
    LOG(LWARNING, ("Can't parse meta configuration:", ex.Msg(), jsonStr));
  }

  if (outMetaConfig.m_serversList.empty())
    return std::nullopt;

  return outMetaConfig;
}
} // namespace downloader
