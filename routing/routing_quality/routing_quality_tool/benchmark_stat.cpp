#include "routing/routing_quality/routing_quality_tool/benchmark_stat.hpp"

#include "routing/routing_quality/routing_quality_tool/benchmark_results.hpp"
#include "routing/routing_quality/routing_quality_tool/error_type_counter.hpp"
#include "routing/routing_quality/routing_quality_tool/utils.hpp"

#include "routing/routing_callbacks.hpp"

#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <algorithm>

namespace
{
using namespace routing;
using namespace routes_builder;
using namespace routing_quality::routing_quality_tool;

bool IsErrorCode(RouterResultCode code)
{
  return code != RouterResultCode::NoError;
}

void LogIfNotConsistent(RoutesBuilder::Result const & oldRes, RoutesBuilder::Result const & newRes)
{
  auto const start = mercator::ToLatLon(oldRes.m_params.m_checkpoints.GetStart());
  auto const finish = mercator::ToLatLon(oldRes.m_params.m_checkpoints.GetFinish());
  CHECK(!oldRes.m_routes.empty(), ());
  CHECK(!newRes.m_routes.empty(), ());
  auto const & oldRoute = oldRes.m_routes.back();
  auto const & newRoute = newRes.m_routes.back();

  bool const sameETA = AlmostEqualAbs(oldRoute.m_eta, newRoute.m_eta, 1.0);
  bool const sameDistance = AlmostEqualAbs(oldRoute.m_distance, newRoute.m_distance, 1.0);
  if (!sameETA || !sameDistance)
  {
    LOG(LINFO, ("old ETA:", oldRoute.m_eta, "old distance:", oldRoute.m_distance, "new ETA:",
                newRoute.m_eta, "new distance:", newRoute.m_distance, start, finish));
  }
}

/// \brief Helps to compare route time building for routes group by old time building.
void FillInfoAboutBuildTimeGroupByPreviousResults(std::vector<std::string> & labels,
                                                  std::vector<std::vector<double>> & bars,
                                                  std::vector<TimeInfo> && times)
{
  bars.clear();
  labels.clear();

  std::sort(times.begin(), times.end(), [](auto const & a, auto const & b) {
    return a.m_oldTime < b.m_oldTime;
  });

  size_t constexpr kSteps = 10;
  size_t const step = times.size() / kSteps;

  size_t startFrom = 0;
  size_t curCount = 0;
  bars.resize(2);
  double curSumOld = 0;
  double curSumNew = 0;
  for (size_t i = 0; i < times.size(); ++i)
  {
    if (curCount < step && i + 1 != times.size())
    {
      ++curCount;
      curSumOld += times[i].m_oldTime;
      curSumNew += times[i].m_newTime;
      continue;
    }

    double const curLeft = times[startFrom].m_oldTime;
    startFrom = i + 1;
    double const curRight = times[i].m_oldTime;
    labels.emplace_back("[" + strings::to_string_dac(curLeft, 2 /* dac */) + "s, " +
                        strings::to_string_dac(curRight, 2 /* dac */) + "s]\\n" +
                        "Routes count:\\n" + std::to_string(curCount));
    if (curCount != 0)
    {
      curSumOld /= curCount;
      curSumNew /= curCount;
    }
    double const k = curSumOld == 0.0 ? 0.0 : curSumNew / curSumOld;
    bars[0].emplace_back(100.0);
    bars[1].emplace_back(100.0 * k);
    curCount = 0;
  }
}

std::vector<double> GetBoostPercents(BenchmarkResults const & oldResults,
                                     BenchmarkResults const & newResults)
{
  std::vector<double> boostPercents;
  for (size_t i = 0; i < oldResults.GetBuildTimes().size(); ++i)
  {
    auto const oldTime = oldResults.GetBuildTimes()[i];
    auto const newTime = newResults.GetBuildTimes()[i];
    if (AlmostEqualAbs(oldTime, newTime, 1e-2))
      continue;

    auto const diffPercent = (oldTime - newTime) / oldTime * 100.0;
    boostPercents.emplace_back(diffPercent);
  }

  return boostPercents;
}
}  // namespace

