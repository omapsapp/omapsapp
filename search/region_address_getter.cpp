#include "search/region_address_getter.hpp"

#include "storage/country_info_getter.hpp"

#include "platform/preferred_languages.hpp"

namespace search
{
RegionAddressGetter::RegionAddressGetter(DataSource const & dataSource,
                                         storage::CountryInfoGetter const & infoGetter)
  : m_reverseGeocoder(dataSource), m_cityFinder(dataSource), m_infoGetter(infoGetter)
{
  m_nameGetter.LoadCountriesTree();
  m_nameGetter.SetLocale(languages::GetCurrentMapLanguage());
}

ReverseGeocoder::RegionAddress RegionAddressGetter::GetNearbyRegionAddress(
    m2::PointD const & center)
{
  return ReverseGeocoder::GetNearbyRegionAddress(center, m_infoGetter, m_cityFinder);
}

std::string RegionAddressGetter::GetLocalizedRegionAddress(
    ReverseGeocoder::RegionAddress const & addr) const
{
  return m_reverseGeocoder.GetLocalizedRegionAddress(addr, m_nameGetter);
}

std::string RegionAddressGetter::GetLocalizedRegionAddress(m2::PointD const & center)
{
  auto const addr = ReverseGeocoder::GetNearbyRegionAddress(center, m_infoGetter, m_cityFinder);
  return m_reverseGeocoder.GetLocalizedRegionAddress(addr, m_nameGetter);
}
}  // namespace search
