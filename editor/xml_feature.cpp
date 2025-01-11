#include "editor/xml_feature.hpp"

#include "indexer/classificator.hpp"
#include "indexer/editable_map_object.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/validate_and_format_contacts.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "geometry/latlon.hpp"

#include "base/exception.hpp"
#include "base/macros.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include <array>
#include <sstream>
#include <string>

namespace editor
{
using std::string, std::string_view;

namespace
{
constexpr char const * kTimestamp = "timestamp";
constexpr char const * kIndex = "mwm_file_index";
constexpr char const * kUploadTimestamp = "upload_timestamp";
constexpr char const * kUploadStatus = "upload_status";
constexpr char const * kUploadError = "upload_error";

string_view constexpr kHouseNumber = "addr:housenumber";
string_view constexpr kCuisine = "cuisine";
string_view constexpr kDietVegetarian = "diet:vegetarian";
string_view constexpr kDietVegan = "diet:vegan";
string_view constexpr kVegetarian = "vegetarian";
string_view constexpr kVegan = "vegan";
string_view constexpr kYes = "yes";

constexpr char const * kUnknownType = "unknown";
constexpr char const * kNodeType = "node";
constexpr char const * kWayType = "way";
constexpr char const * kRelationType = "relation";

string_view constexpr kColon = ":";

pugi::xml_node FindTag(pugi::xml_document const & document, string_view k)
{
  string key = "//tag[@k='";
  key.append(k).append("']");
  return document.select_node(key.data()).node();
}

ms::LatLon GetLatLonFromNode(pugi::xml_node const & node)
{
  ms::LatLon ll;
  if (!strings::to_double(node.attribute("lat").value(), ll.m_lat))
  {
    MYTHROW(editor::NoLatLon,
            ("Can't parse lat attribute:", string(node.attribute("lat").value())));
  }
  if (!strings::to_double(node.attribute("lon").value(), ll.m_lon))
  {
    MYTHROW(editor::NoLatLon,
            ("Can't parse lon attribute:", string(node.attribute("lon").value())));
  }
  return ll;
}

m2::PointD GetMercatorPointFromNode(pugi::xml_node const & node)
{
  m2::PointD p;
  if (!strings::to_double(node.attribute("x").value(), p.x))
    MYTHROW(editor::NoXY, ("Can't parse x attribute: " + string(node.attribute("x").value())));
  if (!strings::to_double(node.attribute("y").value(), p.y))
    MYTHROW(editor::NoXY, ("Can't parse y attribute: " + string(node.attribute("y").value())));
  return p;
}

void ValidateElement(pugi::xml_node const & nodeOrWay)
{
  using editor::XMLFeature;

  if (!nodeOrWay)
    MYTHROW(editor::InvalidXML, ("Document has no valid root element."));

  auto const type = XMLFeature::StringToType(nodeOrWay.name());

  if (type == XMLFeature::Type::Unknown)
    MYTHROW(editor::InvalidXML, ("XMLFeature does not support root tag", nodeOrWay.name()));

  if (type == XMLFeature::Type::Node)
    UNUSED_VALUE(GetLatLonFromNode(nodeOrWay));

  if (!nodeOrWay.attribute(kTimestamp))
    MYTHROW(editor::NoTimestamp, ("Node has no timestamp attribute"));
}
}  // namespace

XMLFeature::XMLFeature(Type const type)
{
  ASSERT_NOT_EQUAL(type, Type::Unknown, ());

  m_document.append_child(TypeToString(type).c_str());
}

XMLFeature::XMLFeature(string const & xml)
{
  m_document.load_string(xml.data());
  ValidateElement(GetRootNode());
}

XMLFeature::XMLFeature(pugi::xml_document const & xml)
{
  m_document.reset(xml);
  ValidateElement(GetRootNode());
}

XMLFeature::XMLFeature(pugi::xml_node const & xml)
{
  m_document.reset();
  m_document.append_copy(xml);
  ValidateElement(GetRootNode());
}

XMLFeature::XMLFeature(XMLFeature const & feature)
{
  m_document.reset(feature.m_document);
  // Don't validate feature: it should already be validated.
}

XMLFeature & XMLFeature::operator=(XMLFeature const & feature)
{
  m_document.reset(feature.m_document);
  // Don't validate feature: it should already be validated.
  return *this;
}

bool XMLFeature::operator==(XMLFeature const & other) const
{
  return ToOSMString() == other.ToOSMString();
}

std::vector<XMLFeature> XMLFeature::FromOSM(string const & osmXml)
{
  pugi::xml_document doc;
  if (doc.load_string(osmXml.data()).status != pugi::status_ok)
    MYTHROW(editor::InvalidXML, ("Not valid XML:", osmXml));

  std::vector<XMLFeature> features;
  for (auto const & n : doc.child("osm").children())
  {
    if (StringToType(n.name()) != Type::Unknown)
      features.emplace_back(n);
  }
  return features;
}

XMLFeature::Type XMLFeature::GetType() const { return StringToType(GetRootNode().name()); }

string XMLFeature::GetTypeString() const { return GetRootNode().name(); }

void XMLFeature::Save(std::ostream & ost) const { m_document.save(ost, "  "); }

string XMLFeature::ToOSMString() const
{
  std::ostringstream ost;
  // Ugly way to wrap into <osm>..</osm> tags.
  // Unfortunately, pugi xml library doesn't allow to insert documents into other documents.
  ost << "<?xml version=\"1.0\"?>\n";
  ost << "<osm>\n";
  m_document.save(ost, "  ", pugi::format_no_declaration | pugi::format_indent);
  ost << "</osm>\n";
  return ost.str();
}

void XMLFeature::ApplyPatch(XMLFeature const & featureWithChanges)
{
  // TODO(mgsergio): Get these alt tags from the config.
  base::StringIL const alternativeTags[] = {
    {"phone", "contact:phone", "contact:mobile", "mobile"},
    {"website", "contact:website", "url"},
    {"fax", "contact:fax"},
    {"email", "contact:email"}
  };

  featureWithChanges.ForEachTag([&alternativeTags, this](string_view k, string_view v)
  {
    // Avoid duplication for similar alternative osm tags.
    for (auto const & alt : alternativeTags)
    {
      auto it = alt.begin();
      ASSERT(it != alt.end(), ());
      if (k == *it)
      {
        for (auto const & tag : alt)
        {
          // Reuse already existing tag if it's present.
          if (HasTag(tag))
          {
            SetTagValue(tag, v);
            return;
          }
        }
      }
    }
    SetTagValue(k, v);
  });
}

m2::PointD XMLFeature::GetMercatorCenter() const
{
  return mercator::FromLatLon(GetLatLonFromNode(GetRootNode()));
}

ms::LatLon XMLFeature::GetCenter() const
{
  ASSERT_EQUAL(GetType(), Type::Node, ());
  return GetLatLonFromNode(GetRootNode());
}

void XMLFeature::SetCenter(ms::LatLon const & ll)
{
  ASSERT_EQUAL(GetType(), Type::Node, ());
  SetAttribute("lat", strings::to_string_dac(ll.m_lat, kLatLonTolerance));
  SetAttribute("lon", strings::to_string_dac(ll.m_lon, kLatLonTolerance));
}

void XMLFeature::SetCenter(m2::PointD const & mercatorCenter)
{
  SetCenter(mercator::ToLatLon(mercatorCenter));
}

std::vector<m2::PointD> XMLFeature::GetGeometry() const
{
  ASSERT_NOT_EQUAL(GetType(), Type::Unknown, ());
  ASSERT_NOT_EQUAL(GetType(), Type::Node, ());
  std::vector<m2::PointD> geometry;
  for (auto const & xCenter : GetRootNode().select_nodes("nd"))
  {
    ASSERT(xCenter.node(), ("no nd attribute."));
    geometry.emplace_back(GetMercatorPointFromNode(xCenter.node()));
  }
  return geometry;
}

string XMLFeature::GetName(std::string_view lang) const
{
  ASSERT_EQUAL(kDefaultLang, StringUtf8Multilang::GetLangByCode(StringUtf8Multilang::kDefaultCode), ());
  ASSERT_EQUAL(kIntlLang, StringUtf8Multilang::GetLangByCode(StringUtf8Multilang::kInternationalCode), ());
  ASSERT_EQUAL(kAltLang, StringUtf8Multilang::GetLangByCode(StringUtf8Multilang::kAltNameCode), ());
  ASSERT_EQUAL(kOldLang, StringUtf8Multilang::GetLangByCode(StringUtf8Multilang::kOldNameCode), ());
  if (lang == kIntlLang)
    return GetTagValue(kIntlName);
  if (lang == kAltLang)
    return GetTagValue(kAltName);
  if (lang == kOldLang)
    return GetTagValue(kOldName);
  if (lang == kDefaultLang || lang.empty())
    return GetTagValue(kDefaultName);
  return GetTagValue(std::string{kDefaultName}.append(kColon).append(lang));
  //return GetTagValue(suffix.insert(0, kDefaultName));
}

string XMLFeature::GetName(uint8_t const langCode) const
{
  return GetName(StringUtf8Multilang::GetLangByCode(langCode));
}

void XMLFeature::SetName(string_view name) { SetName(kDefaultLang, name); }

void XMLFeature::SetName(string_view lang, string_view name)
{
  if (lang == kIntlLang)
  {
    SetTagValue(kIntlName, name);
  }
  else if (lang == kAltLang)
  {
    SetTagValue(kAltName, name);
  }
  else if (lang == kOldLang)
  {
    SetTagValue(kOldName, name);
  }
  else if (lang == kDefaultLang || lang.empty())
  {
    SetTagValue(kDefaultName, name);
  }
  else
  {
    SetTagValue(std::string{kDefaultName}.append(kColon).append(lang), name);
  }
}

void XMLFeature::SetName(uint8_t const langCode, string_view name)
{
  SetName(StringUtf8Multilang::GetLangByCode(langCode), name);
}

string XMLFeature::GetHouse() const { return GetTagValue(kHouseNumber); }

void XMLFeature::SetHouse(string const & house) { SetTagValue(kHouseNumber, house); }

/// https://github.com/organicmaps/organicmaps/issues/1118
/// @todo Make full diet:xxx support.
/// @{
string XMLFeature::GetCuisine() const
{
  auto res = GetTagValue(kCuisine);
  auto const appendCuisine = [&res](std::string_view s)
  {
    if (!res.empty())
      res += ';';
    res += s;
  };

  if (GetTagValue(kDietVegan) == kYes)
    appendCuisine(kVegan);
  if (GetTagValue(kDietVegetarian) == kYes)
    appendCuisine(kVegetarian);
  return res;
}

void XMLFeature::SetCuisine(string cuisine)
{
  auto const findAndErase = [&cuisine](std::string_view s)
  {
    size_t const i = cuisine.find(s);
    if (i != std::string_view::npos)
    {
      size_t from = 0;
      size_t sz = s.size();
      if (i > 0)
      {
        from = i - 1;
        ASSERT_EQUAL(cuisine[from], ';', ());
        ++sz;
      }
      else if (cuisine.size() > sz)
      {
        ASSERT_EQUAL(cuisine[sz], ';', ());
        ++sz;
      }

      cuisine.erase(from, sz);
      return true;
    }
    return false;
  };

  if (findAndErase(kVegan))
    SetTagValue(kDietVegan, kYes);
  else
    RemoveTag(kDietVegan);

  if (findAndErase(kVegetarian))
    SetTagValue(kDietVegetarian, kYes);
  else
    RemoveTag(kDietVegetarian);

  if (!cuisine.empty())
    SetTagValue(kCuisine, cuisine);
  else
    RemoveTag(kCuisine);
}
/// @}

time_t XMLFeature::GetModificationTime() const
{
  return base::StringToTimestamp(GetRootNode().attribute(kTimestamp).value());
}

void XMLFeature::SetModificationTime(time_t const time)
{
  SetAttribute(kTimestamp, base::TimestampToString(time));
}

uint32_t XMLFeature::GetMWMFeatureIndex() const
{
  // Always cast to uint32_t to avoid warnings on different platforms.
  return static_cast<uint32_t>(GetRootNode().attribute(kIndex).as_uint(0));
}

void XMLFeature::SetMWMFeatureIndex(uint32_t index)
{
  SetAttribute(kIndex, strings::to_string(index));
}

time_t XMLFeature::GetUploadTime() const
{
  return base::StringToTimestamp(GetRootNode().attribute(kUploadTimestamp).value());
}

void XMLFeature::SetUploadTime(time_t const time)
{
  SetAttribute(kUploadTimestamp, base::TimestampToString(time));
}

string XMLFeature::GetUploadStatus() const
{
  return GetRootNode().attribute(kUploadStatus).value();
}

void XMLFeature::SetUploadStatus(string const & status) { SetAttribute(kUploadStatus, status); }

string XMLFeature::GetUploadError() const { return GetRootNode().attribute(kUploadError).value(); }

void XMLFeature::SetUploadError(string const & error) { SetAttribute(kUploadError, error); }

osm::EditJournal XMLFeature::GetEditJournal() const
{
  // debug print
  std::ostringstream ost;
  Save(ost);
  LOG(LDEBUG, ("Read in XMLFeature:\n", ost.str()));

  auto readEditJournalList = [] (pugi::xml_node & xmlNode, osm::EditJournal & journal, bool isHistory)
  {
    auto getAttribute = [] (pugi::xml_node & xmlNode, std::string_view attribute)
    {
      pugi::xml_attribute xmlValue = xmlNode.attribute(attribute.data());
      if (xmlValue.empty())
        MYTHROW(editor::InvalidJournalEntry, ("JournalEntry does not contain attribute: ", attribute));

      return xmlValue.value();
    };

    for (auto xmlEntry = xmlNode.child("entry"); xmlEntry; xmlEntry = xmlEntry.next_sibling("entry"))
    {
      osm::JournalEntry entry;

      // JournalEntryType
      std::string strEntryType = getAttribute(xmlEntry, "type");
      std::optional<osm::JournalEntryType> entryType = osm::EditJournal::TypeFromString(strEntryType);
      if (!entryType)
        MYTHROW(editor::InvalidJournalEntry, ("Invalid JournalEntryType:", strEntryType));
      entry.journalEntryType = *entryType;

      // Timestamp
      std::string strTimestamp = getAttribute(xmlEntry, "timestamp");
      entry.timestamp = base::StringToTimestamp(strTimestamp);
      if (entry.timestamp == base::INVALID_TIME_STAMP)
        MYTHROW(editor::InvalidJournalEntry, ("Invalid Timestamp:", strTimestamp));

      // Data
      auto xmlData = xmlEntry.child("data");
      if (!xmlData)
        MYTHROW(editor::InvalidJournalEntry, ("No Data item"));

      switch (entry.journalEntryType)
      {
        case osm::JournalEntryType::TagModification:
        {
          osm::TagModData tagModData;

          tagModData.key = getAttribute(xmlData, "key");
          if (tagModData.key.empty())
            MYTHROW(editor::InvalidJournalEntry, ("Empty key in TagModData"));
          tagModData.old_value = getAttribute(xmlData, "old_value");
          tagModData.new_value = getAttribute(xmlData, "new_value");

          entry.data = tagModData;
          break;
        }
        case osm::JournalEntryType::ObjectCreated:
        {
          osm::ObjCreateData objCreateData;

          // Feature Type
          std::string strType = getAttribute(xmlData, "type");
          if (strType.empty())
            MYTHROW(editor::InvalidJournalEntry, ("Feature type is empty"));
          objCreateData.type = classif().GetTypeByReadableObjectName(strType);
          if (objCreateData.type == IndexAndTypeMapping::INVALID_TYPE)
            MYTHROW(editor::InvalidJournalEntry, ("Invalid Feature Type:", strType));

          // GeomType
          std::string strGeomType = getAttribute(xmlData, "geomType");
          objCreateData.geomType = feature::TypeFromString(strGeomType);
          if (objCreateData.geomType != feature::GeomType::Point)
            MYTHROW(editor::InvalidJournalEntry, ("Invalid geomType (only point features are supported):", strGeomType));

          // Mercator
          objCreateData.mercator = mercator::FromLatLon(GetLatLonFromNode(xmlData));

          entry.data = objCreateData;
          break;
        }
        case osm::JournalEntryType::LegacyObject:
        {
          osm::LegacyObjData legacyObjData;
          legacyObjData.version = getAttribute(xmlData, "version");
          entry.data = legacyObjData;
          break;
        }
      }
      if (isHistory)
        journal.AddJournalHistoryEntry(entry);
      else
        journal.AddJournalEntry(entry);
    }
  };

  osm::EditJournal journal = osm::EditJournal();

  auto xmlJournal = GetRootNode().child("journal");
  if (xmlJournal)
    readEditJournalList(xmlJournal, journal, false);
  else
  {
    // Mark as Legacy Object
    osm::LegacyObjData legacyObjData = {"1.0"};
    time_t timestamp = time(nullptr);
    journal.AddJournalEntry({osm::JournalEntryType::LegacyObject, timestamp, legacyObjData});
    journal.AddJournalHistoryEntry({osm::JournalEntryType::LegacyObject, timestamp, std::move(legacyObjData)});
  }

  auto xmlJournalHistory = GetRootNode().child("journalHistory");
  readEditJournalList(xmlJournalHistory, journal, true);

  LOG(LDEBUG, ("Read in Journal:\n", journal.JournalToString()));

  return journal;
}

void XMLFeature::SetEditJournal(osm::EditJournal const & journal)
{
  LOG(LDEBUG, ("Saving Journal:\n", journal.JournalToString()));

  auto const insertEditJournalList = [] (pugi::xml_node & xmlNode, std::list<osm::JournalEntry> const & journalList)
  {
    xmlNode.append_attribute("version") = "1.0";
    for (osm::JournalEntry const & entry : journalList)
    {
      auto xmlEntry = xmlNode.append_child("entry");
      xmlEntry.append_attribute("type") = osm::EditJournal::ToString(entry.journalEntryType).data();
      xmlEntry.append_attribute("timestamp") = base::TimestampToString(entry.timestamp).data();

      auto xmlData = xmlEntry.append_child("data");
      switch (entry.journalEntryType)
      {
        case osm::JournalEntryType::TagModification:
        {
          osm::TagModData const & tagModData = std::get<osm::TagModData>(entry.data);
          xmlData.append_attribute("key") = tagModData.key.data();
          xmlData.append_attribute("old_value") = tagModData.old_value.data();
          xmlData.append_attribute("new_value") = tagModData.new_value.data();
          break;
        }
        case osm::JournalEntryType::ObjectCreated:
        {
          osm::ObjCreateData const & objCreateData = std::get<osm::ObjCreateData>(entry.data);
          xmlData.append_attribute("type") = classif().GetReadableObjectName(objCreateData.type).data();
          xmlData.append_attribute("geomType") = ToString(objCreateData.geomType).data();
          ms::LatLon ll = mercator::ToLatLon(objCreateData.mercator);
          xmlData.append_attribute("lat") = strings::to_string_dac(ll.m_lat, kLatLonTolerance).data();
          xmlData.append_attribute("lon") = strings::to_string_dac(ll.m_lon, kLatLonTolerance).data();
          break;
        }
        case osm::JournalEntryType::LegacyObject:
        {
          osm::LegacyObjData const & legacyObjData = std::get<osm::LegacyObjData>(entry.data);
          xmlData.append_attribute("version") = legacyObjData.version.data();
          break;
        }
      }
    }
  };

  auto xmlJournal = GetRootNode().append_child("journal");
  insertEditJournalList(xmlJournal, journal.GetJournal());

  auto xmlJournalHistory = GetRootNode().append_child("journalHistory");
  insertEditJournalList(xmlJournalHistory, journal.GetJournalHistory());

  // debug print
  std::ostringstream ost;
  Save(ost);
  LOG(LDEBUG, ("Saving XMLFeature:\n", ost.str()));
}

bool XMLFeature::HasAnyTags() const { return GetRootNode().child("tag"); }

bool XMLFeature::HasTag(string_view key) const { return FindTag(m_document, key); }

bool XMLFeature::HasAttribute(string_view key) const
{
  return GetRootNode().attribute(key.data());
}

bool XMLFeature::HasKey(string_view key) const { return HasTag(key) || HasAttribute(key); }

string XMLFeature::GetTagValue(string_view key) const
{
  auto const tag = FindTag(m_document, key);
  return tag.attribute("v").value();
}

void XMLFeature::SetTagValue(string_view key, string_view value)
{
  strings::Trim(value);
  auto tag = FindTag(m_document, key);
  if (!tag)
  {
    tag = GetRootNode().append_child("tag");
    tag.append_attribute("k").set_value(key.data(), key.size());
    tag.append_attribute("v").set_value(value.data(), value.size());
  }
  else
  {
    tag.attribute("v").set_value(value.data(), value.size());
  }
}

void XMLFeature::RemoveTag(string_view key)
{
  auto tag = FindTag(m_document, key);
  if (tag)
    GetRootNode().remove_child(tag);
}

void XMLFeature::UpdateOSMTag(std::string_view key, std::string_view value)
{
  if (value.empty())
    RemoveTag(key);

  else
  {
    // TODO(mgsergio): Get these alt tags from the config.
    base::StringIL const alternativeTags[] = {
        {"phone", "contact:phone", "contact:mobile", "mobile"},
        {"website", "contact:website", "url"},
        {"fax", "contact:fax"},
        {"email", "contact:email"}
    };

    // Avoid duplication for similar alternative osm tags.
    for (auto const & alt : alternativeTags)
    {
      auto it = alt.begin();
      ASSERT(it != alt.end(), ());
      if (key == *it)
      {
        for (auto const & tag : alt)
        {
          // Reuse already existing tag if it's present.
          if (HasTag(tag))
          {
            SetTagValue(tag, value);
            return;
          }
        }
      }
    }
    SetTagValue(key, value);
  }
}

string XMLFeature::GetAttribute(string const & key) const
{
  return GetRootNode().attribute(key.data()).value();
}

void XMLFeature::SetAttribute(string const & key, string const & value)
{
  auto node = HasAttribute(key) ? GetRootNode().attribute(key.data())
                                : GetRootNode().append_attribute(key.data());

  node.set_value(value.data());
}

pugi::xml_node const XMLFeature::GetRootNode() const { return m_document.first_child(); }

pugi::xml_node XMLFeature::GetRootNode() { return m_document.first_child(); }

bool XMLFeature::AttachToParentNode(pugi::xml_node parent) const
{
  return !parent.append_copy(GetRootNode()).empty();
}

// static
string XMLFeature::TypeToString(Type type)
{
  switch (type)
  {
  case Type::Unknown: return kUnknownType;
  case Type::Node: return kNodeType;
  case Type::Way: return kWayType;
  case Type::Relation: return kRelationType;
  }
  UNREACHABLE();
}

// static
XMLFeature::Type XMLFeature::StringToType(string const & type)
{
  if (type == kNodeType)
    return Type::Node;
  if (type == kWayType)
    return Type::Way;
  if (type == kRelationType)
    return Type::Relation;

  return Type::Unknown;
}

void ApplyPatch(XMLFeature const & xml, osm::EditableMapObject & object)
{
  xml.ForEachName([&object](string_view lang, string_view name)
  {
    object.SetName(name, StringUtf8Multilang::GetLangIndex(lang));
  });

  string const house = xml.GetHouse();
  if (!house.empty())
    object.SetHouseNumber(house);

  auto const cuisineStr = xml.GetCuisine();
  if (!cuisineStr.empty())
    object.SetCuisines(strings::Tokenize(cuisineStr, ";"));

  xml.ForEachTag([&object](string_view k, string v)
  {
    // Skip result because we iterate via *all* tags here.
    (void)object.UpdateMetadataValue(k, std::move(v));
  });
}

XMLFeature ToXML(osm::EditableMapObject const & object, bool serializeType)
{
  bool const isPoint = object.GetGeomType() == feature::GeomType::Point;
  XMLFeature toFeature(isPoint ? XMLFeature::Type::Node : XMLFeature::Type::Way);

  if (isPoint)
  {
    toFeature.SetCenter(object.GetMercator());
  }
  else
  {
    auto const & triangles = object.GetTriangesAsPoints();
    toFeature.SetGeometry(begin(triangles), end(triangles));
  }

  object.GetNameMultilang().ForEach([&toFeature](uint8_t const & lang, string_view name)
  {
    toFeature.SetName(lang, name);
  });

  string const & house = object.GetHouseNumber();
  if (!house.empty())
    toFeature.SetHouse(house);

  auto const cuisines = object.GetCuisines();
  if (!cuisines.empty())
  {
    auto const cuisineStr = strings::JoinStrings(cuisines, ";");
    toFeature.SetCuisine(cuisineStr);
  }

  if (serializeType)
  {
    feature::TypesHolder th = object.GetTypes();
    // TODO(mgsergio): Use correct sorting instead of SortBySpec based on the config.
    th.SortBySpec();
    // TODO(mgsergio): Either improve "OSM"-compatible serialization for more complex types,
    // or save all our types directly, to restore and reuse them in migration of modified features.
    for (uint32_t const type : th)
    {
      if (ftypes::IsCuisineChecker::Instance()(type))
        continue;

      if (ftypes::IsRecyclingTypeChecker::Instance()(type))
        continue;

      if (ftypes::IsRecyclingCentreChecker::Instance()(type))
      {
        toFeature.SetTagValue("amenity", "recycling");
        toFeature.SetTagValue("recycling_type", "centre");
        continue;
      }
      if (ftypes::IsRecyclingContainerChecker::Instance()(type))
      {
        toFeature.SetTagValue("amenity", "recycling");
        toFeature.SetTagValue("recycling_type", "container");
        continue;
      }

      string const strType = classif().GetReadableObjectName(type);
      strings::SimpleTokenizer iter(strType, "-");
      string_view const k = *iter;
      if (++iter)
      {
        // First (main) type is always stored as "k=amenity v=restaurant".
        // Any other "k=amenity v=atm" is replaced by "k=atm v=yes".
        if (toFeature.GetTagValue(k).empty())
          toFeature.SetTagValue(k, *iter);
        else
          toFeature.SetTagValue(*iter, kYes);
      }
      else
      {
        // We're editing building, generic craft, shop, office, amenity etc.
        // Skip it's serialization.
        // TODO(mgsergio): Correcly serialize all types back and forth.
        LOG(LDEBUG, ("Skipping type serialization:", k));
      }
    }
  }

  object.ForEachMetadataItem([&toFeature](string_view tag, string_view value)
  {
    if (osm::isSocialContactTag(tag) && value.find('/') != std::string::npos)
      toFeature.SetTagValue(tag, osm::socialContactToURL(tag, value));
    else
      toFeature.SetTagValue(tag, value);
  });

  return toFeature;
}

XMLFeature TypeToXML(uint32_t type, feature::GeomType geomType, m2::PointD mercator)
{
  ASSERT(geomType == feature::GeomType::Point, ("Only point features can be added"));
  XMLFeature toFeature(XMLFeature::Type::Node);
  toFeature.SetCenter(mercator);

  // Set Type
  if (ftypes::IsRecyclingCentreChecker::Instance()(type))
  {
    toFeature.SetTagValue("amenity", "recycling");
    toFeature.SetTagValue("recycling_type", "centre");
  }
  else if (ftypes::IsRecyclingContainerChecker::Instance()(type))
  {
    toFeature.SetTagValue("amenity", "recycling");
    toFeature.SetTagValue("recycling_type", "container");
  }
  else
  {
    string const strType = classif().GetReadableObjectName(type);
    strings::SimpleTokenizer iter(strType, "-");
    string_view const k = *iter;

    CHECK(++iter, ("Processing Type failed: ", strType));
    // Main type is always stored as "k=amenity v=restaurant".
    toFeature.SetTagValue(k, *iter);

    ASSERT(!(++iter), ("Can not process 3-arity/complex types: ", strType));
  }
  return toFeature;
}

bool FromXML(XMLFeature const & xml, osm::EditableMapObject & object)
{
  ASSERT_EQUAL(XMLFeature::Type::Node, xml.GetType(),
               ("At the moment only new nodes (points) can be created."));
  object.SetPointType();
  object.SetMercator(xml.GetMercatorCenter());
  xml.ForEachName([&object](string_view lang, string_view name)
  {
    object.SetName(name, StringUtf8Multilang::GetLangIndex(lang));
  });

  string const house = xml.GetHouse();
  if (!house.empty())
    object.SetHouseNumber(house);

  auto const cuisineStr = xml.GetCuisine();
  if (!cuisineStr.empty())
    object.SetCuisines(strings::Tokenize(cuisineStr, ";"));

  feature::TypesHolder types = object.GetTypes();

  Classificator const & cl = classif();
  xml.ForEachTag([&](string_view k, string_view v)
  {
    if (object.UpdateMetadataValue(k, string(v)))
      return;

    // Cuisines are already processed before this loop.
    if (k == "cuisine")
      return;

    // We process recycling_type tag together with "amenity"="recycling" later.
    // We currently ignore recycling tag because it's our custom tag and we cannot
    // import it to osm directly.
    if (k == "recycling" || k == "recycling_type")
      return;

    uint32_t type = 0;
    if (k == "amenity" && v == "recycling" && xml.HasTag("recycling_type"))
    {
      auto const typeValue = xml.GetTagValue("recycling_type");
      if (typeValue == "centre")
        type = ftypes::IsRecyclingCentreChecker::Instance().GetType();
      else if (typeValue == "container")
        type = ftypes::IsRecyclingContainerChecker::Instance().GetType();
    }

    // Simple heuristics. It works for types converted from osm with short mapcss rules
    // where k=v from osm is converted to our k-v type (amenity=restaurant, shop=convenience etc.).
    if (type == 0)
      type = cl.GetTypeByPathSafe({k, v});
    if (type == 0)
      type = cl.GetTypeByPathSafe({k});  // building etc.
    if (type == 0)
      type = cl.GetTypeByPathSafe({"amenity", k});  // atm=yes, toilet=yes etc.

    if (type && types.Size() >= feature::kMaxTypesCount)
      LOG(LERROR, ("Can't add type:", k, v, ". Types limit exceeded."));
    else if (type)
      types.Add(type);
    else
    {
      //LOG(LWARNING, ("Can't load/parse type:", k, v));
      /// @todo Refactor to make one ForEachTag loop. Now we have separate ForEachName,
      /// so we can't log any suspicious tag here ...
    }
  });

  object.SetTypes(types);
  return types.Size() > 0;
}

string DebugPrint(XMLFeature const & feature)
{
  std::ostringstream ost;
  feature.Save(ost);
  return ost.str();
}

string DebugPrint(XMLFeature::Type const type) { return XMLFeature::TypeToString(type); }
}  // namespace editor
