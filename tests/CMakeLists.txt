# Fetch boost-ext/ut testing framework
include(FetchContent)
FetchContent_Declare(
  ut
  GIT_REPOSITORY https://github.com/boost-ext/ut
  GIT_TAG v2.1.0
)
FetchContent_MakeAvailable(ut)

# List of all test files
set(
  TEST_SOURCES
  basic_test.cpp
  concept_tests.cpp
  customization_point_tests.cpp
  factory_tests.cpp
  adaptor_tests.cpp
  scheduler_tests.cpp
  edge_case_tests.cpp
  consumer_tests.cpp
  error_handling_tests.cpp
  resource_management_tests.cpp
  race_condition_tests.cpp
  performance_tests.cpp
  integration_tests.cpp
  interoperability_tests.cpp
  module_tests.cpp
  platform_tests.cpp
  compilation_tests.cpp
  advanced_features_test.cpp
  limitations_resolved_test.cpp
  async_scope_basic_tests.cpp
  async_scope_comprehensive_tests.cpp
  let_async_scope_tests.cpp
  when_any_tests.cpp
  retry_tests.cpp
)

# Create test executables and register them
# Note: When FLOW_USE_MODULES is ON, test files should use 'import flow;'
# instead of '#include "flow/execution.hpp"'
foreach(TEST_SOURCE ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(${TEST_NAME} PRIVATE flow::flow Boost::ut)
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
