#pragma once

#include <string>
#include <cstdint>
#include <ctime>

namespace osm {

std::string GetKayakHotelURL(const std::string & countryIsoCode, uint64_t kayakHotelId,
                             const std::string & kayakHotelName, uint64_t kayakCityId,
                             time_t firstDay, time_t lastDay);

std::string GetKayakHotelURLFromURI(const std::string & countryIsoCode, const std::string & uri,
                                    time_t firstDay, time_t lastDay);
} // namespace osm
