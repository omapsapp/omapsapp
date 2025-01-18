project(vulkan_wrapper)

set(SRC
  vulkan_wrapper.cpp
  vulkan_wrapper.h
)

add_library(${PROJECT_NAME} ${SRC})

if (WITH_SYSTEM_PROVIDED_3PARTY)
  find_package(VulkanHeaders REQUIRED)
  target_link_libraries(${PROJECT_NAME} Vulkan::Headers)
else()
  set(VulkanHeaders_INCLUDE_DIR ../Vulkan-Headers/include)
  target_include_directories(${PROJECT_NAME} PUBLIC ../Vulkan-Headers/include)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC .)

if (PLATFORM_LINUX)
  target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_XLIB_KHR)
endif()

# dlopen
target_link_libraries(${PROJECT_NAME} $<$<BOOL:CMAKE_DL_LIBS>:${CMAKE_DL_LIBS}>)
