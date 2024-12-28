#pragma once

#include <string>

enum MapStyle
{
  MapStyleDefaultLight = 0,
  MapStyleDefaultDark = 1,
  MapStyleMerged = 2,
  MapStyleVehicleLight = 3,
  MapStyleVehicleDark = 4,
  MapStyleOutdoorsLight = 5,
  MapStyleOutdoorsDark = 6,
  // Add new map style here

  // Specifies number of MapStyle enum values, must be last
  MapStyleCount
};

extern MapStyle const kDefaultMapStyle;

extern MapStyle MapStyleFromSettings(std::string const & str);
extern std::string MapStyleToString(MapStyle mapStyle);
extern std::string DebugPrint(MapStyle mapStyle);
extern bool MapStyleIsDark(MapStyle mapStyle);
extern MapStyle GetDarkMapStyleVariant(MapStyle mapStyle);
extern MapStyle GetLightMapStyleVariant(MapStyle mapStyle);
