cmake_minimum_required(VERSION 3.10)
project(video_generator)

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(COMMON_COMPILE_FLAGS "-Wall -Wconversion -Wformat -Werror -Wextra")
    set(COMMON_COMPILE_FLAGS "${COMMON_COMPILE_FLAGS} -fno-strict-aliasing -fno-builtin-memcmp")

    if (APPLE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_COMPILE_FLAGS}")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_COMPILE_FLAGS}")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_COMPILE_FLAGS} -std=c++11 -fno-rtti")
    if (UNIX)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    endif()
endif()

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
