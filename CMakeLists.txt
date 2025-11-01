cmake_minimum_required(VERSION 3.16)
project(MySeedFinder LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

add_subdirectory(external/cubiomes)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/cubiomes
)

add_executable(seedfinder
    src/main.cpp
)

if (TARGET cubiomes_static)
  target_link_libraries(seedfinder PRIVATE cubiomes_static)
  add_dependencies(seedfinder cubiomes_static)
elseif (TARGET cubiomes)
  target_link_libraries(seedfinder PRIVATE cubiomes)
  add_dependencies(seedfinder cubiomes)
else()
  message(FATAL_ERROR "Neither cubiomes_static nor cubiomes target found")
endif()

find_package(Threads REQUIRED)
target_link_libraries(seedfinder PRIVATE Threads::Threads)

if (NOT MSVC)
  target_link_libraries(seedfinder PRIVATE m)
endif()
