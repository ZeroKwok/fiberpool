cmake_minimum_required(VERSION 3.10)

add_executable(01_simple 01_simple.cpp)
target_link_libraries(01_simple fiber_pool)

add_executable(02_unittest 02_unittest.cpp)
target_link_libraries(02_unittest fiber_pool)