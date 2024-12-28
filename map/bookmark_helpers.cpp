#include "map/bookmark_helpers.hpp"

#include "drape_frontend/visual_params.hpp"

#include "kml/serdes.hpp"
#include "kml/serdes_binary.hpp"
#include "kml/serdes_gpx.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature_data.hpp"

#include "platform/localization.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"

#include "coding/file_reader.hpp"
#include "coding/file_writer.hpp"
#include "coding/internal/file_data.hpp"
#include "coding/zip_reader.hpp"

#include "base/file_name_utils.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <map>
#include <sstream>

namespace
{
struct BookmarkMatchInfo
{
  BookmarkMatchInfo(kml::BookmarkIcon icon, BookmarkBaseType type)
    : m_icon(icon)
    , m_type(type)
  {}

  kml::BookmarkIcon m_icon;
  BookmarkBaseType m_type;
};

std::map<std::string, BookmarkMatchInfo> const kFeatureTypeToBookmarkMatchInfo = {
  {"amenity-veterinary", {kml::BookmarkIcon::Animals, BookmarkBaseType::Animals}},
  {"leisure-dog_park", {kml::BookmarkIcon::Animals, BookmarkBaseType::Animals}},
  {"tourism-zoo", {kml::BookmarkIcon::Animals, BookmarkBaseType::Animals}},

  {"amenity-bar", {kml::BookmarkIcon::Bar, BookmarkBaseType::Food}},
  {"amenity-biergarten", {kml::BookmarkIcon::Pub, BookmarkBaseType::Food}},
  {"amenity-pub", {kml::BookmarkIcon::Pub, BookmarkBaseType::Food}},
  {"amenity-cafe", {kml::BookmarkIcon::Cafe, BookmarkBaseType::Food}},

  {"amenity-place_of_worship-buddhist", {kml::BookmarkIcon::Buddhism, BookmarkBaseType::ReligiousPlace}},

  {"amenity-college", {kml::BookmarkIcon::Building, BookmarkBaseType::Building}},
  {"amenity-courthouse", {kml::BookmarkIcon::Building, BookmarkBaseType::Building}},
  {"amenity-kindergarten", {kml::BookmarkIcon::Building, BookmarkBaseType::Building}},
  {"amenity-library", {kml::BookmarkIcon::Building, BookmarkBaseType::Building}},
  {"amenity-police", {kml::BookmarkIcon::Building, BookmarkBaseType::Building}},
  {"amenity-prison", {kml::BookmarkIcon::Building, BookmarkBaseType::Building}},
  {"amenity-school", {kml::BookmarkIcon::Building, BookmarkBaseType::Building}},
  {"building-university", {kml::BookmarkIcon::Building, BookmarkBaseType::Building}},
  {"office", {kml::BookmarkIcon::Building, BookmarkBaseType::Building}},
  {"office-diplomatic", {kml::BookmarkIcon::Building, BookmarkBaseType::Building}},
  {"office-lawyer", {kml::BookmarkIcon::Building, BookmarkBaseType::Building}},

  {"amenity-grave_yard-christian", {kml::BookmarkIcon::Christianity, BookmarkBaseType::ReligiousPlace}},
  {"amenity-place_of_worship-christian", {kml::BookmarkIcon::Christianity, BookmarkBaseType::ReligiousPlace}},
  {"landuse-cemetery-christian", {kml::BookmarkIcon::Christianity, BookmarkBaseType::ReligiousPlace}},

  {"amenity-casino", {kml::BookmarkIcon::Entertainment, BookmarkBaseType::Entertainment}},
  {"amenity-cinema", {kml::BookmarkIcon::Entertainment, BookmarkBaseType::Entertainment}},
  {"amenity-nightclub", {kml::BookmarkIcon::Entertainment, BookmarkBaseType::Entertainment}},
  {"shop-bookmaker", {kml::BookmarkIcon::Entertainment, BookmarkBaseType::Entertainment}},
  {"tourism-theme_park", {kml::BookmarkIcon::Entertainment, BookmarkBaseType::Entertainment}},
    
  {"amenity-theatre", {kml::BookmarkIcon::Theatre, BookmarkBaseType::Entertainment}},

  {"amenity-atm", {kml::BookmarkIcon::Bank, BookmarkBaseType::Exchange}},
  {"amenity-bank", {kml::BookmarkIcon::Bank, BookmarkBaseType::Exchange}},
  {"shop-money_lender", {kml::BookmarkIcon::Bank, BookmarkBaseType::Exchange}},

  {"amenity-bureau_de_change", {kml::BookmarkIcon::Exchange, BookmarkBaseType::Exchange}},

  {"amenity-bbq", {kml::BookmarkIcon::Food, BookmarkBaseType::Food}},
  {"amenity-cafe", {kml::BookmarkIcon::Food, BookmarkBaseType::Food}},
  {"amenity-fast_food", {kml::BookmarkIcon::Food, BookmarkBaseType::Food}},
  {"amenity-food_court", {kml::BookmarkIcon::Food, BookmarkBaseType::Food}},
  {"amenity-restaurant", {kml::BookmarkIcon::Food, BookmarkBaseType::Food}},
  {"leisure-picnic_table", {kml::BookmarkIcon::Food, BookmarkBaseType::Food}},
  {"tourism-picnic_site", {kml::BookmarkIcon::Food, BookmarkBaseType::Food}},

  {"amenity-charging_station", {kml::BookmarkIcon::Gas, BookmarkBaseType::Gas}},
  {"amenity-fuel", {kml::BookmarkIcon::Gas, BookmarkBaseType::Gas}},

  {"tourism-alpine_hut", {kml::BookmarkIcon::Hotel, BookmarkBaseType::Hotel}},
  {"tourism-camp_site", {kml::BookmarkIcon::Hotel, BookmarkBaseType::Hotel}},
  {"tourism-chalet", {kml::BookmarkIcon::Hotel, BookmarkBaseType::Hotel}},
  {"tourism-guest_house", {kml::BookmarkIcon::Hotel, BookmarkBaseType::Hotel}},
  {"tourism-hostel", {kml::BookmarkIcon::Hotel, BookmarkBaseType::Hotel}},
  {"tourism-hotel", {kml::BookmarkIcon::Hotel, BookmarkBaseType::Hotel}},
  {"tourism-motel", {kml::BookmarkIcon::Hotel, BookmarkBaseType::Hotel}},
  {"tourism-resort", {kml::BookmarkIcon::Hotel, BookmarkBaseType::Hotel}},
  {"tourism-wilderness_hut", {kml::BookmarkIcon::Hotel, BookmarkBaseType::Hotel}},
  {"tourism-apartment", {kml::BookmarkIcon::Hotel, BookmarkBaseType::Hotel}},

  {"amenity-place_of_worship-muslim", {kml::BookmarkIcon::Islam, BookmarkBaseType::ReligiousPlace}},

  {"amenity-place_of_worship-jewish", {kml::BookmarkIcon::Judaism, BookmarkBaseType::ReligiousPlace}},

  {"amenity-childcare", {kml::BookmarkIcon::Medicine, BookmarkBaseType::Medicine}},
  {"amenity-clinic", {kml::BookmarkIcon::Medicine, BookmarkBaseType::Medicine}},
  {"amenity-dentist", {kml::BookmarkIcon::Medicine, BookmarkBaseType::Medicine}},
  {"amenity-doctors", {kml::BookmarkIcon::Medicine, BookmarkBaseType::Medicine}},
  {"amenity-hospital", {kml::BookmarkIcon::Medicine, BookmarkBaseType::Medicine}},
  {"emergency-defibrillator", {kml::BookmarkIcon::Medicine, BookmarkBaseType::Medicine}},
    
  {"amenity-pharmacy", {kml::BookmarkIcon::Pharmacy, BookmarkBaseType::Medicine}},

  {"natural-bare_rock", {kml::BookmarkIcon::Mountain, BookmarkBaseType::Mountain}},
  {"natural-cave_entrance", {kml::BookmarkIcon::Mountain, BookmarkBaseType::Mountain}},
  {"natural-peak", {kml::BookmarkIcon::Mountain, BookmarkBaseType::Mountain}},
  {"natural-rock", {kml::BookmarkIcon::Mountain, BookmarkBaseType::Mountain}},
  {"natural-volcano", {kml::BookmarkIcon::Mountain, BookmarkBaseType::Mountain}},

  {"amenity-arts_centre", {kml::BookmarkIcon::Art, BookmarkBaseType::Museum}},
  {"tourism-gallery", {kml::BookmarkIcon::Art, BookmarkBaseType::Museum}},

  {"tourism-museum", {kml::BookmarkIcon::Museum, BookmarkBaseType::Museum}},

  {"boundary-national_park", {kml::BookmarkIcon::Park, BookmarkBaseType::Park}},
  {"landuse-forest", {kml::BookmarkIcon::Park, BookmarkBaseType::Park}},
  {"leisure-garden", {kml::BookmarkIcon::Park, BookmarkBaseType::Park}},
  {"leisure-nature_reserve", {kml::BookmarkIcon::Park, BookmarkBaseType::Park}},
  {"leisure-park", {kml::BookmarkIcon::Park, BookmarkBaseType::Park}},

  {"amenity-bicycle_parking", {kml::BookmarkIcon::Parking, BookmarkBaseType::Parking}},
  {"amenity-motorcycle_parking", {kml::BookmarkIcon::Parking, BookmarkBaseType::Parking}},
  {"amenity-parking", {kml::BookmarkIcon::Parking, BookmarkBaseType::Parking}},
  {"highway-services", {kml::BookmarkIcon::Parking, BookmarkBaseType::Parking}},
  {"tourism-caravan_site", {kml::BookmarkIcon::Parking, BookmarkBaseType::Parking}},
  {"amenity-vending_machine-parking_tickets", {kml::BookmarkIcon::Parking, BookmarkBaseType::Parking}},

  {"amenity-ice_cream", {kml::BookmarkIcon::Shop, BookmarkBaseType::Shop}},
  {"amenity-marketplace", {kml::BookmarkIcon::Shop, BookmarkBaseType::Shop}},
  {"amenity-vending_machine", {kml::BookmarkIcon::Shop, BookmarkBaseType::Shop}},
  {"shop", {kml::BookmarkIcon::Shop, BookmarkBaseType::Shop}},

  {"amenity-place_of_worship", {kml::BookmarkIcon::Sights, BookmarkBaseType::ReligiousPlace}},
  {"historic-archaeological_site", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"historic-boundary_stone", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"historic-castle", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"historic-fort", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"historic-memorial", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"historic-monument", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"historic-ruins", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"historic-ship", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"historic-tomb", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"historic-wayside_cross", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"historic-wayside_shrine", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"tourism-artwork", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"tourism-attraction", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},
  {"waterway-waterfall", {kml::BookmarkIcon::Sights, BookmarkBaseType::Sights}},

  {"tourism-information", {kml::BookmarkIcon::Information, BookmarkBaseType::Sights}},
  {"tourism-information-office", {kml::BookmarkIcon::Information, BookmarkBaseType::Sights}},
  {"tourism-information-visitor_centre", {kml::BookmarkIcon::Information, BookmarkBaseType::Sights}},

  {"leisure-fitness_centre", {kml::BookmarkIcon::Sport, BookmarkBaseType::Entertainment}},
  {"leisure-skiing", {kml::BookmarkIcon::Sport, BookmarkBaseType::Entertainment}},
  {"leisure-sports_centre-climbing", {kml::BookmarkIcon::Sport, BookmarkBaseType::Entertainment}},
  {"leisure-sports_centre-shooting", {kml::BookmarkIcon::Sport, BookmarkBaseType::Entertainment}},
  {"leisure-sports_centre-yoga", {kml::BookmarkIcon::Sport, BookmarkBaseType::Entertainment}},
  {"sport", {kml::BookmarkIcon::Sport, BookmarkBaseType::Entertainment}},
    
  {"leisure-stadium", {kml::BookmarkIcon::Stadium, BookmarkBaseType::Entertainment}},

  {"leisure-sports_centre-swimming", {kml::BookmarkIcon::Swim, BookmarkBaseType::Swim}},
  {"leisure-swimming_pool", {kml::BookmarkIcon::Swim, BookmarkBaseType::Swim}},
  {"leisure-water_park", {kml::BookmarkIcon::Swim, BookmarkBaseType::Swim}},
  {"natural-beach", {kml::BookmarkIcon::Swim, BookmarkBaseType::Swim}},
  {"sport-diving", {kml::BookmarkIcon::Swim, BookmarkBaseType::Swim}},
  {"sport-scuba_diving", {kml::BookmarkIcon::Swim, BookmarkBaseType::Swim}},
  {"sport-swimming", {kml::BookmarkIcon::Swim, BookmarkBaseType::Swim}},

  {"aeroway-aerodrome", {kml::BookmarkIcon::Transport, BookmarkBaseType::None}},
  {"aeroway-aerodrome-international", {kml::BookmarkIcon::Transport, BookmarkBaseType::None}},
  {"amenity-bus_station", {kml::BookmarkIcon::Transport, BookmarkBaseType::None}},
  {"amenity-car_sharing", {kml::BookmarkIcon::Transport, BookmarkBaseType::None}},
  {"amenity-ferry_terminal", {kml::BookmarkIcon::Transport, BookmarkBaseType::None}},
  {"amenity-taxi", {kml::BookmarkIcon::Transport, BookmarkBaseType::None}},
  {"building-train_station", {kml::BookmarkIcon::Transport, BookmarkBaseType::Building}},
  {"highway-bus_stop", {kml::BookmarkIcon::Transport, BookmarkBaseType::None}},
  {"public_transport-platform", {kml::BookmarkIcon::Transport, BookmarkBaseType::None}},
  {"railway-halt", {kml::BookmarkIcon::Transport, BookmarkBaseType::None}},
  {"railway-station", {kml::BookmarkIcon::Transport, BookmarkBaseType::None}},
  {"railway-tram_stop", {kml::BookmarkIcon::Transport, BookmarkBaseType::None}},

  {"tourism-viewpoint", {kml::BookmarkIcon::Viewpoint, BookmarkBaseType::Sights}},

  {"amenity-drinking_water", {kml::BookmarkIcon::Water, BookmarkBaseType::Water}},
  {"amenity-fountain", {kml::BookmarkIcon::Water, BookmarkBaseType::Water}},
  {"amenity-water_point", {kml::BookmarkIcon::Water, BookmarkBaseType::Water}},
  {"man_made-water_tap", {kml::BookmarkIcon::Water, BookmarkBaseType::Water}},
  {"natural-spring", {kml::BookmarkIcon::Water, BookmarkBaseType::Water}},

  {"shop-funeral_directors", {kml::BookmarkIcon::None, BookmarkBaseType::None}}
};

void ValidateKmlData(std::unique_ptr<kml::FileData> & data)
{
  if (!data)
    return;

  for (auto & t : data->m_tracksData)
  {
    if (t.m_layers.empty())
      t.m_layers.emplace_back(kml::KmlParser::GetDefaultTrackLayer());
  }
}

/// @todo(KK): This code is a temporary solution for the filtering the duplicated points in KMLs.
/// When the deserealizer reads the data from the KML that uses <gx:Track>
/// as a first step will be parsed the list of the timestamps <when> and than the list of the coordinates <gx:coord>.
/// So the filtering can be done only when all the data is parsed.
void RemoveDuplicatedTrackPoints(std::unique_ptr<kml::FileData> & data)
{
  for (auto & trackData : data->m_tracksData)
  {
    auto const & geometry = trackData.m_geometry;
    kml::MultiGeometry validGeometry;

    for (size_t lineIndex = 0; lineIndex < geometry.m_lines.size(); ++lineIndex)
    {
      auto const & line = geometry.m_lines[lineIndex];
      auto const & timestamps = geometry.m_timestamps[lineIndex];

      if (line.empty())
      {
        LOG(LWARNING, ("Empty line in track:", trackData.m_name[kml::kDefaultLang]));
        continue;
      }

      bool const hasTimestamps = geometry.HasTimestampsFor(lineIndex);
      if (hasTimestamps && timestamps.size() != line.size())
        MYTHROW(kml::DeserializerKml::DeserializeException, ("Timestamps count", timestamps.size(), "doesn't match points count", line.size()));

      validGeometry.m_lines.emplace_back();
      validGeometry.m_timestamps.emplace_back();

      auto & validLine = validGeometry.m_lines.back();
      auto & validTimestamps = validGeometry.m_timestamps.back();

      for (size_t pointIndex = 0; pointIndex < line.size(); ++pointIndex)
      {
        auto const & currPoint = line[pointIndex];

        // We don't expect vertical surfaces, so do not compare heights here.
        // Will get a lot of duplicating points otherwise after import some user KMLs.
        // https://github.com/organicmaps/organicmaps/issues/3895
        if (validLine.empty() || !AlmostEqualAbs(validLine.back().GetPoint(), currPoint.GetPoint(), kMwmPointAccuracy))
        {
          validLine.push_back(currPoint);
          if (hasTimestamps)
            validTimestamps.push_back(timestamps[pointIndex]);
        }
      }
    }

    trackData.m_geometry = std::move(validGeometry);
  }
}

bool IsBadCharForPath(strings::UniChar c)
{
  if (c < ' ')
    return true;
  for (strings::UniChar const illegalChar : {':', '/', '\\', '<', '>', '\"', '|', '?', '*'})
  {
    if (illegalChar == c)
      return true;
  }

  return false;
}
}  // namespace

