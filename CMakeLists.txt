cmake_minimum_required(VERSION 3.26)

project(cudf-test-harness LANGUAGES CXX CUDA)

# Default to Debug build with full debug symbols
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0")
set(CMAKE_CUDA_FLAGS_DEBUG "-g -G -O0")

# Find cuDF
find_package(cudf REQUIRED)

# Create the executable
add_executable(cudf-test-harness
  src/main.cpp
)

target_include_directories(cudf-test-harness PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Use the conda environment's libstdc++ to match cuDF
# Must be linked before other libraries to override system libstdc++
target_link_libraries(cudf-test-harness PRIVATE
  $ENV{CONDA_PREFIX}/lib/libstdc++.so
  cudf::cudf
  ${CMAKE_DL_LIBS}
)

# Set RPATH to find conda libraries at runtime
set_target_properties(cudf-test-harness PROPERTIES
  BUILD_RPATH "$ENV{CONDA_PREFIX}/lib"
  INSTALL_RPATH "$ENV{CONDA_PREFIX}/lib"
)

# Install target
install(TARGETS cudf-test-harness
  RUNTIME DESTINATION bin
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
