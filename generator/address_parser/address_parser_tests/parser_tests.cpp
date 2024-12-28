#include "testing/testing.hpp"

#include "../processor.hpp"
#include "../tiger_parser.hpp"

#include "generator/generator_tests_support/test_generator.hpp"

#include "search/house_to_street_table.hpp"

#include "indexer/ftypes_matcher.hpp"

#include <fstream>

namespace addr_parser_tests
{

UNIT_TEST(TigerParser_Smoke)
{
  double constexpr kEpsLL = 1.0E-6;

  tiger::AddressEntry e;
  TEST(!tiger::ParseLine("from;to;interpolation;street;city;state;postcode;geometry", e), ());

  e = {};
  TEST(tiger::ParseLine("9587;9619;all;;Platte;MO;64152;LINESTRING(-94.691917 39.210393,-94.692370 39.210351)", e), ());

  e = {};
  TEST(tiger::ParseLine("698;600;all;Boston St;Berkeley;WV;25401;LINESTRING(-77.970484 39.464604,-77.970540 39.464630)", e), ());
  TEST_EQUAL(e.m_from, "600", ());
  TEST_EQUAL(e.m_to, "698", ());
  TEST_EQUAL(e.m_street, "Boston St", ());
  TEST_EQUAL(e.m_postcode, "25401", ());
  TEST_EQUAL(e.m_interpol, feature::InterpolType::Any, ());
  TEST_EQUAL(e.m_geom.size(), 1, ());

  TEST(ms::LatLon((39.464604 + 39.464630) / 2.0, (-77.970484 -77.970540) / 2.0).EqualDxDy(e.m_geom.back(), kEpsLL), ());

  e = {};
  TEST(tiger::ParseLine("798;700;all;Boston St;Berkeley;WV;25401;LINESTRING(-77.968929 39.463906,-77.969118 39.463990,-77.969427 39.464129,-77.969946 39.464353,-77.970027 39.464389)", e), ());
  TEST_EQUAL(e.m_from, "700", ());
  TEST_EQUAL(e.m_to, "798", ());
  TEST_EQUAL(e.m_street, "Boston St", ());
  TEST_EQUAL(e.m_postcode, "25401", ());
  TEST_EQUAL(e.m_interpol, feature::InterpolType::Any, ());
  TEST_EQUAL(e.m_geom.size(), 4, ());

  TEST(ms::LatLon(39.463906, -77.968929).EqualDxDy(e.m_geom.back(), kEpsLL), ());
  TEST(ms::LatLon(39.464389, -77.970027).EqualDxDy(e.m_geom.front(), kEpsLL), ());

  TEST(tiger::ParseLine("0;98;even;Austin Ln;Mifflin;PA;17044;LINESTRING(-77.634119 40.597239,-77.634200 40.597288,-77.634679 40.598169,-77.634835 40.598393,-77.635116 40.598738,-77.635518 40.599388,-77.635718 40.599719,-77.635833 40.599871,-77.635856 40.599920)", e), ());
  TEST_EQUAL(e.m_from, "0", ());
  TEST_EQUAL(e.m_to, "98", ());
}

class TmpDir
{
  std::string const m_path = "./addrs";
public:
  std::string const & Get() const { return m_path; }

  TmpDir()
  {
    (void)Platform::RmDirRecursively(m_path);
    TEST(Platform::MkDirChecked(m_path), ());
  }
  ~TmpDir()
  {
    TEST(Platform::RmDirRecursively(m_path), ());
  }
};

UNIT_TEST(Processor_Smoke)
{
  std::string const kTestFileName = "temp.txt";
  TmpDir outPath;

  {
    std::ofstream of(kTestFileName);
    of << "698;600;all;Boston St;Berkeley;WV;25401;LINESTRING(-77.970484 39.464604,-77.970540 39.464630)" << "\n"
       << "901;975;all;Pease Rd;Sutter;CA;95991;LINESTRING(-121.623833 39.171033,-121.626563 39.171015)" << "\n"
       << "7677;7601;all;N Main St;Livingston;NY;14560;LINESTRING(-77.595115 42.645859,-77.595450 42.648883)" << "\n"
       << "1;99;all;Stokes Dr;Pend Oreille;WA;99156;LINESTRING(-117.209595 48.058395,-117.209584 48.058352)" << "\n";
  }

  addr_generator::Processor processor("./data" /* dataPath */, outPath.Get(), 2 /* numThreads */);

  {
    std::ifstream ifile(kTestFileName);
    processor.Run(ifile);
  }

  Platform::FilesList files;
  Platform::GetFilesByExt(outPath.Get(), TEMP_ADDR_EXTENSION, files);
  TEST_EQUAL(files.size(), 4, ());

  CHECK(base::DeleteFileX(kTestFileName), ());
}

class TestFixture : public generator::tests_support::TestRawGenerator
{
protected:
  TmpDir m_outPath;
  std::string m_mwmName;

  ftypes::IsAddressInterpolChecker const & m_interpolChecker;
  uint32_t m_addrNodeType;

public:
  TestFixture() : m_interpolChecker(ftypes::IsAddressInterpolChecker::Instance())
  {
    auto const & cl = classif();
    m_addrNodeType = cl.GetTypeByPath({"building", "address"});
  }