std::string GetBookmarksDirectory()
{
  return base::JoinPath(GetPlatform().SettingsDir(), "bookmarks");
}

std::string GetTrashDirectory()
{
  std::string const trashDir = base::JoinPath(GetPlatform().SettingsDir(), std::string{kTrashDirectoryName});
  if (!Platform::IsFileExistsByFullPath(trashDir) && !Platform::MkDirChecked(trashDir))
    CHECK(false, ("Failed to create .Trash directory."));
  return trashDir;
}

std::string RemoveInvalidSymbols(std::string const & name)
{
  strings::UniString filtered;
  filtered.reserve(name.size());
  auto it = name.begin();
  while (it != name.end())
  {
    auto const c = ::utf8::unchecked::next(it);
    if (!IsBadCharForPath(c))
      filtered.push_back(c);
  }
  return strings::ToUtf8(filtered);
}

// Returns extension with a dot in a lower case.
std::string GetLowercaseFileExt(std::string const & filePath)
{
  return strings::MakeLowerCase(base::GetFileExtension(filePath));
}

std::string GenerateUniqueFileName(std::string const & path, std::string name, std::string_view ext)
{
  // Remove extension, if file name already contains it.
  if (name.ends_with(ext))
    name.resize(name.size() - ext.size());

  size_t counter = 1;
  std::string suffix, res;
  do
  {
    res = name;
    res = base::JoinPath(path, res.append(suffix).append(ext));
    if (!Platform::IsFileExistsByFullPath(res))
      break;
    suffix = strings::to_string(counter++);
  } while (true);

  return res;
}

