#pragma once

#include "indexer/feature_data.hpp"

#include <string>

struct MetadataTagProcessorImpl
{
  MetadataTagProcessorImpl(FeatureBuilderParams & params) : m_params(params) {}

  std::string ValidateAndFormat_maxspeed(std::string const & v) const;
  std::string ValidateAndFormat_stars(std::string const & v) const;
  std::string ValidateAndFormat_operator(std::string const & v) const;
  std::string ValidateAndFormat_url(std::string const & v) const;
  std::string ValidateAndFormat_phone(std::string const & v) const;
  std::string ValidateAndFormat_opening_hours(std::string const & v) const;
  std::string ValidateAndFormat_ele(std::string const & v) const;
  std::string ValidateAndFormat_turn_lanes(std::string const & v) const;
  std::string ValidateAndFormat_turn_lanes_forward(std::string const & v) const;
  std::string ValidateAndFormat_turn_lanes_backward(std::string const & v) const;
  std::string ValidateAndFormat_email(std::string const & v) const;
  std::string ValidateAndFormat_postcode(std::string const & v) const;
  std::string ValidateAndFormat_flats(std::string const & v) const;
  std::string ValidateAndFormat_internet(std::string v) const;
  std::string ValidateAndFormat_height(std::string const & v) const;
  std::string ValidateAndFormat_building_levels(std::string v) const;
  std::string ValidateAndFormat_level(std::string v) const;
  std::string ValidateAndFormat_denomination(std::string const & v) const;
  std::string ValidateAndFormat_wikipedia(std::string v) const;
  std::string ValidateAndFormat_airport_iata(std::string const & v) const;
  std::string ValidateAndFormat_duration(std::string const & v) const;

protected:
  FeatureBuilderParams & m_params;
};

class MetadataTagProcessor : private MetadataTagProcessorImpl
{
public:
  /// Make base class constructor public.
  using MetadataTagProcessorImpl::MetadataTagProcessorImpl;

  void operator()(std::string const & k, std::string const & v)
  {
    if (v.empty())
      return;

    using feature::Metadata;
    Metadata & md = m_params.GetMetadata();

    Metadata::EType mdType;
    if (!Metadata::TypeFromString(k, mdType))
    {
      // Specific cases which do not map directly to our metadata types.
      if (k == "building:min_level")
      {
        // Converting this attribute into height only if min_height has not been already set.
        if (!md.Has(Metadata::FMD_MIN_HEIGHT))
          md.Set(Metadata::FMD_MIN_HEIGHT, ValidateAndFormat_building_levels(v));
      }
      return;
    }

    std::string valid;
    switch (mdType)
    {
    case Metadata::FMD_OPEN_HOURS: valid = ValidateAndFormat_opening_hours(v); break;
    case Metadata::FMD_FAX_NUMBER:  // The same validator as for phone.
    case Metadata::FMD_PHONE_NUMBER: valid = ValidateAndFormat_phone(v); break;
    case Metadata::FMD_STARS: valid = ValidateAndFormat_stars(v); break;
    case Metadata::FMD_OPERATOR: valid = ValidateAndFormat_operator(v); break;
    case Metadata::FMD_URL:  // The same validator as for website.
    case Metadata::FMD_FACEBOOK_PAGE:  // The same validator as for website.
    case Metadata::FMD_INSTAGRAM_PAGE:  // The same validator as for website.
    case Metadata::FMD_TWITTER_PAGE:  // The same validator as for website.
    case Metadata::FMD_VK_PAGE:  // The same validator as for website.
    case Metadata::FMD_WEBSITE: valid = ValidateAndFormat_url(v); break;
    case Metadata::FMD_INTERNET: valid = ValidateAndFormat_internet(v); break;
    case Metadata::FMD_ELE: valid = ValidateAndFormat_ele(v); break;
    case Metadata::FMD_TURN_LANES: valid = ValidateAndFormat_turn_lanes(v); break;
    case Metadata::FMD_TURN_LANES_FORWARD: valid = ValidateAndFormat_turn_lanes_forward(v); break;
    case Metadata::FMD_TURN_LANES_BACKWARD: valid = ValidateAndFormat_turn_lanes_backward(v); break;
    case Metadata::FMD_EMAIL: valid = ValidateAndFormat_email(v); break;
    case Metadata::FMD_POSTCODE: valid = ValidateAndFormat_postcode(v); break;
    case Metadata::FMD_WIKIPEDIA: valid = ValidateAndFormat_wikipedia(v); break;
    case Metadata::FMD_FLATS: valid = ValidateAndFormat_flats(v); break;
    case Metadata::FMD_MIN_HEIGHT:  // The same validator as for height.
    case Metadata::FMD_HEIGHT: valid = ValidateAndFormat_height(v); break;
    case Metadata::FMD_DENOMINATION: valid = ValidateAndFormat_denomination(v); break;
    case Metadata::FMD_BUILDING_LEVELS: valid = ValidateAndFormat_building_levels(v); break;
    // Parse banner_url tag added by TagsMixer.
    case Metadata::FMD_BANNER_URL: valid = ValidateAndFormat_url(v); break;
    case Metadata::FMD_LEVEL: valid = ValidateAndFormat_level(v); break;
    case Metadata::FMD_AIRPORT_IATA: valid = ValidateAndFormat_airport_iata(v); break;
    case Metadata::FMD_DURATION: valid = ValidateAndFormat_duration(v); break;
    // Used for old data compatibility only and should not be set:
    case Metadata::FMD_CUISINE:
    // Metadata types we do not get from OSM.
    case Metadata::FMD_SPONSORED_ID:
    case Metadata::FMD_PRICE_RATE:
    case Metadata::FMD_RATING:
    case Metadata::FMD_BRAND:
    case Metadata::FMD_TEST_ID:
    case Metadata::FMD_COUNT: CHECK(false, (mdType, "should not be parsed from OSM."));
    }
    md.Set(mdType, valid);
  }
};
