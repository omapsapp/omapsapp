project(desktop)

QT6_ADD_RESOURCES(RES_SOURCES res/resources.qrc)

set(SRC
  about.cpp
  about.hpp
  bookmark_dialog.cpp
  bookmark_dialog.hpp
  build_style/build_common.cpp
  build_style/build_common.h
  build_style/build_drules.cpp
  build_style/build_drules.h
  build_style/build_phone_pack.cpp
  build_style/build_phone_pack.h
  build_style/build_skins.cpp
  build_style/build_skins.h
  build_style/build_statistics.cpp
  build_style/build_statistics.h
  build_style/build_style.cpp
  build_style/build_style.h
  build_style/run_tests.cpp
  build_style/run_tests.h
  create_feature_dialog.cpp
  create_feature_dialog.hpp
  draw_widget.cpp
  draw_widget.hpp
  editor_dialog.cpp
  editor_dialog.hpp
  info_dialog.cpp
  info_dialog.hpp
  main.cpp
  mainwindow.cpp
  mainwindow.hpp
  mwms_borders_selection.cpp
  mwms_borders_selection.hpp
  osm_auth_dialog.cpp
  osm_auth_dialog.hpp
  place_page_dialog_common.cpp
  place_page_dialog_common.hpp
  place_page_dialog_developer.cpp
  place_page_dialog_developer.hpp
  place_page_dialog_user.cpp
  place_page_dialog_user.hpp
  preferences_dialog.cpp
  preferences_dialog.hpp
  popup_menu_holder.cpp
  popup_menu_holder.hpp
  routing_settings_dialog.cpp
  routing_settings_dialog.hpp
  routing_turns_visualizer.cpp
  routing_turns_visualizer.hpp
  ruler.cpp
  ruler.hpp
  screenshoter.cpp
  screenshoter.hpp
  search_panel.cpp
  search_panel.hpp
  selection.hpp
  update_dialog.cpp
  update_dialog.hpp
)

omim_add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${RES_SOURCES} ${SRC})

set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC ON)

target_link_libraries(${PROJECT_NAME}
  generator  # For borders::LoadBorders
  qt_common
  map
  gflags::gflags
)

if (BUILD_DESIGNER)
  target_link_libraries(${PROJECT_NAME} generator)
  set(BUNDLE_NAME "OMaps.Designer")
  set(BUNDLE_DISPLAY_NAME "Organic Maps Designer")
  set(BUNDLE_ICON designer.icns)
else()
  set(BUNDLE_NAME "OMaps")
  set(BUNDLE_DISPLAY_NAME "Organic Maps Desktop")
  set(BUNDLE_ICON mac.icns)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${BUNDLE_NAME})

set(BUNDLE_EXECUTABLE ${BUNDLE_NAME})

set(BUNDLE_FOLDER ${CMAKE_BINARY_DIR}/${BUNDLE_NAME}.app)
set(RESOURCES_FOLDER ${BUNDLE_FOLDER}/Contents/Resources)
set(DATA_DIR ${OMIM_ROOT}/data)

execute_process(
  COMMAND mkdir -p ${RESOURCES_FOLDER}/shaders_compiler
  COMMAND cp ${PROJECT_SOURCE_DIR}/res/mac.icns ${RESOURCES_FOLDER}
  COMMAND cp ${PROJECT_SOURCE_DIR}/res/designer.icns ${RESOURCES_FOLDER}
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
  welcome.html

  categories.txt
  categories_cuisines.txt
  classificator.txt
  colors.txt
  copyright.html
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

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/)
install(DIRECTORY ${OMIM_ROOT}/data DESTINATION ${CMAKE_INSTALL_PREFIX}/share/organicmaps/)

if (PLATFORM_LINUX)
  install(FILES ${OMIM_ROOT}/packaging/app.organicmaps.desktop.metainfo.xml DESTINATION ${CMAKE_INSTALL_PREFIX}/share/metainfo/)
  install(FILES ${OMIM_ROOT}/qt/res/app.organicmaps.desktop.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications/)
  install(FILES ${OMIM_ROOT}/qt/res/logo.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/512x512/apps/ RENAME organicmaps.png)
endif()

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

if (PLATFORM_MAC)
  if (NOT APP_VERSION)
    set(BUNDLE_VERSION "UNKNOWN")
  else()
    set(BUNDLE_VERSION ${APP_VERSION})
  endif()

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/res/Info.plist.in ${CMAKE_CURRENT_SOURCE_DIR}/res/Info.plist)

  set_target_properties(${PROJECT_NAME}
    PROPERTIES
      OUTPUT_NAME ${BUNDLE_NAME}
      MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/res/Info.plist
  )
endif()