std::string GenerateValidAndUniqueFilePathForKML(std::string const & fileName)
{
  std::string filePath = RemoveInvalidSymbols(fileName);
  if (filePath.empty())
    filePath = kDefaultBookmarksFileName;

  return GenerateUniqueFileName(GetBookmarksDirectory(), std::move(filePath), kKmlExtension);
}

std::string GenerateValidAndUniqueFilePathForGPX(std::string const & fileName)
{
  std::string filePath = RemoveInvalidSymbols(fileName);
  if (filePath.empty())
    filePath = kDefaultBookmarksFileName;

  return GenerateUniqueFileName(GetBookmarksDirectory(), std::move(filePath), kGpxExtension);
}

std::string GenerateValidAndUniqueTrashedFilePath(std::string const & fileName)
{
  std::string extension = base::GetFileExtension(fileName);
  std::string filePath = RemoveInvalidSymbols(fileName);
  if (filePath.empty())
    filePath = kDefaultBookmarksFileName;
  return GenerateUniqueFileName(GetTrashDirectory(), std::move(filePath), extension);
}

std::string const kDefaultBookmarksFileName = "Bookmarks";

// Populate empty category & track names based on file name: assign file name to category name,
// if there is only one unnamed track - assign file name to it, otherwise add numbers 1, 2, 3...
// to file name to build names for all unnamed tracks
void FillEmptyNames(std::unique_ptr<kml::FileData> & kmlData, std::string const & file)
{
  auto start = file.find_last_of('/') + 1;
  auto end = file.find_last_of('.');
  if (end == std::string::npos)
    end = file.size();
  auto const name = file.substr(start, end - start);

  if (kmlData->m_categoryData.m_name.empty())
    kmlData->m_categoryData.m_name[kml::kDefaultLang] = name;

  if (kmlData->m_tracksData.empty())
    return;

  auto const emptyNames = std::count_if(kmlData->m_tracksData.begin(), kmlData->m_tracksData.end(),
                                  [](const kml::TrackData & t) { return t.m_name.empty(); });
  if (emptyNames == 0)
    return;

  auto emptyTrackNum = 1;
  for (auto & track : kmlData->m_tracksData)
  {
    if (track.m_name.empty())
    {
      if (emptyNames == 1)
      {
        track.m_name[kml::kDefaultLang] = name;
        return;
      }
      else
      {
        track.m_name[kml::kDefaultLang] = name + " " + std::to_string(emptyTrackNum);
        emptyTrackNum++;
      }
    }
  }
}

