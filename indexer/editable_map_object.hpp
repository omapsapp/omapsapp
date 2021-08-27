#pragma once

#include "indexer/feature_data.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/map_object.hpp"

#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"

#include "coding/string_utf8_multilang.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace osm
{
/// Holds information to construct editor's UI.
struct EditableProperties
{
  EditableProperties() = default;
  EditableProperties(std::vector<feature::Metadata::EType> const & metadata, bool name,
                     bool address, bool cuisine)
    : m_name(name), m_address(address), m_cuisine(cuisine), m_metadata(metadata)
  {
  }

  bool m_name = false;
  /// If true, enables editing of house number, street address and postcode.
  bool m_address = false;
  bool m_cuisine = false;
  std::vector<feature::Metadata::EType> m_metadata;
  bool IsEditable() const { return m_name || m_address || m_cuisine || !m_metadata.empty(); }
};

struct LocalizedName
{
  LocalizedName(int8_t code, std::string const & name);
  LocalizedName(std::string const & langCode, std::string const & name);

  /// m_code, m_lang and m_langName are defined in StringUtf8Multilang.
  int8_t const m_code;
  /// Non-owning pointers to internal static char const * array.
  char const * const m_lang;
  char const * const m_langName;
  std::string const m_name;
};

/// Class which contains vector of localized names with following priority:
///  1. Names for Mwm languages
///  2. User`s language name
///  3. Other names
/// and mandatoryNamesCount - count of names which should be always shown.
struct NamesDataSource
{
  std::vector<LocalizedName> names;
  size_t mandatoryNamesCount = 0;
};

struct FakeName
{
  FakeName(int8_t code, std::string filledName)
    : m_code(code)
    , m_filledName(filledName)
  {
  }

  int8_t m_code;
  std::string m_filledName;
};
/// Contains information about fake names which were added for user convenience.
struct FakeNames
{
  void Clear()
  {
    m_names.clear();
    m_defaultName.clear();
  }

  std::vector<FakeName> m_names;
  std::string m_defaultName;
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
  static int8_t const kMaximumLevelsEditableByUsers;

  bool IsNameEditable() const;
  bool IsAddressEditable() const;

  std::vector<Props> GetEditableProperties() const;
  // TODO(AlexZ): Remove this method and use GetEditableProperties() in UI.
  std::vector<feature::Metadata::EType> const & GetEditableFields() const;

  /// See comment for NamesDataSource class.
  NamesDataSource GetNamesDataSource(bool addFakes = true);
  LocalizedStreet const & GetStreet() const;
  std::vector<LocalizedStreet> const & GetNearbyStreets() const;
  std::string GetPostcode() const;
  std::string GetWikipedia() const;

  void ForEachMetadataItem(
      bool skipSponsored,
      std::function<void(std::string const & tag, std::string const & value)> const & fn) const;

  // These two methods should only be used in tests.
  uint64_t GetTestId() const;
  void SetTestId(uint64_t id);

  void SetEditableProperties(osm::EditableProperties const & props);
  //  void SetFeatureID(FeatureID const & fid);
  void SetName(StringUtf8Multilang const & name);
  void SetName(std::string name, int8_t langCode = StringUtf8Multilang::kDefaultCode);
  void SetMercator(m2::PointD const & center);
  void SetType(uint32_t featureType);
  void SetTypes(feature::TypesHolder const & types);
  void SetID(FeatureID const & fid);

  void SetStreet(LocalizedStreet const & st);
  void SetNearbyStreets(std::vector<LocalizedStreet> && streets);
  void SetHouseNumber(std::string const & houseNumber);
  bool UpdateMetadataValue(std::string const & key, std::string const & value);
  void SetPostcode(std::string const & postcode);
  void SetPhone(std::string const & phone);
  void SetFax(std::string const & fax);

  void SetEmail(std::string const & email);
  void SetWebsite(std::string website);
  void SetFacebookPage(std::string facebookPage);
  void SetInstagramPage(std::string instagramPage);
  void SetTwitterPage(std::string twitterPage);
  void SetVkPage(std::string vkPage);
  void SetWikipedia(std::string const & wikipedia);

  void SetInternet(Internet internet);
  void SetStars(int stars);
  void SetOperator(std::string const & op);

  void SetElevation(double ele);
  void SetFlats(std::string const & flats);

  void SetBuildingLevels(std::string const & buildingLevels);
  void SetLevel(std::string const & level);
  /// @param[in] cuisine is a vector of osm cuisine ids.
  void SetCuisines(std::vector<std::string> const & cuisines);
  void SetOpeningHours(std::string const & openingHours);

  /// Special mark that it's a point feature, not area or line.
  void SetPointType();
  /// Enables advanced mode with direct access to default name and disables any recalculations.
  void EnableNamesAdvancedMode() { m_namesAdvancedMode = true; }
  bool IsNamesAdvancedModeEnabled() const { return m_namesAdvancedMode; }
  /// Remove blank names and default name duplications.
  void RemoveBlankAndDuplicationsForDefault();
  /// Calls RemoveBlankNames or RemoveFakeNames depending on mode.
  void RemoveNeedlessNames();

  static bool ValidateBuildingLevels(std::string const & buildingLevels);
  static bool ValidateHouseNumber(std::string const & houseNumber);
  static bool ValidateFlats(std::string const & flats);
  static bool ValidatePostCode(std::string const & postCode);
  static bool ValidatePhoneList(std::string const & phone);
  static bool ValidateWebsite(std::string const & site);
  static bool ValidateFacebookPage(std::string const & facebookPage);
  static bool ValidateInstagramPage(std::string const & instagramPage);
  static bool ValidateTwitterPage(std::string const & twitterPage);
  static bool ValidateVkPage(std::string const & vkPage);
  static bool ValidateEmail(std::string const & email);
  static bool ValidateLevel(std::string const & level);
  static bool ValidateName(std::string const & name);

  /// Check whether langCode can be used as default name.
  static bool CanUseAsDefaultName(int8_t const langCode, std::vector<int8_t> const & nativeMwmLanguages);

  /// See comment for NamesDataSource class.
  static NamesDataSource GetNamesDataSource(StringUtf8Multilang const & source,
                                            std::vector<int8_t> const & nativeMwmLanguages,
                                            int8_t const userLanguage);
  /// Removes fake names (which were added for user convenience) from name.
  static void RemoveFakeNames(FakeNames const & fakeNames, StringUtf8Multilang & name);

private:
  LocalizedStreet m_street;
  std::vector<LocalizedStreet> m_nearbyStreets;
  EditableProperties m_editableProperties;
  FakeNames m_fakeNames;
  bool m_namesAdvancedMode = false;
};
}  // namespace osm
