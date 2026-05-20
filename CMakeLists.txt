cmake_minimum_required(VERSION 3.30.4)

project(cudf-test-harness LANGUAGES C CXX CUDA)

# Default to Debug build with full debug symbols
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0")
set(CMAKE_CUDA_FLAGS_DEBUG "-g -G -O0")

include(FetchContent)
find_package(CUDAToolkit REQUIRED)

if(CUDAToolkit_VERSION VERSION_LESS 13.0)
  message(FATAL_ERROR "cuDF 26.04.00 source builds require CUDA Toolkit 13.0 or newer")
endif()

foreach(option
  BUILD_TESTS
  BUILD_BENCHMARKS
  CUDF_BUILD_TESTUTIL
  CUDF_BUILD_STREAMS_TEST_UTIL
  CUDF_KVIKIO_REMOTE_IO
)
  set(${option} OFF CACHE BOOL "Disabled for cudf-test-harness cuDF source build" FORCE)
endforeach()

FetchContent_Declare(
  cudf
  GIT_REPOSITORY https://github.com/rapidsai/cudf.git
  GIT_TAG f9c3cf195768647ac39d98674a614ca414cd1baa
  SOURCE_SUBDIR cpp
)
FetchContent_MakeAvailable(cudf)

# Create the executable
add_executable(cudf-test-harness
  src/main.cpp
)

target_include_directories(cudf-test-harness PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cudf-test-harness PRIVATE
  cudf::cudf
  ${CMAKE_DL_LIBS}
)

# Set RPATH for bundled distribution (looks for libs relative to binary)
set_target_properties(cudf-test-harness PROPERTIES
  INSTALL_RPATH "$ORIGIN/lib"
  BUILD_WITH_INSTALL_RPATH OFF
)

# Install target
install(TARGETS cudf-test-harness
  RUNTIME DESTINATION .
  COMPONENT harness
)

# Optional: Build test library
option(BUILD_TEST_LIB "Build the test export library" ON)
if(BUILD_TEST_LIB)
  add_library(test_export_lib SHARED
    test/test_export_lib.cpp
  )
  set_target_properties(test_export_lib PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
  )
  target_link_libraries(test_export_lib PRIVATE
    CUDA::cudart
  )
endif()
