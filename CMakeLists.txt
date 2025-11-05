cmake_minimum_required(VERSION 3.16)
project(MySeedFinder LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# This AI slop is needed to remove -Wextra flag from cubiomes for MSVC compatibility
if (MSVC)
  set(_cubiomes_cmake "${CMAKE_CURRENT_SOURCE_DIR}/external/cubiomes/CMakeLists.txt")
  if (EXISTS "${_cubiomes_cmake}")
    set(_backup "${_cubiomes_cmake}.cmake_backup")
    if (NOT EXISTS "${_backup}")
      file(READ "${_cubiomes_cmake}" _orig_cmake)
      file(WRITE "${_backup}" "${_orig_cmake}")
    else()
      file(READ "${_backup}" _orig_cmake)
    endif()

    # remove problematic set(CMAKE_C_FLAGS ...) lines
    set(_patched "${_orig_cmake}")
    foreach(_pat
      "set\\(CMAKE_C_FLAGS[^\\)]*\\)"
      "set\\(CMAKE_C_FLAGS_DEBUG[^\\)]*\\)"
      "set\\(CMAKE_C_FLAGS_RELEASE[^\\)]*\\)"
      "set\\(CMAKE_C_FLAGS_RELWITHDEBINFO[^\\)]*\\)"
      "set\\(CMAKE_C_FLAGS_MINSIZEREL[^\\)]*\\)"
    )
      string(REGEX REPLACE "${_pat}" "" _patched "${_patched}")
    endforeach()

    string(REGEX REPLACE "-Wall" "" _patched "${_patched}")
    string(REGEX REPLACE "-Wextra" "" _patched "${_patched}")
    string(REGEX REPLACE "-g3" "" _patched "${_patched}")
    string(REGEX REPLACE "-O0" "" _patched "${_patched}")
    string(STRIP "${_patched}" _patched)

    # overwrite the submodule CMakeLists with the patched version
    file(WRITE "${_cubiomes_cmake}" "${_patched}")
    message(STATUS "Patched external/cubiomes/CMakeLists.txt for MSVC (backup at ${_backup})")
  endif()
endif()

add_subdirectory(external/cubiomes)

set(SEEDFINDER_SOURCES
    src/main.cpp
    src/seed_finder.cpp
    src/quad_temple_finder.cpp
    src/location_finder.cpp
)

add_executable(seedfinder ${SEEDFINDER_SOURCES})

target_include_directories(seedfinder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/cubiomes
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
