cmake_minimum_required(VERSION 2.8)

set(app_video videogen)
set(app_av audiovideogen)

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/src/lib
)

set(av_gen_src
  ${SOURCE_DIR}/examples/avgen.c
)

if (WIN32)
set(video_gen_src
  ${SOURCE_DIR}/examples/videogen.c
)
else ()
set(video_gen_src
  ${SOURCE_DIR}/examples/videogen.c
  ${SOURCE_DIR}/examples/getopt_long.c
)
endif()

add_executable(${app_video} ${video_gen_src} ${lib_sources})
add_executable(${app_av} ${av_gen_src} ${lib_sources})

target_link_libraries(${app_video} videogenerator)
target_link_libraries(${app_av} videogenerator)

if (UNIX AND NOT APPLE)
  target_link_libraries(${app_video} m)
  target_link_libraries(${app_av} m)
endif()

install(TARGETS ${app_video} ${app_av} DESTINATION bin)