if (PLATFORM_WIN)
  target_sources(${PROJECT_NAME} PRIVATE res/windows.rc)
endif()

if (BUILD_DESIGNER)
  execute_process(
    COMMAND cp -rf ${OMIM_ROOT}/data/resources-mdpi_light/ ${OMIM_ROOT}/data/resources-mdpi_design/
    COMMAND cp -rf ${OMIM_ROOT}/data/resources-hdpi_light/ ${OMIM_ROOT}/data/resources-hdpi_design/
    COMMAND cp -rf ${OMIM_ROOT}/data/resources-xhdpi_light/ ${OMIM_ROOT}/data/resources-xhdpi_design/
    COMMAND cp -rf ${OMIM_ROOT}/data/resources-xxhdpi_light/ ${OMIM_ROOT}/data/resources-xxhdpi_design/
    COMMAND cp -rf ${OMIM_ROOT}/data/resources-6plus_light/ ${OMIM_ROOT}/data/resources-6plus_design/
    COMMAND cp -rf ${OMIM_ROOT}/data/resources-xxxhdpi_light/ ${OMIM_ROOT}/data/resources-xxxhdpi_design/
    COMMAND cp -f ${OMIM_ROOT}/data/drules_proto_default_light.bin ${OMIM_ROOT}/data/drules_proto_default_design.bin
    COMMAND cp -f ${OMIM_ROOT}/data/colors.txt ${OMIM_ROOT}/data/colors_design.txt
    COMMAND cp -f ${OMIM_ROOT}/data/patterns.txt ${OMIM_ROOT}/data/patterns_design.txt
  )

  copy_resources(
    colors_design.txt
    drules_proto_default_design.bin
    mapcss-dynamic.txt
    mapcss-mapping.csv
    patterns_design.txt
    resources-mdpi_design
    resources-hdpi_design
    resources-xhdpi_design
    resources-xxhdpi_design
    resources-xxxhdpi_design
    resources-6plus_design
  )

  execute_process(
    COMMAND cp ${OMIM_ROOT}/tools/python/recalculate_geom_index.py ${RESOURCES_FOLDER}
    COMMAND cp ${OMIM_ROOT}/tools/python/generate_styles_override.py ${RESOURCES_FOLDER}
    COMMAND cp -rf ${OMIM_ROOT}/tools/kothic ${RESOURCES_FOLDER}/kothic/
    COMMAND cp -f ${OMIM_ROOT}/tools/python/stylesheet/drules_info.py ${RESOURCES_FOLDER}/kothic/src/
    COMMAND cp -rf ${OMIM_ROOT}/tools/python/stylesheet/ ${RESOURCES_FOLDER}/kothic/src/
    COMMAND cp -f ${OMIM_ROOT}/3party/protobuf/protobuf-3.3.0-py2.7.egg ${RESOURCES_FOLDER}/kothic/
  )

  # Generate DMG
  install(DIRECTORY ${DATA_DIR}/styles DESTINATION .)
  set(BUNDLES
    ${CMAKE_BINARY_DIR}/generator_tool.app
    ${CMAKE_BINARY_DIR}/skin_generator_tool.app
    ${CMAKE_BINARY_DIR}/style_tests.app
  )
  install(CODE "
    foreach(BUNDLE ${BUNDLE_FOLDER} ${BUNDLES})
      execute_process(
        COMMAND \"${QT_PATH}/bin/macdeployqt\" \"\${BUNDLE}\"
      )
    endforeach()
    foreach(BUNDLE ${BUNDLES})
      execute_process(
        COMMAND cp -rf \"\${BUNDLE}\" \"${RESOURCES_FOLDER}\"
      )
    endforeach()
  ")
  install(TARGETS ${PROJECT_NAME} DESTINATION .)

  set(CPACK_GENERATOR DragNDrop)
  set(CPACK_DMG_FORMAT UDZO)
  set(CPACK_DMG_VOLUME_NAME ${BUNDLE_NAME})
  set(CPACK_PACKAGE_ICON ${PROJECT_SOURCE_DIR}/res/designer.icns)
  include(CPack)
elseif (BUILD_STANDALONE)
  add_custom_command(TARGET desktop POST_BUILD
    COMMAND "${QT_PATH}/bin/macdeployqt" ${BUNDLE_FOLDER}
    COMMAND ${Python3_EXECUTABLE} "${OMIM_ROOT}/tools/macdeployqtfix/macdeployqtfix.py" -q -nl ${BUNDLE_FOLDER}/Contents/MacOS/${BUNDLE_NAME} ${QT_PATH}
    COMMAND echo "Fixing Qt dependencies finished."
    VERBATIM
    COMMENT "Fixing Qt dependencies for standalone desktop app"
  )
endif()

add_subdirectory(qt_common)
