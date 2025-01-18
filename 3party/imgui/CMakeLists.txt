project(imgui)

set(SRC
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui.cpp
  imgui/backends/imgui_impl_glfw.cpp
)

add_library(${PROJECT_NAME} ${SRC})
target_include_directories(${PROJECT_NAME} 
  PRIVATE ${OMIM_ROOT}/3party/glfw/include
  PUBLIC ${OMIM_ROOT}/3party/imgui/imgui
  PUBLIC .
)
