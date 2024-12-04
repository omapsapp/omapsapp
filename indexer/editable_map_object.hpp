#pragma once

#include "indexer/feature_data.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/map_object.hpp"
#include "indexer/edit_journal.hpp"

#include "coding/string_utf8_multilang.hpp"

#include <functional>
#include <string>
#include <vector>

namespace osm
{
/// Holds information to construct editor's UI.
struct EditableProperties
{
  EditableProperties() = default;
  EditableProperties(std::vector<feature::Metadata::EType> metadata, bool name,
                     bool address, bool cuisine)
    : m_metadata(std::move(metadata)), m_name(name), m_address(address), m_cuisine(cuisine)
  {
  }

  bool IsEditable() const { return m_name || m_address || m_cuisine || !m_metadata.empty(); }

  std::vector<feature::Metadata::EType> m_metadata;
  bool m_name = false;
  /// Can edit house number, street address and postcode.
  bool m_address = false;
  bool m_cuisine = false;
};

struct LocalizedName
{
  LocalizedName(int8_t code, std::string_view name);
  LocalizedName(std::string const & langCode, std::string const & name);

  /// m_code, m_lang and m_langName are defined in StringUtf8Multilang.
  int8_t const m_code;
  /// Non-owning pointers to internal static char const * array.
  std::string_view const m_lang;
  std::string_view const m_langName;
  std::string const m_name;
};

/// Class which contains vector of localized names with following priority:
///  1. Default Name
///  2. Other names
/// and mandatoryNamesCount - count of names which should be always shown.
struct NamesDataSource
{
  std::vector<LocalizedName> names;
  size_t mandatoryNamesCount = 0;
};

struct LocalizedStreet
{
  std::string m_defaultName;
  std::string m_localizedName;

  bool operator==(LocalizedStreet const & st) const { return m_defaultName == st.m_defaultName; }
};

class EditableMapObject : public MapObject
{
public:
  static uint8_t constexpr kMaximumLevelsEditableByUsers = 50;

  bool IsNameEditable() const;
  bool IsAddressEditable() const;

  /// @todo Can implement polymorphic approach here and store map<MetadataID, MetadataEntryIFace>.
  /// All store/load/valid operations will be via MetadataEntryIFace interface instead of switch-case.
  std::vector<MetadataID> GetEditableProperties() const;

  /// See comment for NamesDataSource class.
  NamesDataSource GetNamesDataSource();
  LocalizedStreet const & GetStreet() const;
  std::vector<LocalizedStreet> const & GetNearbyStreets() const;

  /// @note { tag, value } are temporary string views and can't be stored for later use.
  void ForEachMetadataItem(std::function<void(std::string_view tag, std::string_view value)> const & fn) const;

  // Used only in testing framework.
  void SetTestId(uint64_t id);

  void SetEditableProperties(osm::EditableProperties const & props);
  //  void SetFeatureID(FeatureID const & fid);
  void SetName(StringUtf8Multilang const & name);
  void SetName(std::string_view name, int8_t langCode);
  void SetMercator(m2::PointD const & center);
  void SetType(uint32_t featureType);
  void SetTypes(feature::TypesHolder const & types);
  void SetID(FeatureID const & fid);

  void SetStreet(LocalizedStreet const & st);
  void SetNearbyStreets(std::vector<LocalizedStreet> && streets);
  void SetHouseNumber(std::string const & houseNumber);
  void SetPostcode(std::string const & postcode);

  static bool IsValidMetadata(MetadataID type, std::string const & value);
  void SetMetadata(MetadataID type, std::string value);
  bool UpdateMetadataValue(std::string_view key, std::string value);

  void SetOpeningHours(std::string oh);
  void SetInternet(feature::Internet internet);

  /// @param[in] cuisine is a vector of osm cuisine ids.
private:
  template <class T> void SetCuisinesImpl(std::vector<T> const & cuisines);
public:
  void SetCuisines(std::vector<std::string_view> const & cuisines);
  void SetCuisines(std::vector<std::string> const & cuisines);

  /// Special mark that it's a point feature, not area or line.
  void SetPointType();
  /// Remove blank names
  void RemoveBlankNames();

  static bool ValidateBuildingLevels(std::string const & buildingLevels);
  static bool ValidateHouseNumber(std::string const & houseNumber);
  static bool ValidateFlats(std::string const & flats);
  static bool ValidatePostCode(std::string const & postCode);
  static bool ValidatePhoneList(std::string const & phone);
  static bool ValidateEmail(std::string const & email);
  static bool ValidateLevel(std::string const & level);
  static bool ValidateName(std::string const & name);

  /// Journal that stores changes to map object
  EditJournal const & GetJournal() const;
  void SetJournal(EditJournal && editJournal);
  EditingLifecycle GetEditingLifecycle() const;
  void MarkAsCreated(uint32_t type, feature::GeomType geomType, m2::PointD mercator);
  void ClearJournal();
  void ApplyEditsFromJournal(EditJournal const & journal);
  void ApplyJournalEntry(JournalEntry const & entry);
  void LogDiffInJournal(EditableMapObject const & unedited_emo);

  /// Check whether langCode can be used as default name.
  static bool CanUseAsDefaultName(int8_t const langCode, std::vector<int8_t> const & nativeMwmLanguages);

  /// See comment for NamesDataSource class.
  static NamesDataSource GetNamesDataSource(StringUtf8Multilang const & source,
                                            std::vector<int8_t> const & nativeMwmLanguages,
                                            int8_t const userLanguage);

  /// Compares editable fields connected with feature ignoring street.
  friend bool AreObjectsEqualIgnoringStreet(EditableMapObject const & lhs, EditableMapObject const & rhs);

private:
  LocalizedStreet m_street;
  std::vector<LocalizedStreet> m_nearbyStreets;
  EditableProperties m_editableProperties;
  osm::EditJournal m_journal;
};
}  // namespace osm
