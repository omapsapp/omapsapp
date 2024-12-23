#include "transit/world_feed/world_feed.hpp"

#include "transit/transit_entities.hpp"

#include "platform/platform.hpp"
#include "platform/measurement_utils.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <iosfwd>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>

#include "cppjansson/cppjansson.hpp"

namespace
{
// TODO(o.khlopkova) Set average speed for each type of transit separately - trains, buses, etc.
// Average transit speed. Approximately 40 km/h.
static double constexpr kAvgTransitSpeedMpS = 11.1;

::transit::TransitId constexpr kInvalidLineId = std::numeric_limits<::transit::TransitId>::max();

template <class C, class ID>
void AddToRegions(C & container, ID const & id, transit::Regions const & regions)
{
  for (auto const & region : regions)
    container[region].emplace(id);
}

template <class C, class ID, class S>
transit::Regions AddToRegions(C & container, S const & splitter, ID const & id,
                              m2::PointD const & point)
{
  auto const & regions = splitter.GetAffiliations(point);
  CHECK_LESS_OR_EQUAL(
      regions.size(), 1,
      ("Point", mercator::ToLatLon(point), "belongs to multiple regions:", regions));

  AddToRegions(container, id, regions);
  return regions;
}

template <class C>
void AddToRegionsIfMatches(C & container, transit::TransitId idForAdd,
                           transit::IdsInRegion const & stopsInRegion, transit::TransitId stopId)
{
  for (auto const & [region, stopIds] : stopsInRegion)
  {
    if (stopIds.find(stopId) != stopIds.end())
      container[region].emplace(idForAdd);
  }
}

void WriteJson(json_t * node, std::ofstream & output)
{
  std::unique_ptr<char, JSONFreeDeleter> buffer(json_dumps(node, JSON_COMPACT));
  std::string record(buffer.get());
  output << record << std::endl;
}

base::JSONPtr PointToJson(m2::PointD const & point)
{
  auto coords = base::NewJSONObject();
  ToJSONObject(*coords, "x", point.x);
  ToJSONObject(*coords, "y", point.y);
  return coords;
}

base::JSONPtr ShapeLinkToJson(transit::ShapeLink const & shapeLink)
{
  auto node = base::NewJSONObject();
  ToJSONObject(*node, "id", shapeLink.m_shapeId);
  ToJSONObject(*node, "start_index", shapeLink.m_startIndex);
  ToJSONObject(*node, "end_index", shapeLink.m_endIndex);
  return node;
}

base::JSONPtr FrequenciesToJson(
    std::map<::transit::TimeInterval, ::transit::Frequency> const & frequencyIntervals)
{
  auto timeIntervalsArr = base::NewJSONArray();

  for (auto const & [timeInterval, frequency] : frequencyIntervals)
  {
    auto tiNode = base::NewJSONObject();
    ToJSONObject(*tiNode, "time_interval", timeInterval.GetRaw());
    ToJSONObject(*tiNode, "frequency", frequency);
    json_array_append_new(timeIntervalsArr.get(), tiNode.release());
  }

  return timeIntervalsArr;
}

base::JSONPtr ScheduleToJson(transit::Schedule const & schedule)
{
  auto scheduleNode = base::NewJSONObject();
  ToJSONObject(*scheduleNode, "def_frequency", schedule.GetFrequency());

  if (auto const & intervals = schedule.GetServiceIntervals(); !intervals.empty())
  {
    auto dateIntervalsArr = base::NewJSONArray();

    for (auto const & [datesInterval, frequencyIntervals] : intervals)
    {
      auto diNode = base::NewJSONObject();
      ToJSONObject(*diNode, "dates_interval", datesInterval.GetRaw());

      json_object_set_new(diNode.get(), "time_intervals",
                          FrequenciesToJson(frequencyIntervals.GetFrequencies()).release());
      json_array_append_new(dateIntervalsArr.get(), diNode.release());
    }

    json_object_set_new(scheduleNode.get(), "intervals", dateIntervalsArr.release());
  }

  if (auto const & exceptions = schedule.GetServiceExceptions(); !exceptions.empty())
  {
    auto exceptionsArr = base::NewJSONArray();

    for (auto const & [exception, frequencyIntervals] : exceptions)
    {
      auto exNode = base::NewJSONObject();
      ToJSONObject(*exNode, "exception", exception.GetRaw());

      json_object_set_new(exNode.get(), "time_intervals",
                          FrequenciesToJson(frequencyIntervals.GetFrequencies()).release());
      json_array_append_new(exceptionsArr.get(), exNode.release());
    }

    json_object_set_new(scheduleNode.get(), "exceptions", exceptionsArr.release());
  }

  return scheduleNode;
}

template <class T>
base::JSONPtr VectorToJson(std::vector<T> const & items)
{
  auto arr = base::NewJSONArray();

  for (auto const & item : items)
  {
    auto node = base::NewJSONInt(item);
    json_array_append_new(arr.get(), node.release());
  }

  return arr;
}

[[maybe_unused]] base::JSONPtr TranslationsToJson(transit::Translations const & translations)
{
  auto translationsArr = base::NewJSONArray();

  for (auto const & [lang, text] : translations)
  {
    auto translationJson = base::NewJSONObject();
    ToJSONObject(*translationJson, "lang", lang);
    ToJSONObject(*translationJson, "text", text);
    json_array_append_new(translationsArr.get(), translationJson.release());
  }

  return translationsArr;
}

template <class C, class S>
bool DumpData(C const & container, S const & idSet, std::string const & path, bool overwrite)
{
  std::ofstream output;
  output.exceptions(std::ofstream::failbit | std::ofstream::badbit);

  try
  {
    std::ios_base::openmode mode = overwrite ? std::ofstream::trunc : std::ofstream::app;
    output.open(path, mode);

    if (!output.is_open())
      return false;

    container.Write(idSet, output);
  }
  catch (std::ofstream::failure const & se)
  {
    LOG(LWARNING, ("Exception saving json to file", path, se.what()));
    return false;
  }

  return true;
}

struct StopOnShape
{
  transit::TransitId m_id = 0;
  size_t m_index = 0;
};

std::optional<size_t> GetStopIndex(
    std::unordered_map<transit::TransitId, std::vector<size_t>> const & stopIndexes,
    transit::TransitId id, size_t fromIndex, transit::Direction direction)
{
  auto it = stopIndexes.find(id);
  CHECK(it != stopIndexes.end(), (id));

  std::optional<size_t> bestIdx;
  for (auto const & index : it->second)
  {
    if (direction == transit::Direction::Forward && index >= fromIndex &&
        (!bestIdx || index < bestIdx))
      bestIdx = index;
    if (direction == transit::Direction::Backward && index <= fromIndex &&
        (!bestIdx || index > bestIdx))
      bestIdx = index;
  }
  return bestIdx;
}

std::optional<std::pair<StopOnShape, StopOnShape>> GetStopPairOnShape(
    std::unordered_map<transit::TransitId, std::vector<size_t>> const & stopIndexes,
    transit::StopsOnLines const & stopsOnLines, size_t index, size_t fromIndex,
    transit::Direction direction)
{
  auto const & stopIds = stopsOnLines.m_stopSeq;

  CHECK(!stopIds.empty(), ());
  CHECK_LESS(index, stopIds.size() - 1, ());

  StopOnShape stop1;
  StopOnShape stop2;

  stop1.m_id = stopIds[index];
  stop2.m_id = (!stopsOnLines.m_isValid && stopIds.size() == 1) ? stop1.m_id : stopIds[index + 1];

  if (stopsOnLines.m_isValid)
  {
    auto const index1 = GetStopIndex(stopIndexes, stop1.m_id, fromIndex, direction);
    if (!index1)
      return {};
    stop1.m_index = *index1;
    auto const index2 = GetStopIndex(stopIndexes, stop2.m_id, *index1, direction);
    if (!index2)
      return {};
    stop2.m_index = *index2;
  }
  return std::pair<StopOnShape, StopOnShape>(stop1, stop2);
}

struct Link
{
  Link(transit::TransitId lineId, transit::TransitId shapeId, size_t shapeSize);
  transit::TransitId m_lineId;
  transit::TransitId m_shapeId;
  size_t m_shapeSize;
};

Link::Link(transit::TransitId lineId, transit::TransitId shapeId, size_t shapeSize)
  : m_lineId(lineId), m_shapeId(shapeId), m_shapeSize(shapeSize)
{
}
}  // namespace