std::unique_ptr<kml::FileData> LoadKmlFile(std::string const & file, KmlFileType fileType)
{
  std::unique_ptr<kml::FileData> kmlData;
  try
  {
    kmlData = LoadKmlData(FileReader(file), fileType);
    if (kmlData != nullptr)
      FillEmptyNames(kmlData, file);
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("KML", fileType, "loading failure:", e.what()));
    kmlData.reset();
  }
  if (kmlData == nullptr)
    LOG(LWARNING, ("Loading bookmarks failed, file", file));
  return kmlData;
}

std::vector<std::string> GetKMLOrGPXFilesPathsToLoad(std::string const & filePath)
{
  std::string const fileExt = GetLowercaseFileExt(filePath);
  if (fileExt == kKmlExtension)
  {
    return GetFilePathsToLoadFromKml(filePath);
  }
  else if (fileExt == kGpxExtension)
  {
    return GetFilePathsToLoadFromGpx(filePath);
  }
  else if (fileExt == kKmbExtension)
  {
    return GetFilePathsToLoadFromKmb(filePath);
  }
  else if (fileExt == kKmzExtension)
  {
    return GetFilePathsToLoadFromKmz(filePath);
  }
  else
  {
    LOG(LWARNING, ("Unknown file type", filePath));
    return {};
  }
}

