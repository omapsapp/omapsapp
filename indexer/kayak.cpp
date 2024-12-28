#include "indexer/kayak.hpp"

#include "base/string_utils.hpp"

#include "coding/url.hpp"

#include <ctime>
#include <map>
#include <sstream>
#include <string>

namespace osm
{

using namespace std;
using strings::to_uint64;

namespace
{
const string KAYAK_AFFILIATE = "kan_267335";

const map<string, string> KAYAK_DOMAINS = {
    {"AR", "www.kayak.com.ar"},
    {"AU", "www.kayak.com.au"},
    {"AT", "www.at.kayak.com"},
    {"BE", "www.be.kayak.com"},
    {"BO", "www.kayak.bo"},
    {"BR", "www.kayak.com.br"},
    {"CA", "www.ca.kayak.com"},
    // {"CAT", "www.kayak.cat"},
    {"CL", "www.kayak.cl"},
    {"CN", "www.cn.kayak.com"},
    {"CO", "www.kayak.com.co"},
    {"CR", "www.kayak.co.cr"},
    {"CZ", "www.cz.kayak.com"},
    {"DK", "www.kayak.dk"},
    {"DO", "www.kayak.com.do"},
    {"EC", "www.kayak.com.ec"},
    {"SV", "www.kayak.com.sv"},
    //{"EE", ""},
    {"FI", "www.fi.kayak.com"},
    {"FR", "www.kayak.fr"},
    {"DE", "www.kayak.de"},
    {"GR", "www.gr.kayak.com"},
    {"GT", "www.kayak.com.gt"},
    {"HN", "www.kayak.com.hn"},
    {"HK", "www.kayak.com.hk"},
    {"IN", "www.kayak.co.in"},
    {"ID", "www.kayak.co.id"},
    {"IE", "www.kayak.ie"},
    {"IL", "www.il.kayak.com"},
    {"IT", "www.kayak.it"},
    {"JP", "www.kayak.co.jp"},
    {"MY", "www.kayak.com.my"},
    {"MX", "www.kayak.com.mx"},
    {"NL", "www.kayak.nl"},
    {"NZ", "www.nz.kayak.com"},
    {"NI", "www.kayak.com.ni"},
    {"NO", "www.kayak.no"},
    {"PA", "www.kayak.com.pa"},
    {"PY", "www.kayak.com.py"},
    {"PE", "www.kayak.com.pe"},
    {"PH", "www.kayak.com.ph"},
    {"PL", "www.kayak.pl"},
    {"PT", "www.kayak.pt"},
    {"PR", "www.kayak.com.pr"},
    // {"QA", ""},
    {"RO", "www.ro.kayak.com"},
    {"SA", "www.en.kayak.sa"},
    {"SG", "www.kayak.sg"},
    {"ZA", "www.za.kayak.com"},
    {"KR", "www.kayak.co.kr"},
    {"ES", "www.kayak.es"},
    {"SE", "www.kayak.se"},
    {"CH", "www.kayak.ch"},
    {"TW", "www.tw.kayak.com"},
    {"TH", "www.kayak.co.th"},
    {"TR", "www.kayak.com.tr"},
    {"UA", "www.ua.kayak.com"},
    {"AE", "www.kayak.ae"},
    {"UK", "www.kayak.co.uk"},
    {"US", "www.kayak.com"},
    {"UY", "www.kayak.com.uy"},
    {"VE", "www.kayak.co.ve"},
    {"VN", "www.vn.kayak.com"},
};
}  // namespace

string GetKayakHotelURL(const string & countryIsoCode, uint64_t kayakHotelId,
                        const string & kayakHotelName, uint64_t kayakCityId,
                        time_t firstDay, time_t lastDay)
{
  // https://www.kayak.com.tr/hotels/Elexus-Hotel-Resort--Spa--Casino,Kyrenia-c7163-h2651619-details/2023-10-03/2023-10-04/1adults

  stringstream url;

  url << "https://";
  auto const it = KAYAK_DOMAINS.find(countryIsoCode);
  url << ((it == KAYAK_DOMAINS.end()) ? KAYAK_DOMAINS.find("US")->second : it->second);
  url << "/in?" << "a=" << KAYAK_AFFILIATE << "&url=";
  url << "/hotels/";
  url << url::Slug(kayakHotelName) << ",";
  url << "-c" << kayakCityId << "-h" << kayakHotelId << "-details";
  struct tm firstDayTm, lastDayTm;
  localtime_r(&firstDay, &firstDayTm);
  localtime_r(&lastDay, &lastDayTm);
  char firstDayBuf[sizeof("9999-99-99")];
  char lastDayBuf[sizeof("9999-99-99")];
  strftime(firstDayBuf, sizeof(firstDayBuf), "%Y-%m-%d", &firstDayTm);
  strftime(lastDayBuf, sizeof(lastDayBuf), "%Y-%m-%d", &lastDayTm);
  url << "/" << firstDayBuf << "/" << lastDayBuf << "/1adults";

  return url.str();
}

string GetKayakHotelURLFromURI(const string & countryIsoCode, const string & uri,
                               time_t firstDay, time_t lastDay)
{
  // Elexus Hotel Resort & Spa & Casino,-c7163-h1696321580

  size_t h = uri.rfind("-h");
  if (h == string::npos)
    return {};

  size_t c = uri.rfind(",-c", h);
  if (c == string::npos)
    return {};

  string kayakHotelName = uri.substr(0, c);
  uint64_t kayakHotelId;
  uint64_t kayakCityId;
  if (!to_uint64(uri.substr(h + 2).c_str(), kayakHotelId) ||
      !to_uint64(uri.substr(c + 3, h - c - 3).c_str(), kayakCityId))
    return {};

  return GetKayakHotelURL(countryIsoCode, kayakHotelId, kayakHotelName, kayakCityId, firstDay, lastDay);
}

}  // namespace osm
