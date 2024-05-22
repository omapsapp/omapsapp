#include "generator/srtm_parser.hpp"

#include "routing/routing_helpers.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_altitude.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/feature_processor.hpp"

#include "geometry/mercator.hpp"
#include "geometry/point_with_altitude.hpp"

#include "platform/country_file.hpp"
#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "base/logging.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <gflags/gflags.h>

DEFINE_string(srtm_path, "", "Path to directory with SRTM files");
DEFINE_string(mwm_path, "", "Path to mwm files (writable dir)");

int main(int argc, char * argv[])
{
  gflags::SetUsageMessage("SRTM coverage checker.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  Platform & platform = GetPlatform();
  if (!FLAGS_mwm_path.empty())
    platform.SetWritableDirForTests(FLAGS_mwm_path);

  if (FLAGS_srtm_path.empty())
  {
    LOG(LERROR, ("SRTM files directory is not specified."));
    return -1;
  }

  LOG(LINFO, ("writable dir =", platform.WritableDir()));
  LOG(LINFO, ("srtm dir =", FLAGS_srtm_path));

  std::vector<platform::LocalCountryFile> localFiles;
  FindAllLocalMapsAndCleanup(std::numeric_limits<int64_t>::max() /* latestVersion */, localFiles);

  generator::SrtmTileManager manager(FLAGS_srtm_path);
  classificator::Load();

  for (auto & file : localFiles)
  {
    file.SyncWithDisk();
    if (!file.OnDisk(MapFileType::Map))
    {
      LOG(LINFO, ("Warning! Routing file not found for:", file.GetCountryName()));
      continue;
    }

    auto const path = file.GetPath(MapFileType::Map);
    LOG(LINFO, ("Mwm", path, "is being processed."));

    size_t all = 0;
    size_t good = 0;
    feature::ForEachFeature(path, [&](FeatureType & ft, uint32_t /* fid */) {
      if (!routing::IsRoad(feature::TypesHolder(ft)))
        return;

      ft.ParseGeometry(FeatureType::BEST_GEOMETRY);
      all += ft.GetPointsCount();

      for (size_t i = 0; i < ft.GetPointsCount(); ++i)
      {
        auto const height = manager.GetHeight(mercator::ToLatLon(ft.GetPoint(i)));
        if (height != geometry::kInvalidAltitude)
          good++;
      }
    });

    auto const bad = all - good;
    auto const percent = all == 0 ? 0.0 : bad * 100.0 / all;
    LOG(LINFO, (percent > 10.0 ? "Huge" : "Low", "error rate in:", file.GetCountryName(),
                "good:", good, "bad:", bad, "all:", all, "%:", percent));
  }

  return 0;
}
