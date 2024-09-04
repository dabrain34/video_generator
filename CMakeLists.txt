cmake_minimum_required(VERSION 2.8)

option(BUILD_EXAMPLES "Build demos" ON)

include(${CMAKE_CURRENT_LIST_DIR}/build/Triplet.cmake)

set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/src/)

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/src/lib
)

add_subdirectory(src/lib)

if(BUILD_EXAMPLES AND NOT DEFINED DEQP_TARGET)
    add_subdirectory(src/examples)
endif()
