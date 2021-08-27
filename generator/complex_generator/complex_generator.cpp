// This is a program that generates complexes on the basis of the last generation of maps.
// Complexes are a hierarchy of interesting geographical features.
// For the program to work correctly, you need to have in your file system:
// top_directory
//      |_ planet.o5m
//      |_maps_build
//             |_190223(for example 2019 Feb. 23rd)
//             |_intermediate_data
//             |_osm2ft
//
// It's easy if you use maps_generator. For example:
//
// $ python -m maps_generator --skip="coastline" --countries="Russia_Moscow"
//
// $ ./complex_generator --maps_build_path=path/to/maps_build \
//   --user_resource_path=path/to/omim/data --output=output.txt

#include "generator/filter_complex.hpp"
#include "generator/filter_interface.hpp"
#include "generator/final_processor_complex.hpp"
#include "generator/generate_info.hpp"
#include "generator/hierarchy.hpp"
#include "generator/hierarchy_entry.hpp"
#include "generator/intermediate_data.hpp"
#include "generator/processor_factory.hpp"
#include "generator/raw_generator.hpp"
#include "generator/translator_factory.hpp"
#include "generator/utils.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/map_style_reader.hpp"

#include "platform/platform.hpp"

#include "coding/endianness.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"

#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>

#include "defines.hpp"

#include "3party/gflags/src/gflags/gflags.h"

DEFINE_string(node_storage, "map",
              "Type of storage for intermediate points representation. Available: raw, map, mem.");
DEFINE_string(user_resource_path, "", "User defined resource path for classificator.txt and etc.");
DEFINE_string(maps_build_path, "",
              "Directory of any of the previous map generations. It is assumed that it will "
              "contain a directory with mwm(for example 190423) and a directory with mappings from "
              "osm is to a feature id.");
DEFINE_bool(popularity, false, "Build complexes for calculation of popularity of objects.");
DEFINE_string(output, "", "Output filename");
DEFINE_bool(debug, false, "Debug mode.");

MAIN_WITH_ERROR_HANDLING([](int argc, char ** argv) {
  CHECK(IsLittleEndian(), ("Only little-endian architectures are supported."));

  Platform & pl = GetPlatform();
  google::SetUsageMessage(
      "complex_generator is a program that generates complexes on the basis of "
      "the last generation of maps. Complexes are a hierarchy of interesting "
      "geographical features.");
  google::SetVersionString(pl.Version());
  google::ParseCommandLineFlags(&argc, &argv, true);

  auto threadsCount = pl.CpuCores();
  CHECK(!FLAGS_user_resource_path.empty(), ());
  pl.SetResourceDir(FLAGS_user_resource_path);
  classificator::Load();

  feature::GenerateInfo genInfo;
  genInfo.m_osmFileName = base::JoinPath(FLAGS_maps_build_path, "..", "planet.o5m");
  genInfo.SetOsmFileType("o5m");
  genInfo.SetNodeStorageType(FLAGS_node_storage);
  genInfo.m_intermediateDir = base::JoinPath(FLAGS_maps_build_path, "intermediate_data");
  genInfo.m_tmpDir = base::JoinPath(FLAGS_maps_build_path, "complex", "tmp");
  CHECK(Platform::MkDirRecursively(genInfo.m_tmpDir), ());

  generator::hierarchy::PrintFn print;
  generator::hierarchy::GetMainTypeFn getMainType = generator::hierarchy::GetMainType;
  std::shared_ptr<generator::FilterInterface> filter = std::make_shared<generator::FilterComplex>();

  if (FLAGS_debug)
  {
    print = static_cast<std::string (*)(generator::HierarchyEntry const &)>(generator::DebugPrint);
  }
  else
  {
    print = [](auto const & entry) {
      return generator::hierarchy::HierarchyEntryToCsvString(entry);
    };
  }

  generator::RawGenerator rawGenerator(genInfo, threadsCount);
  auto processor = CreateProcessor(generator::ProcessorType::Complex, rawGenerator.GetQueue(),
                                   genInfo.m_intermediateDir, false /* haveBordersForWholeWorld */);
  generator::cache::IntermediateDataObjectsCache objectsCache;
  auto const cache = std::make_shared<generator::cache::IntermediateData>(objectsCache, genInfo);
  auto translator = CreateTranslator(generator::TranslatorType::Complex, processor, cache, genInfo);
  auto finalProcessor = std::make_shared<generator::ComplexFinalProcessor>(
      genInfo.m_tmpDir, FLAGS_output, threadsCount);

  finalProcessor->SetPrintFunction(print);
  finalProcessor->SetGetMainTypeFunction(getMainType);
  finalProcessor->SetGetNameFunction(generator::hierarchy::GetName);
  finalProcessor->SetFilter(filter);
  finalProcessor->UseBuildingPartsInfo(
      genInfo.GetIntermediateFileName(BUILDING_PARTS_MAPPING_FILE));

  if (FLAGS_popularity)
  {
    // Directory FLAGS_maps_build_path must contain 'osm2ft' directory with *.mwm.osm2ft
    auto const osm2FtPath = base::JoinPath(FLAGS_maps_build_path, "osm2ft");
    // Find directory with *.mwm. Directory FLAGS_maps_build_path must contain directory with *.mwm,
    // whose name must consist of six digits.
    Platform::FilesList files;
    pl.GetFilesByRegExp(FLAGS_maps_build_path, "[0-9]{6}", files);
    CHECK_EQUAL(files.size(), 1, ());
    auto const mwmPath = base::JoinPath(FLAGS_maps_build_path, files[0]);
    finalProcessor->UseCentersEnricher(mwmPath, osm2FtPath);
  }

  rawGenerator.GenerateCustom(translator, finalProcessor);
  CHECK(rawGenerator.Execute(), ());
  return EXIT_SUCCESS;
});
