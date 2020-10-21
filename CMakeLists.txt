cmake_minimum_required(VERSION 3.10)

project(fiber_pool)

set(CMAKE_CXX_STANDARD 17)

option(ENABLE_BUILD_EXAMPLE "Build the example" ON)

find_package(Boost REQUIRED fiber thread)

# 输出查找结果
message(Boost_INCLUDE_DIRS " ${Boost_INCLUDE_DIRS}")
message(Boost_LIBRARY_DIRS " ${Boost_LIBRARY_DIRS}")

# 设置Boost包含目录
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${PROJECT_NAME} 
            src/fiber_pool.cpp
            src/shared_work.cpp)

# 某些环境下不会自动连接
#target_link_libraries(${PROJECT_NAME} PRIVATE 
#    Boost::fiber 
#    Boost::thread)

if (ENABLE_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()