std::vector<std::string> GetFilePathsToLoadFromKmz(std::string const & filePath)
{  // Extract KML files from KMZ archive and save to temp KMLs with unique name.
  std::vector<std::string> kmlFilePaths;
  try
  {
    ZipFileReader::FileList files;
    ZipFileReader::FilesList(filePath, files);
    files.erase(std::remove_if(files.begin(), files.end(),
        [](auto const & file){ return GetLowercaseFileExt(file.first) != kKmlExtension; }),
        files.end());
    for (auto const & [kmlFileInZip, size] : files)
    {
      auto const name = base::FileNameFromFullPath(kmlFileInZip);
      auto fileSavePath = GenerateValidAndUniqueFilePathForKML(kmlFileInZip);
      ZipFileReader::UnzipFile(filePath, kmlFileInZip, fileSavePath);
      kmlFilePaths.push_back(std::move(fileSavePath));
    }
  }
  catch (RootException const & e)
  {
    LOG(LWARNING, ("Error unzipping file", filePath, e.Msg()));
  }
  return kmlFilePaths;
}

std::vector<std::string> GetFilePathsToLoadFromKmb(std::string const & filePath)
{  // Convert input file and save to temp KML with unique name.
  auto kmlData = LoadKmlFile(filePath, KmlFileType::Binary);
  if (kmlData == nullptr)
    return {};

  auto fileSavePath = GenerateValidAndUniqueFilePathForKML(base::FileNameFromFullPath(filePath));
  if (!SaveKmlFileByExt(*kmlData, fileSavePath))
    return {};
  return {std::move(fileSavePath)};
}

