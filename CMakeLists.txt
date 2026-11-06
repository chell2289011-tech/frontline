cmake_minimum_required(VERSION 3.16)
project(Frontline2022 CXX)
set(CMAKE_CXX_STANDARD 17)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

add_library(sim src/sim.cpp)
add_executable(frontline src/main.cpp)
target_include_directories(frontline PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
target_link_libraries(frontline sim ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES})

add_executable(headless src/headless.cpp)
target_link_libraries(headless sim)

add_custom_command(TARGET frontline POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:frontline>/assets)
