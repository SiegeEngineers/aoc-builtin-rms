cmake_minimum_required(VERSION 3.9.4)

project(aoc-builtin-rms
  LANGUAGES C
)

if (NOT MSVC)
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT error)
if (IPO_SUPPORTED)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

if (NOT MSVC)
  add_compile_options(-m32)
  add_compile_options(-Wall)
endif()
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-DNOMINMAX)
add_definitions(-DEZXML_NOMMAP)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
if (NOT CMAKE_BUILD_TYPE STREQUAL Debug)
  add_definitions(-DNDEBUG)
endif()

include_directories(include/)

set(SOURCE_FILES
  include/ezxml/ezxml.c
  aoc-builtin-rms.c
  hook.c
  main.c
)

add_library(aoc-builtin-rms SHARED ${SOURCE_FILES})

if (UNIX)
  set_target_properties(aoc-builtin-rms PROPERTIES
      PREFIX ""
      SUFFIX ".dll"
      POSITION_INDEPENDENT_CODE OFF)
endif()
install(TARGETS aoc-builtin-rms
        LIBRARY DESTINATION lib)

