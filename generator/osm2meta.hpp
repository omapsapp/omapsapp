#pragma once

#include "indexer/feature_data.hpp"
#include "indexer/validate_and_format_contacts.hpp"

#include <string>

struct MetadataTagProcessorImpl
{
  MetadataTagProcessorImpl(FeatureBuilderParams & params) : m_params(params) {}

  std::string ValidateAndFormat_maxspeed(std::string const & v) const;
  static std::string ValidateAndFormat_stars(std::string const & v) ;
  std::string ValidateAndFormat_operator(std::string const & v) const;
  static std::string ValidateAndFormat_url(std::string const & v) ;
  static std::string ValidateAndFormat_phone(std::string const & v) ;
  static std::string ValidateAndFormat_opening_hours(std::string const & v) ;
  std::string ValidateAndFormat_ele(std::string const & v) const;
  static std::string ValidateAndFormat_destination(std::string const & v) ;
  static std::string ValidateAndFormat_local_ref(std::string const & v) ;
  static std::string ValidateAndFormat_destination_ref(std::string const & v) ;
  static std::string ValidateAndFormat_junction_ref(std::string const & v) ;
  static std::string ValidateAndFormat_turn_lanes(std::string const & v);
  static std::string ValidateAndFormat_email(std::string const & v) ;
  static std::string ValidateAndFormat_postcode(std::string const & v) ;
  static std::string ValidateAndFormat_flats(std::string const & v) ;
  static std::string ValidateAndFormat_internet(std::string v) ;
  static std::string ValidateAndFormat_height(std::string const & v) ;
  static std::string ValidateAndFormat_building_levels(std::string v) ;
  static std::string ValidateAndFormat_level(std::string v) ;
  static std::string ValidateAndFormat_denomination(std::string const & v) ;
  static std::string ValidateAndFormat_wikipedia(std::string v) ;
  static std::string ValidateAndFormat_wikimedia_commons(std::string v) ;
  std::string ValidateAndFormat_airport_iata(std::string const & v) const;
  static std::string ValidateAndFormat_brand(std::string const & v);
  std::string ValidateAndFormat_duration(std::string const & v) const;
  static std::string ValidateAndFormat_capacity(std::string const & v);
  static std::string ValidateAndFormat_drive_through(std::string v);

protected:
  FeatureBuilderParams & m_params;
};

class MetadataTagProcessor : private MetadataTagProcessorImpl
{
  StringUtf8Multilang m_description;

  struct LangFlags
  {
    // Select one value with priority:
    // - Default
    // - English
    // - Others
    bool Add(std::string_view lang)
    {
      if (lang.empty())
      {
        m_defAdded = true;
        return true;
      }
      else if (lang == "en")
      {
        if (m_defAdded)
          return false;
        m_enAdded = true;
        return true;
      }

      return !m_defAdded && !m_enAdded;
    }

    bool m_defAdded = false;
    bool m_enAdded = false;
  };

  /// @todo Make operator and brand multilangual like description.
  LangFlags m_operatorF, m_brandF;

public:
  /// Make base class constructor public.
  using MetadataTagProcessorImpl::MetadataTagProcessorImpl;

  // Assume that processor is created once, and we can make additional finalization in dtor.
  ~MetadataTagProcessor();

  void operator()(std::string const & k, std::string const & v);
};
