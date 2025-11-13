# Build examples
# Note: When FLOW_USE_MODULES is ON, source files should use 'import flow;'
# instead of '#include "flow/execution.hpp"'

add_executable(hello_world hello_world.cpp)
target_link_libraries(hello_world PRIVATE flow::flow)

add_executable(error_handling error_handling.cpp)
target_link_libraries(error_handling PRIVATE flow::flow)

add_executable(parallel_transform parallel_transform.cpp)
target_link_libraries(parallel_transform PRIVATE flow::flow)

add_executable(when_any_example when_any_example.cpp)
target_link_libraries(when_any_example PRIVATE flow::flow)

add_executable(try_schedule_example try_schedule_example.cpp)
target_link_libraries(try_schedule_example PRIVATE flow::flow)
