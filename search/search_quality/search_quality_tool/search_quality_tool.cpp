#include "search/search_quality/helpers.hpp"

#include "search/search_tests_support/test_search_engine.hpp"
#include "search/search_tests_support/test_search_request.hpp"

#include "search/ranking_info.hpp"
#include "search/result.hpp"
#include "search/search_params.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/data_source.hpp"
#include "indexer/mwm_set.hpp"

#include "platform/platform_tests_support/helpers.hpp"

#include "platform/country_file.hpp"
#include "platform/platform.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point2d.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/stl_helpers.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include <gflags/gflags.h>

using namespace search::search_quality;
using namespace search::tests_support;
using namespace search;
using namespace std::chrono;
using namespace std;

DEFINE_string(data_path, "", "Path to data directory (resources dir)");
DEFINE_string(locale, "en", "Locale of all the search queries");
DEFINE_int32(num_threads, 1, "Number of search engine threads");
DEFINE_string(mwm_list_path, "",
              "Path to a file containing the names of available mwms, one per line");
DEFINE_string(mwm_path, "", "Path to mwm files (writable dir)");
DEFINE_string(queries_path, "", "Path to the file with queries");
DEFINE_int32(top, 1, "Number of top results to show for every query");
DEFINE_string(viewport, "", "Viewport to use when searching (default, moscow, london, zurich)");
DEFINE_string(check_completeness, "", "Path to the file with completeness data");
DEFINE_string(ranking_csv_file, "", "File ranking info will be exported to");

string const kDefaultQueriesPathSuffix =
    "/../search/search_quality/search_quality_tool/queries.txt";
string const kEmptyResult = "<empty>";

// Unlike strings::Tokenize, this function allows for empty tokens.
void Split(string const & s, char delim, vector<string> & parts)
{
  istringstream iss(s);
  string part;
  while (getline(iss, part, delim))
    parts.push_back(part);
}

struct CompletenessQuery
{
  DECLARE_EXCEPTION(MalformedQueryException, RootException);

  explicit CompletenessQuery(string && s)
  {
    s.append(" ");

    vector<string> parts;
    Split(s, ';', parts);
    if (parts.size() != 7)
    {
      MYTHROW(MalformedQueryException,
              ("Can't split", s, ", found", parts.size(), "part(s):", parts));
    }

    auto const idx = parts[0].find(':');
    if (idx == string::npos)
      MYTHROW(MalformedQueryException, ("Could not find \':\':", s));

    string mwmName = parts[0].substr(0, idx);
    string const kMwmSuffix = ".mwm";
    if (!mwmName.ends_with(kMwmSuffix))
      MYTHROW(MalformedQueryException, ("Bad mwm name:", s));

    string const featureIdStr = parts[0].substr(idx + 1);
    uint64_t featureId;
    if (!strings::to_uint64(featureIdStr, featureId))
      MYTHROW(MalformedQueryException, ("Bad feature id:", s));

    string const type = parts[1];
    double lon, lat;
    if (!strings::to_double(parts[2].c_str(), lon) || !strings::to_double(parts[3].c_str(), lat))
      MYTHROW(MalformedQueryException, ("Bad lon-lat:", s));

    string const city = parts[4];
    string const street = parts[5];
    string const house = parts[6];

    mwmName = mwmName.substr(0, mwmName.size() - kMwmSuffix.size());
    string country = mwmName;
    replace(country.begin(), country.end(), '_', ' ');

    m_query = country + " " + city + " " + street + " " + house + " ";
    m_mwmName = mwmName;
    m_featureId = static_cast<uint32_t>(featureId);
    m_lat = lat;
    m_lon = lon;
  }

  string m_query;
  unique_ptr<TestSearchRequest> m_request;
  string m_mwmName;
  uint32_t m_featureId = 0;
  double m_lat = 0;
  double m_lon = 0;
};

string MakePrefixFree(string const & query) { return query + " "; }

