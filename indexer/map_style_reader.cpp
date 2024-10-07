#include "map_style_reader.hpp"

#include "platform/platform.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"

namespace
{
std::string const kSuffixDark = "dark";
std::string const kSuffixLight = "light";
std::string const kSuffixDefaultDark = "default_dark";
std::string const kSuffixDefaultLight = "default_light";
std::string const kSuffixVehicleDark = "vehicle_dark";
std::string const kSuffixVehicleLight = "vehicle_light";
std::string const kSuffixOutdoorsLight = "outdoors_light";
std::string const kSuffixOutdoorsDark = "outdoors_dark";

std::string const kStylesOverrideDir = "styles";

#ifdef BUILD_DESIGNER
std::string const kSuffixDesignTool = "design";
#endif // BUILD_DESIGNER

std::string GetStyleRulesSuffix(MapStyle mapStyle)
{
#ifdef BUILD_DESIGNER
  return kSuffixDesignTool;
#else
  switch (mapStyle)
  {
  case MapStyleDefaultDark:
    return kSuffixDefaultDark;
  case MapStyleDefaultLight:
    return kSuffixDefaultLight;
  case MapStyleVehicleDark:
    return kSuffixVehicleDark;
  case MapStyleVehicleLight:
    return kSuffixVehicleLight;
  case MapStyleOutdoorsLight:
    return kSuffixOutdoorsLight;
  case MapStyleOutdoorsDark:
    return kSuffixOutdoorsDark;
  case MapStyleMerged:
    return "";

  case MapStyleCount:
    break;
  }
  LOG(LWARNING, ("Unknown map style", mapStyle));
  return kSuffixDefaultLight;
#endif // BUILD_DESIGNER
}

std::string GetStyleResourcesSuffix(MapStyle mapStyle)
{
#ifdef BUILD_DESIGNER
  return kSuffixDesignTool;
#else
  // We use the same resources for all light/day and dark/night styles
  // to avoid textures duplication and package size increasing.
  switch (mapStyle)
  {
  case MapStyleDefaultDark:
  case MapStyleVehicleDark:
  case MapStyleOutdoorsDark:
    return kSuffixDark;
  case MapStyleDefaultLight:
  case MapStyleVehicleLight:
  case MapStyleOutdoorsLight:
    return kSuffixLight;
  case MapStyleMerged:
    return "";

  case MapStyleCount:
    break;
  }
  LOG(LWARNING, ("Unknown map style", mapStyle));
  return kSuffixLight;
#endif // BUILD_DESIGNER
}
}  // namespace

StyleReader::StyleReader()
  : m_mapStyle(kDefaultMapStyle)
{}

void StyleReader::SetCurrentStyle(MapStyle mapStyle)
{
  m_mapStyle = mapStyle;
}

MapStyle StyleReader::GetCurrentStyle() const
{
  return m_mapStyle;
}

bool StyleReader::IsCarNavigationStyle() const
{
  return m_mapStyle == MapStyle::MapStyleVehicleLight ||
         m_mapStyle == MapStyle::MapStyleVehicleDark;
}

ReaderPtr<Reader> StyleReader::GetDrawingRulesReader() const
{
  std::string rulesFile = std::string("drules_proto_") + GetStyleRulesSuffix(GetCurrentStyle()) + ".bin";

  auto overriddenRulesFile =
      base::JoinPath(GetPlatform().WritableDir(), kStylesOverrideDir, rulesFile);
  if (Platform::IsFileExistsByFullPath(overriddenRulesFile))
    rulesFile = overriddenRulesFile;

#ifdef BUILD_DESIGNER
  // For Designer tool we have to look first into the resource folder.
  return GetPlatform().GetReader(rulesFile, "rwf");
#else
  return GetPlatform().GetReader(rulesFile);
#endif
}

ReaderPtr<Reader> StyleReader::GetResourceReader(std::string const & file,
                                                 std::string_view density) const
{
  std::string resFile = base::JoinPath("symbols", std::string{density}, GetStyleResourcesSuffix(GetCurrentStyle()), file);

  auto overriddenResFile = base::JoinPath(GetPlatform().WritableDir(), kStylesOverrideDir, resFile);
  if (GetPlatform().IsFileExistsByFullPath(overriddenResFile))
    resFile = overriddenResFile;

#ifdef BUILD_DESIGNER
  // For Designer tool we have to look first into the resource folder.
  return GetPlatform().GetReader(resFile, "rwf");
#else
  return GetPlatform().GetReader(resFile);
#endif
}

ReaderPtr<Reader> StyleReader::GetDefaultResourceReader(std::string const & file) const
{
  return GetPlatform().GetReader(base::JoinPath("symbols/default", file));
}

StyleReader & GetStyleReader()
{
  static StyleReader instance;
  return instance;
}