namespace transit
{
// Static fields.
std::unordered_set<std::string> WorldFeed::m_agencyHashes;

EdgeTransferId::EdgeTransferId(TransitId fromStopId, TransitId toStopId)
  : m_fromStopId(fromStopId), m_toStopId(toStopId)
{
}

bool EdgeTransferId::operator==(EdgeTransferId const & other) const
{
  return std::tie(m_fromStopId, m_toStopId) == std::tie(other.m_fromStopId, other.m_toStopId);
}

size_t EdgeTransferIdHasher::operator()(EdgeTransferId const & key) const
{
  size_t seed = 0;
  boost::hash_combine(seed, key.m_fromStopId);
  boost::hash_combine(seed, key.m_toStopId);
  return seed;
}

ShapeData::ShapeData(std::vector<m2::PointD> const & points) : m_points(points) {}

IdGenerator::IdGenerator(std::string const & idMappingPath) : m_idMappingPath(idMappingPath)
{
  LOG(LINFO, ("Inited generator with", m_curId, "start id and path to mappings", m_idMappingPath));
  CHECK(!m_idMappingPath.empty(), ());

  if (!Platform::IsFileExistsByFullPath(m_idMappingPath))
  {
    LOG(LINFO, ("Mapping", m_idMappingPath, "is not yet created."));
    return;
  }

  std::ifstream mappingFile;
  mappingFile.exceptions(std::ifstream::badbit);

  try
  {
    mappingFile.open(m_idMappingPath);
    CHECK(mappingFile.is_open(), ("Could not open", m_idMappingPath));

    std::string idStr;
    std::string hash;

    if (!std::getline(mappingFile, idStr))
    {
      LOG(LINFO, ("Mapping", m_idMappingPath, "is empty."));
      return;
    }

    // The first line of the mapping file is current free id.
    m_curId = static_cast<TransitId>(std::stol(idStr));

    // Next lines are sequences of id and hash pairs, each on new line.
    while (std::getline(mappingFile, idStr))
    {
      std::getline(mappingFile, hash);

      auto const id = static_cast<TransitId>(std::stol(idStr));
      CHECK(m_hashToId.emplace(hash, id).second, (hash, id));
    }
  }
  catch (std::ifstream::failure const & se)
  {
    LOG(LERROR, ("Exception reading file with mappings", m_idMappingPath, se.what()));
  }

  LOG(LINFO, ("Loaded", m_hashToId.size(), "hash-to-id mappings. Current free id:", m_curId));
}

TransitId IdGenerator::MakeId(const std::string & hash)
{
  CHECK(!hash.empty(), ("Empty hash cannot be added to the mapping."));

  auto [it, inserted] = m_hashToId.emplace(hash, 0);
  if (!inserted)
    return it->second;

  it->second = m_curId++;

  return it->second;
}

void IdGenerator::Save()
{
  LOG(LINFO, ("Started saving", m_hashToId.size(), "mappings to", m_idMappingPath));

  std::ofstream mappingFile;
  mappingFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);

  try
  {
    mappingFile.open(m_idMappingPath, std::ofstream::out | std::ofstream::trunc);
    CHECK(mappingFile.is_open(), ("Path to the mapping file does not exist:", m_idMappingPath));

    mappingFile << m_curId << std::endl;

    for (auto const & [hash, id] : m_hashToId)
      mappingFile << id << std::endl << hash << std::endl;
  }
  catch (std::ofstream::failure const & se)
  {
    LOG(LERROR, ("Exception writing file with mappings", m_idMappingPath, se.what()));
  }
}

StopsOnLines::StopsOnLines(IdList const & ids) : m_stopSeq(ids) {}

void StopData::UpdateTimetable(TransitId lineId, gtfs::StopTime const & stopTime)
{
  bool const arrivalIsSet = stopTime.arrival_time.is_provided();
  bool const departureIsSet = stopTime.departure_time.is_provided();

  if (!arrivalIsSet && !departureIsSet)
    return;

  auto arrival = arrivalIsSet ? stopTime.arrival_time : stopTime.departure_time;
  auto departure = departureIsSet ? stopTime.departure_time : stopTime.arrival_time;

  arrival.limit_hours_to_24max();
  departure.limit_hours_to_24max();

  TimeInterval const timeInterval(arrival, departure);

  auto [it, inserted] = m_timetable.emplace(lineId, std::vector<TimeInterval>{timeInterval});
  if (!inserted)
    it->second.push_back(timeInterval);
}

WorldFeed::WorldFeed(IdGenerator & generator, IdGenerator & generatorEdges,
                     ColorPicker & colorPicker, feature::CountriesFilesAffiliation & mwmMatcher)
  : m_idGenerator(generator)
  , m_idGeneratorEdges(generatorEdges)
  , m_colorPicker(colorPicker)
  , m_affiliation(mwmMatcher)
{
}

bool WorldFeed::FillNetworks()
{
  for (const auto & agency : m_feed.get_agencies())
  {
    // For one agency_name there can be multiple agency_id in the same feed.
    std::string const agencyHash = BuildHash(agency.agency_id, agency.agency_name);
    static size_t constexpr kMaxGtfsHashSize = 100;
    if (m_gtfsHash.size() + agencyHash.size() <= kMaxGtfsHashSize)
      m_gtfsHash += agencyHash;

    if (!m_gtfsIdToHash[FieldIdx::AgencyIdx].emplace(agency.agency_id, agencyHash).second)
    {
      LOG(LINFO, ("agency_id duplicates in same feed:", agencyHash));
      continue;
    }

    // agency_id is required when the dataset provides data for routes from more than one agency.
    // Otherwise agency_id field can be empty in routes.txt and other files. So we add it too:
    if (m_feed.get_agencies().size() == 1)
      m_gtfsIdToHash[FieldIdx::AgencyIdx].emplace("", agencyHash);

    if (!m_agencyHashes.insert(agencyHash).second)
    {
      LOG(LINFO, ("Agency hash copy from other feed:", agencyHash, "Skipped."));
      m_agencySkipList.insert(agencyHash);
    }

    CHECK(m_networks.m_data.emplace(m_idGenerator.MakeId(agencyHash), agency.agency_name).second, ());
  }

  return !m_networks.m_data.empty();
}

bool WorldFeed::FillRoutes()
{
  for (const auto & route : m_feed.get_routes())
  {
    // Filters irrelevant types, e.g. taxi, subway.
    if (!IsRelevantType(route.route_type))
      continue;

    std::string const routeType = ToString(route.route_type);
    // Filters unrecognized types.
    if (routeType.empty())
      continue;

    auto const itAgencyHash = m_gtfsIdToHash[FieldIdx::AgencyIdx].find(route.agency_id);
    if (itAgencyHash == m_gtfsIdToHash[FieldIdx::AgencyIdx].end())
    {
      LOG(LINFO, ("Unknown agency", route.agency_id));
      continue;
    }

    auto const & agencyHash = itAgencyHash->second;
    CHECK(!agencyHash.empty(), ("Empty hash for agency id:", route.agency_id));

    // Avoids duplicates of agencies with linked routes.
    if (m_agencySkipList.find(agencyHash) != m_agencySkipList.end())
      continue;

    std::string const routeHash = BuildHash(agencyHash, route.route_id);
    CHECK(m_gtfsIdToHash[FieldIdx::RoutesIdx].emplace(route.route_id, routeHash).second, (route.route_id, routeHash));

    RouteData data;
    data.m_networkId = m_idGenerator.MakeId(agencyHash);
    data.m_color = m_colorPicker.GetNearestColor(route.route_color);
    data.m_routeType = routeType;
    data.m_title = route.route_long_name.empty() ? route.route_short_name : route.route_long_name;

    CHECK(m_routes.m_data.emplace(m_idGenerator.MakeId(routeHash), std::move(data)).second, ());
  }

  return !m_routes.m_data.empty();
}

bool WorldFeed::SetFeedLanguage()
{
  static char constexpr kNativeForCountry[] = "default";

  m_feedLanguage = m_feed.get_feed_info().feed_lang;
  if (m_feedLanguage.empty())
  {
    m_feedLanguage = kNativeForCountry;
    return false;
  }

  strings::AsciiToLower(m_feedLanguage);

  StringUtf8Multilang multilang;
  if (multilang.GetLangIndex(m_feedLanguage) != StringUtf8Multilang::kUnsupportedLanguageCode)
    return true;

  LOG(LINFO, ("Unsupported language:", m_feedLanguage));
  m_feedLanguage = kNativeForCountry;
  return false;
}

void WorldFeed::AddShape(GtfsIdToHash::iterator & iter, gtfs::Shape const & shapeItems,
                         TransitId lineId)
{
  std::string const shapeHash = BuildHash(m_gtfsHash, shapeItems[0].shape_id);
  iter->second = shapeHash;
  auto const shapeId = m_idGenerator.MakeId(shapeHash);

  auto [it, inserted] = m_shapes.m_data.emplace(shapeId, ShapeData());

  if (inserted)
  {
    std::vector<m2::PointD> points;
    // Reserve items also for future insertion of stops projections:
    points.reserve(shapeItems.size() * 1.3);

    for (auto const & point : shapeItems)
      points.push_back(mercator::FromLatLon(point.shape_pt_lat, point.shape_pt_lon));

    it->second.m_points = points;
  }

  it->second.m_lineIds.insert(lineId);
}

bool WorldFeed::UpdateStop(TransitId stopId, gtfs::StopTime const & stopTime,
                           std::string const & stopHash, TransitId lineId)
{
  auto [it, inserted] = m_stops.m_data.emplace(stopId, StopData());

  // If the stop is already present we replenish the schedule and return.
  if (!inserted)
  {
    it->second.UpdateTimetable(lineId, stopTime);
    return true;
  }

  auto const stop = m_feed.get_stop(stopTime.stop_id);
  if (stop)
  {
    m_gtfsIdToHash[FieldIdx::StopsIdx].emplace(stopTime.stop_id, stopHash);
  }
  else
  {
    LOG(LINFO, ("Stop is present in stop_times, but not in stops:", stopId, stopTime.stop_id, lineId));
    return false;
  }

  StopData data;
  data.m_point = mercator::FromLatLon(stop->stop_lat, stop->stop_lon);
  data.m_title = stop->stop_name;
  data.m_gtfsParentId = stop->parent_station;
  data.UpdateTimetable(lineId, stopTime);
  it->second = std::move(data);
  return true;
}

