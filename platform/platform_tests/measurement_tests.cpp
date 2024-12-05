#include "testing/testing.hpp"

#include "platform/measurement_utils.hpp"

#include "base/math.hpp"

#include <iostream>
#include <string>

using namespace measurement_utils;
using namespace platform;

std::string AddGroupingSeparators(std::string const & valueString, std::string const & groupingSeparator)
{
  std::string out(valueString);

  if (out.size() > 4 && !groupingSeparator.empty())
      for (int pos = out.size() - 3; pos > 0; pos -= 3)
        out.insert(pos, groupingSeparator);

  return out;
}

UNIT_TEST(LatLonToDMS_Origin)
{
  TEST_EQUAL(FormatLatLonAsDMS(0, 0, false), "00°00′00″ 00°00′00″", ());
  TEST_EQUAL(FormatLatLonAsDMS(0, 0, false, 3), "00°00′00″ 00°00′00″", ());
}

UNIT_TEST(LatLonToDMS_Rounding)
{
  // Here and after data is from Wiki: http://bit.ly/datafotformatingtest
  // Boston
  TEST_EQUAL(FormatLatLonAsDMS(42.358056, -71.063611, false, 0), "42°21′29″N 71°03′49″W", ());
  // Minsk
  TEST_EQUAL(FormatLatLonAsDMS(53.916667, 27.55, false, 0), "53°55′00″N 27°33′00″E", ());
  // Rio
  TEST_EQUAL(FormatLatLonAsDMS(-22.908333, -43.196389, false, 0), "22°54′30″S 43°11′47″W", ());
}

UNIT_TEST(LatLonToDMS_NoRounding)
{
  // Paris
  TEST_EQUAL(FormatLatLonAsDMS(48.8567, 2.3508, false, 2), "48°51′24.12″N 02°21′02.88″E", ());
  // Vatican
  TEST_EQUAL(FormatLatLonAsDMS(41.904, 12.453, false, 2), "41°54′14.4″N 12°27′10.8″E", ());

  TEST_EQUAL(FormatLatLonAsDMS(21.981112, -159.371112, false, 2), "21°58′52″N 159°22′16″W", ());
}

UNIT_TEST(FormatOsmLink)
{
  // Zero point
  TEST_EQUAL(FormatOsmLink(0, 0, 5), "https://osm.org/go/wAAAA-?m", ());
  // Eifel tower
  TEST_EQUAL(FormatOsmLink(48.85825, 2.29450, 15), "https://osm.org/go/0BOdUs9e--?m", ());
  // Buenos Aires
  TEST_EQUAL(FormatOsmLink(-34.6061, -58.4360, 10), "https://osm.org/go/Mnx6SB?m", ());

  // Formally, lat = -90 and lat = 90 are the same for OSM links, but Mercator is valid until 85.
  auto link = FormatOsmLink(-90, -180, 10);
  TEST(link == "https://osm.org/go/AAAAAA?m" || link == "https://osm.org/go/~~~~~~?m", (link));
  link = FormatOsmLink(90, 180, 10);
  TEST(link == "https://osm.org/go/AAAAAA?m" || link == "https://osm.org/go/~~~~~~?m", (link));
}

UNIT_TEST(FormatSpeedNumeric)
{
  TEST_EQUAL(FormatSpeedNumeric(10, Units::Metric), "36", ());
  TEST_EQUAL(FormatSpeedNumeric(1, Units::Metric), "4", ());

  TEST_EQUAL(FormatSpeedNumeric(10, Units::Imperial), "22", ());
  TEST_EQUAL(FormatSpeedNumeric(1, Units::Imperial), "2", ());
}

