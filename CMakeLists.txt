cmake_minimum_required(VERSION 3.14)
project(mm-sim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The simulations are CPU-bound, so single-config generators default to Release.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MMSIM_BUILD_TESTS "Build the unit tests (fetches Catch2)" ON)

# MinGW executables otherwise need the compiler's runtime DLLs on PATH to start.
if(MINGW)
  add_link_options(-static)
endif()

# Warnings sit on an interface target so they apply to our code but not to Catch2.
add_library(mmsim_warnings INTERFACE)
if(MSVC)
  target_compile_options(mmsim_warnings INTERFACE /W4 /WX)
else()
  target_compile_options(mmsim_warnings INTERFACE -Wall -Wextra -Werror)
endif()

add_library(mmsim STATIC
  src/avellaneda_stoikov_mm.cpp
  src/fill_model.cpp
  src/inventory_skew_mm.cpp
  src/market_maker.cpp
  src/price_process.cpp
  src/simulator.cpp
  src/stats.cpp
  src/symmetric_mm.cpp
)
target_include_directories(mmsim PUBLIC include)
target_link_libraries(mmsim PRIVATE mmsim_warnings)

if(MMSIM_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.0
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  include(Catch)
  enable_testing()

  add_executable(mmsim_tests
    tests/test_avellaneda_stoikov_mm.cpp
    tests/test_fill_model.cpp
    tests/test_market_maker.cpp
    tests/test_price_process.cpp
    tests/test_simulator.cpp
    tests/test_stats.cpp
  )
  target_link_libraries(mmsim_tests PRIVATE mmsim mmsim_warnings Catch2::Catch2WithMain)
  catch_discover_tests(mmsim_tests)
endif()