std::pair<TransitId, std::string> WorldFeed::GetStopIdAndHash(std::string const & stopGtfsId)
{
  std::string const stopHash = BuildHash(m_gtfsHash, stopGtfsId);
  auto const stopId = m_idGenerator.MakeId(stopHash);
  return {stopId, stopHash};
}

bool WorldFeed::FillStopsEdges()
{
  gtfs::StopTimes allStopTimes = m_feed.get_stop_times();
  std::sort(allStopTimes.begin(), allStopTimes.end(),
            [](const gtfs::StopTime & t1, const gtfs::StopTime & t2)
            { return t1.trip_id < t2.trip_id; });

  std::vector<std::unordered_map<TransitId, LineData>::iterator> linesForRemoval;

  for (auto it = m_lines.m_data.begin(); it != m_lines.m_data.end(); ++it)
  {
    TransitId const & lineId = it->first;
    LineData & lineData = it->second;
    TransitId const & shapeId = lineData.m_shapeLink.m_shapeId;
    gtfs::StopTimes stopTimes = GetStopTimesForTrip(allStopTimes, lineData.m_gtfsTripId);

    if (stopTimes.size() < 2)
    {
      LOG(LINFO, ("Invalid stop times count for trip:", stopTimes.size(), lineData.m_gtfsTripId));
      linesForRemoval.push_back(it);
      continue;
    }

    lineData.m_stopIds.reserve(stopTimes.size());

    for (size_t i = 0; i < stopTimes.size() - 1; ++i)
    {
      auto const & stopTime1 = stopTimes[i];
      auto const & stopTime2 = stopTimes[i + 1];

      // This situation occurs even in valid GTFS feeds.
      if (stopTime1.stop_id == stopTime2.stop_id && stopTimes.size() > 2)
        continue;

      auto [stop1Id, stop1Hash] = GetStopIdAndHash(stopTime1.stop_id);
      auto [stop2Id, stop2Hash] = GetStopIdAndHash(stopTime2.stop_id);

      lineData.m_stopIds.push_back(stop1Id);
      if (!UpdateStop(stop1Id, stopTime1, stop1Hash, lineId))
        return false;

      if (i == stopTimes.size() - 2)
      {
        lineData.m_stopIds.push_back(stop2Id);
        if (!UpdateStop(stop2Id, stopTime2, stop2Hash, lineId))
          return false;
      }

      std::string const edgeHash =
          BuildHash(std::to_string(lineId), std::to_string(stop1Id), std::to_string(stop2Id));
      EdgeData data;
      data.m_featureId = m_idGeneratorEdges.MakeId(edgeHash);
      data.m_shapeLink.m_shapeId = shapeId;
      data.m_weight =
          static_cast<transit::EdgeWeight>(stopTime2.arrival_time.get_total_seconds() -
                                           stopTime1.departure_time.get_total_seconds());

      if (data.m_weight == 0)
      {
        double const distBetweenStopsM =
            stopTime2.shape_dist_traveled - stopTime1.shape_dist_traveled;
        if (distBetweenStopsM > 0)
          data.m_weight = std::ceil(distBetweenStopsM / kAvgTransitSpeedMpS);
      }

      auto [itEdge, insertedEdge] = m_edges.m_data.emplace(EdgeId(stop1Id, stop2Id, lineId), data);

      // There can be two identical pairs of stops on the same trip.
      if (!insertedEdge)
      {
        CHECK_EQUAL(itEdge->second.m_shapeLink.m_shapeId, data.m_shapeLink.m_shapeId,
                    (stopTime1.stop_id, stopTime2.stop_id));

        // We choose the most pessimistic alternative.
        if (data.m_weight > itEdge->second.m_weight)
          itEdge->second = data;
      }
    }
  }

  for (auto & it : linesForRemoval)
    m_lines.m_data.erase(it);

  return !m_edges.m_data.empty();
}

bool WorldFeed::FillLinesAndShapes()
{
  std::unordered_map<gtfs::Id, gtfs::Shape> shapes;
  for (const auto & shape : m_feed.get_shapes())
    shapes[shape.shape_id].emplace_back(shape);

  for (auto & shape : shapes)
  {
    std::sort(shape.second.begin(), shape.second.end(),
              base::LessBy(&gtfs::ShapePoint::shape_pt_sequence));
  }

  auto const getShape = [&shapes](gtfs::Id const & gtfsShapeId) -> gtfs::Shape const &
  { return shapes[gtfsShapeId]; };

  std::unordered_map<gtfs::Id, gtfs::StopTimes> stopTimes;
  for (const auto & stop_time : m_feed.get_stop_times())
    stopTimes[stop_time.trip_id].emplace_back(stop_time);

  for (auto & stop_time : stopTimes)
  {
    std::sort(stop_time.second.begin(), stop_time.second.end(),
              base::LessBy(&gtfs::StopTime::stop_sequence));
  }

  for (const auto & trip : m_feed.get_trips())
  {
    // We skip routes filtered on the route preparation stage.
    auto const itRoute = m_gtfsIdToHash[FieldIdx::RoutesIdx].find(trip.route_id);
    if (itRoute == m_gtfsIdToHash[FieldIdx::RoutesIdx].end())
      continue;

    // Skip trips with corrupted shapes.
    if (trip.shape_id.empty())
      continue;

    std::string const & routeHash = itRoute->second;

    std::string stopIds;

    for (auto const & stopTime : stopTimes[trip.trip_id])
      stopIds += stopTime.stop_id + kDelimiter;

    std::string const lineHash = BuildHash(routeHash, trip.shape_id, stopIds);
    auto const lineId = m_idGenerator.MakeId(lineHash);

    auto [itShape, insertedShape] = m_gtfsIdToHash[ShapesIdx].emplace(trip.shape_id, "");

    // Skip invalid shape.
    if (!insertedShape && itShape->second.empty())
      continue;

    if (insertedShape)
    {
      auto const & shapeItems = getShape(trip.shape_id);
      // Skip trips with corrupted shapes.
      if (shapeItems.size() < 2)
      {
        LOG(LINFO, ("Invalid shape. Length:", shapeItems.size(), "Shape id", trip.shape_id));
        continue;
      }

      AddShape(itShape, shapeItems, lineId);
    }

    auto [it, inserted] = m_lines.m_data.emplace(lineId, LineData());
    if (!inserted)
    {
      it->second.m_gtfsServiceIds.emplace(trip.service_id);
      continue;
    }

    TransitId const shapeId = m_idGenerator.MakeId(itShape->second);

    LineData data;
    data.m_title = trip.trip_short_name;
    data.m_routeId = m_idGenerator.MakeId(routeHash);
    data.m_shapeId = shapeId;
    data.m_gtfsTripId = trip.trip_id;
    data.m_gtfsServiceIds.emplace(trip.service_id);
    // |m_stopIds|, |m_intervals| and |m_serviceDays| will be filled on the next steps.
    it->second = std::move(data);

    m_gtfsIdToHash[TripsIdx].emplace(trip.trip_id, lineHash);
  }

  return !m_lines.m_data.empty() && !m_shapes.m_data.empty();
}