UNIT_TEST(OSMDistanceToMetersString)
{
  TEST_EQUAL(OSMDistanceToMetersString(""), "", ());
  TEST_EQUAL(OSMDistanceToMetersString("INF"), "", ());
  TEST_EQUAL(OSMDistanceToMetersString("NAN"), "", ());
  TEST_EQUAL(OSMDistanceToMetersString("not a number"), "", ());
  TEST_EQUAL(OSMDistanceToMetersString("10й"), "10", ());
  TEST_EQUAL(OSMDistanceToMetersString("0"), "0", ());
  TEST_EQUAL(OSMDistanceToMetersString("0.0"), "0", ());
  TEST_EQUAL(OSMDistanceToMetersString("0.0000000"), "0", ());
  TEST_EQUAL(OSMDistanceToMetersString("22.5"), "22.5", ());
  TEST_EQUAL(OSMDistanceToMetersString("+21.5"), "21.5", ());
  TEST_EQUAL(OSMDistanceToMetersString("1e+2"), "100", ());
  TEST_EQUAL(OSMDistanceToMetersString("5 метров"), "5", ());
  TEST_EQUAL(OSMDistanceToMetersString(" 4.4 "), "4.4", ());
  TEST_EQUAL(OSMDistanceToMetersString("8-15"), "15", ());
  TEST_EQUAL(OSMDistanceToMetersString("8-15 ft"), "4.57", ());
  TEST_EQUAL(OSMDistanceToMetersString("8-й километр"), "8", ());
  // Do not support lists for distance values.
  TEST_EQUAL(OSMDistanceToMetersString("8;9;10"), "", ());
  TEST_EQUAL(OSMDistanceToMetersString("8;9;10 meters"), "", ());
  TEST_EQUAL(OSMDistanceToMetersString("8;9;10 km"), "", ());
  TEST_EQUAL(OSMDistanceToMetersString("10;20!111"), "", ());
  TEST_EQUAL(OSMDistanceToMetersString("10;20\""), "", ());
  TEST_EQUAL(OSMDistanceToMetersString("-100.3"), "-100.3", ());
  TEST_EQUAL(OSMDistanceToMetersString("99.0000000"), "99", ());
  TEST_EQUAL(OSMDistanceToMetersString("8900.000023"), "8900", ());
  TEST_EQUAL(OSMDistanceToMetersString("-300.9999"), "-301", ());
  TEST_EQUAL(OSMDistanceToMetersString("-300.9"), "-300.9", ());
  TEST_EQUAL(OSMDistanceToMetersString("15 m"), "15", ());
  TEST_EQUAL(OSMDistanceToMetersString("15.9 m"), "15.9", ());
  TEST_EQUAL(OSMDistanceToMetersString("15.9m"), "15.9", ());
  TEST_EQUAL(OSMDistanceToMetersString("3000 ft"), "914.4", ());
  TEST_EQUAL(OSMDistanceToMetersString("3000ft"), "914.4", ());
  TEST_EQUAL(OSMDistanceToMetersString("100 feet"), "30.48", ());
  TEST_EQUAL(OSMDistanceToMetersString("100feet"), "30.48", ());
  TEST_EQUAL(OSMDistanceToMetersString("3 nmi"), "5556", ());
  TEST_EQUAL(OSMDistanceToMetersString("3 mi"), "4828.03", ());
  TEST_EQUAL(OSMDistanceToMetersString("3.3 km"), "3300", ());
  TEST_EQUAL(OSMDistanceToMetersString("105'"), "32", ());
  TEST_EQUAL(OSMDistanceToMetersString("11'"), "3.35", ());
  TEST_EQUAL(OSMDistanceToMetersString("11 3\""), "11", ());
  TEST_EQUAL(OSMDistanceToMetersString("11 3'"), "11", ());
  TEST_EQUAL(OSMDistanceToMetersString("11\"'"), "0.28", ());
  TEST_EQUAL(OSMDistanceToMetersString("11'\""), "3.35", ());
  TEST_EQUAL(OSMDistanceToMetersString("11'4\""), "3.45", ());
  TEST_EQUAL(OSMDistanceToMetersString("11\""), "0.28", ());
  TEST_EQUAL(OSMDistanceToMetersString("100 \""), "100", ());

  TEST_EQUAL(OSMDistanceToMetersString("0", false), "", ());
  TEST_EQUAL(OSMDistanceToMetersString("-15", false), "", ());
  TEST_EQUAL(OSMDistanceToMetersString("15.12345", false, 1), "15.1", ());
  TEST_EQUAL(OSMDistanceToMetersString("15.123", false, 4), "15.123", ());
  TEST_EQUAL(OSMDistanceToMetersString("15.654321", true, 1), "15.7", ());
}

UNIT_TEST(UnitsConversion)
{
  double const kEps = 1e-5;
  TEST(AlmostEqualAbs(MilesToMeters(MetersToMiles(1000.0)), 1000.0, kEps), ());
  TEST(AlmostEqualAbs(MilesToMeters(1.0), 1609.344, kEps), ());

  TEST(AlmostEqualAbs(MiphToKmph(KmphToMiph(100.0)), 100.0, kEps), ());
  TEST(AlmostEqualAbs(MiphToKmph(100.0), 160.9344, kEps), (MiphToKmph(100.0)));

  TEST(AlmostEqualAbs(ToSpeedKmPH(100.0, Units::Imperial), 160.9344, kEps), ());
  TEST(AlmostEqualAbs(ToSpeedKmPH(100.0, Units::Metric), 100.0, kEps), ());

  TEST(AlmostEqualAbs(KmphToMps(3.6), 1.0, kEps), ());
  TEST(AlmostEqualAbs(MpsToKmph(1.0), 3.6, kEps), ());
}

UNIT_TEST(ToStringPrecisionLocale)
{
  double d1 = 9.8;
  int pr1 = 1;

  double d2 = 12345.0;
  int pr2 = 0;
  std::string d2String("12345");

  struct TestData
  {
    std::string localeName;
    std::string d1String;
  };

  TestData testData[] = {
          // Locale name ,   Decimal
          { "en_US.UTF-8",   "9.8"},
          { "es_ES.UTF-8",   "9,8"},
          { "fr_FR.UTF-8",   "9,8"},
          { "ru_RU.UTF-8",   "9,8"}
  };

  for (TestData const & data : testData)
  {
    Locale loc;

    if (!GetLocale(data.localeName, loc))
    {
      std::cout << "Locale '" << data.localeName << "' not found!! Skipping test..." << std::endl;
      continue;
    }

    TEST_EQUAL(measurement_utils::ToStringPrecisionLocale(loc, d1, pr1), data.d1String, ());
    TEST_EQUAL(measurement_utils::ToStringPrecisionLocale(loc, d2, pr2),
               AddGroupingSeparators(d2String, loc.m_groupingSeparator), ());
  }
}
