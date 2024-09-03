cmake_minimum_required(VERSION 2.8)

include(${CMAKE_CURRENT_LIST_DIR}/build/Triplet.cmake)

set(app videogen)

set(sd ${CMAKE_CURRENT_LIST_DIR}/src/)


include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/src/lib
)

set(lib_sources
  ${sd}/lib/video_generator.c
)

set(lib_headers
  ${sd}/lib/video_generator.h
)

set(video_gen_src
  ${sd}/examples/videogen.c
  ${sd}/examples/getopt_long.c
)

add_library(videogenerator ${lib_sources})
install(TARGETS videogenerator ARCHIVE DESTINATION lib)
install(FILES ${lib_headers} DESTINATION include)

add_executable(${app} ${video_gen_src} ${lib_sources})
#add_executable(${app_av} ${av_gen_src} ${lib_sources})

if (UNIX AND NOT APPLE)
  target_link_libraries(${app} m)
  #target_link_libraries(${app_av} m)
endif()

if (WIN32)
  target_link_libraries(${app} videogenerator)
else()
  target_link_libraries(${app} videogenerator pthread)
endif()

install(TARGETS ${app} DESTINATION bin)




