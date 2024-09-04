cmake_minimum_required(VERSION 2.8)

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/src/lib
)

set(lib_sources
  ${SOURCE_DIR}/lib/video_generator.c
)

set(lib_headers
  ${SOURCE_DIR}/lib/video_generator.h
)

if(WIN32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_library(videogenerator ${lib_sources})
install(TARGETS videogenerator ARCHIVE DESTINATION lib)
install(FILES ${lib_headers} DESTINATION include)