std::vector<std::string> GetFilePathsToLoadFromGpx(std::string const & filePath)
{  // Copy input file to temp GPX with unique name.
  auto fileSavePath = GenerateValidAndUniqueFilePathForGPX(base::FileNameFromFullPath(filePath));
  if (!base::CopyFileX(filePath, fileSavePath))
    return {};
  return {std::move(fileSavePath)};
}

std::vector<std::string> GetFilePathsToLoadFromKml(std::string const & filePath)
{  // Copy input file to temp output KML with unique name.
  auto fileSavePath = GenerateValidAndUniqueFilePathForKML(base::FileNameFromFullPath(filePath));
  if (!base::CopyFileX(filePath, fileSavePath))
    return {};
  return {std::move(fileSavePath)};
}

std::unique_ptr<kml::FileData> LoadKmlData(Reader const & reader, KmlFileType fileType)
{
  auto data = std::make_unique<kml::FileData>();
  try
  {
    if (fileType == KmlFileType::Binary)
    {
      kml::binary::DeserializerKml des(*data);
      des.Deserialize(reader);
    }
    else if (fileType == KmlFileType::Text)
    {
      kml::DeserializerKml des(*data);
      des.Deserialize(reader);
    }
    else if (fileType == KmlFileType::Gpx)
    {
      kml::DeserializerGpx des(*data);
      des.Deserialize(reader);
    }
    else
    {
      CHECK(false, ("Not supported KmlFileType"));
    }
    ValidateKmlData(data);
    RemoveDuplicatedTrackPoints(data);
  }
  catch (Reader::Exception const & e)
  {
    LOG(LWARNING, ("KML", fileType, "reading failure:", e.what()));
    return nullptr;
  }
  catch (kml::binary::DeserializerKml::DeserializeException const & e)
  {
    LOG(LWARNING, ("KML", fileType, "deserialization failure:", e.what()));
    return nullptr;
  }
  catch (kml::DeserializerKml::DeserializeException const & e)
  {
    LOG(LWARNING, ("KML", fileType, "deserialization failure:", e.what()));
    return nullptr;
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("KML", fileType, "loading failure:", e.what()));
    return nullptr;
  }
  return data;
}

