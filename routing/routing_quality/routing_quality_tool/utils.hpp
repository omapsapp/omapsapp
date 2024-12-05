#pragma once

#include "routing/routing_quality/api/api.hpp"
#include "routing/routes_builder/routes_builder.hpp"

#include "routing/routing_callbacks.hpp"

#include "platform/platform.hpp"

#include "geometry/latlon.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace routing_quality
{
namespace routing_quality_tool
{
enum class ComparisonType
{
  MapsmeVsMapsme,
  MapsmeVsApi
};

template <typename Result>
std::vector<std::pair<Result, std::string>> LoadResults(std::string const & dir)
{
  std::vector<std::pair<Result, std::string>> result;

  std::vector<std::string> files;
  Platform::GetFilesByExt(dir, Result::kDumpExtension, files);
  std::sort(files.begin(), files.end());

  double lastPercent = 0.0;
  double count = 0;

  for (auto const & file : files)
  {
    double const curPercent = (count + 1) / files.size() * 100.0;
    if (curPercent - lastPercent > 5.0 || count + 1 == files.size())
    {
      lastPercent = curPercent;
      LOG(LINFO, ("Progress:", lastPercent, "%"));
    }

    auto const fullPath = base::JoinPath(dir, file);
    result.emplace_back(Result::Load(fullPath), fullPath);
    ++count;
  }

  return result;
}

template <typename AnotherResult>
bool AreRoutesWithSameEnds(routing::routes_builder::RoutesBuilder::Result const & mapsmeResult,
                           AnotherResult const & anotherResult)
{
  CHECK_EQUAL(mapsmeResult.m_params.m_checkpoints.GetPoints().size(), 2, ());
  CHECK_EQUAL(mapsmeResult.GetVehicleType(), anotherResult.GetVehicleType(), ());

  auto const & start = mapsmeResult.GetStartPoint();
  auto const & finish = mapsmeResult.GetFinishPoint();

  auto const & anotherStart = anotherResult.GetStartPoint();
  auto const & anotherFinish = anotherResult.GetFinishPoint();

  double constexpr kEps = 1e-10;
  return AlmostEqualAbs(start, anotherStart, kEps) &&
         AlmostEqualAbs(finish, anotherFinish, kEps);
}

template <typename AnotherResult>
bool FindAnotherResponse(routing::routes_builder::RoutesBuilder::Result const & mapsmeResult,
                         std::vector<std::pair<AnotherResult, std::string>> const & apiResults,
                         std::pair<AnotherResult, std::string> & anotherResult)
{
  for (auto const & result : apiResults)
  {
    if (!AreRoutesWithSameEnds(mapsmeResult, result.first))
      continue;

    anotherResult = result;
    return true;
  }

  return false;
}

struct Result
{
  Result(std::string mapsmeDumpPath, std::string anotherDumpPath,
         ms::LatLon const & start, ms::LatLon const & finish,
         double similarity, double etaDiffPercent);

  bool operator<(Result const & rhs) const;

  std::string m_mapsmeDumpPath;
  std::string m_anotherDumpPath;

  ms::LatLon m_start;
  ms::LatLon m_finish;

  // value in range: [0, 1]
  double m_similarity;
  double m_etaDiffPercent;
};

class RoutesSaver
{
public:
  RoutesSaver(std::string targetDir, ComparisonType comparisonType);

  void PushRoute(Result const & result);
  void PushError(routing::RouterResultCode code, ms::LatLon const & start, ms::LatLon const & finish);
  void PushRivalError(ms::LatLon const & start, ms::LatLon const & finish);
  void TurnToNextFile();
  std::string GetCurrentPath() const;
  std::string const & GetTargetDir() const;
  ComparisonType GetComparsionType() const;

private:
  void OpenOutputStream();
  void WriteStartAndFinish(std::ofstream & output, ms::LatLon const & start, ms::LatLon const & finish);

  std::ofstream m_apiErrors;
  std::string m_targetDir;
  uint32_t m_fileNumber = 0;
  std::ofstream m_output;
  std::map<routing::RouterResultCode, std::ofstream> m_errorRoutes;
  ComparisonType m_comparisonType;
};

class SimilarityCounter
{
public:
  struct Interval
  {
    std::string m_name;
    // [m_left, m_right)
    double m_left;
    double m_right;
  };

  struct Item
  {
    Item(std::string name, uint32_t n) : m_intervalName(std::move(name)), m_routesNumber(n) {}

    std::string m_intervalName;
    uint32_t m_routesNumber = 0;
  };

  static std::vector<Interval> const kIntervals;

  explicit SimilarityCounter(RoutesSaver & routesSaver);
  ~SimilarityCounter();

  void Push(Result const & result);
  void CreateKmlFiles(double percent, std::vector<Result> const & results);

private:
  RoutesSaver & m_routesSaver;
  size_t m_currentInterval = 0;
  std::vector<Item> m_routesCounter;
};

void CreatePythonScriptForDistribution(std::string const & pythonScriptPath,
                                       std::string const & title,
                                       std::vector<double> const & values);

void CreatePythonGraphByPointsXY(std::string const & pythonScriptPath,
                                 std::string const & xlabel,
                                 std::string const & ylabel,
                                 std::vector<std::vector<m2::PointD>> const & graphics,
                                 std::vector<std::string> const & legends);

/// \brief Create python file, that show bar graph, where labels of bars are keys of |stat| and
/// heights area values of |stat|.
void CreatePythonBarByMap(std::string const & pythonScriptPath,
                          std::vector<std::string> const & barLabels,
                          std::vector<std::vector<double>> const & barHeights,
                          std::vector<std::string> const & legends,
                          std::string const & xlabel,
                          std::string const & ylabel,
                          bool drawPercents = true);
}  // namespace routing_quality_tool
}  // namespace routing_quality
