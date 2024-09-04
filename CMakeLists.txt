cmake_minimum_required(VERSION 2.8)

option(BUILD_EXAMPLES "Build examples" ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/build/Triplet.cmake)

set(VIDEO_GENERATOR_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/)
set(VIDEO_GENERATOR_INCLUDE_DIR ${VIDEO_GENERATOR_SOURCE_DIR}/lib CACHE PATH "Path to video generator include directory")
set(VIDEO_GENERATOR_LIB videogenerator CACHE PATH "The name of video generator library")
set(VIDEO_GENERATOR_STATIC_LIB videogenerator-static CACHE PATH "The name of static video generator library")

include_directories(
  ${VIDEO_GENERATOR_INCLUDE_DIR}
)

add_subdirectory(src/lib)

if(BUILD_EXAMPLES AND NOT DEFINED DEQP_TARGET)
    add_subdirectory(src/examples)
endif()