bool SaveGpxData(kml::FileData & kmlData, Writer & writer)
{
  try
  {
    kml::gpx::SerializerGpx ser(kmlData);
    ser.Serialize(writer);
  }
  catch (Writer::Exception const & e)
  {
    LOG(LWARNING, ("GPX writing failure:", e.what()));
    return false;
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("GPX serialization failure:", e.what()));
    return false;
  }
  return true;
}

bool SaveKmlFile(kml::FileData & kmlData, std::string const & file, KmlFileType fileType)
{
  FileWriter writer(file);
  switch (fileType)
  {
  case KmlFileType::Text:  // fallthrough
  case KmlFileType::Binary: return SaveKmlData(kmlData, writer, fileType);
  case KmlFileType::Gpx: return SaveGpxData(kmlData, writer);
  default:
  {
    LOG(LWARNING, ("Unexpected KmlFileType", fileType));
    return false;
  }
  }
}

bool SaveKmlFileSafe(kml::FileData & kmlData, std::string const & file, KmlFileType fileType)
{
  LOG(LINFO, ("Save kml file of type", fileType, "to", file));
  return base::WriteToTempAndRenameToFile(file, [&kmlData, fileType](std::string const & fileName)
  {
    return SaveKmlFile(kmlData, fileName, fileType);
  });
}

bool SaveKmlFileByExt(kml::FileData & kmlData, std::string const & file)
{
  auto const ext = base::GetFileExtension(file);
  return SaveKmlFileSafe(kmlData, file, ext == kKmbExtension ? KmlFileType::Binary
                                                             : KmlFileType::Text);
}

bool SaveKmlData(kml::FileData & kmlData, Writer & writer, KmlFileType fileType)
{
  try
  {
    if (fileType == KmlFileType::Binary)
    {
      kml::binary::SerializerKml ser(kmlData);
      ser.Serialize(writer);
    }
    else if (fileType == KmlFileType::Text)
    {
      kml::SerializerKml ser(kmlData);
      ser.Serialize(writer);
    }
    else
    {
      CHECK(false, ("Not supported KmlFileType"));
    }
  }
  catch (Writer::Exception const & e)
  {
    LOG(LWARNING, ("KML", fileType, "writing failure:", e.what()));
    return false;
  }
  catch (std::exception const & e)
  {
    LOG(LWARNING, ("KML", fileType, "serialization failure:", e.what()));
    return false;
  }
  return true;
}

void ResetIds(kml::FileData & kmlData)
{
  kmlData.m_categoryData.m_id = kml::kInvalidMarkGroupId;
  for (auto & bmData : kmlData.m_bookmarksData)
    bmData.m_id = kml::kInvalidMarkId;
  for (auto & trackData : kmlData.m_tracksData)
    trackData.m_id = kml::kInvalidTrackId;
  for (auto & compilationData : kmlData.m_compilationsData)
    compilationData.m_id = kml::kInvalidMarkGroupId;
}

bool TruncType(std::string & type)
{
  auto const pos = type.rfind('-');
  if (pos == std::string::npos)
    return false;
  type.resize(pos);
  return true;
}

