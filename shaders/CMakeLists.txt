project(shaders)

add_library(${PROJECT_NAME})

set(shader_files
    GL/area.vsh.glsl
    GL/area3d.vsh.glsl
    GL/area3d_outline.vsh.glsl
    GL/arrow3d.fsh.glsl
    GL/arrow3d.vsh.glsl
    GL/arrow3d_outline.fsh.glsl
    GL/arrow3d_shadow.fsh.glsl
    GL/arrow3d_shadow.vsh.glsl
    GL/arrow3d_textured.fsh.glsl
    GL/arrow3d_textured.vsh.glsl
    GL/circle.fsh.glsl
    GL/circle.vsh.glsl
    GL/circle_point.fsh.glsl
    GL/circle_point.vsh.glsl
    GL/colored_symbol.fsh.glsl
    GL/colored_symbol.vsh.glsl
    GL/colored_symbol_billboard.vsh.glsl
    GL/dashed_line.fsh.glsl
    GL/dashed_line.vsh.glsl
    GL/debug_rect.fsh.glsl
    GL/debug_rect.vsh.glsl
    GL/hatching_area.fsh.glsl
    GL/hatching_area.vsh.glsl
    GL/line.fsh.glsl
    GL/line.vsh.glsl
    GL/masked_texturing.fsh.glsl
    GL/masked_texturing.vsh.glsl
    GL/masked_texturing_billboard.vsh.glsl
    GL/my_position.vsh.glsl
    GL/path_symbol.vsh.glsl
    GL/position_accuracy3d.vsh.glsl
    GL/route.fsh.glsl
    GL/route.vsh.glsl
    GL/route_arrow.fsh.glsl
    GL/route_arrow.vsh.glsl
    GL/route_dash.fsh.glsl
    GL/route_marker.fsh.glsl
    GL/route_marker.vsh.glsl
    GL/ruler.vsh.glsl
    GL/screen_quad.vsh.glsl
    GL/selection_line.fsh.glsl
    GL/selection_line.vsh.glsl
    GL/shaders_lib.glsl
    GL/smaa_blending_weight.fsh.glsl
    GL/smaa_blending_weight.vsh.glsl
    GL/smaa_edges.fsh.glsl
    GL/smaa_edges.vsh.glsl
    GL/smaa_final.fsh.glsl
    GL/smaa_final.vsh.glsl
    GL/solid_color.fsh.glsl
    GL/text.fsh.glsl
    GL/text.vsh.glsl
    GL/text_billboard.vsh.glsl
    GL/text_outlined.vsh.glsl
    GL/text_outlined_billboard.vsh.glsl
    GL/text_outlined_gui.vsh.glsl
    GL/texturing.fsh.glsl
    GL/texturing.vsh.glsl
    GL/texturing3d.fsh.glsl
    GL/texturing_billboard.vsh.glsl
    GL/texturing_gui.vsh.glsl
    GL/traffic.fsh.glsl
    GL/traffic.vsh.glsl
    GL/traffic_circle.fsh.glsl
    GL/traffic_circle.vsh.glsl
    GL/traffic_line.fsh.glsl
    GL/traffic_line.vsh.glsl
    GL/transit.fsh.glsl
    GL/transit.vsh.glsl
    GL/transit_circle.fsh.glsl
    GL/transit_circle.vsh.glsl
    GL/transit_marker.fsh.glsl
    GL/transit_marker.vsh.glsl
    GL/user_mark.fsh.glsl
    GL/user_mark.vsh.glsl
    GL/user_mark_billboard.vsh.glsl
)

add_custom_command(
  WORKING_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS
    gl_shaders_preprocessor.py
    ${shader_files}
    GL/shader_index.txt
    programs.hpp
  COMMAND
    ${Python3_EXECUTABLE}
    ARGS
      gl_shaders_preprocessor.py
      GL/
      shader_index.txt
      programs.hpp
      shaders_lib.glsl
      ./
      gl_shaders
  OUTPUT
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_shaders.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gl_shaders.hpp
)

source_group("Shader Index" FILES GL/shader_index.txt)

target_sources(${PROJECT_NAME}
  PRIVATE
    ${shader_files}
    gl_program_info.hpp
    gl_program_params.cpp
    gl_program_params.hpp
    gl_program_pool.cpp
    gl_program_pool.hpp
    gl_shaders.cpp
    gl_shaders.hpp
    program_manager.cpp
    program_manager.hpp
    program_params.cpp
    program_params.hpp
    program_pool.hpp
    programs.hpp
    GL/shader_index.txt
    vulkan_program_params.cpp
    vulkan_program_params.hpp
    vulkan_program_pool.cpp
    vulkan_program_pool.hpp
)

if (PLATFORM_IPHONE OR PLATFORM_MAC)
  target_sources(${PROJECT_NAME}
    PRIVATE
      metal_program_params.hpp
      metal_program_params.mm
      metal_program_pool.hpp
      metal_program_pool.mm
      program_manager_metal.mm
  )
endif()

# Do not include glm's CMakeLists.txt, because it's outdated and not necessary.
target_include_directories(${PROJECT_NAME} PUBLIC "${OMIM_ROOT}/3party/glm")

target_link_libraries(${PROJECT_NAME} drape)

if (PLATFORM_MAC)
  set_target_properties(${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
endif()

omim_add_test_subdirectory(shaders_tests)
