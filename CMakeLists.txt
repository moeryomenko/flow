cmake_minimum_required(VERSION 3.23)

project(
  flow
  VERSION 1.0.0
  DESCRIPTION "Modern C++ asynchronous execution library based on P2300R10"
  LANGUAGES
  CXX
)

# Project-wide settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(FLOW_BUILD_EXAMPLES "Build example applications" ON)
option(FLOW_BUILD_TESTS "Build test suite" ON)
option(FLOW_INSTALL "Generate install target" ON)
option(FLOW_USE_MODULES "Use C++23 modules if available (experimental, requires CMake 3.28+)" OFF)

# Check CMake version for modules support
if(FLOW_USE_MODULES AND CMAKE_VERSION VERSION_LESS "3.28")
  message(FATAL_ERROR "C++ modules support requires CMake 3.28 or higher. "
                      "Current version: ${CMAKE_VERSION}. "
                      "Please upgrade CMake or set FLOW_USE_MODULES=OFF.")
endif()

if(FLOW_USE_MODULES)
  message(STATUS "C++ modules enabled - using CMake FILE_SET CXX_MODULES")

  # Check compiler support for modules
  if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    message(WARNING "Apple Clang does not yet fully support CMake's C++ module scanning. "
                    "Consider using Clang from Homebrew (brew install llvm) or disabling modules.")
  endif()

  # Enable module scanning globally
  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()

# Include CMake utilities
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Create main library target
if(FLOW_USE_MODULES)
  # Use C++ modules (experimental)
  add_library(flow)
  add_library(flow::flow ALIAS flow)

  target_sources(
    flow
    PUBLIC
    FILE_SET CXX_MODULES FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/flow.cppm
  )

  target_compile_features(flow PUBLIC cxx_std_23)

  target_include_directories(
    flow
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
else()
  # Traditional header-only interface library
  add_library(flow INTERFACE)
  add_library(flow::flow ALIAS flow)

  target_include_directories(
    flow
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  target_compile_features(flow INTERFACE cxx_std_23)
endif()

# Platform-specific settings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  if(FLOW_USE_MODULES)
    target_compile_options(
      flow
      PUBLIC
      -Wall
      -Wextra
      -Wpedantic
      $<$<CONFIG:Debug>:-g
      -O0>
      $<$<CONFIG:Release>:-O3
      -DNDEBUG>
    )
  else()
    target_compile_options(
      flow
      INTERFACE
      -Wall
      -Wextra
      -Wpedantic
      $<$<CONFIG:Debug>:-g
      -O0>
      $<$<CONFIG:Release>:-O3
      -DNDEBUG>
    )
  endif()
elseif(MSVC)
  if(FLOW_USE_MODULES)
    target_compile_options(
      flow
      PUBLIC
      /W4
      /permissive-
      $<$<CONFIG:Debug>:/Od
      /Zi>
      $<$<CONFIG:Release>:/O2
      /DNDEBUG>
    )
  else()
    target_compile_options(
      flow
      INTERFACE
      /W4
      /permissive-
      $<$<CONFIG:Debug>:/Od
      /Zi>
      $<$<CONFIG:Release>:/O2
      /DNDEBUG>
    )
  endif()
endif()

# Find threading library (required for schedulers)
find_package(Threads REQUIRED)
if(FLOW_USE_MODULES)
  target_link_libraries(flow PUBLIC Threads::Threads)
else()
  target_link_libraries(flow INTERFACE Threads::Threads)
endif()

# Formatting target
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(CLANG_FORMAT_EXECUTABLE)
  file(
    GLOB_RECURSE
    ALL_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
  )

  add_custom_target(
    format
    COMMAND ${CLANG_FORMAT_EXECUTABLE} -i -style=file ${ALL_SOURCE_FILES}
    COMMENT "Running clang-format on source files"
    VERBATIM
  )

  message(STATUS "clang-format found: ${CLANG_FORMAT_EXECUTABLE}")
  message(STATUS "  Run 'cmake --build . --target format' to format code")
else()
  message(STATUS "clang-format not found - format target not available")
endif()

# Linting target
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
if(CLANG_TIDY_EXECUTABLE)
  file(
    GLOB_RECURSE
    ALL_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp
  )

  add_custom_target(
    lint
    COMMAND
    ${CLANG_TIDY_EXECUTABLE}
    -p
    ${CMAKE_CURRENT_BINARY_DIR}
    ${ALL_SOURCE_FILES}
    --
    -std=c++23
    -I${CMAKE_CURRENT_SOURCE_DIR}/include
    COMMENT "Running clang-tidy on source files"
    VERBATIM
  )

  add_custom_target(
    lint-fix
    COMMAND
    ${CLANG_TIDY_EXECUTABLE}
    -p
    ${CMAKE_CURRENT_BINARY_DIR}
    -fix
    ${ALL_SOURCE_FILES}
    --
    -std=c++23
    -I${CMAKE_CURRENT_SOURCE_DIR}/include
    COMMENT "Running clang-tidy with auto-fix on source files"
    VERBATIM
  )

  message(STATUS "clang-tidy found: ${CLANG_TIDY_EXECUTABLE}")
  message(STATUS "  Run 'cmake --build . --target lint' to lint code")
  message(
    STATUS "  Run 'cmake --build . --target lint-fix' to lint and auto-fix code"
  )
else()
  message(STATUS "clang-tidy not found - lint target not available")
endif()

# Build examples
if(FLOW_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Build tests
if(FLOW_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Installation
if(FLOW_INSTALL)
  # Install headers
  install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.hpp"
  )

  # Install module interface file if using modules
  if(FLOW_USE_MODULES)
    install(
      FILES ${CMAKE_CURRENT_SOURCE_DIR}/flow.cppm
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/flow
    )
  endif()

  # Install targets
  install(
    TARGETS flow
    EXPORT flowTargets
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/flow
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  # Install export set
  install(
    EXPORT flowTargets
    FILE flowTargets.cmake
    NAMESPACE flow::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flow
  )

  # Create and install package config files
  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/flowConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/flowConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flow
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/flowConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/flowConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/flowConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flow
  )
endif()

# Export targets for use in build tree
export(
  EXPORT flowTargets
  FILE ${CMAKE_CURRENT_BINARY_DIR}/flowTargets.cmake
  NAMESPACE flow::
)

# Print configuration summary
message(STATUS "")
message(STATUS "flow configuration summary:")
message(STATUS "  Version:              ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:         ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build examples:       ${FLOW_BUILD_EXAMPLES}")
message(STATUS "  Build tests:          ${FLOW_BUILD_TESTS}")
message(STATUS "  Install:              ${FLOW_INSTALL}")
message(STATUS "  Use C++ modules:      ${FLOW_USE_MODULES}")
message(
  STATUS
  "  Compiler:             ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)
message(STATUS "")