BookmarkBaseType GetBookmarkBaseType(std::vector<uint32_t> const & featureTypes)
{
  auto const & c = classif();
  for (auto typeIndex : featureTypes)
  {
    auto const type = c.GetTypeForIndex(typeIndex);
    auto typeStr = c.GetReadableObjectName(type);

    do
    {
      auto const itType = kFeatureTypeToBookmarkMatchInfo.find(typeStr);
      if (itType != kFeatureTypeToBookmarkMatchInfo.cend())
        return itType->second.m_type;
    } while (TruncType(typeStr));
  }
  return BookmarkBaseType::None;
}

kml::BookmarkIcon GetBookmarkIconByFeatureType(uint32_t type)
{
  auto typeStr = classif().GetReadableObjectName(type);

  do
  {
    auto const itIcon = kFeatureTypeToBookmarkMatchInfo.find(typeStr);
    if (itIcon != kFeatureTypeToBookmarkMatchInfo.cend())
      return itIcon->second.m_icon;
  } while (TruncType(typeStr));

  return kml::BookmarkIcon::None;
}

void SaveFeatureTypes(feature::TypesHolder const & types, kml::BookmarkData & bmData)
{
  auto const & c = classif();
  feature::TypesHolder copy(types);
  copy.SortBySpec();
  bmData.m_featureTypes.reserve(copy.Size());
  for (auto it = copy.begin(); it != copy.end(); ++it)
  {
    if (c.IsTypeValid(*it))
    {
      bmData.m_featureTypes.push_back(c.GetIndexForType(*it));
      if (bmData.m_icon == kml::BookmarkIcon::None)
        bmData.m_icon = GetBookmarkIconByFeatureType(*it);
    }
  }
}

std::string GetPreferredBookmarkStr(kml::LocalizableString const & name)
{
  return kml::GetPreferredBookmarkStr(name, languages::GetCurrentNorm());
}

std::string GetPreferredBookmarkStr(kml::LocalizableString const & name, feature::RegionData const & regionData)
{
  return kml::GetPreferredBookmarkStr(name, regionData, languages::GetCurrentNorm());
}

std::string GetLocalizedFeatureType(std::vector<uint32_t> const & types)
{
  return kml::GetLocalizedFeatureType(types);
}

std::string GetLocalizedBookmarkBaseType(BookmarkBaseType type)
{
  switch (type)
  {
  case BookmarkBaseType::None: return {};
  case BookmarkBaseType::Hotel: return platform::GetLocalizedString("hotels");
  case BookmarkBaseType::Animals: return platform::GetLocalizedString("animals");
  case BookmarkBaseType::Building: return platform::GetLocalizedString("buildings");
  case BookmarkBaseType::Entertainment: return platform::GetLocalizedString("entertainment");
  case BookmarkBaseType::Exchange: return platform::GetLocalizedString("money");
  case BookmarkBaseType::Food: return platform::GetLocalizedString("food_places");
  case BookmarkBaseType::Gas: return platform::GetLocalizedString("fuel_places");
  case BookmarkBaseType::Medicine: return platform::GetLocalizedString("medicine");
  case BookmarkBaseType::Mountain: return platform::GetLocalizedString("mountains");
  case BookmarkBaseType::Museum: return platform::GetLocalizedString("museums");
  case BookmarkBaseType::Park: return platform::GetLocalizedString("parks");
  case BookmarkBaseType::Parking: return platform::GetLocalizedString("parkings");
  case BookmarkBaseType::ReligiousPlace: return platform::GetLocalizedString("religious_places");
  case BookmarkBaseType::Shop: return platform::GetLocalizedString("shops");
  case BookmarkBaseType::Sights: return platform::GetLocalizedString("tourist_places");
  case BookmarkBaseType::Swim: return platform::GetLocalizedString("swim_places");
  case BookmarkBaseType::Water: return platform::GetLocalizedString("water");
  case BookmarkBaseType::Count: CHECK(false, ("Invalid bookmark base type")); return {};
  }
  UNREACHABLE();
}

std::string GetPreferredBookmarkName(kml::BookmarkData const & bmData)
{
  return kml::GetPreferredBookmarkName(bmData, languages::GetCurrentOrig());
}

void ExpandRectForPreview(m2::RectD & rect)
{
  if (!rect.IsValid())
    return;

  rect.Scale(df::kBoundingBoxScale);
}
