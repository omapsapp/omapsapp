#include "map/benchmark_tools.hpp"
#include "map/framework.hpp"

#include "drape_frontend/drape_measurer.hpp"
#include "drape_frontend/scenario_manager.hpp"

#include "storage/country_info_getter.hpp"

#include "platform/downloader_defines.hpp"
#include "platform/http_client.hpp"
#include "platform/platform.hpp"

#include "coding/reader.hpp"

#include "geometry/mercator.hpp"

#include "base/file_name_utils.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <set>
#include <string>
#include <vector>

#include "3party/jansson/myjansson.hpp"

namespace
{
struct BenchmarkHandle
{
  std::vector<df::ScenarioManager::ScenarioData> m_scenariosToRun;
  size_t m_currentScenario = 0;
  std::vector<storage::CountryId> m_regionsToDownload;
  size_t m_regionsToDownloadCounter = 0;

#ifdef DRAPE_MEASURER_BENCHMARK
  std::vector<std::pair<std::string, df::DrapeMeasurer::DrapeStatistic>> m_drapeStatistic;
#endif
};

void RunScenario(Framework * framework, std::shared_ptr<BenchmarkHandle> handle)
{
  if (handle->m_currentScenario >= handle->m_scenariosToRun.size())
  {
#ifdef DRAPE_MEASURER_BENCHMARK
    for (auto const & it : handle->m_drapeStatistic)
    {
      LOG(LINFO, ("\n ***** Report for scenario", it.first, "*****\n",
                  it.second.ToString(),
                  "\n ***** Report for scenario", it.first, "*****\n"));

    }
#endif
    return;
  }

  auto & scenarioData = handle->m_scenariosToRun[handle->m_currentScenario];

  // TODO: fix
  framework->GetDrapeEngine(0)->RunScenario(std::move(scenarioData),
                                           [handle](std::string const & name)
  {
#ifdef DRAPE_MEASURER_BENCHMARK
    df::DrapeMeasurer::Instance().Start();
#endif
  },
  [framework, handle](std::string const & name)
  {
#ifdef DRAPE_MEASURER_BENCHMARK
    df::DrapeMeasurer::Instance().Stop();
    auto const drapeStatistic = df::DrapeMeasurer::Instance().GetDrapeStatistic();
    handle->m_drapeStatistic.push_back(make_pair(name, drapeStatistic));
#endif
    GetPlatform().RunTask(Platform::Thread::Gui, [framework, handle]()
    {
      handle->m_currentScenario++;
      RunScenario(framework, handle);
    });
  });
}
}  // namespace

namespace benchmark
{
void RunGraphicsBenchmark(Framework * framework)
{
#ifdef SCENARIO_ENABLE
  using namespace df;

  // Load scenario from file.
  auto const fn = base::JoinPath(GetPlatform().SettingsDir(), "graphics_benchmark.json");
  if (!GetPlatform().IsFileExistsByFullPath(fn))
    return;
  
  std::string benchmarkData;
  try
  {
    ReaderPtr<Reader>(GetPlatform().GetReader(fn)).ReadAsString(benchmarkData);
  }
  catch (RootException const & e)
  {
    LOG(LCRITICAL, ("Error reading benchmark file: ", e.what()));
    return;
  }
  
  std::shared_ptr<BenchmarkHandle> handle = std::make_shared<BenchmarkHandle>();

  // Parse scenarios.
  std::vector<m2::PointD> points;
  try
  {
    base::Json root(benchmarkData.c_str());
    json_t * scenariosNode = json_object_get(root.get(), "scenarios");
    if (scenariosNode == nullptr || !json_is_array(scenariosNode))
      return;
    size_t const sz = json_array_size(scenariosNode);
    handle->m_scenariosToRun.resize(sz);
    for (size_t i = 0; i < sz; ++i)
    {
      auto scenarioElem = json_array_get(scenariosNode, i);
      if (scenarioElem == nullptr)
        return;
      FromJSONObject(scenarioElem, "name", handle->m_scenariosToRun[i].m_name);
      json_t * stepsNode = json_object_get(scenarioElem, "steps");
      if (stepsNode != nullptr && json_is_array(stepsNode))
      {
        size_t const stepsCount = json_array_size(stepsNode);
        auto & scenario = handle->m_scenariosToRun[i].m_scenario;
        scenario.reserve(stepsCount);
        for (size_t j = 0; j < stepsCount; ++j)
        {
          auto stepElem = json_array_get(stepsNode, j);
          if (stepElem == nullptr)
            return;
          std::string actionType;
          FromJSONObject(stepElem, "actionType", actionType);
          if (actionType == "waitForTime")
          {
            json_int_t timeInSeconds = 0;
            FromJSONObject(stepElem, "time", timeInSeconds);
            scenario.push_back(std::unique_ptr<ScenarioManager::Action>(
                new ScenarioManager::WaitForTimeAction(std::chrono::seconds(timeInSeconds))));
          }
          else if (actionType == "centerViewport")
          {
            json_t * centerNode = json_object_get(stepElem, "center");
            if (centerNode == nullptr)
              return;
            double lat = 0.0, lon = 0.0;
            FromJSONObject(centerNode, "lat", lat);
            FromJSONObject(centerNode, "lon", lon);
            json_int_t zoomLevel = -1;
            FromJSONObject(stepElem, "zoomLevel", zoomLevel);
            m2::PointD const pt = mercator::FromLatLon(lat, lon);
            points.push_back(pt);
            scenario.push_back(std::unique_ptr<ScenarioManager::Action>(
                                 new ScenarioManager::CenterViewportAction(pt, static_cast<int>(zoomLevel))));
          }
        }
      }
    }
  }
  catch (base::Json::Exception const & e)
  {
    return;
  }
  if (handle->m_scenariosToRun.empty())
    return;

  // Find out regions to download.
  std::set<storage::CountryId> regions;
  for (m2::PointD const & pt : points)
    regions.insert(framework->GetCountryInfoGetter().GetRegionCountryId(pt));

  for (auto const & countryId : regions)
  {
    storage::NodeStatuses statuses;
    framework->GetStorage().GetNodeStatuses(countryId, statuses);
    if (statuses.m_status != storage::NodeStatus::OnDisk)
      handle->m_regionsToDownload.push_back(countryId);
  }

  // Download regions and run scenarios after downloading.
  if (!handle->m_regionsToDownload.empty())
  {
    framework->GetStorage().Subscribe(
        [framework, handle](storage::CountryId const & countryId) {
          if (base::IsExist(handle->m_regionsToDownload, countryId))
          {
            handle->m_regionsToDownloadCounter++;
            if (handle->m_regionsToDownloadCounter == handle->m_regionsToDownload.size())
            {
              handle->m_regionsToDownload.clear();
              RunScenario(framework, handle);
            }
          }
        },
        [](storage::CountryId const &, downloader::Progress const &) {});

    for (auto const & countryId : handle->m_regionsToDownload)
      framework->GetStorage().DownloadNode(countryId);
    return;
  }

  // Run scenarios without downloading.
  RunScenario(framework, handle);
#endif
}
}  // namespace benchmark
