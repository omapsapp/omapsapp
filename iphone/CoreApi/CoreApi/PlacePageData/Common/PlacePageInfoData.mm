#import "PlacePageInfoData+Core.h"

#import "OpeningHours.h"

#import <CoreApi/StringUtils.h>

#include "platform/localization.hpp"

#include "indexer/validate_and_format_contacts.hpp"
#include "indexer/kayak.hpp"
#include "indexer/feature_meta.hpp"

#include "map/place_page_info.hpp"

using namespace place_page;
using namespace osm;

/// Get localized metadata value string when string format is "type.feature.value".
NSString * GetLocalizedMetadataValueString(MapObject::MetadataID metaID, std::string const & value) {
  return ToNSString(platform::GetLocalizedTypeName(feature::ToString(metaID) + "." + value));
}

@implementation PlacePageInfoData

@end

@implementation PlacePageInfoData (Core)

- (instancetype)initWithRawData:(Info const &)rawData ohLocalization:(id<IOpeningHoursLocalization>)localization {
  self = [super init];
  if (self)
  {
    auto const cuisines = rawData.FormatCuisines();
    if (!cuisines.empty())
      _cuisine = ToNSString(cuisines);

    /// @todo Refactor PlacePageInfoData to have a map of simple string properties.
    using MetadataID = MapObject::MetadataID;
    rawData.ForEachMetadataReadable([&](MetadataID metaID, std::string const & value)
    {
      switch (metaID)
      {
        case MetadataID::FMD_OPEN_HOURS:
          _openingHoursString = ToNSString(value);
          _openingHours = [[OpeningHours alloc] initWithRawString:_openingHoursString
                                                     localization:localization];
          break;
        case MetadataID::FMD_PHONE_NUMBER:
        {
          _phone = ToNSString(value);
          NSString *filteredDigits = [[_phone componentsSeparatedByCharactersInSet:
                                       [[NSCharacterSet decimalDigitCharacterSet] invertedSet]]
                                      componentsJoinedByString:@""];
          NSString *resultNumber = [_phone hasPrefix:@"+"] ? [NSString stringWithFormat:@"+%@", filteredDigits] : filteredDigits;
          _phoneUrl = [NSURL URLWithString:[NSString stringWithFormat:@"tel://%@", resultNumber]];
          break;
        }
        case MetadataID::FMD_WEBSITE: _website = ToNSString(value); break;
        case MetadataID::FMD_EXTERNAL_URI:
        {
          NSString *countryIsoCode = [[NSLocale currentLocale] objectForKey:NSLocaleCountryCode] ?: @"US";
          time_t firstDaySec = time_t([[NSDate date] timeIntervalSince1970]);
          time_t lastDaySec = firstDaySec + 86400;
          std::string kayakUrl = osm::GetKayakHotelURLFromURI([countryIsoCode UTF8String], value, firstDaySec, lastDaySec);
          if (!kayakUrl.empty())
            _kayak = ToNSString(kayakUrl);
          break;
        }
        case MetadataID::FMD_WIKIPEDIA: _wikipedia = ToNSString(value); break;
        case MetadataID::FMD_WIKIMEDIA_COMMONS: _wikimediaCommons = ToNSString(value); break;
        case MetadataID::FMD_EMAIL:
          _email = ToNSString(value);
          _emailUrl = [NSURL URLWithString:[NSString stringWithFormat:@"mailto:%@", _email]];
          break;
        case MetadataID::FMD_CONTACT_FACEBOOK: _facebook = ToNSString(value); break;
        case MetadataID::FMD_CONTACT_INSTAGRAM: _instagram = ToNSString(value); break;
        case MetadataID::FMD_CONTACT_TWITTER: _twitter = ToNSString(value); break;
        case MetadataID::FMD_CONTACT_VK: _vk = ToNSString(value); break;
        case MetadataID::FMD_CONTACT_LINE: _line = ToNSString(value); break;
        case MetadataID::FMD_OPERATOR: _ppOperator = [NSString stringWithFormat:NSLocalizedString(@"operator", nil), ToNSString(value)]; break;
        case MetadataID::FMD_INTERNET:
          _wifiAvailable = (rawData.GetInternet() == feature::Internet::No)
              ? NSLocalizedString(@"no_available", nil) : NSLocalizedString(@"yes_available", nil);
          break;
        case MetadataID::FMD_LEVEL: _level = ToNSString(value); break;
        case MetadataID::FMD_CAPACITY: _capacity = [NSString stringWithFormat:NSLocalizedString(@"capacity", nil), ToNSString(value)]; break;
        case MetadataID::FMD_WHEELCHAIR: _wheelchair = ToNSString(platform::GetLocalizedTypeName(value)); break;
        case MetadataID::FMD_DRIVE_THROUGH:
          if (value == "yes")
            _driveThrough = NSLocalizedString(@"drive_through", nil);
          break;
        case MetadataID::FMD_WEBSITE_MENU: _websiteMenu = ToNSString(value); break;
        case MetadataID::FMD_SELF_SERVICE: _selfService = GetLocalizedMetadataValueString(metaID, value); break;
        case MetadataID::FMD_OUTDOOR_SEATING:
          if (value == "yes")
            _outdoorSeating = NSLocalizedString(@"outdoor_seating", nil);
          break;
        case MetadataID::FMD_NETWORK: _network = [NSString stringWithFormat:NSLocalizedString(@"network", nil), ToNSString(value)]; break;
        default:
          break;
      }
    });

    _atm = rawData.HasAtm() ? NSLocalizedString(@"type.amenity.atm", nil) : nil;

    _address = rawData.GetSecondarySubtitle().empty() ? nil : @(rawData.GetSecondarySubtitle().c_str());
    _coordFormats = @[@(rawData.GetFormattedCoordinate(place_page::CoordinatesFormat::LatLonDMS).c_str()),
                      @(rawData.GetFormattedCoordinate(place_page::CoordinatesFormat::LatLonDecimal).c_str()),
                      @(rawData.GetFormattedCoordinate(place_page::CoordinatesFormat::OLCFull).c_str()),
                      @(rawData.GetFormattedCoordinate(place_page::CoordinatesFormat::OSMLink).c_str()),
                      @(rawData.GetFormattedCoordinate(place_page::CoordinatesFormat::UTM).c_str()),
                      @(rawData.GetFormattedCoordinate(place_page::CoordinatesFormat::MGRS).c_str())];
  }
  return self;
}

@end
