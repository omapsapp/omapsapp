#include "testing/testing.hpp"

#include "indexer/data_source.hpp"
#include "indexer/features_vector.hpp"
#include "indexer/mwm_set.hpp"

#include "platform/local_country_file.hpp"

#include <map>
#include <string>
#include <vector>

namespace features_vector_test
{
using namespace platform;
using namespace std;

// Postcodes with frequences.
// One can easily get this list of frequences with postcodes:
//
// cat data/minsk-pass.osm.bz2 | bunzip2 | grep 'addr:postcode' |
// sed "s/.*v='\(.*\)'.*/\1/g" |
// sort | uniq -c | awk '{ printf("{%s, %s},\n", $2, $1) }'
//
// Note, (220006, 145) (220007, 271) are broken and not included in minsk-pass.mwm,
// corresponding postcode frequencies are decremented.
vector<pair<int, int>> kCodeFreq = {
  {220000, 2},
  {220001, 3},
  {220004, 10},
  {220006, 144},
  {220007, 270},
  {220010, 4},
  {220011, 1},
  {220014, 3},
  {220030, 247},
  {220033, 7},
  {220036, 204},
  {220039, 15},
  {220048, 1},
  {220050, 4},
  {220069, 5},
  {220073, 1},
  {220089, 1},
  {220121, 1},
  {721816, 1}
};

UNIT_TEST(FeaturesVectorTest_ParseMetadata)
{
  string const kCountryName = "minsk-pass";

  map<string, int> expected;
  for (auto const & p : kCodeFreq)
    expected[strings::to_string(p.first)] = p.second;

  LocalCountryFile localFile = LocalCountryFile::MakeForTesting(kCountryName);

  FrozenDataSource dataSource;
  auto result = dataSource.RegisterMap(localFile);
  TEST_EQUAL(result.second, MwmSet::RegResult::Success, ());

  auto const & id = result.first;
  MwmSet::MwmHandle handle = dataSource.GetMwmHandleById(id);
  TEST(handle.IsAlive(), ());

  auto const * value = handle.GetValue();
  FeaturesVector fv(value->m_cont, value->GetHeader(), value->m_table.get(), value->m_metaDeserializer.get());

  map<string, int> actual;
  fv.ForEach([&](FeatureType & ft, uint32_t)
  {
    string const postcode(ft.GetMetadata(feature::Metadata::FMD_POSTCODE));
    if (!postcode.empty())
      ++actual[postcode];
  });
  TEST_EQUAL(expected, actual, ());
}
} // namespace features_vector_test
