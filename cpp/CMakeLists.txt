cmake_minimum_required(VERSION 3.16)

project(lixel_openapi_cpp VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(_lixel_is_top_level ON)
else()
  set(_lixel_is_top_level OFF)
endif()

option(LIXEL_OPENAPI_BUILD_TESTS "Build the unit tests" ${_lixel_is_top_level})
set(LIXEL_OPENAPI_SANITIZER "" CACHE STRING
    "Sanitizer for our own targets: address, thread, undefined, or empty")
set_property(CACHE LIXEL_OPENAPI_SANITIZER PROPERTY STRINGS "" address thread undefined)

# ---------------------------------------------------------------------------
# Language level and defaults
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# The wire format has one source of truth: <repo>/proto. Python and C++ both use it.
get_filename_component(LIXEL_OPENAPI_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../proto" ABSOLUTE)
set(LIXEL_OPENAPI_PROTO_FILE "${LIXEL_OPENAPI_PROTO_ROOT}/openapi/aiot_msgs.proto")
if(NOT EXISTS "${LIXEL_OPENAPI_PROTO_FILE}")
  message(FATAL_ERROR "Protobuf definition not found: ${LIXEL_OPENAPI_PROTO_FILE}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(LixelSanitizer)
include(LixelDependencies)
include(LixelProto)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
lixel_generate_proto(lixel_openapi_proto)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/lixel/openapi/version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/lixel/openapi/version.h"
  @ONLY)

add_library(lixel_openapi STATIC
  src/version.cpp
)
add_library(LixelOpenApi::lixel_openapi ALIAS lixel_openapi)

target_include_directories(lixel_openapi
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
)
target_link_libraries(lixel_openapi
  PUBLIC
    lixel_openapi_proto
  PRIVATE
    lixel::paho
    PkgConfig::LZ4
    Threads::Threads
)
lixel_enable_warnings(lixel_openapi)
lixel_apply_sanitizer(lixel_openapi)

if(LIXEL_OPENAPI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

message(STATUS "lixel_openapi ${PROJECT_VERSION}: protobuf ${LIXEL_PROTOBUF_VERSION} "
               "(${LIXEL_PROTOBUF_SOURCE}), paho.mqtt.c ${LIXEL_PAHO_SOURCE}, "
               "C++${CMAKE_CXX_STANDARD}, build type ${CMAKE_BUILD_TYPE}")