// If n == 1, prints the query and the top result separated by a tab.
// Otherwise, prints the query on a separate line
// and then prints n top results on n lines starting with tabs.
void PrintTopResults(string const & query, vector<Result> const & results, size_t n,
                     double elapsedSeconds)
{
  cout << query;
  char timeBuf[100];
  snprintf(timeBuf, sizeof(timeBuf), "\t[%.3fs]", elapsedSeconds);
  if (n > 1)
    cout << timeBuf;
  for (size_t i = 0; i < n; ++i)
  {
    if (n > 1)
      cout << endl;
    cout << "\t";
    if (i < results.size())
      // todo(@m) Print more information: coordinates, viewport, etc.
      cout << results[i].GetString();
    else
      cout << kEmptyResult;
  }
  if (n == 1)
    cout << timeBuf;
  cout << endl;
}

void CalcStatistics(vector<double> const & a, double & avg, double & maximum, double & var,
                    double & stdDev)
{
  avg = 0;
  maximum = 0;
  var = 0;
  stdDev = 0;

  for (auto const x : a)
  {
    avg += static_cast<double>(x);
    maximum = max(maximum, static_cast<double>(x));
  }

  double n = static_cast<double>(a.size());
  if (a.size() > 0)
    avg /= n;

  for (double const x : a)
    var += math::Pow2(x - avg);
  if (a.size() > 1)
    var /= n - 1;
  stdDev = sqrt(var);
}

// Returns the position of the result that is expected to be found by geocoder completeness
// tests in the |result| vector or -1 if it does not occur there.
int FindResult(DataSource & dataSource, string const & mwmName, uint32_t const featureId,
               double const lat, double const lon, vector<Result> const & results)
{
  CHECK_LESS_OR_EQUAL(results.size(), numeric_limits<int>::max(), ());
  auto const mwmId = dataSource.GetMwmIdByCountryFile(platform::CountryFile(mwmName));
  FeatureID const expectedFeatureId(mwmId, featureId);
  for (size_t i = 0; i < results.size(); ++i)
  {
    auto const & r = results[i];
    if (r.GetFeatureID() == expectedFeatureId)
      return static_cast<int>(i);
  }

  // Another attempt. If the queries are stale, feature id is useless.
  // However, some information may be recovered from (lat, lon).
  double const kEps = 1e-2;
  for (size_t i = 0; i < results.size(); ++i)
  {
    auto const & r = results[i];
    if (r.HasPoint() &&
        AlmostEqualAbs(r.GetFeatureCenter(), mercator::FromLatLon(lat, lon), kEps))
    {
      double const dist =
          mercator::DistanceOnEarth(r.GetFeatureCenter(), mercator::FromLatLon(lat, lon));
      LOG(LDEBUG, ("dist =", dist));
      return static_cast<int>(i);
    }
  }
  return -1;
}

// Reads queries in the format
//   CountryName.mwm:featureId;type;lon;lat;city;street;<housenumber or housename>
// from |path|, executes them against the |engine| with viewport set to |viewport|
// and reports the number of queries whose expected result is among the returned results.
// Exact feature id is expected, but a close enough (lat, lon) is good too.
void CheckCompleteness(string const & path, DataSource & dataSource, TestSearchEngine & engine,
                       m2::RectD const & viewport, string const & locale)
{
  base::ScopedLogAbortLevelChanger const logAbortLevel(LCRITICAL);

  ifstream stream(path.c_str());
  CHECK(stream.is_open(), ("Can't open", path));

  base::Timer timer;

  uint32_t totalQueries = 0;
  uint32_t malformedQueries = 0;
  uint32_t expectedResultsFound = 0;
  uint32_t expectedResultsTop1 = 0;

  // todo(@m) Process the queries on the fly and do not keep them.
  vector<CompletenessQuery> queries;

  string line;
  while (getline(stream, line))
  {
    ++totalQueries;
    try
    {
      CompletenessQuery q(std::move(line));
      q.m_request =
          make_unique<TestSearchRequest>(engine, q.m_query, locale, Mode::Everywhere, viewport);
      queries.push_back(std::move(q));
    }
    catch (CompletenessQuery::MalformedQueryException & e)
    {
      LOG(LERROR, (e.what()));
      ++malformedQueries;
    }
  }

  for (auto & q : queries)
  {
    q.m_request->Run();

    LOG(LDEBUG, (q.m_query, q.m_request->Results()));
    int pos = FindResult(dataSource, q.m_mwmName, q.m_featureId, q.m_lat, q.m_lon,
                         q.m_request->Results());
    if (pos >= 0)
      ++expectedResultsFound;
    if (pos == 0)
      ++expectedResultsTop1;
  }

  double const expectedResultsFoundPercentage =
      totalQueries == 0
          ? 0
          : 100.0 * static_cast<double>(expectedResultsFound) / static_cast<double>(totalQueries);
  double const expectedResultsTop1Percentage =
      totalQueries == 0
          ? 0
          : 100.0 * static_cast<double>(expectedResultsTop1) / static_cast<double>(totalQueries);

  cout << "Time spent on checking completeness: " << timer.ElapsedSeconds() << "s." << endl;
  cout << "Total queries: " << totalQueries << endl;
  cout << "Malformed queries: " << malformedQueries << endl;
  cout << "Expected results found: " << expectedResultsFound << " ("
       << expectedResultsFoundPercentage << "%)." << endl;
  cout << "Expected results found in the top1 slot: " << expectedResultsTop1 << " ("
       << expectedResultsTop1Percentage << "%)." << endl;
}

