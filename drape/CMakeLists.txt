project(drape)

set(DRAPE_TESTABLE_SRC
  attribute_buffer_mutator.cpp
  attribute_buffer_mutator.hpp
  attribute_provider.cpp
  attribute_provider.hpp
  batcher.cpp
  batcher.hpp
  batcher_helpers.cpp
  batcher_helpers.hpp
  binding_info.cpp
  binding_info.hpp
  buffer_base.cpp
  buffer_base.hpp
  color.hpp
  constants.hpp
  cpu_buffer.cpp
  cpu_buffer.hpp
  data_buffer.cpp
  data_buffer.hpp
  data_buffer_impl.hpp
  debug_renderer.hpp
  drape_diagnostics.hpp
  drape_global.hpp
  drape_routine.hpp
  dynamic_texture.hpp
  font_constants.hpp
  font_texture.cpp
  font_texture.hpp
  framebuffer.cpp
  framebuffer.hpp
  gl_constants.cpp
  gl_constants.hpp
  gl_gpu_program.cpp
  gl_gpu_program.hpp
  gl_extensions_list.cpp
  gl_extensions_list.hpp
  gl_functions.hpp
  gl_includes.hpp
  glsl_func.hpp
  glsl_types.hpp
  glyph.hpp
  glyph_manager.cpp
  glyph_manager.hpp
  gpu_buffer.cpp
  gpu_buffer.hpp
  gpu_program.hpp
  graphics_context.hpp
  graphics_context_factory.cpp
  graphics_context_factory.hpp
  hw_texture.cpp
  hw_texture.hpp
  index_buffer.cpp
  index_buffer.hpp
  index_buffer_mutator.cpp
  index_buffer_mutator.hpp
  index_storage.cpp
  index_storage.hpp
  mesh_object.cpp
  mesh_object.hpp
  object_pool.hpp
  oglcontext.cpp
  oglcontext.hpp
  overlay_handle.cpp
  overlay_handle.hpp
  overlay_tree.cpp
  overlay_tree.hpp
  pointers.cpp
  pointers.hpp
  render_bucket.cpp
  render_bucket.hpp
  render_state.cpp
  render_state.hpp
  shader.cpp
  shader.hpp
  static_texture.cpp
  static_texture.hpp
  stipple_pen_resource.cpp
  stipple_pen_resource.hpp
  support_manager.cpp
  support_manager.hpp
  symbols_texture.cpp
  symbols_texture.hpp
  harfbuzz_shaping.cpp
  harfbuzz_shaping.hpp
  texture.cpp
  texture.hpp
  texture_manager.cpp
  texture_manager.hpp
  texture_of_colors.cpp
  texture_of_colors.hpp
  texture_types.hpp
  tm_read_resources.hpp
  uniform_value.cpp
  uniform_value.hpp
  utils/glyph_usage_tracker.cpp
  utils/glyph_usage_tracker.hpp
  utils/gpu_mem_tracker.cpp
  utils/gpu_mem_tracker.hpp
  utils/projection.cpp
  utils/projection.hpp
  utils/vertex_decl.cpp
  utils/vertex_decl.hpp
  vertex_array_buffer.cpp
  vertex_array_buffer.hpp
  viewport.cpp
  viewport.hpp
  visual_scale.hpp
  vulkan/vulkan_base_context.cpp
  vulkan/vulkan_base_context.hpp
  vulkan/vulkan_context_factory.cpp
  vulkan/vulkan_context_factory.hpp
  vulkan/vulkan_gpu_buffer_impl.cpp
  vulkan/vulkan_gpu_buffer_impl.hpp
  vulkan/vulkan_gpu_program.hpp
  vulkan/vulkan_layers.cpp
  vulkan/vulkan_layers.hpp
  vulkan/vulkan_memory_manager.cpp
  vulkan/vulkan_memory_manager.hpp
  vulkan/vulkan_mesh_object_impl.cpp
  vulkan/vulkan_object_manager.cpp
  vulkan/vulkan_object_manager.hpp
  vulkan/vulkan_param_descriptor.cpp
  vulkan/vulkan_param_descriptor.hpp
  vulkan/vulkan_pipeline.cpp
  vulkan/vulkan_pipeline.hpp
  vulkan/vulkan_staging_buffer.cpp
  vulkan/vulkan_staging_buffer.hpp
  vulkan/vulkan_texture.cpp
  vulkan/vulkan_texture.hpp
  vulkan/vulkan_utils.cpp
  vulkan/vulkan_utils.hpp
  vulkan/vulkan_vertex_array_buffer_impl.cpp
)

if (PLATFORM_IPHONE OR PLATFORM_MAC)
  append(DRAPE_TESTABLE_SRC
    metal/metal_base_context.hpp
    metal/metal_base_context.mm
    metal/metal_cleaner.hpp
    metal/metal_cleaner.mm
    metal/metal_gpu_buffer_impl.mm
    metal/metal_gpu_program.hpp
    metal/metal_mesh_object_impl.mm
    metal/metal_states.hpp
    metal/metal_states.mm
    metal/metal_texture.hpp
    metal/metal_texture.mm
    metal/metal_vertex_array_buffer_impl.mm
    metal/render_state_metal.mm
  )
endif()

set(SRC ${DRAPE_TESTABLE_SRC} gl_functions.cpp)

if (PLATFORM_IPHONE)
  append(SRC
    hw_texture_ios.hpp
    hw_texture_ios.mm
  )
endif()

omim_add_library(${PROJECT_NAME} ${SRC})

# Do not include glm's CMakeLists.txt, because it's outdated and not necessary.
target_include_directories(${PROJECT_NAME} PUBLIC ${OMIM_ROOT}/3party/glm)

if (PLATFORM_LINUX)
  find_package(OpenGL)
endif()

if (PLATFORM_MAC)
  set_target_properties(${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
endif()

set(DRAPE_LINK_LIBRARIES
  indexer
  platform
  geometry
  coding
  base
  Freetype::Freetype
  vulkan_wrapper
  stb_image
  harfbuzz::harfbuzz
  ICU::i18n
  expat::expat
  $<$<BOOL:${PLATFORM_MAC}>:-framework\ OpenGL>
  $<$<BOOL:${PLATFORM_MAC}>:-framework\ Metal>
  $<$<BOOL:${PLATFORM_LINUX}>:OpenGL::GL>
)

target_link_libraries(${PROJECT_NAME} ${DRAPE_LINK_LIBRARIES})

omim_add_test_subdirectory(drape_tests)

omim_add_tool_subdirectory(fonts_tool)
