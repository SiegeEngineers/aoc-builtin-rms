cmake_minimum_required(VERSION 3.9.4)

project(aoc-builtin-rms)

if (UNIX)
  set(CMAKE_CROSS_COMPILING 1)
  set(CMAKE_SYSTEM_NAME Windows)
  set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
  set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
  set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
endif()

if (NOT MSVC)
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

add_compile_options(-m32)
add_compile_options(-Wall)
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-DNOMINMAX)
add_definitions(-DEZXML_NOMMAP)

include_directories(include/)

set(SOURCE_FILES
  include/ezxml/ezxml.c
  aoc-builtin-rms.c
  hook.c
  main.c
  main.def
)

add_library(aoc-builtin-rms SHARED ${SOURCE_FILES})

if (UNIX)
  set_target_properties(aoc-builtin-rms PROPERTIES
      PREFIX ""
      SUFFIX ".dll")
endif()
install(TARGETS aoc-builtin-rms
        LIBRARY DESTINATION lib)

