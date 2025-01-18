project(dev_sandbox)

set(SRC
  main.cpp
  imgui_renderer.cpp
  imgui_renderer.hpp
)

if (${PLATFORM_MAC})
  append(SRC
    main.mm
    ../iphone/Maps/Classes/MetalContextFactory.h
    ../iphone/Maps/Classes/MetalContextFactory.mm
  )

  file(GLOB_RECURSE SHADER_SOURCES_FILES ${OMIM_ROOT}/shaders/Metal/*.metal)
  add_metal_shader_library(shaders_metal
    ${SHADER_SOURCES_FILES}
  )
endif()

if (${PLATFORM_LINUX})
  append(SRC
    main_linux.cpp
  )
endif()

omim_add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SRC})

target_link_libraries(${PROJECT_NAME}
  generator  # For borders::LoadBorders
  qt_common
  map
  gflags::gflags
  glfw
  imgui
)

# Installing Vulkan SDK is optional, however without it Vulkan dynamic libraries
# should be discovered via system paths and validation layers may not be available
find_package(Vulkan QUIET)
if (Vulkan_FOUND)
  message(STATUS "Vulkan found")
  target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
endif()

if(PLATFORM_MAC)
  target_embed_metal_shader_libraries(${PROJECT_NAME} shaders_metal)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC GL_SILENCE_DEPRECATION)
if (PLATFORM_MAC)
  set_target_properties(${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
endif()

if (PLATFORM_LINUX)
  target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_XLIB_KHR)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${OMIM_ROOT}/3party/glfw/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${OMIM_ROOT}/3party/imgui)

set(BUNDLE_NAME "OMapsDevSandbox")
set(BUNDLE_DISPLAY_NAME "Organic Maps: Developer Sandbox")

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${BUNDLE_NAME})

set(BUNDLE_EXECUTABLE ${BUNDLE_NAME})

set(BUNDLE_FOLDER ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${BUNDLE_NAME}.app)
set(RESOURCES_FOLDER ${BUNDLE_FOLDER}/Contents/Resources)
set(DATA_DIR ${OMIM_ROOT}/data)

execute_process(
  COMMAND mkdir -p ${RESOURCES_FOLDER}/shaders_compiler
)

function(copy_resources)
  foreach(file ${ARGN})
    execute_process(
      COMMAND cp -r ${DATA_DIR}/${file} ${RESOURCES_FOLDER}
    )
  endforeach()
endfunction()

copy_resources(
  countries-strings
  resources-default
  resources-mdpi_light
  resources-hdpi_light
  resources-xhdpi_light
  resources-xxhdpi_light
  resources-xxxhdpi_light
  resources-6plus_light

  categories.txt
  categories_cuisines.txt
  classificator.txt
  colors.txt
  countries.txt
  drules_proto_default_light.bin
  drules_proto_default_dark.bin
  drules_proto_vehicle_light.bin
  drules_proto_vehicle_dark.bin
  editor.config
  fonts_blacklist.txt
  fonts_whitelist.txt
  packed_polygons.bin
  patterns.txt
  transit_colors.txt
  types.txt
  unicode_blocks.txt
  World.mwm
  WorldCoasts.mwm

  00_NotoNaskhArabic-Regular.ttf
  00_NotoSansBengali-Regular.ttf
  00_NotoSansHebrew-Regular.ttf
  00_NotoSansMalayalam-Regular.ttf
  00_NotoSansThai-Regular.ttf
  00_NotoSerifDevanagari-Regular.ttf
  01_dejavusans.ttf
  02_droidsans-fallback.ttf
  03_jomolhari-id-a3d.ttf
  04_padauk.ttf
  05_khmeros.ttf
  06_code2000.ttf
  07_roboto_medium.ttf
)

if (NOT PLATFORM_LINUX)
  # On Linux, ICU data is loaded from the shared library.
  copy_resources(icudt75l.dat)
endif()

if (PLATFORM_MAC)
  execute_process(
    COMMAND cp -r ${OMIM_ROOT}/tools/shaders_compiler/macos ${RESOURCES_FOLDER}/shaders_compiler
  )
elseif (PLATFORM_LINUX)
  execute_process(
    COMMAND cp -r ${OMIM_ROOT}/tools/shaders_compiler/linux ${RESOURCES_FOLDER}/shaders_compiler
  )
endif()
