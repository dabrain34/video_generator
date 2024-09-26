cmake_minimum_required(VERSION 3.10)

set(VG_APP_VGEN videogen)
set(VG_APP_AVGEN audiovideogen)

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")
endif()

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/src/lib
)

set(VG_APP_AVGEN_SRC
  ${VIDEO_GENERATOR_SOURCE_DIR}/examples/avgen.c
)

if (WIN32)
set(VG_APP_VGEN_SRC
  ${VIDEO_GENERATOR_SOURCE_DIR}/examples/videogen.c
)
else ()
set(VG_APP_VGEN_SRC
  ${VIDEO_GENERATOR_SOURCE_DIR}/examples/videogen.c
  ${VIDEO_GENERATOR_SOURCE_DIR}/examples/getopt_long.c
)
endif()

add_executable(${VG_APP_VGEN} ${VG_APP_VGEN_SRC})
add_executable(${VG_APP_AVGEN} ${VG_APP_AVGEN_SRC} )

target_link_libraries(${VG_APP_VGEN} ${VIDEO_GENERATOR_STATIC_LIB})
target_link_libraries(${VG_APP_AVGEN} ${VIDEO_GENERATOR_STATIC_LIB})

if (UNIX AND NOT APPLE)
  target_link_libraries(${VG_APP_VGEN} m)
  target_link_libraries(${VG_APP_AVGEN} m)
endif()

install(TARGETS ${VG_APP_VGEN} ${VG_APP_AVGEN} DESTINATION bin)
