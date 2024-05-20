project(generator)

set(SRC
  addresses_collector.cpp
  addresses_collector.hpp
  affiliation.cpp
  affiliation.hpp
  altitude_generator.cpp
  altitude_generator.hpp
  borders.cpp
  borders.hpp
  boundary_postcodes_enricher.cpp
  boundary_postcodes_enricher.hpp
  brands_loader.cpp
  brands_loader.hpp
  camera_info_collector.cpp
  camera_info_collector.hpp
  cells_merger.cpp
  cells_merger.hpp
  centers_table_builder.cpp
  centers_table_builder.hpp
  check_model.cpp
  check_model.hpp
  cities_boundaries_builder.cpp
  cities_boundaries_builder.hpp
  cities_boundaries_checker.cpp
  cities_boundaries_checker.hpp
  cities_ids_builder.cpp
  cities_ids_builder.hpp
  city_roads_generator.cpp
  city_roads_generator.hpp
  cluster_finder.hpp
  coastlines_generator.cpp
  coastlines_generator.hpp
  collection_base.hpp
  collector_boundary_postcode.cpp
  collector_boundary_postcode.hpp
  collector_building_parts.cpp
  collector_building_parts.hpp
  collector_camera.cpp
  collector_camera.hpp
  collector_collection.cpp
  collector_collection.hpp
  collector_interface.hpp
  collector_mini_roundabout.cpp
  collector_mini_roundabout.hpp
  collector_routing_city_boundaries.cpp
  collector_routing_city_boundaries.hpp
  collector_tag.cpp
  collector_tag.hpp
#  complex_loader.cpp
#  complex_loader.hpp
  composite_id.cpp
  composite_id.hpp
  cross_mwm_osm_ways_collector.cpp
  cross_mwm_osm_ways_collector.hpp
  descriptions_section_builder.cpp
  descriptions_section_builder.hpp
  dumper.cpp
  dumper.hpp
  factory_utils.hpp
  feature_builder.cpp
  feature_builder.hpp
  feature_emitter_iface.hpp
  feature_generator.cpp
  feature_generator.hpp
  feature_helpers.cpp
  feature_helpers.hpp
  feature_maker.cpp
  feature_maker.hpp
  feature_maker_base.cpp
  feature_maker_base.hpp
  feature_merger.cpp
  feature_merger.hpp
  feature_processing_layers.cpp
  feature_processing_layers.hpp
  feature_sorter.cpp
  feature_sorter.hpp
  features_processing_helpers.hpp
  filter_collection.cpp
  filter_collection.hpp
#  filter_complex.cpp
#  filter_complex.hpp
  filter_elements.cpp
  filter_elements.hpp
  filter_interface.hpp
  filter_planet.cpp
  filter_planet.hpp
  filter_roads.cpp
  filter_roads.hpp
  filter_world.cpp
  filter_world.hpp
  final_processor_cities.cpp
  final_processor_cities.hpp
  final_processor_coastline.cpp
  final_processor_coastline.hpp
#  final_processor_complex.cpp
#  final_processor_complex.hpp
  final_processor_country.cpp
  final_processor_country.hpp
  final_processor_interface.hpp
  final_processor_utils.cpp
  final_processor_utils.hpp
  final_processor_world.cpp
  final_processor_world.hpp
  gen_mwm_info.cpp
  gen_mwm_info.hpp
  generate_info.hpp
  geometry_holder.hpp
#  hierarchy.cpp
#  hierarchy.hpp
#  hierarchy_entry.cpp
#  hierarchy_entry.hpp
  holes.cpp
  holes.hpp
  intermediate_data.cpp
  intermediate_data.hpp
  intermediate_elements.hpp
  isolines_generator.cpp
  isolines_generator.hpp
  isolines_section_builder.cpp
  isolines_section_builder.hpp
  maxspeeds_builder.cpp
  maxspeeds_builder.hpp
  maxspeeds_collector.cpp
  maxspeeds_collector.hpp
  maxspeeds_parser.cpp
  maxspeeds_parser.hpp
  metalines_builder.cpp
  metalines_builder.hpp
  mini_roundabout_info.cpp
  mini_roundabout_info.hpp
  mini_roundabout_transformer.cpp
  mini_roundabout_transformer.hpp
  node_mixer.cpp
  node_mixer.hpp
  osm2meta.cpp
  osm2meta.hpp
  osm2type.cpp
  osm2type.hpp
  osm_element.cpp
  osm_element.hpp
  osm_element_helpers.cpp
  osm_element_helpers.hpp
  osm_o5m_source.hpp
  osm_source.cpp
  osm_xml_source.hpp
  place_processor.cpp
  place_processor.hpp
  platform_helpers.cpp
  platform_helpers.hpp
  popular_places_section_builder.cpp
  popular_places_section_builder.hpp
  postcode_points_builder.cpp
  postcode_points_builder.hpp
  processor_coastline.cpp
  processor_coastline.hpp
#  processor_complex.cpp
#  processor_complex.hpp
  processor_country.cpp
  processor_country.hpp
  processor_factory.hpp
  processor_interface.hpp
  processor_noop.hpp
#  processor_simple.cpp
#  processor_simple.hpp
  processor_world.cpp
  processor_world.hpp
  raw_generator.cpp
  raw_generator.hpp
  raw_generator_writer.cpp
  raw_generator_writer.hpp
  region_meta.cpp
  region_meta.hpp
  relation_tags.cpp
  relation_tags.hpp
  relation_tags_enricher.cpp
  relation_tags_enricher.hpp
  restriction_collector.cpp
  restriction_collector.hpp
  restriction_generator.cpp
  restriction_generator.hpp
  restriction_writer.cpp
  restriction_writer.hpp
  road_access_generator.cpp
  road_access_generator.hpp
  routing_city_boundaries_processor.cpp
  routing_city_boundaries_processor.hpp
  routing_helpers.cpp
  routing_helpers.hpp
  routing_index_generator.cpp
  routing_index_generator.hpp
  routing_world_roads_generator.cpp
  routing_world_roads_generator.hpp
  search_index_builder.cpp
  search_index_builder.hpp
  srtm_parser.cpp
  srtm_parser.hpp
  statistics.cpp
  statistics.hpp
  tag_admixer.hpp
  tesselator.cpp
  tesselator.hpp
  towns_dumper.cpp
  towns_dumper.hpp
  traffic_generator.cpp
  traffic_generator.hpp
  transit_generator.cpp
  transit_generator.hpp
  transit_generator_experimental.cpp
  transit_generator_experimental.hpp
  translator.cpp
  translator.hpp
  translator_coastline.cpp
  translator_coastline.hpp
  translator_collection.cpp
  translator_collection.hpp
#  translator_complex.cpp
#  translator_complex.hpp
  translator_country.cpp
  translator_country.hpp
  translator_factory.hpp
  translator_interface.hpp
  translator_world.cpp
  translator_world.hpp
  translators_pool.cpp
  translators_pool.hpp
  turn_lanes_metadata_processor.cpp
  turn_lanes_metadata_processor.hpp
  unpack_mwm.cpp
  unpack_mwm.hpp
  utils.cpp
  utils.hpp
  ways_merger.cpp
  ways_merger.hpp
  way_nodes_mapper.hpp
  wiki_url_dumper.cpp
  wiki_url_dumper.hpp
  water_boundary_checker.hpp
  world_map_generator.hpp
)

omim_add_library(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME}
  transit
  search
  routing
  storage
  descriptions
  indexer
  cppjansson
  expat::expat
  tess2
  $<$<BOOL:CMAKE_DL_LIBS>:${CMAKE_DL_LIBS}>  # dladdr from boost::stacktrace
)

# Disable unity build to avoid boost::geometry macro linker errors.
set_source_files_properties(
  affiliation.cpp
  final_processor_country.cpp
  hierarchy.cpp
  PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

if (NOT (SKIP_TOOLS AND SKIP_TESTS))
  # Used in both tests and some tools.
  add_subdirectory(generator_tests_support)
endif()
omim_add_test_subdirectory(generator_tests)
omim_add_test_subdirectory(generator_integration_tests)

omim_add_tool_subdirectory(generator_tool)
#omim_add_tool_subdirectory(complex_generator)
omim_add_tool_subdirectory(feature_segments_checker)
omim_add_tool_subdirectory(srtm_coverage_checker)
add_subdirectory(world_roads_builder)
