#include "testing/testing.hpp"

#include "search/latlon_match.hpp"

#include "base/math.hpp"

namespace latlon_match_test
{
using namespace search;

// We expect the results to be quite precise.
double const kEps = 1e-12;

void TestAlmostEqual(double actual, double expected)
{
  TEST(base::AlmostEqualAbsOrRel(actual, expected, kEps), (actual, expected));
}

UNIT_TEST(LatLon_Match_Smoke)
{
  double lat, lon;

  TEST(!MatchLatLonDegree("10,20", lat, lon), ());

  TEST(MatchLatLonDegree("10, 20", lat, lon), ());
  TestAlmostEqual(lat, 10.0);
  TestAlmostEqual(lon, 20.0);

  TEST(MatchLatLonDegree("10.0 20.0", lat, lon), ());
  TestAlmostEqual(lat, 10.0);
  TestAlmostEqual(lon, 20.0);

  TEST(MatchLatLonDegree("10.0, 20,0", lat, lon), ());
  TestAlmostEqual(lat, 10.0);
  TestAlmostEqual(lon, 20.0);

  TEST(MatchLatLonDegree("10.10, 20.20", lat, lon), ());
  TestAlmostEqual(lat, 10.1);
  TestAlmostEqual(lon, 20.2);

  TEST(MatchLatLonDegree("10,10 20,20", lat, lon), ());
  TestAlmostEqual(lat, 10.1);
  TestAlmostEqual(lon, 20.2);

  TEST(MatchLatLonDegree("10,10, 20,20", lat, lon), ());
  TestAlmostEqual(lat, 10.1);
  TestAlmostEqual(lon, 20.2);

  TEST(MatchLatLonDegree("-10,10, 20,20", lat, lon), ());
  TestAlmostEqual(lat, -10.1);
  TestAlmostEqual(lon, 20.2);
  
  TEST(MatchLatLonDegree("10,10, -20,20", lat, lon), ());
  TestAlmostEqual(lat, 10.1);
  TestAlmostEqual(lon, -20.2);

  TEST(MatchLatLonDegree("-10,10, -20,20", lat, lon), ());
  TestAlmostEqual(lat, -10.1);
  TestAlmostEqual(lon, -20.2);

  TEST(MatchLatLonDegree("-10,10 20,20", lat, lon), ());
  TestAlmostEqual(lat, -10.1);
  TestAlmostEqual(lon, 20.2);
  
  TEST(MatchLatLonDegree("10,10 -20,20", lat, lon), ());
  TestAlmostEqual(lat, 10.1);
  TestAlmostEqual(lon, -20.2);

  TEST(MatchLatLonDegree("-10,10 -20,20", lat, lon), ());
  TestAlmostEqual(lat, -10.1);
  TestAlmostEqual(lon, -20.2);

  TEST(MatchLatLonDegree("-22.3534 -42.7076\n", lat, lon), ());
  TestAlmostEqual(lat, -22.3534);
  TestAlmostEqual(lon, -42.7076);

  TEST(MatchLatLonDegree("-22,3534 -42,7076\n", lat, lon), ());
  TestAlmostEqual(lat, -22.3534);
  TestAlmostEqual(lon, -42.7076);

  // The ".123" form is not accepted, so our best-effort
  // parse results in "10" and "20".
  TEST(MatchLatLonDegree(".10, ,20", lat, lon), ());
  TestAlmostEqual(lat, 10.0);
  TestAlmostEqual(lon, 20.0);

  TEST(!MatchLatLonDegree("34-31", lat, lon), ());
  TEST(!MatchLatLonDegree("34/31", lat, lon), ());
  TEST(!MatchLatLonDegree("34,31", lat, lon), ());

  /// @todo 5E-5 eats as full double here. This is a very fancy case, but anyway ...
  TEST(!MatchLatLonDegree("N5E-5", lat, lon), ());
  TEST(!MatchLatLonDegree("5E-5", lat, lon), ());

  TEST(MatchLatLonDegree("N5W-5", lat, lon), ());
  TestAlmostEqual(lat, 5);
  TestAlmostEqual(lon, 5);
  // Same as "N5 E-5"
  TEST(MatchLatLonDegree("5 E-5", lat, lon), ());
  TestAlmostEqual(lat, 5);
  TestAlmostEqual(lon, -5);

  TEST(!MatchLatLonDegree("., .", lat, lon), ());
  TEST(!MatchLatLonDegree("10, .", lat, lon), ());

  TEST(MatchLatLonDegree("0*30\', 1*0\'30\"", lat, lon), ());
  TestAlmostEqual(lat, 0.5);
  TestAlmostEqual(lon, 1.00833333333333);

  TEST(MatchLatLonDegree("50  *, 40 *", lat, lon), ());
  TestAlmostEqual(lat, 50.0);
  TestAlmostEqual(lon, 40.0);

  TEST(!MatchLatLonDegree("50* 40*, 30*", lat, lon), ());

  TEST(MatchLatLonDegree("(-50°30\'30\" -49°59\'59\"", lat, lon), ());
  TestAlmostEqual(lat, -50.50833333333333);
  TestAlmostEqual(lon, -49.99972222222222);

  TEST(!MatchLatLonDegree("50°, 30\"", lat, lon), ());
  TEST(!MatchLatLonDegree("50\', -50°", lat, lon), ());
  TEST(!MatchLatLonDegree("-90*50\'50\", -50°", lat, lon), ());

  TEST(MatchLatLonDegree("(-89*, 360*)", lat, lon), ());
  TestAlmostEqual(lat, -89.0);
  TestAlmostEqual(lon, 0.0);

  TEST(MatchLatLonDegree("-89*15.5\' N; 120*30\'50.5\" e", lat, lon), ());
  TestAlmostEqual(lat, -89.25833333333333);
  TestAlmostEqual(lon, 120.51402777777778);

  TEST(MatchLatLonDegree("N55°45′20.99″ E37°37′03.62″", lat, lon), ());
  TestAlmostEqual(lat, 55.755830555555556);
  TestAlmostEqual(lon, 37.617672222222222);

  {
    TEST(MatchLatLonDegree("N-55°45′20.99″ E-37°37′03.62″", lat, lon), ());
    double lat1, lon1;
    TEST(MatchLatLonDegree("S55°45′20.99″ W37°37′03.62″", lat1, lon1), ());
    TestAlmostEqual(lat, lat1);
    TestAlmostEqual(lon, lon1);
  }

  TEST(MatchLatLonDegree("55°45’20.9916\"N, 37°37’3.6228\"E hsdfjgkdsjbv", lat, lon), ());
  TestAlmostEqual(lat, 55.755831);
  TestAlmostEqual(lon, 37.617673);

  TEST(MatchLatLonDegree("55°45′20.9916″S, 37°37′3.6228″W", lat, lon), ());
  TestAlmostEqual(lat, -55.755831);
  TestAlmostEqual(lon, -37.617673);

  // We can receive already normalized string, and double quotes become two single quotes
  TEST(MatchLatLonDegree("55°45′20.9916′′S, 37°37′3.6228′′W", lat, lon), ());
  TestAlmostEqual(lat, -55.755831);
  TestAlmostEqual(lon, -37.617673);

  TEST(MatchLatLonDegree("W55°45′20.9916″, S37°37′3.6228″", lat, lon), ());
  TestAlmostEqual(lon, -55.755831);
  TestAlmostEqual(lat, -37.617673);

  TEST(MatchLatLonDegree("55°45′20.9916″ W 37°37′3.6228″ N", lat, lon), ());
  TestAlmostEqual(lon, -55.755831);
  TestAlmostEqual(lat, 37.617673);

  TEST(!MatchLatLonDegree("55°45′20.9916″W 37°37′3.6228″E", lat, lon), ());
  TEST(!MatchLatLonDegree("N55°45′20.9916″ S37°37′3.6228″", lat, lon), ());

  TEST(MatchLatLonDegree("54° 25' 0N 1° 53' 46W", lat, lon), ());
  TestAlmostEqual(lat, 54.41666666666667);
  TestAlmostEqual(lon, -1.89611111111111);

  TEST(MatchLatLonDegree("47.33471°N 8.53112°E", lat, lon), ());
  TestAlmostEqual(lat, 47.33471);
  TestAlmostEqual(lon, 8.53112);

  TEST(MatchLatLonDegree("N 51* 33.217 E 11* 10.113", lat, lon), ());
  TestAlmostEqual(lat, 51.55361666666667);
  TestAlmostEqual(lon, 11.16855);

  TEST(!MatchLatLonDegree("N 51* 33.217 E 11* 60.113", lat, lon), ());
  TEST(!MatchLatLonDegree("N 51* -33.217 E 11* 10.113", lat, lon), ());
  TEST(!MatchLatLonDegree("N 33.217\' E 11* 10.113", lat, lon), ());
  TEST(!MatchLatLonDegree("N 51* 33.217 E 11* 10.113\"", lat, lon), ());
}

UNIT_TEST(LatLon_Match_False)
{
  double lat, lon;
  TEST(!MatchLatLonDegree("2 1st", lat, lon), ());
}

} // namespace latlon_match_test
