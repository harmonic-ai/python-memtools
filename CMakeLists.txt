cmake_minimum_required(VERSION 3.10)



# Project setup

project(python-memtools)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(LOCAL_INCLUDE_DIR "/usr/local/include")
set(LOCAL_LIB_DIR "/usr/local/lib")
list(APPEND CMAKE_PREFIX_PATH ${LOCAL_LIB_DIR})
include_directories(${LOCAL_INCLUDE_DIR})
link_directories(${LOCAL_LIB_DIR})



# Library search

find_package(phosg REQUIRED)



# Executable definition

file(GLOB_RECURSE SOURCES "src/*.cc")
add_executable(python-memtools ${SOURCES})
target_compile_options(python-memtools PRIVATE "-Wall" "-Werror")
set(PYMEMTOOLS_PYTHON_VERSION "310" CACHE STRING "CPython ABI version to target (310 or 314)")
target_compile_definitions(python-memtools PRIVATE PYMEMTOOLS_PYTHON_VERSION=${PYMEMTOOLS_PYTHON_VERSION})
target_link_libraries(python-memtools phosg pthread readline)