void RunRequests(TestSearchEngine & engine, m2::RectD const & viewport, string queriesPath,
                 string const & locale, string const & rankingCSVFile, size_t top)
{
  vector<string> queries;
  {
    if (queriesPath.empty())
      queriesPath = base::JoinPath(GetPlatform().WritableDir(), kDefaultQueriesPathSuffix);
    ReadStringsFromFile(queriesPath, queries);
  }

  vector<unique_ptr<TestSearchRequest>> requests;
  for (size_t i = 0; i < queries.size(); ++i)
  {
    // todo(@m) Add a bool flag to search with prefixes?
    requests.emplace_back(make_unique<TestSearchRequest>(engine, MakePrefixFree(queries[i]), locale,
                                                         Mode::Everywhere, viewport));
  }

  ofstream csv;
  bool dumpCSV = false;
  if (!rankingCSVFile.empty())
  {
    csv.open(rankingCSVFile);
    if (!csv.is_open())
      LOG(LERROR, ("Can't open file for CSV dump:", rankingCSVFile));
    else
      dumpCSV = true;
  }

  if (dumpCSV)
  {
    RankingInfo::PrintCSVHeader(csv);
    csv << endl;
  }

  vector<double> responseTimes(queries.size());
  for (size_t i = 0; i < queries.size(); ++i)
  {
    requests[i]->Run();
    auto rt = duration_cast<milliseconds>(requests[i]->ResponseTime()).count();
    responseTimes[i] = static_cast<double>(rt) / 1000;
    PrintTopResults(MakePrefixFree(queries[i]), requests[i]->Results(), top, responseTimes[i]);

    if (dumpCSV)
    {
      for (auto const & result : requests[i]->Results())
      {
        result.GetRankingInfo().ToCSV(csv);
        csv << endl;
      }
    }
  }

  double averageTime;
  double maxTime;
  double varianceTime;
  double stdDevTime;
  CalcStatistics(responseTimes, averageTime, maxTime, varianceTime, stdDevTime);

  cout << fixed << setprecision(3);
  cout << endl;
  cout << "Maximum response time: " << maxTime << "s" << endl;
  cout << "Average response time: " << averageTime << "s"
       << " (std. dev. " << stdDevTime << "s)" << endl;
}

int main(int argc, char * argv[])
{
  platform::tests_support::ChangeMaxNumberOfOpenFiles(kMaxOpenFiles);
  CheckLocale();

  gflags::SetUsageMessage("Search quality tests.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  SetPlatformDirs(FLAGS_data_path, FLAGS_mwm_path);

  classificator::Load();

  FrozenDataSource dataSource;
  InitDataSource(dataSource, FLAGS_mwm_list_path);

  auto engine = InitSearchEngine(dataSource, FLAGS_locale, FLAGS_num_threads);
  engine->InitAffiliations();

  m2::RectD viewport;
  InitViewport(FLAGS_viewport, viewport);

  ios_base::sync_with_stdio(false);

  if (!FLAGS_check_completeness.empty())
  {
    CheckCompleteness(FLAGS_check_completeness, dataSource, *engine, viewport, FLAGS_locale);
    return 0;
  }

  RunRequests(*engine, viewport, FLAGS_queries_path, FLAGS_locale, FLAGS_ranking_csv_file,
              static_cast<size_t>(FLAGS_top));
  return 0;
}