  void Parse(std::stringstream & ss)
  {
    addr_generator::Processor processor("./data" /* dataPath */, m_outPath.Get(), 1 /* numThreads */);
    processor.Run(ss);

    Platform::FilesList files;
    Platform::GetFilesByExt(m_outPath.Get(), TEMP_ADDR_EXTENSION, files);
    TEST_EQUAL(files.size(), 1, ());

    m_mwmName = std::move(files.front());
    base::GetNameWithoutExt(m_mwmName);
  }

  void BuildAndCheck(std::string const & osmFile, int addrInterpolExpected, int addrNodeExpected)
  {
    // Build mwm.
    GetGenInfo().m_addressesDir = m_outPath.Get();

    BuildFB(osmFile, m_mwmName, false /* makeWorld */);

    int addrInterpolCount = 0, addrNodeCount = 0;
    ForEachFB(m_mwmName, [&](feature::FeatureBuilder const & fb)
    {
      auto const & params = fb.GetParams();
      if (m_interpolChecker(fb.GetTypes()))
      {
        ++addrInterpolCount;
        TEST(!params.GetStreet().empty(), ());
      }
      if (fb.HasType(m_addrNodeType))
      {
        ++addrNodeCount;
        TEST(!params.GetStreet().empty(), ());
      }
    });

    TEST_EQUAL(addrInterpolCount, addrInterpolExpected, ());
    if (addrNodeExpected >= 0)
      TEST_EQUAL(addrNodeCount, addrNodeExpected, ());

    BuildFeatures(m_mwmName);
  }
};

UNIT_CLASS_TEST(TestFixture, Generator_Smoke)
{
  std::stringstream ss;
  ss << "698;600;all;Boston St;Berkeley;WV;25401;LINESTRING(-77.970484 39.464604,-77.970540 39.464630)" << "\n"
     << "798;700;all;Boston St;Berkeley;WV;25401;LINESTRING(-77.968929 39.463906,-77.969118 39.463990,-77.969427 39.464129,-77.969946 39.464353,-77.970027 39.464389)" << "\n";

  Parse(ss);

  BuildAndCheck("./data/test_data/osm/us_tiger_1.osm", 1 /* addrInterpolExpected */, 3 /* addrNodeExpected */);
}

UNIT_CLASS_TEST(TestFixture, Generator_Filter_SF1)
{
  std::stringstream ss;
  ss << "601;699;all;Francisco St;San Francisco;CA;94133;LINESTRING(-122.416189 37.804256,-122.416526 37.804215)" << "\n";

  Parse(ss);

  BuildAndCheck("./data/test_data/osm/us_tiger_2.osm", 0 /* addrInterpolExpected */, 0 /* addrNodeExpected */);
}

UNIT_CLASS_TEST(TestFixture, Generator_Filter_NY)
{
  std::stringstream ss;
  ss << "600;698;even;E 14th St;New York;NY;10009;LINESTRING(-73.977910 40.729298,-73.975890 40.728462)" << "\n";

  Parse(ss);

  BuildAndCheck("./data/test_data/osm/us_tiger_3.osm", 0 /* addrInterpolExpected */, -1 /* addrNodeExpected */);
}

UNIT_CLASS_TEST(TestFixture, Generator_Filter_SF2)
{
  std::stringstream ss;
  ss << "744;752;all;Francisco St;San Francisco;CA;94133;LINESTRING(-122.417593 37.804248,-122.417686 37.804236)" << "\n"
     << "754;798;even;Francisco St;San Francisco;CA;94133;LINESTRING(-122.417933 37.804205,-122.418204 37.804171)" << "\n";

  Parse(ss);

  BuildAndCheck("./data/test_data/osm/us_tiger_4.osm", 0 /* addrInterpolExpected */, 0 /* addrNodeExpected */);
}

UNIT_CLASS_TEST(TestFixture, Generator_Street_Name)
{
  std::stringstream ss;
  ss << "599;501;odd;Seventh St;Marshall;MN;56757;LINESTRING(-96.878165 48.451707,-96.878047 48.451722,-96.877150 48.451844,-96.877034 48.451860)" << "\n"
     << "598;500;even;Seventh St;Marshall;MN;56757;LINESTRING(-96.878214 48.451868,-96.878097 48.451884,-96.877200 48.452006,-96.877084 48.452022)" << "\n";

  Parse(ss);

  BuildAndCheck("./data/test_data/osm/us_tiger_5.osm", 2 /* addrInterpolExpected */, 4 /* addrNodeExpected */);

  BuildSearch(m_mwmName);

  FrozenDataSource dataSource;
  auto const res = dataSource.RegisterMap(platform::LocalCountryFile::MakeTemporary(GetMwmPath(m_mwmName)));
  CHECK_EQUAL(res.second, MwmSet::RegResult::Success, ());

  FeaturesLoaderGuard guard(dataSource, res.first);

  size_t const numFeatures = guard.GetNumFeatures();
  size_t count = 0;
  for (size_t id = 0; id < numFeatures; ++id)
  {
    auto ft = guard.GetFeatureByIndex(id);
    if (m_interpolChecker(*ft))
    {
      ++count;

      auto value = guard.GetHandle().GetValue();
      if (!value->m_house2street)
        value->m_house2street = search::LoadHouseToStreetTable(*value);

      auto res = value->m_house2street->Get(id);
      TEST(res, ());

      auto street = guard.GetFeatureByIndex(res->m_streetId);
      TEST_EQUAL(street->GetName(StringUtf8Multilang::kDefaultCode), "7th Street", ());
    }
  }

  TEST_EQUAL(count, 2, ());
}

} // namespace addr_parser_tests
