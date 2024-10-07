# Directories Structure

## Platforms
- `android/` - Android UI.

- `iphone/` - iOS UI.
- `xcode/` - XCode workspace.

- `qt/` - desktop application.

## Data

`data/` folder contains data files for the application: maps, styles, country borders, etc.:

- `benchmarks/` -
- `borders/` - polygons describing countries' borders.
- `conf/isolines/` - per-country isoline profiles.
- `vulkan_shaders/` -

- `countries.txt` - map files hierarchy and checksums.
- `countries_meta.txt` - country/region languages and driving sides.
- `hierarchy.txt` - countries/map regions hierarchy, languages used and Wikidata IDs.

- `faq.html` - FAQ text displayed in the "?"/Help screen.
- `copyright.html` - attributions to 3rd-party libraries, map data, icons, fonts.

- `minsk-pass.mwm`,`minsk-pass.osm.bz2` - a small map used for tests.

There are some other files not mentioned here.

### Map features / classificator

- `mapcss-mapping.csv` - mapping between OSM tags and OM types.
- `replaced_tags.txt` - merging similar OSM tags.
- `mixed_tags.txt` - pedestrian streets of high popularity.

- `editor.config` - built-in OSM data editor configuration (editable POIs, their attributes, etc.).
- `config.xsd` - XML schema for `editor.config`.

Automatically generated:
- `classificator.txt` - hierarchical list of all OM types.
- `types.txt`

### Styles and icons

- `symbols/` - symbols for different screen DPIs
- `symbols/default/` - default resources used on all sceen DPIs
- `symbols-svg/` - social networks icons
- `search-icons/svg/` - source SVG files for search categories icons
- `styles/` - map [style files](STYLES.md#files)

Automatically [generated](STYLES.md#technical-details):
- `symbols/` - icons skin files in various resolutions for `dark` and `light` themes.
- `drules_proto*` - binary drawing rules files.
- `colors.txt`,`patterns.txt`,`visibility.txt`

### Strings and translations

[Translation files](TRANSLATIONS.md#translation-files):
- `strings/`
- `categories.txt`,`categories_cuisines.txt`,`categories_brands.txt`,`countries_names.txt`

Misc strings:
- `mwm_names_en.txt` - English names for map regions.
- `countries_synonyms.csv` - alternative country names.
- `synonyms.txt` - country and region names abbreviations and short names.
- `languages.txt` - native language names.

Automatically [generated](TRANSLATIONS.md#technical-details):
- `countries-strings/` - country and map region names JSON localization files.
- `sound-strings/` - Text-To-Speech JSON localization files.

## Tools
- `tools/` - various scripts for building packages and maps, testing, managing translations etc.

- `generator/` - map building tool.
- `poly_borders/` - borders post-processing tool.
- `skin_generator/` - a console app for building skin files with icons and symbols.
- `topography_generator/` - generates isolines from SRTM data.
- `track_generator/` - generates smooth tracks based on waypoints from KML.

## C++ Core

- `3party/` - external libraries, sometimes modified.
- `base/` - some base things, like macros, logging, caches etc.
- `cmake/` - CMake helper files.
- `coding/` - I/O classes and data processing.
- `descriptions/` -
- `drape_frontend/` - scene and resource manager for the Drape library.
- `drape/` - the new graphics library core.
- `editor/` - built-in OSM data editor.
- `feature_list/` -
- `ge0/` - external API of the application.
- `geometry/` - geometry primitives we use.
- `indexer/` - processor for map files, classificator, styles.
- `kml/` - manipulation of KML files.
- `map/` - app business logic, including a scene manager.
- `openlr/` -
- `packaging/` - packaging specs for various distributions.
- `platform/` - platform abstraction classes: file paths, http requests, location services.
- `pyhelpers/` -
- `qt_tstfrm/` - widgets for visual testing.
- `routing_common/` -
- `routing/` - in-app routing engine.
- `search/` - ranking and searching classes.
- `shaders/` - shaders for rendering.
- `std/` - standard headers wrappers, for Boost, STL, C-rt.
- `storage/` - map reading function.
- `testing/` - common interfaces for tests.
- `track_analyzing/` -
- `tracking/` -
- `traffic/` - real-time traffic information.
- `transit/` - experimental GTFS-based public transport support.

## Documentation

The main docs are in the `docs/` directory, however some tools have their own READMEs, etc.
