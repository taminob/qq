cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(
  qq
  VERSION 0.0.0
  DESCRIPTION "Tool for storing and quickly executing functions"
  LANGUAGES C CXX)

option(QQ_ENABLE_TESTS "Enable compilation of tests" OFF)
option(QQ_ENABLE_COVERAGE "Enable compilation with test coverage flags" OFF)
option(QQ_ENABLE_ADDRESS_SANITIZE
       "Enable compilation with address sanitize flags" OFF)
option(QQ_ENABLE_THREAD_SANITIZE
       "Enable compilation with thread sanitize flags" OFF)
option(QQ_ENABLE_UNDEFINED_SANITIZE
       "Enable compilation with undefined sanitize flags" OFF)
option(QQ_ENABLE_UNREACHABLE_SANITIZE
       "Enable compilation with unreachable sanitize flags" OFF)

find_package(Boost 1.70.0 REQUIRED CONFIG COMPONENTS headers program_options json)

set(PPPLUGIN_SHARED OFF)
find_package(ppplugin REQUIRED)

# enable generation of compile_commands.json file
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(QQ_WARNING_OPTIONS
    "-Wall -Wextra"
    CACHE STRING "Warning options for C++ compilation")

if(${QQ_ENABLE_ADDRESS_SANITIZE})
  set(QQ_SANITIZE_OPTIONS
      "-fsanitize=address ${QQ_SANITIZE_OPTIONS}"
      CACHE STRING "Sanitization options for C++ compilation")
endif()
if(${QQ_ENABLE_THREAD_SANITIZE})
  set(CMAKE_CXX_FLAGS
      "-fsanitize=thread ${QQ_SANITIZE_OPTIONS}"
      CACHE STRING "Sanitization options for C++ compilation")
endif()
if(${QQ_ENABLE_UNDEFINED_SANITIZE})
  set(CMAKE_CXX_FLAGS
      "-fsanitize=undefined ${QQ_SANITIZE_OPTIONS}"
      CACHE STRING "Sanitization options for C++ compilation")
endif()
if(${QQ_ENABLE_UNREACHABLE_SANITIZE})
  set(CMAKE_CXX_FLAGS
      "-fsanitize=unreachable ${QQ_SANITIZE_OPTIONS}"
      CACHE STRING "Sanitization options for C++ compilation")
endif()

add_subdirectory(src)
