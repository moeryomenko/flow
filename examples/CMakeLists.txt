add_executable(hello_world hello_world.cpp)
target_link_libraries(hello_world PRIVATE flow::flow)

add_executable(error_handling error_handling.cpp)
target_link_libraries(error_handling PRIVATE flow::flow)

add_executable(parallel_transform parallel_transform.cpp)
target_link_libraries(parallel_transform PRIVATE flow::flow)