namespace routing_quality::routing_quality_tool
{
using namespace routing;
using namespace routes_builder;
// Shows distribution of routes time building.
static std::string const kPythonDistTimeBuilding = "show_route_time_building_dist.py";
// Shows graph of "how many routes in percents build in less time than some T".
static std::string const kPythonGraphTimeAndCount = "show_time_count_graph.py";
// Bar graph of routing errors. Labels - string representation of errors, heights - number of such
// errors.
static std::string const kPythonBarError = "show_errors_bar.py";
// Shows percent of boost distribution. Where boost percent equals:
// 100.0 * (old_time - new_time) / old_time
static std::string const kPythonBarBoostPercentDistr = "show_boost_distr.py";
// Shows distribution of non-zero difference of ETA between old and new mapsme version.
static std::string const kPythonEtaDiff = "eta_diff.py";
// The same as above but in percents.
static std::string const kPythonEtaDiffPercent = "eta_diff_percent.py";
// Groups routes by previous time building and draws two types of bars. The first one (old mapsme)
// has the same height in all groups and the second ones' height is proportional less or more
// according to difference in average time building between old and new version. The example you can
// see here: https://github.com/mapsme/omim/pull/12401
static std::string const kPythonSmartDistr = "show_smart_boost_distr.py";

void RunBenchmarkStat(
    std::vector<std::pair<RoutesBuilder::Result, std::string>> const & mapsmeResults,
    std::string const & dirForResults)
{
  BenchmarkResults benchmarkResults;
  ErrorTypeCounter errorTypeCounter;
  for (auto const & resultItem : mapsmeResults)
  {
    auto const & result = resultItem.first;
    errorTypeCounter.PushError(result.m_code);
    if (result.m_code != RouterResultCode::NoError)
      continue;

    CHECK(!result.m_routes.empty(), ());
    benchmarkResults.PushBuildTime(result.m_buildTimeSeconds);
  }

  auto pythonScriptPath = base::JoinPath(dirForResults, kPythonDistTimeBuilding);
  CreatePythonScriptForDistribution(pythonScriptPath, "Route building time, seconds",
                                    benchmarkResults.GetBuildTimes());

  LOG(LINFO, ("Average route time building:", benchmarkResults.GetAverageBuildTime(), "seconds."));

  auto times = benchmarkResults.GetBuildTimes();
  std::sort(times.begin(), times.end());
  std::vector<m2::PointD> countToTimes(times.size());
  for (size_t i = 0; i < countToTimes.size(); ++i)
  {
    countToTimes[i].x = times[i];
    countToTimes[i].y = static_cast<double>(i + 1) / times.size() * 100.0;
  }

  pythonScriptPath = base::JoinPath(dirForResults, kPythonGraphTimeAndCount);
  CreatePythonGraphByPointsXY(pythonScriptPath,
                              "Building time, seconds" /* xlabel */,
                              "Percent of routes built less than" /* ylabel */,
                              {countToTimes}, {"mapsme"} /* legends */);

  pythonScriptPath = base::JoinPath(dirForResults, kPythonBarError);

  std::vector<std::string> labels;
  std::vector<double> heights;
  FillLabelsAndErrorTypeDistribution(labels, heights, errorTypeCounter);

  CreatePythonBarByMap(pythonScriptPath, labels, {heights}, {"mapsme"} /* legends */,
                       "Type of errors" /* xlabel */, "Number of errors" /* ylabel */);
}

void RunBenchmarkComparison(
    std::vector<std::pair<RoutesBuilder::Result, std::string>> && mapsmeResults,
    std::vector<std::pair<RoutesBuilder::Result, std::string>> && mapsmeOldResults,
    std::string const & dirForResults)
{
  BenchmarkResults benchmarkResults;
  BenchmarkResults benchmarkOldResults;
  ErrorTypeCounter errorTypeCounter;
  ErrorTypeCounter errorTypeCounterOld;
  std::vector<double> etaDiffsPercent;
  std::vector<double> etaDiffs;

  std::vector<TimeInfo> times;

  for (size_t i = 0; i < mapsmeResults.size(); ++i)
  {
    auto const & mapsmeResult = mapsmeResults[i].first;

    std::pair<RoutesBuilder::Result, std::string> mapsmeOldResultPair;
    if (!FindAnotherResponse(mapsmeResult, mapsmeOldResults, mapsmeOldResultPair))
    {
      LOG(LDEBUG, ("Can not find pair for:", i));
      continue;
    }

    auto const & mapsmeOldResult = mapsmeOldResultPair.first;

    errorTypeCounter.PushError(mapsmeResult.m_code);
    errorTypeCounterOld.PushError(mapsmeOldResult.m_code);

    if (IsErrorCode(mapsmeResult.m_code) && !IsErrorCode(mapsmeOldResult.m_code))
    {
      auto const start = mercator::ToLatLon(mapsmeResult.m_params.m_checkpoints.GetStart());
      auto const finish = mercator::ToLatLon(mapsmeResult.m_params.m_checkpoints.GetFinish());
      LOG_FORCE(LWARNING, ("New version returns error code:", mapsmeResult.m_code,
                           "but old returns NoError for:", start, finish));
    }

    if (IsErrorCode(mapsmeResult.m_code) || IsErrorCode(mapsmeOldResult.m_code))
      continue;

    LogIfNotConsistent(mapsmeOldResult, mapsmeResult);

    CHECK(!mapsmeOldResult.m_routes.empty() && !mapsmeResult.m_routes.empty(), ());
    auto const etaDiff =
        (mapsmeOldResult.m_routes.back().m_eta - mapsmeResult.m_routes.back().m_eta);
    auto const etaDiffPercent = etaDiff / mapsmeOldResult.m_routes.back().m_eta * 100.0;

    etaDiffs.emplace_back(etaDiff);
    etaDiffsPercent.emplace_back(etaDiffPercent);

    auto const oldTime = mapsmeOldResult.m_buildTimeSeconds;
    auto const newTime = mapsmeResult.m_buildTimeSeconds;
    auto const diffPercent = (oldTime - newTime) / oldTime * 100.0;
    // Warn about routes building time degradation.
    double constexpr kSlowdownPercent = -10.0;
    if (diffPercent < kSlowdownPercent)
    {
      auto const start = mercator::ToLatLon(mapsmeResult.m_params.m_checkpoints.GetStart());
      auto const finish = mercator::ToLatLon(mapsmeResult.m_params.m_checkpoints.GetFinish());
      LOG(LINFO, ("oldTime:", oldTime, "newTime:", newTime, "diffPercent:", diffPercent, start, finish));
    }

    benchmarkResults.PushBuildTime(mapsmeResult.m_buildTimeSeconds);
    benchmarkOldResults.PushBuildTime(mapsmeOldResult.m_buildTimeSeconds);

    times.emplace_back(mapsmeOldResult.m_buildTimeSeconds, mapsmeResult.m_buildTimeSeconds);
  }

  LOG(LINFO, ("Comparing", benchmarkResults.GetBuildTimes().size(), "routes."));

  auto const oldAverage = benchmarkOldResults.GetAverageBuildTime();
  auto const newAverage = benchmarkResults.GetAverageBuildTime();
  auto const averageTimeDiff = (oldAverage - newAverage) / oldAverage * 100.0;
  LOG(LINFO, ("Average route time building. "
              "Old version:", oldAverage,
              "New version:", newAverage,
              "(", -averageTimeDiff, "% )"));

  std::vector<std::vector<m2::PointD>> graphics;
  for (auto const & results : {benchmarkOldResults, benchmarkResults})
  {
    auto times = results.GetBuildTimes();
    std::sort(times.begin(), times.end());
    std::vector<m2::PointD> countToTimes(times.size());
    for (size_t i = 0; i < countToTimes.size(); ++i)
    {
      countToTimes[i].x = times[i];
      countToTimes[i].y = static_cast<double>(i + 1) / times.size() * 100.0;
    }

    graphics.emplace_back(std::move(countToTimes));
  }

  auto pythonScriptPath = base::JoinPath(dirForResults, kPythonGraphTimeAndCount);
  CreatePythonGraphByPointsXY(pythonScriptPath, "Building time, seconds" /* xlabel */,
                              "Percent of routes built less than" /* ylabel */, graphics,
                              {"old mapsme", "new mapsme"} /* legends */);

  std::vector<std::string> labels;
  std::vector<std::vector<double>> errorsCount;
  FillLabelsAndErrorTypeDistribution(labels, errorsCount, errorTypeCounter, errorTypeCounterOld);

  pythonScriptPath = base::JoinPath(dirForResults, kPythonBarError);
  CreatePythonBarByMap(pythonScriptPath, labels, errorsCount,
                       {"old mapsme", "new mapsme"} /* legends */, "Type of errors" /* xlabel */,
                       "Number of errors" /* ylabel */);

  auto const boostPercents = GetBoostPercents(benchmarkOldResults, benchmarkResults);
  pythonScriptPath = base::JoinPath(dirForResults, kPythonBarBoostPercentDistr);
  CreatePythonScriptForDistribution(pythonScriptPath, "Boost percent" /* title */, boostPercents);

  pythonScriptPath = base::JoinPath(dirForResults, kPythonEtaDiff);
  CreatePythonScriptForDistribution(pythonScriptPath, "ETA diff distribution" /* title */,
                                    etaDiffs);

  pythonScriptPath = base::JoinPath(dirForResults, kPythonEtaDiffPercent);
  CreatePythonScriptForDistribution(pythonScriptPath, "ETA diff percent distribution" /* title */,
                                    etaDiffsPercent);

  std::vector<std::vector<double>> bars;
  FillInfoAboutBuildTimeGroupByPreviousResults(labels, bars, std::move(times));
  pythonScriptPath = base::JoinPath(dirForResults, kPythonSmartDistr);
  CreatePythonBarByMap(
      pythonScriptPath, labels, bars, {"old mapsme", "new mapsme"} /* legends */,
      "Intervals of groups (build time in old mapsme)" /* xlabel */,
      "Boost\\nRight column is so lower/higher than the left\\n how much the average build time "
      "has decreased in each group)" /* ylabel */,
      false /* drawPercents */);
}
}  // namespace routing_quality::routing_quality_tool
