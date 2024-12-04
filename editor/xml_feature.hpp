#pragma once

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/edit_journal.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "base/string_utils.hpp"

#include <cstdint>
#include <ctime>
#include <iostream>
#include <vector>

#include <pugixml.hpp>

namespace osm
{
class EditableMapObject;
}

namespace editor
{
DECLARE_EXCEPTION(XMLFeatureError, RootException);
DECLARE_EXCEPTION(InvalidXML, XMLFeatureError);
DECLARE_EXCEPTION(NoLatLon, XMLFeatureError);
DECLARE_EXCEPTION(NoXY, XMLFeatureError);
DECLARE_EXCEPTION(NoTimestamp, XMLFeatureError);
DECLARE_EXCEPTION(NoHeader, XMLFeatureError);
DECLARE_EXCEPTION(InvalidJournalEntry, XMLFeatureError);

class XMLFeature
{
  static constexpr std::string_view kDefaultName = "name";
  static constexpr std::string_view kLocalName = "name:";
  static constexpr std::string_view kIntlName = "int_name";
  static constexpr std::string_view kAltName = "alt_name";
  static constexpr std::string_view kOldName = "old_name";
  static constexpr std::string_view kDefaultLang = "default";
  static constexpr std::string_view kIntlLang = kIntlName;
  static constexpr std::string_view kAltLang = kAltName;
  static constexpr std::string_view kOldLang = kOldName;

public:
  // Used in point to string serialization.
  static constexpr int kLatLonTolerance = 7;

  enum class Type
  {
    Unknown,
    Node,
    Way,
    Relation
  };

  /// Creates empty node or way.
  XMLFeature(Type const type);
  XMLFeature(std::string const & xml);
  XMLFeature(pugi::xml_document const & xml);
  XMLFeature(pugi::xml_node const & xml);
  XMLFeature(XMLFeature const & feature);

  XMLFeature & operator=(XMLFeature const & feature);

  // TODO: It should make "deep" compare instead of converting to strings.
  // Strings comparison does not work if tags order is different but tags are equal.
  bool operator==(XMLFeature const & other) const;
  /// @returns nodes, ways and relations from osmXml. Vector can be empty.
  static std::vector<XMLFeature> FromOSM(std::string const & osmXml);

  void Save(std::ostream & ost) const;
  std::string ToOSMString() const;

  /// Tags from featureWithChanges are applied to this(osm) feature.
  void ApplyPatch(XMLFeature const & featureWithChanges);

  Type GetType() const;
  std::string GetTypeString() const;

  m2::PointD GetMercatorCenter() const;
  ms::LatLon GetCenter() const;
  void SetCenter(ms::LatLon const & ll);
  void SetCenter(m2::PointD const & mercatorCenter);

  std::vector<m2::PointD> GetGeometry() const;

  /// Sets geometry in mercator to match against FeatureType's geometry in mwm
  /// when megrating to a new mwm build.
  /// Geometry points are now stored in <nd x="..." y="..." /> nodes like in osm <way>.
  /// But they are not the same as osm's. I.e. osm's one stores reference to a <node>
  /// with it's own data and lat, lon. Here we store only cooridanes in mercator.
  template <typename Iterator>
  void SetGeometry(Iterator begin, Iterator end)
  {
    ASSERT_NOT_EQUAL(GetType(), Type::Unknown, ());
    ASSERT_NOT_EQUAL(GetType(), Type::Node, ());

    for (; begin != end; ++begin)
    {
      auto nd = GetRootNode().append_child("nd");
      nd.append_attribute("x") = strings::to_string_dac(begin->x, kLatLonTolerance).data();
      nd.append_attribute("y") = strings::to_string_dac(begin->y, kLatLonTolerance).data();
    }
  }

  template <typename Collection>
  void SetGeometry(Collection const & geometry)
  {
    SetGeometry(begin(geometry), end(geometry));
  }

  std::string GetName(std::string_view lang) const;
  std::string GetName(uint8_t const langCode = StringUtf8Multilang::kDefaultCode) const;

