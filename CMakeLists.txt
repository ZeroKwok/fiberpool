cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0074 NEW)

project(fiber_pool VERSION 0.1.1.0)

set(CMAKE_CXX_STANDARD 17)

option(FIBERPOOL_BUILD_EXAMPLE          "Whether to build example"            ON)
option(FIBERPOOL_BUILD_SHARED_LIBRARY   "Whether to build a shared library"   ON)
option(FIBERPOOL_ENABLE_STATIC_RUNTIME  "Enable link with runtime statically" OFF)

if(MSVC AND FIBERPOOL_ENABLE_STATIC_RUNTIME)
    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endforeach(flag_var)
endif()

if(MSVC)
    add_compile_options(/utf-8)
endif()

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib")
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib")

set(CMAKE_DEBUG_POSTFIX "d")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(internal_util)
include(library_regular_naming)

# FindBoost.cmake
set(Boost_USE_STATIC_LIBS ON)
add_compile_definitions(BOOST_LIB_DIAGNOSTIC=1)

set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(Boost REQUIRED fiber thread)

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

set(SOURCE_FILES src/fiber_pool.cpp src/shared_work.cpp)

if(FIBERPOOL_BUILD_SHARED_LIBRARY)
    add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
    target_compile_definitions(${PROJECT_NAME} PUBLIC 
        $<BUILD_INTERFACE:FIBERPOOL_BUILD_SHARED_LIB>
        $<INSTALL_INTERFACE:FIBERPOOL_BUILD_SHARED_LIB> )
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SAFESEH:NO")
else()
    add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
endif()

library_regular_naming(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>)
	
if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        Boost::fiber 
        Boost::thread)
endif()

file(GLOB INCLUDE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${INCLUDE_FILES}")

install(EXPORT ${PROJECT_NAME} FILE ${PROJECT_NAME}Config.cmake DESTINATION cmake)
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib 
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/example DESTINATION ./)
install_pdb(${PROJECT_NAME})

if (FIBERPOOL_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()