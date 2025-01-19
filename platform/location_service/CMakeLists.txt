project(location_service)

if (NOT SKIP_QT_GUI AND PLATFORM_DESKTOP AND PLATFORM_LINUX)
  message("Building with Qt Positioning")
  find_package(Qt6 REQUIRED COMPONENTS Positioning)
  set(QT_LOCATION_SERVICE true)
elseif(NOT SKIP_QT_GUI AND PLATFORM_DESKTOP AND PLATFORM_MAC)
  set(APPLE_LOCATION_SERVICE true)
endif()

set(SRC
  location_service.cpp
  location_service.hpp
  $<$<BOOL:${APPLE_LOCATION_SERVICE}>:apple_location_service.mm>
  $<$<BOOL:${QT_LOCATION_SERVICE}>:qt_location_service.hpp>
  $<$<BOOL:${QT_LOCATION_SERVICE}>:qt_location_service.cpp>
)

omim_add_library(${PROJECT_NAME} ${SRC})

if (QT_LOCATION_SERVICE)
  target_compile_definitions(${PROJECT_NAME} PRIVATE "QT_LOCATION_SERVICE")
  target_link_libraries(${PROJECT_NAME} Qt6::Positioning)
  set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC ON)
elseif(APPLE_LOCATION_SERVICE)
  target_compile_definitions(${PROJECT_NAME} PRIVATE "APPLE_LOCATION_SERVICE")
  target_compile_options(${PROJECT_NAME} PRIVATE -fobjc-arc -Wno-nullability-completeness)
  target_link_libraries(${PROJECT_NAME} -framework\ CoreLocation)
endif()