  template <typename Fn>
  void ForEachName(Fn && func) const
  {
    size_t const kPrefixLen = kLocalName.size();

    for (auto const & tag : GetRootNode().select_nodes("tag"))
    {
      std::string_view const key = tag.node().attribute("k").value();

      if (key.substr(0, kPrefixLen) == kLocalName)
        func(key.substr(kPrefixLen), tag.node().attribute("v").value());
      else if (key == kDefaultName)
        func(kDefaultLang, tag.node().attribute("v").value());
      else if (key == kIntlName)
        func(kIntlLang, tag.node().attribute("v").value());
      else if (key == kAltName)
        func(kAltLang, tag.node().attribute("v").value());
      else if (key == kOldName)
        func(kOldLang, tag.node().attribute("v").value());
    }
  }

  void SetName(std::string_view name);
  void SetName(std::string_view lang, std::string_view name);
  void SetName(uint8_t const langCode, std::string_view name);

  std::string GetHouse() const;
  void SetHouse(std::string const & house);

  std::string GetCuisine() const;
  void SetCuisine(std::string cuisine);

  /// Our and OSM modification time are equal.
  time_t GetModificationTime() const;
  void SetModificationTime(time_t const time);

  /// @name XML storage format helpers.
  //@{
  uint32_t GetMWMFeatureIndex() const;
  void SetMWMFeatureIndex(uint32_t index);

  /// @returns base::INVALID_TIME_STAMP if there were no any upload attempt.
  time_t GetUploadTime() const;
  void SetUploadTime(time_t const time);

  std::string GetUploadStatus() const;
  void SetUploadStatus(std::string const & status);

  std::string GetUploadError() const;
  void SetUploadError(std::string const & error);

  osm::EditJournal GetEditJournal() const;
  void SetEditJournal(osm::EditJournal const & journal);
  //@}

  bool HasAnyTags() const;
  bool HasTag(std::string_view key) const;
  bool HasAttribute(std::string_view key) const;
  bool HasKey(std::string_view key) const;

  template <typename Fn>
  void ForEachTag(Fn && func) const
  {
    for (auto const & tag : GetRootNode().select_nodes("tag"))
      func(tag.node().attribute("k").value(), tag.node().attribute("v").value());
  }

  std::string GetTagValue(std::string_view key) const;
  void SetTagValue(std::string_view key, std::string_view value);
  void RemoveTag(std::string_view key);

  /// Wrapper for SetTagValue and RemoveTag, avoids duplication for similar alternative osm tags
  void UpdateOSMTag(std::string_view key, std::string_view value);

  std::string GetAttribute(std::string const & key) const;
  void SetAttribute(std::string const & key, std::string const & value);

  bool AttachToParentNode(pugi::xml_node parent) const;

  static std::string TypeToString(Type type);
  static Type StringToType(std::string const & type);

private:
  pugi::xml_node const GetRootNode() const;
  pugi::xml_node GetRootNode();

  pugi::xml_document m_document;
};

/// Rewrites all but geometry and types.
/// Should be applied to existing features only (in mwm files).
void ApplyPatch(XMLFeature const & xml, osm::EditableMapObject & object);

/// @param serializeType if false, types are not serialized.
/// Useful for applying modifications to existing OSM features, to avoid issues when someone
/// has changed a type in OSM, but our users uploaded invalid outdated type after modifying feature.
XMLFeature ToXML(osm::EditableMapObject const & object, bool serializeType);

/// Used to generate XML for created objects in the new editor
XMLFeature TypeToXML(uint32_t type, feature::GeomType geomType, m2::PointD mercator);

/// Creates new feature, including geometry and types.
/// @Note: only nodes (points) are supported at the moment.
bool FromXML(XMLFeature const & xml, osm::EditableMapObject & object);

std::string DebugPrint(XMLFeature const & feature);
std::string DebugPrint(XMLFeature::Type const type);
} // namespace editor