void WorldFeed::ModifyLinesAndShapes()
{
  std::vector<Link> links;
  links.reserve(m_lines.m_data.size());

  for (auto const & [lineId, lineData] : m_lines.m_data)
  {
    links.emplace_back(lineId, lineData.m_shapeId,
                       m_shapes.m_data[lineData.m_shapeId].m_points.size());
  }

  // We sort links by shape length so we could search for shapes in which i-th shape is included
  // only in the left part of the array [0, i).
  std::sort(links.begin(), links.end(),
            [](Link const & link1, Link const & link2)
            { return link1.m_shapeSize > link2.m_shapeSize; });

  size_t subShapesCount = 0;

  // Shape ids of shapes fully contained in other shapes.
  IdSet shapesForRemoval;

  // Shape id matching to the line id linked to this shape id.
  std::unordered_map<TransitId, TransitId> matchingCache;

  for (size_t i = 1; i < links.size(); ++i)
  {
    auto const lineIdNeedle = links[i].m_lineId;
    auto & lineDataNeedle = m_lines.m_data[lineIdNeedle];
    auto const shapeIdNeedle = links[i].m_shapeId;

    auto [itCache, inserted] = matchingCache.emplace(shapeIdNeedle, 0);
    if (!inserted)
    {
      if (itCache->second != 0)
      {
        lineDataNeedle.m_shapeId = 0;
        lineDataNeedle.m_shapeLink = m_lines.m_data[itCache->second].m_shapeLink;
      }
      continue;
    }

    auto const & pointsNeedle = m_shapes.m_data[shapeIdNeedle].m_points;
    auto const pointsNeedleRev = GetReversed(pointsNeedle);

    for (size_t j = 0; j < i; ++j)
    {
      auto const & lineIdHaystack = links[j].m_lineId;

      // We skip shapes which are already included into other shapes.
      if (m_lines.m_data[lineIdHaystack].m_shapeId == 0)
        continue;

      auto const shapeIdHaystack = links[j].m_shapeId;

      if (shapeIdHaystack == shapeIdNeedle)
        continue;

      auto const & pointsHaystack = m_shapes.m_data[shapeIdHaystack].m_points;

      auto const it = std::search(pointsHaystack.begin(), pointsHaystack.end(),
                                  pointsNeedle.begin(), pointsNeedle.end());

      if (it == pointsHaystack.end())
      {
        auto const itRev = std::search(pointsHaystack.begin(), pointsHaystack.end(),
                                       pointsNeedleRev.begin(), pointsNeedleRev.end());
        if (itRev == pointsHaystack.end())
          continue;

        // Shape with |pointsNeedleRev| polyline is fully contained in the shape with
        // |pointsHaystack| polyline.
        lineDataNeedle.m_shapeId = 0;
        lineDataNeedle.m_shapeLink.m_shapeId = shapeIdHaystack;
        lineDataNeedle.m_shapeLink.m_endIndex =
            static_cast<uint32_t>(std::distance(pointsHaystack.begin(), itRev));
        lineDataNeedle.m_shapeLink.m_startIndex = static_cast<uint32_t>(
            lineDataNeedle.m_shapeLink.m_endIndex + pointsNeedleRev.size() - 1);

        itCache->second = lineIdNeedle;
        shapesForRemoval.insert(shapeIdNeedle);
        ++subShapesCount;
        break;
      }

      // Shape with |pointsNeedle| polyline is fully contained in the shape |pointsHaystack|.
      lineDataNeedle.m_shapeId = 0;
      lineDataNeedle.m_shapeLink.m_shapeId = shapeIdHaystack;
      lineDataNeedle.m_shapeLink.m_startIndex =
          static_cast<uint32_t>(std::distance(pointsHaystack.begin(), it));

      CHECK_GREATER_OR_EQUAL(pointsNeedle.size(), 2, ());

      lineDataNeedle.m_shapeLink.m_endIndex =
          static_cast<uint32_t>(lineDataNeedle.m_shapeLink.m_startIndex + pointsNeedle.size() - 1);

      itCache->second = lineIdNeedle;
      shapesForRemoval.insert(shapeIdNeedle);
      ++subShapesCount;
      break;
    }
  }

  for (auto & [lineId, lineData] : m_lines.m_data)
  {
    if (lineData.m_shapeId == 0)
      continue;

    lineData.m_shapeLink.m_shapeId = lineData.m_shapeId;
    lineData.m_shapeLink.m_startIndex = 0;

    CHECK_GREATER_OR_EQUAL(m_shapes.m_data[lineData.m_shapeId].m_points.size(), 2, ());

    lineData.m_shapeLink.m_endIndex =
        static_cast<uint32_t>(m_shapes.m_data[lineData.m_shapeId].m_points.size() - 1);

    lineData.m_shapeId = 0;
  }

  for (auto const shapeId : shapesForRemoval)
    m_shapes.m_data.erase(shapeId);

  LOG(LINFO, ("Deleted", subShapesCount, "sub-shapes.", m_shapes.m_data.size(), "left."));
}

void WorldFeed::FillLinesSchedule()
{
  for (auto & [lineId, lineData] : m_lines.m_data)
  {
    auto const & tripId = lineData.m_gtfsTripId;
    auto const & frequencies = m_feed.get_frequencies(tripId);

    for (auto const & serviceId : lineData.m_gtfsServiceIds)
    {
      for (auto const & dateException : m_feed.get_calendar_dates(serviceId))
        lineData.m_schedule.AddDateException(dateException.date, dateException.exception_type,
                                             frequencies);

      std::optional<gtfs::CalendarItem> calendar = m_feed.get_calendar(serviceId);
      if (calendar)
        lineData.m_schedule.AddDatesInterval(calendar.value(), frequencies);
    }
  }
}

std::optional<Direction> WorldFeed::ProjectStopsToShape(
    ShapesIter & itShape, StopsOnLines const & stopsOnLines,
    std::unordered_map<TransitId, std::vector<size_t>> & stopsToIndexes)
{
  IdList const & stopIds = stopsOnLines.m_stopSeq;
  TransitId const shapeId = itShape->first;

  auto const tryProject = [&](Direction direction)
  {
    auto shape = itShape->second.m_points;
    std::optional<m2::PointD> prevPoint = std::nullopt;
    for (size_t i = 0; i < stopIds.size(); ++i)
    {
      auto const & stopId = stopIds[i];
      auto const itStop = m_stops.m_data.find(stopId);
      CHECK(itStop != m_stops.m_data.end(), (stopId));
      auto const & stop = itStop->second;

      size_t const prevIdx = i == 0 ? (direction == Direction::Forward ? 0 : shape.size() - 1)
                                    : stopsToIndexes[stopIds[i - 1]].back();
      auto const [curIdx, pointInserted] =
          PrepareNearestPointOnTrack(stop.m_point, prevPoint, prevIdx, direction, shape);

      if (curIdx > shape.size())
      {
        CHECK(!itShape->second.m_lineIds.empty(), (shapeId));
        TransitId const lineId = *stopsOnLines.m_lines.begin();

        LOG(LWARNING,
            ("Error projecting stops to the shape. GTFS trip id",
             m_lines.m_data[lineId].m_gtfsTripId, "shapeId", shapeId, "stopId", stopId, "i", i,
             "previous index on shape", prevIdx, "trips count", stopsOnLines.m_lines.size()));
        return false;
      }

      prevPoint = std::optional<m2::PointD>(stop.m_point);

      if (pointInserted)
      {
        for (auto & indexesList : stopsToIndexes)
        {
          for (auto & stopIndex : indexesList.second)
          {
            if (stopIndex >= curIdx)
              ++stopIndex;
          }
        }

        for (auto const & lineId : m_shapes.m_data[shapeId].m_lineIds)
        {
          auto & line = m_lines.m_data[lineId];

          if (line.m_shapeLink.m_startIndex >= curIdx)
            ++line.m_shapeLink.m_startIndex;

          if (line.m_shapeLink.m_endIndex >= curIdx)
            ++line.m_shapeLink.m_endIndex;
        }
      }

      stopsToIndexes[stopId].push_back(curIdx);
    }

    itShape->second.m_points = std::move(shape);
    return true;
  };

  if (tryProject(Direction::Forward))
    return Direction::Forward;

  if (tryProject(Direction::Backward))
    return Direction::Backward;

  return {};
}

std::unordered_map<TransitId, std::vector<StopsOnLines>> WorldFeed::GetStopsForShapeMatching()
{
  // Shape id and list of stop sequences matched to the corresponding lines.
  std::unordered_map<TransitId, std::vector<StopsOnLines>> stopsOnShapes;

  // We build lists of stops relevant to corresponding shapes. There could be multiple different
  // stops lists linked to the same shape.
  // Example: line 8, shapeId 52, stops: [25, 26, 27]. line 9, shapeId 52, stops: [10, 9, 8, 7, 6].

  for (auto const & [lineId, lineData] : m_lines.m_data)
  {
    auto & shapeData = stopsOnShapes[lineData.m_shapeLink.m_shapeId];
    bool found = false;

    for (auto & stopsOnLines : shapeData)
    {
      if (stopsOnLines.m_stopSeq == lineData.m_stopIds)
      {
        found = true;
        stopsOnLines.m_lines.insert(lineId);
        break;
      }
    }

    if (!found)
    {
      StopsOnLines stopsOnLines(lineData.m_stopIds);
      stopsOnLines.m_lines.insert(lineId);
      shapeData.emplace_back(std::move(stopsOnLines));
    }
  }

  return stopsOnShapes;
}

std::pair<size_t, size_t> WorldFeed::ModifyShapes()
{
  auto stopsOnShapes = GetStopsForShapeMatching();
  size_t invalidStopSequences = 0;
  size_t validStopSequences = 0;

  for (auto & [shapeId, stopsLists] : stopsOnShapes)
  {
    CHECK(!stopsLists.empty(), (shapeId));

    auto itShape = m_shapes.m_data.find(shapeId);
    CHECK(itShape != m_shapes.m_data.end(), (shapeId));

    std::unordered_map<TransitId, std::vector<size_t>> stopToShapeIndex;

    for (auto & stopsOnLines : stopsLists)
    {
      if (stopsOnLines.m_stopSeq.size() < 2)
      {
        TransitId const lineId = *stopsOnLines.m_lines.begin();
        LOG(LWARNING, ("Error in stops count. Lines count:", stopsOnLines.m_stopSeq.size(),
                       "GTFS trip id:", m_lines.m_data[lineId].m_gtfsTripId));
        stopsOnLines.m_isValid = false;
        ++invalidStopSequences;
      }
      else if (auto const direction = ProjectStopsToShape(itShape, stopsOnLines, stopToShapeIndex))
      {
        stopsOnLines.m_direction = *direction;
        ++validStopSequences;
      }
      else
      {
        stopsOnLines.m_isValid = false;
        ++invalidStopSequences;
      }
    }

    for (auto & stopsOnLines : stopsLists)
    {
      IdList const & stopIds = stopsOnLines.m_stopSeq;
      auto const & lineIds = stopsOnLines.m_lines;
      auto indexes = stopToShapeIndex;
      auto const direction = stopsOnLines.m_direction;

      size_t lastIndex = direction == Direction::Forward ? 0 : std::numeric_limits<size_t>::max();
      for (size_t i = 0; i < stopIds.size() - 1; ++i)
      {
        auto const stops = GetStopPairOnShape(indexes, stopsOnLines, i, lastIndex, direction);
        if (!stops && stopsOnLines.m_isValid)
        {
          stopsOnLines.m_isValid = false;
          ++invalidStopSequences;
          --validStopSequences;
        }

        for (auto const lineId : lineIds)
        {
          if (!stopsOnLines.m_isValid)
          {
            m_lines.m_data.erase(lineId);
            // todo: use std::erase_if after c++20
            for (auto it = m_edges.m_data.begin(); it != m_edges.m_data.end();)
            {
              if (it->first.m_lineId == lineId)
                it = m_edges.m_data.erase(it);
              else
                ++it;
            }
          }
          else
          {
            CHECK(stops, ());
            auto const [stop1, stop2] = *stops;
            lastIndex = stop2.m_index;

            // Update |EdgeShapeLink| with shape segment start and end points.
            auto itEdge = m_edges.m_data.find(EdgeId(stop1.m_id, stop2.m_id, lineId));
            if (itEdge == m_edges.m_data.end())
              continue;

            itEdge->second.m_shapeLink.m_startIndex = static_cast<uint32_t>(stop1.m_index);
            itEdge->second.m_shapeLink.m_endIndex = static_cast<uint32_t>(stop2.m_index);

            if (indexes[stop1.m_id].size() > 1)
              indexes[stop1.m_id].erase(indexes[stop1.m_id].begin());
          }
        }
      }
    }
  }

  return {invalidStopSequences, validStopSequences};
}

