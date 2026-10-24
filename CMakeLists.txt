cmake_minimum_required(VERSION 3.21)
project(doip_uds_server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DOIP_BUILD_TESTS "Build unit tests" ON)
option(DOIP_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

include(FetchContent)

# Warnings for all of our own targets
add_library(project_warnings INTERFACE)
if(MSVC)
  target_compile_options(project_warnings INTERFACE /W4 $<$<BOOL:${DOIP_WARNINGS_AS_ERRORS}>:/WX>)
else()
  target_compile_options(project_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    $<$<BOOL:${DOIP_WARNINGS_AS_ERRORS}>:-Werror>)
endif()

if(DOIP_BUILD_TESTS)
  find_package(GTest CONFIG QUIET)
  if(NOT GTest_FOUND)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    FetchContent_MakeAvailable(googletest)
  endif()
endif()

# Core library: DoIP + UDS logic (filled from milestone 2 on)
add_library(doip_uds_core
  src/doip_header.cpp
  src/vehicle_identification.cpp
  src/version.cpp
)
target_include_directories(doip_uds_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(doip_uds_core PRIVATE project_warnings)

# Server executable
add_executable(doip_server apps/doip_server/main.cpp)
target_link_libraries(doip_server PRIVATE doip_uds_core project_warnings)

if(DOIP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