void WorldFeed::FillTransfers()
{
  for (auto const & transfer : m_feed.get_transfers())
  {
    if (transfer.transfer_type == gtfs::TransferType::NotPossible)
      continue;

    // Check that the two stops from the transfer are present in the global feed.
    auto const itStop1 = m_gtfsIdToHash[FieldIdx::StopsIdx].find(transfer.from_stop_id);
    if (itStop1 == m_gtfsIdToHash[FieldIdx::StopsIdx].end())
      continue;

    auto const itStop2 = m_gtfsIdToHash[FieldIdx::StopsIdx].find(transfer.to_stop_id);
    if (itStop2 == m_gtfsIdToHash[FieldIdx::StopsIdx].end())
      continue;

    TransitId const & stop1Id = m_idGenerator.MakeId(itStop1->second);
    TransitId const & stop2Id = m_idGenerator.MakeId(itStop2->second);

    // Usual case in GTFS feeds (don't ask why). We skip duplicates.
    if (stop1Id == stop2Id)
      continue;

    std::string const transitHash = BuildHash(itStop1->second, itStop2->second);
    TransitId const transitId = m_idGenerator.MakeId(transitHash);

    TransferData data;
    data.m_stopsIds = {stop1Id, stop2Id};

    auto & stop1 = m_stops.m_data.at(stop1Id);
    auto & stop2 = m_stops.m_data.at(stop2Id);

    // Coordinate of the transfer is the midpoint between two stops.
    data.m_point = stop1.m_point.Mid(stop2.m_point);

    if (m_transfers.m_data.emplace(transitId, data).second)
    {
      EdgeTransferId const transferId(stop1Id, stop2Id);

      EdgeData data;
      data.m_weight = static_cast<EdgeWeight>(transfer.min_transfer_time);
      std::string const edgeHash = BuildHash(std::to_string(kInvalidLineId),
                                             std::to_string(stop1Id), std::to_string(stop2Id));

      data.m_featureId = m_idGeneratorEdges.MakeId(edgeHash);

      if (!m_edgesTransfers.m_data.emplace(transferId, data).second)
        LOG(LWARNING, ("Transfers copy", transfer.from_stop_id, transfer.to_stop_id));

      LinkTransferIdToStop(stop1, transitId);
      LinkTransferIdToStop(stop2, transitId);
    }
  }
}

void WorldFeed::FillGates()
{
  std::unordered_map<std::string, std::vector<GateData>> parentToGates;
  for (auto const & stop : m_feed.get_stops())
  {
    if (stop.location_type == gtfs::StopLocationType::EntranceExit && !stop.parent_station.empty())
    {
      GateData gate;
      gate.m_gtfsId = stop.stop_id;
      gate.m_point = mercator::FromLatLon(stop.stop_lat, stop.stop_lon);
      gate.m_isEntrance = true;
      gate.m_isExit = true;
      parentToGates[stop.parent_station].emplace_back(gate);
    }
  }

  if (parentToGates.empty())
    return;

  for (auto & [stopId, stopData] : m_stops.m_data)
  {
    if (stopData.m_gtfsParentId.empty())
      continue;

    auto it = parentToGates.find(stopData.m_gtfsParentId);
    if (it == parentToGates.end())
      continue;

    for (auto & gate : it->second)
    {
      TimeFromGateToStop weight;
      weight.m_stopId = stopId;
      // We do not divide distance by average speed because average speed in stations, terminals,
      // etc is roughly 1 meter/second.
      weight.m_timeSeconds = mercator::DistanceOnEarth(stopData.m_point, gate.m_point);

      gate.m_weights.emplace_back(weight);
    }
  }

  for (auto const & pairParentGates : parentToGates)
  {
    auto const & gates = pairParentGates.second;
    for (auto const & gate : gates)
    {
      TransitId id;
      std::tie(id, std::ignore) = GetStopIdAndHash(gate.m_gtfsId);
      m_gates.m_data.emplace(id, gate);
    }
  }
}

bool WorldFeed::SpeedExceedsMaxVal(EdgeId const & edgeId, EdgeData const & edgeData)
{
  m2::PointD const & stop1 = m_stops.m_data.at(edgeId.m_fromStopId).m_point;
  m2::PointD const & stop2 = m_stops.m_data.at(edgeId.m_toStopId).m_point;

  static double const maxSpeedMpS = measurement_utils::KmphToMps(routing::kTransitMaxSpeedKMpH);
  double const speedMpS = mercator::DistanceOnEarth(stop1, stop2) / edgeData.m_weight;

  bool speedExceedsMaxVal = speedMpS > maxSpeedMpS;
  if (speedExceedsMaxVal)
  {
    LOG(LWARNING,
        ("Invalid edge weight conflicting with kTransitMaxSpeedKMpH:", edgeId.m_fromStopId,
         edgeId.m_toStopId, edgeId.m_lineId, "speed (km/h):", measurement_utils::MpsToKmph(speedMpS),
         "maxSpeed (km/h):", routing::kTransitMaxSpeedKMpH));
  }

  return speedExceedsMaxVal;
}

bool WorldFeed::ClearFeedByLineIds(std::unordered_set<TransitId> const & corruptedLineIds)
{
  std::unordered_set<TransitId> corruptedRouteIds;
  std::unordered_set<TransitId> corruptedShapeIds;
  std::unordered_set<TransitId> corruptedNetworkIds;

  for (auto lineId : corruptedLineIds)
  {
    LineData const & lineData = m_lines.m_data[lineId];
    corruptedRouteIds.emplace(lineData.m_routeId);
    corruptedNetworkIds.emplace(m_routes.m_data.at(lineData.m_routeId).m_networkId);
    corruptedShapeIds.emplace(lineData.m_shapeId);
  }

  for (auto const & [lineId, lineData] : m_lines.m_data)
  {
    if (corruptedLineIds.find(lineId) != corruptedLineIds.end())
      continue;

    // We keep in lists for deletion only ids which are not linked to valid entities.
    DeleteIfExists(corruptedRouteIds, lineData.m_routeId);
    DeleteIfExists(corruptedShapeIds, lineData.m_shapeId);
  }

  for (auto const & [routeId, routeData] : m_routes.m_data)
  {
    if (corruptedRouteIds.find(routeId) == corruptedRouteIds.end())
      DeleteIfExists(corruptedNetworkIds, routeData.m_networkId);
  }

  DeleteAllEntriesByIds(m_shapes.m_data, corruptedShapeIds);
  DeleteAllEntriesByIds(m_routes.m_data, corruptedRouteIds);
  DeleteAllEntriesByIds(m_networks.m_data, corruptedNetworkIds);
  DeleteAllEntriesByIds(m_lines.m_data, corruptedLineIds);

  std::unordered_set<TransitId> corruptedStopIds;

  // We fill |corruptedStopIds| and delete corresponding edges from |m_edges|.
  for (auto it = m_edges.m_data.begin(); it != m_edges.m_data.end();)
  {
    if (corruptedLineIds.find(it->first.m_lineId) != corruptedLineIds.end())
    {
      corruptedStopIds.emplace(it->first.m_fromStopId);
      corruptedStopIds.emplace(it->first.m_toStopId);
      it = m_edges.m_data.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // We remove transfer edges linked to the corrupted stop ids.
  for (auto it = m_edgesTransfers.m_data.begin(); it != m_edgesTransfers.m_data.end();)
  {
    if (corruptedStopIds.find(it->first.m_fromStopId) != corruptedStopIds.end() ||
        corruptedStopIds.find(it->first.m_toStopId) != corruptedStopIds.end())
    {
      it = m_edgesTransfers.m_data.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // We remove transfers linked to the corrupted stop ids.
  for (auto it = m_transfers.m_data.begin(); it != m_transfers.m_data.end();)
  {
    auto & transferData = it->second;
    DeleteAllEntriesByIds(transferData.m_stopsIds, corruptedStopIds);

    if (transferData.m_stopsIds.size() < 2)
      it = m_transfers.m_data.erase(it);
    else
      ++it;
  }

  // We remove gates linked to the corrupted stop ids.
  for (auto it = m_gates.m_data.begin(); it != m_gates.m_data.end();)
  {
    auto & gateData = it->second;

    for (auto itW = gateData.m_weights.begin(); itW != gateData.m_weights.end();)
    {
      if (corruptedStopIds.find(itW->m_stopId) != corruptedStopIds.end())
        itW = gateData.m_weights.erase(itW);
      else
        ++itW;
    }

    if (gateData.m_weights.empty())
      it = m_gates.m_data.erase(it);
    else
      ++it;
  }

  DeleteAllEntriesByIds(m_stops.m_data, corruptedStopIds);

  LOG(LINFO, ("Count of lines linked to the corrupted edges:", corruptedLineIds.size(),
              ", routes:", corruptedRouteIds.size(), ", networks:", corruptedNetworkIds.size(),
              ", shapes:", corruptedShapeIds.size()));

  return !m_networks.m_data.empty() && !m_routes.m_data.empty() && !m_lines.m_data.empty() &&
         !m_stops.m_data.empty() && !m_edges.m_data.empty();
}

bool WorldFeed::UpdateEdgeWeights()
{
  std::unordered_set<TransitId> corruptedLineIds;

  for (auto & [edgeId, edgeData] : m_edges.m_data)
  {
    if (edgeData.m_weight == 0)
    {
      auto const & polyLine = m_shapes.m_data.at(edgeData.m_shapeLink.m_shapeId).m_points;

      bool const isInverted = edgeData.m_shapeLink.m_startIndex > edgeData.m_shapeLink.m_endIndex;

      size_t const startIndex =
          isInverted ? edgeData.m_shapeLink.m_endIndex : edgeData.m_shapeLink.m_startIndex;
      size_t const endIndex =
          isInverted ? edgeData.m_shapeLink.m_startIndex : edgeData.m_shapeLink.m_endIndex;

      auto const edgePolyLine =
          std::vector<m2::PointD>(polyLine.begin() + startIndex, polyLine.begin() + endIndex + 1);

      if (edgePolyLine.size() < 1)
      {
        LOG(LWARNING, ("Invalid edge with too short shape polyline:", edgeId.m_fromStopId,
                       edgeId.m_toStopId, edgeId.m_lineId));
        corruptedLineIds.emplace(edgeId.m_lineId);
        continue;
      }

      double edgeLengthM = 0.0;

      for (size_t i = 0; i < edgePolyLine.size() - 1; ++i)
        edgeLengthM += mercator::DistanceOnEarth(edgePolyLine[i], edgePolyLine[i + 1]);

      if (edgeLengthM == 0.0)
      {
        LOG(LWARNING, ("Invalid edge with 0 length:", edgeId.m_fromStopId, edgeId.m_toStopId,
                       edgeId.m_lineId));
        corruptedLineIds.emplace(edgeId.m_lineId);
        continue;
      }

      edgeData.m_weight = std::ceil(edgeLengthM / kAvgTransitSpeedMpS);
    }

    // We check that edge weight doesn't violate A* invariant in routing runtime.
    if (SpeedExceedsMaxVal(edgeId, edgeData))
      corruptedLineIds.emplace(edgeId.m_lineId);
  }

  if (!corruptedLineIds.empty())
    return ClearFeedByLineIds(corruptedLineIds);

  return true;
}

bool WorldFeed::SetFeed(gtfs::Feed && feed)
{
  m_feed = std::move(feed);
  m_gtfsIdToHash.resize(FieldIdx::IdxCount);

  // The order of the calls is important. First we set default feed language. Then fill networks.
  // Then, based on network ids, we generate routes and so on.

  // Code for setting default feed language is commented until we need to extract language name
  // and save feed translations.
  // SetFeedLanguage();

  if (!FillNetworks())
  {
    LOG(LWARNING, ("Could not fill networks."));
    return false;
  }
  LOG(LINFO, ("Filled networks."));

  if (!FillRoutes())
  {
    LOG(LWARNING, ("Could not fill routes."));
    return false;
  }
  LOG(LINFO, ("Filled routes."));

  if (!FillLinesAndShapes())
  {
    LOG(LWARNING, ("Could not fill lines.", m_lines.m_data.size()));
    return false;
  }
  LOG(LINFO, ("Filled lines and shapes."));

  ModifyLinesAndShapes();
  LOG(LINFO, ("Modified lines and shapes."));

  FillLinesSchedule();

  LOG(LINFO, ("Filled schedule for lines."));

  if (!FillStopsEdges())
  {
    LOG(LWARNING, ("Could not fill stops", m_stops.m_data.size()));
    return false;
  }
  LOG(LINFO, ("Filled stop timetables and road graph edges."));

  auto const [badShapesCount, goodShapesCount] = ModifyShapes();
  LOG(LINFO, ("Modified shapes."));

  if (goodShapesCount == 0 && badShapesCount >= 0)
  {
    LOG(LINFO, ("Corrupted shapes count exceeds allowable limit."));
    return false;
  }

  FillTransfers();
  LOG(LINFO, ("Filled transfers."));

  FillGates();
  LOG(LINFO, ("Filled gates."));

  if (!UpdateEdgeWeights())
  {
    LOG(LWARNING, ("Found inconsistencies while updating edge weights."));
    return false;
  }

  LOG(LINFO, ("Updated edges weights."));
  return true;
}

void Networks::Write(IdSet const & ids, std::ofstream & stream) const
{
  for (auto networkId : ids)
  {
    auto const & networkTitle = m_data.find(networkId)->second;

    auto node = base::NewJSONObject();
    ToJSONObject(*node, "id", networkId);
    ToJSONObject(*node, "title", networkTitle);

    WriteJson(node.get(), stream);
  }
}

void Routes::Write(IdSet const & ids, std::ofstream & stream) const
{
  for (auto routeId : ids)
  {
    auto const & route = m_data.find(routeId)->second;
    auto node = base::NewJSONObject();
    ToJSONObject(*node, "id", routeId);
    ToJSONObject(*node, "network_id", route.m_networkId);
    ToJSONObject(*node, "color", route.m_color);
    ToJSONObject(*node, "type", route.m_routeType);
    ToJSONObject(*node, "title", route.m_title);

    WriteJson(node.get(), stream);
  }
}

void Lines::Write(std::unordered_map<TransitId, LineSegmentInRegion> const & ids,
                  std::ofstream & stream) const
{
  for (auto const & [lineId, data] : ids)
  {
    auto const & line = m_data.find(lineId)->second;
    auto node = base::NewJSONObject();
    ToJSONObject(*node, "id", lineId);
    ToJSONObject(*node, "route_id", line.m_routeId);
    json_object_set_new(node.get(), "shape", ShapeLinkToJson(data.m_shapeLink).release());
    ToJSONObject(*node, "title", line.m_title);

    // Save only stop ids inside current region.
    json_object_set_new(node.get(), "stops_ids", VectorToJson(data.m_stopIds).release());
    json_object_set_new(node.get(), "schedule", ScheduleToJson(line.m_schedule).release());

    WriteJson(node.get(), stream);
  }
}

void LinesMetadata::Write(std::unordered_map<TransitId, LineSegmentInRegion> const & linesInRegion,
                          std::ofstream & stream) const
{
  for (auto const & [lineId, lineData] : linesInRegion)
  {
    auto it = m_data.find(lineId);
    if (it == m_data.end())
      continue;

    auto const & lineMetaData = it->second;
    auto node = base::NewJSONObject();
    ToJSONObject(*node, "id", lineId);

    auto segmentsOnShape = base::NewJSONArray();

    for (auto const & lineSegmentOrder : lineMetaData)
    {
      auto segmentData = base::NewJSONObject();
      ToJSONObject(*segmentData, "order", lineSegmentOrder.m_order);
      ToJSONObject(*segmentData, "start_index", lineSegmentOrder.m_segment.m_startIdx);
      ToJSONObject(*segmentData, "end_index", lineSegmentOrder.m_segment.m_endIdx);
      json_array_append_new(segmentsOnShape.get(), segmentData.release());
    }

    json_object_set_new(node.get(), "shape_segments", segmentsOnShape.release());

    WriteJson(node.get(), stream);
  }
}

void Shapes::Write(IdSet const & ids, std::ofstream & stream) const
{
  for (auto shapeId : ids)
  {
    auto const & shape = m_data.find(shapeId)->second;
    auto node = base::NewJSONObject();
    ToJSONObject(*node, "id", shapeId);
    auto pointsArr = base::NewJSONArray();

    for (auto const & point : shape.m_points)
      json_array_append_new(pointsArr.get(), PointToJson(point).release());

    json_object_set_new(node.get(), "points", pointsArr.release());

    WriteJson(node.get(), stream);
  }
}

void Stops::Write(IdSet const & ids, std::ofstream & stream) const
{
  for (auto stopId : ids)
  {
    auto const & stop = m_data.find(stopId)->second;
    auto node = base::NewJSONObject();

    ToJSONObject(*node, "id", stopId);

    if (stop.m_osmId != 0)
    {
      ToJSONObject(*node, "osm_id", stop.m_osmId);

      if (stop.m_featureId != 0)
        ToJSONObject(*node, "feature_id", stop.m_featureId);
    }

    json_object_set_new(node.get(), "point", PointToJson(stop.m_point).release());

    if (!stop.m_title.empty())
      ToJSONObject(*node, "title", stop.m_title);

    if (!stop.m_timetable.empty())
    {
      auto timeTableArr = base::NewJSONArray();

      for (auto const & [lineId, timeIntervals] : stop.m_timetable)
      {
        auto lineTimetableItem = base::NewJSONObject();
        ToJSONObject(*lineTimetableItem, "line_id", lineId);

        std::vector<size_t> rawValues;
        rawValues.reserve(timeIntervals.size());

        for (auto const & timeInterval : timeIntervals)
          rawValues.push_back(timeInterval.GetRaw());

        json_object_set_new(lineTimetableItem.get(), "intervals",
                            VectorToJson(rawValues).release());
        json_array_append_new(timeTableArr.get(), lineTimetableItem.release());
      }

      json_object_set_new(node.get(), "timetable", timeTableArr.release());
    }

    if (!stop.m_transferIds.empty())
      json_object_set_new(node.get(), "transfer_ids", VectorToJson(stop.m_transferIds).release());

    WriteJson(node.get(), stream);
  }
}

void Edges::Write(IdEdgeSet const & ids, std::ofstream & stream) const
{
  for (auto const & edgeId : ids)
  {
    auto const & edge = m_data.find(edgeId)->second;
    auto node = base::NewJSONObject();
    ToJSONObject(*node, "line_id", edgeId.m_lineId);
    ToJSONObject(*node, "stop_id_from", edgeId.m_fromStopId);
    ToJSONObject(*node, "stop_id_to", edgeId.m_toStopId);

    CHECK_NOT_EQUAL(edge.m_featureId, std::numeric_limits<uint32_t>::max(),
                    (edgeId.m_lineId, edgeId.m_fromStopId, edgeId.m_toStopId));
    ToJSONObject(*node, "feature_id", edge.m_featureId);

    CHECK_GREATER(edge.m_weight, 0, (edgeId.m_fromStopId, edgeId.m_toStopId, edgeId.m_lineId));
    ToJSONObject(*node, "weight", edge.m_weight);

    json_object_set_new(node.get(), "shape", ShapeLinkToJson(edge.m_shapeLink).release());

    WriteJson(node.get(), stream);
  }
}

void EdgesTransfer::Write(IdEdgeTransferSet const & ids, std::ofstream & stream) const
{
  for (auto const & edgeTransferId : ids)
  {
    auto const & edgeData = m_data.find(edgeTransferId)->second;
    auto node = base::NewJSONObject();

    ToJSONObject(*node, "stop_id_from", edgeTransferId.m_fromStopId);
    ToJSONObject(*node, "stop_id_to", edgeTransferId.m_toStopId);
    ToJSONObject(*node, "weight", edgeData.m_weight);

    CHECK_NOT_EQUAL(edgeData.m_featureId, std::numeric_limits<uint32_t>::max(),
                    (edgeTransferId.m_fromStopId, edgeTransferId.m_toStopId));
    ToJSONObject(*node, "feature_id", edgeData.m_featureId);

    WriteJson(node.get(), stream);
  }
}

void Transfers::Write(IdSet const & ids, std::ofstream & stream) const
{
  for (auto transferId : ids)
  {
    auto const & transfer = m_data.find(transferId)->second;

    auto node = base::NewJSONObject();
    ToJSONObject(*node, "id", transferId);
    json_object_set_new(node.get(), "point", PointToJson(transfer.m_point).release());
    json_object_set_new(node.get(), "stops_ids", VectorToJson(transfer.m_stopsIds).release());

    WriteJson(node.get(), stream);
  }
}

void Gates::Write(IdSet const & ids, std::ofstream & stream) const
{
  for (auto gateId : ids)
  {
    auto const & gate = m_data.find(gateId)->second;
    if (gate.m_weights.empty())
      continue;

    auto node = base::NewJSONObject();
    if (gate.m_osmId == 0)
      ToJSONObject(*node, "id", gateId);
    else
      ToJSONObject(*node, "osm_id", gate.m_osmId);

    auto weightsArr = base::NewJSONArray();

    for (auto const & weight : gate.m_weights)
    {
      auto weightJson = base::NewJSONObject();
      ToJSONObject(*weightJson, "stop_id", weight.m_stopId);
      ToJSONObject(*weightJson, "time_to_stop", weight.m_timeSeconds);
      json_array_append_new(weightsArr.get(), weightJson.release());
    }

    json_object_set_new(node.get(), "weights", weightsArr.release());
    ToJSONObject(*node, "exit", gate.m_isExit);
    ToJSONObject(*node, "entrance", gate.m_isEntrance);

    json_object_set_new(node.get(), "point", PointToJson(gate.m_point).release());

    WriteJson(node.get(), stream);
  }
}

void WorldFeed::SplitFeedIntoRegions()
{
  LOG(LINFO, ("Started splitting feed into regions."));

  SplitStopsBasedData();
  LOG(LINFO, ("Split stops into", m_splitting.m_stops.size(), "regions."));
  SplitLinesBasedData();
  SplitSupplementalData();

  m_feedIsSplitIntoRegions = true;

  LOG(LINFO, ("Finished splitting feed into regions."));
}

void WorldFeed::SplitStopsBasedData()
{
  // Fill regional stops from edges.
  for (auto const & [edgeId, data] : m_edges.m_data)
  {
    auto const [regFrom, regTo] = ExtendRegionsByPair(edgeId.m_fromStopId, edgeId.m_toStopId);
    AddToRegions(m_splitting.m_edges, edgeId, regFrom);
    AddToRegions(m_splitting.m_edges, edgeId, regTo);
  }

  // Fill regional stops from transfer edges.
  for (auto const & [edgeTransferId, data] : m_edgesTransfers.m_data)
  {
    auto const [regFrom, regTo] =
        ExtendRegionsByPair(edgeTransferId.m_fromStopId, edgeTransferId.m_toStopId);
    AddToRegions(m_splitting.m_edgesTransfers, edgeTransferId, regFrom);
    AddToRegions(m_splitting.m_edgesTransfers, edgeTransferId, regTo);
  }
}

bool IsSplineSubset(IdList const & stops, IdList const & stopsOther)
{
  if (stops.size() >= stopsOther.size())
    return false;

  for (TransitId stopId : stops)
  {
    if (!base::IsExist(stopsOther, stopId))
      return false;
  }

  return true;
}

std::optional<TransitId> WorldFeed::GetParentLineForSpline(TransitId lineId) const
{
  LineData const & lineData = m_lines.m_data.at(lineId);
  IdList const & stops = lineData.m_stopIds;
  TransitId const routeId = lineData.m_routeId;

  for (auto const & [lineIdOther, lineData] : m_lines.m_data)
  {
    if (lineIdOther == lineId)
      continue;

    LineData const & lineDataOther = m_lines.m_data.at(lineIdOther);
    if (lineDataOther.m_routeId != routeId)
      continue;

    if (IsSplineSubset(stops, lineDataOther.m_stopIds))
      return lineIdOther;
  }

  return std::nullopt;
}

TransitId WorldFeed::GetSplineParent(TransitId lineId, std::string const & region) const
{
  TransitId parentId = lineId;
  auto const & linesInRegion = m_splitting.m_lines.at(region);
  auto itSplineParent = linesInRegion.find(parentId);

  while (itSplineParent != linesInRegion.end())
  {
    if (!itSplineParent->second.m_splineParent)
      break;

    parentId = itSplineParent->second.m_splineParent.value();
    itSplineParent = linesInRegion.find(parentId);
  }

  return parentId;
}

bool WorldFeed::PrepareEdgesInRegion(std::string const & region)
{
  auto & edgeIdsInRegion = m_splitting.m_edges.at(region);

  for (auto const & edgeId : edgeIdsInRegion)
  {
    TransitId const parentLineId = GetSplineParent(edgeId.m_lineId, region);
    TransitId const shapeId = m_lines.m_data.at(parentLineId).m_shapeLink.m_shapeId;

    auto & edgeData = m_edges.m_data.at(edgeId);

    auto itShape = m_shapes.m_data.find(shapeId);
    CHECK(itShape != m_shapes.m_data.end(), ("Shape does not exist."));

    auto const & shapePoints = itShape->second.m_points;
    CHECK(!shapePoints.empty(), ("Shape is empty."));

    auto const it = m_edgesOnShapes.find(edgeId);
    if (it == m_edgesOnShapes.end())
      continue;

    for (auto const & polyline : it->second)
    {
      std::tie(edgeData.m_shapeLink.m_startIndex, edgeData.m_shapeLink.m_endIndex) =
          FindSegmentOnShape(shapePoints, polyline);
      if (!(edgeData.m_shapeLink.m_startIndex == 0 && edgeData.m_shapeLink.m_endIndex == 0))
        break;
    }

    if (edgeData.m_shapeLink.m_startIndex == 0 && edgeData.m_shapeLink.m_endIndex == 0)
    {
      for (auto const & polyline : it->second)
      {
        std::tie(edgeData.m_shapeLink.m_startIndex, edgeData.m_shapeLink.m_endIndex) =
            FindSegmentOnShape(shapePoints, GetReversed(polyline));
        if (!(edgeData.m_shapeLink.m_startIndex == 0 && edgeData.m_shapeLink.m_endIndex == 0))
          break;
      }
    }

    if (edgeData.m_shapeLink.m_startIndex == 0 && edgeData.m_shapeLink.m_endIndex == 0)
    {
      std::tie(edgeData.m_shapeLink.m_startIndex, edgeData.m_shapeLink.m_endIndex) =
          FindPointsOnShape(shapePoints, it->second[0].front(), it->second[0].back());
    }

    CHECK(edgeData.m_shapeLink.m_startIndex != 0 || edgeData.m_shapeLink.m_endIndex != 0,
          (edgeData.m_shapeLink.m_shapeId));

    auto const & pointOnShapeStart = shapePoints[edgeData.m_shapeLink.m_startIndex];
    auto const & pointOnShapeEnd = shapePoints[edgeData.m_shapeLink.m_endIndex];

    auto const & pointStopStart = m_stops.m_data.at(edgeId.m_fromStopId).m_point;

    if (mercator::DistanceOnEarth(pointOnShapeStart, pointStopStart) >
        mercator::DistanceOnEarth(pointOnShapeEnd, pointStopStart))
    {
      std::swap(edgeData.m_shapeLink.m_startIndex, edgeData.m_shapeLink.m_endIndex);
    }
  }

  return !edgeIdsInRegion.empty();
}

void WorldFeed::SplitLinesBasedData()
{
  std::map<std::string, std::set<TransitId>> additionalStops;
  // Fill regional lines and corresponding shapes and routes.
  for (auto const & [lineId, lineData] : m_lines.m_data)
  {
    for (auto const & [region, stopIds] : m_splitting.m_stops)
    {
      auto const & [firstStopIdx, lastStopIdx] = GetStopsRange(lineData.m_stopIds, stopIds);

      // Line doesn't intersect this region.
      if (!StopIndexIsSet(firstStopIdx))
        continue;

      auto & lineInRegion = m_splitting.m_lines[region][lineId];

      // We save stop ids which belong to the region and its surroundings.
      for (size_t i = firstStopIdx; i <= lastStopIdx; ++i)
      {
        lineInRegion.m_stopIds.emplace_back(lineData.m_stopIds[i]);
        additionalStops[region].insert(lineData.m_stopIds[i]);
      }

      m_splitting.m_shapes[region].emplace(lineData.m_shapeLink.m_shapeId);
      m_splitting.m_routes[region].emplace(lineData.m_routeId);
    }
  }

  for (auto & [region, linesInRegion] : m_splitting.m_lines)
  {
    for (auto & [lineId, lineInRegion] : linesInRegion)
      lineInRegion.m_splineParent = GetParentLineForSpline(lineId);
  }

  for (auto & [region, edges] : m_splitting.m_edges)
  {
    PrepareEdgesInRegion(region);
  }

  for (auto & [region, linesInRegion] : m_splitting.m_lines)
  {
    for (auto & [lineId, lineInRegion] : linesInRegion)
    {
      auto const & lineData = m_lines.m_data.at(lineId);
      auto const & stopIds = m_splitting.m_stops.at(region);
      auto const & [firstStopIdx, lastStopIdx] = GetStopsRange(lineData.m_stopIds, stopIds);

      if (firstStopIdx == lastStopIdx)
        continue;

      CHECK(StopIndexIsSet(firstStopIdx), ());

      lineInRegion.m_shapeLink.m_shapeId = lineData.m_shapeLink.m_shapeId;

      auto const edgeFirst = m_edges.m_data.at(
          EdgeId(lineData.m_stopIds[firstStopIdx], lineData.m_stopIds[firstStopIdx + 1], lineId));
      if (edgeFirst.m_shapeLink.m_startIndex == 0 && edgeFirst.m_shapeLink.m_endIndex == 0)
        continue;

      auto const edgeLast = m_edges.m_data.at(
          EdgeId(lineData.m_stopIds[lastStopIdx - 1], lineData.m_stopIds[lastStopIdx], lineId));
      if (edgeLast.m_shapeLink.m_startIndex == 0 && edgeLast.m_shapeLink.m_endIndex == 0)
        continue;

      lineInRegion.m_shapeLink.m_startIndex =
          std::min(edgeFirst.m_shapeLink.m_startIndex, edgeLast.m_shapeLink.m_endIndex);
      lineInRegion.m_shapeLink.m_endIndex =
          std::max(edgeFirst.m_shapeLink.m_startIndex, edgeLast.m_shapeLink.m_endIndex);

      if (lineInRegion.m_shapeLink.m_startIndex == 0 && lineInRegion.m_shapeLink.m_endIndex == 0)
      {
        CHECK_EQUAL(edgeFirst.m_shapeLink.m_shapeId, edgeLast.m_shapeLink.m_shapeId, ());
        CHECK_EQUAL(edgeFirst.m_shapeLink.m_startIndex, edgeLast.m_shapeLink.m_endIndex, ());
        CHECK_EQUAL(edgeFirst.m_shapeLink.m_endIndex, edgeLast.m_shapeLink.m_startIndex, ());

        lineInRegion.m_shapeLink.m_startIndex =
            std::min(edgeFirst.m_shapeLink.m_startIndex, edgeFirst.m_shapeLink.m_endIndex);
        lineInRegion.m_shapeLink.m_endIndex =
            std::max(edgeFirst.m_shapeLink.m_startIndex, edgeFirst.m_shapeLink.m_endIndex);
      }

      m_splitting.m_shapes[region].emplace(lineData.m_shapeLink.m_shapeId);
      m_splitting.m_routes[region].emplace(lineData.m_routeId);
    }
  }

  for (auto const & [region, ids] : additionalStops)
    m_splitting.m_stops[region].insert(ids.begin(), ids.end());

  // Fill regional networks based on routes.
  for (auto const & [region, routeIds] : m_splitting.m_routes)
  {
    for (auto routeId : routeIds)
      m_splitting.m_networks[region].emplace(m_routes.m_data[routeId].m_networkId);
  }
}

void WorldFeed::SplitSupplementalData()
{
  for (auto const & [gateId, gateData] : m_gates.m_data)
  {
    for (auto const & weight : gateData.m_weights)
      AddToRegionsIfMatches(m_splitting.m_gates, gateId, m_splitting.m_stops, weight.m_stopId);
  }

  for (auto const & [transferId, transferData] : m_transfers.m_data)
  {
    for (auto const & stopId : transferData.m_stopsIds)
      AddToRegionsIfMatches(m_splitting.m_transfers, transferId, m_splitting.m_stops, stopId);
  }
}

std::pair<Regions, Regions> WorldFeed::ExtendRegionsByPair(TransitId fromId, TransitId toId)
{
  auto const & pointFrom = m_stops.m_data[fromId].m_point;
  auto const & pointTo = m_stops.m_data[toId].m_point;
  auto const regionsFrom = AddToRegions(m_splitting.m_stops, m_affiliation, fromId, pointFrom);
  auto const regionsTo = AddToRegions(m_splitting.m_stops, m_affiliation, toId, pointTo);

  AddToRegions(m_splitting.m_stops, toId, regionsFrom);
  AddToRegions(m_splitting.m_stops, fromId, regionsTo);
  return {regionsFrom, regionsTo};
}

void WorldFeed::SaveRegions(std::string const & worldFeedDir, std::string const & region,
                            bool overwrite)
{
  auto const path = base::JoinPath(worldFeedDir, region);
  CHECK(Platform::MkDirRecursively(path), (path));

  CHECK(DumpData(m_networks, m_splitting.m_networks[region], base::JoinPath(path, kNetworksFile),
                 overwrite),
        ());
  CHECK(DumpData(m_routes, m_splitting.m_routes[region], base::JoinPath(path, kRoutesFile),
                 overwrite),
        ());
  CHECK(DumpData(m_lines, m_splitting.m_lines[region], base::JoinPath(path, kLinesFile), overwrite),
        ());

  CHECK(DumpData(m_linesMetadata, m_splitting.m_lines[region],
                 base::JoinPath(path, kLinesMetadataFile), overwrite),
        ());

  CHECK(DumpData(m_shapes, m_splitting.m_shapes[region], base::JoinPath(path, kShapesFile),
                 overwrite),
        ());
  CHECK(DumpData(m_stops, m_splitting.m_stops[region], base::JoinPath(path, kStopsFile), overwrite),
        ());
  CHECK(DumpData(m_edges, m_splitting.m_edges[region], base::JoinPath(path, kEdgesFile), overwrite),
        ());
  CHECK(DumpData(m_edgesTransfers, m_splitting.m_edgesTransfers[region],
                 base::JoinPath(path, kEdgesTransferFile), overwrite),
        ());
  CHECK(DumpData(m_transfers, m_splitting.m_transfers[region], base::JoinPath(path, kTransfersFile),
                 overwrite),
        ());
  CHECK(DumpData(m_gates, m_splitting.m_gates[region], base::JoinPath(path, kGatesFile), overwrite),
        ());
}

bool WorldFeed::Save(std::string const & worldFeedDir, bool overwrite)
{
  CHECK(!worldFeedDir.empty(), ());
  CHECK(!m_networks.m_data.empty(), ());

  if (m_routes.m_data.empty() || m_lines.m_data.empty() || m_stops.m_data.empty() ||
      m_shapes.m_data.empty())
  {
    LOG(LINFO, ("Skipping feed with routes, lines, stops, shapes sizes:", m_routes.m_data.size(),
                m_lines.m_data.size(), m_stops.m_data.size(), m_shapes.m_data.size()));
    return false;
  }

  CHECK(!m_edges.m_data.empty(), ());

  if (!m_feedIsSplitIntoRegions)
    SplitFeedIntoRegions();

  LOG(LINFO, ("Saving feed to", worldFeedDir));
  for (auto const & regionAndData : m_splitting.m_stops)
    SaveRegions(worldFeedDir, regionAndData.first, overwrite);

  return true;
}

void LinkTransferIdToStop(StopData & stop, TransitId transferId)
{
  // We use vector instead of unordered set because we assume that transfers count for stop doesn't
  // exceed 2 or maybe 4.
  if (!base::IsExist(stop.m_transferIds, transferId))
    stop.m_transferIds.push_back(transferId);
}
}  // namespace transit
