# Third-party dependencies, pinned.
#
#   protobuf   pinned to v21.12 (3.21.12). The generated C++ code and the runtime
#              must come from the same release, so the default is to build the
#              pinned release unless the system one is new enough.
#   paho.mqtt.c pinned to v1.3.13, the same release the device side builds against.
#   lz4, OpenSSL come from the system (stable ABI, available as packages).

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
pkg_check_modules(LZ4 REQUIRED IMPORTED_TARGET liblz4)

# ---------------------------------------------------------------------------
# protobuf
# ---------------------------------------------------------------------------
set(LIXEL_PROTOBUF_PINNED_TAG "v21.12")
set(LIXEL_PROTOBUF_PINNED_VERSION "3.21.12")
set(LIXEL_PROTOBUF_MIN_AUTO "3.21.0")

set(LIXEL_OPENAPI_PROTOBUF_PROVIDER "AUTO" CACHE STRING
    "protobuf source: AUTO (system if >= ${LIXEL_PROTOBUF_MIN_AUTO}, else build the pinned release), SYSTEM, or FETCH")
set_property(CACHE LIXEL_OPENAPI_PROTOBUF_PROVIDER PROPERTY STRINGS AUTO SYSTEM FETCH)
if(NOT LIXEL_OPENAPI_PROTOBUF_PROVIDER MATCHES "^(AUTO|SYSTEM|FETCH)$")
  message(FATAL_ERROR "LIXEL_OPENAPI_PROTOBUF_PROVIDER must be AUTO, SYSTEM or FETCH")
endif()

# Read the version of the system protoc WITHOUT calling find_package(Protobuf).
# find_package creates imported targets (protobuf::protoc, protobuf::libprotobuf)
# in this directory scope; if we then decided to build the pinned release, those
# system targets would shadow the fetched ones of the same name and the generated
# code could silently be built against a different protobuf release.
set(_lixel_system_protoc_version "")
find_program(LIXEL_SYSTEM_PROTOC protoc)
if(LIXEL_SYSTEM_PROTOC)
  execute_process(COMMAND "${LIXEL_SYSTEM_PROTOC}" --version
                  OUTPUT_VARIABLE _lixel_protoc_out OUTPUT_STRIP_TRAILING_WHITESPACE
                  RESULT_VARIABLE _lixel_protoc_rc ERROR_QUIET)
  if(_lixel_protoc_rc EQUAL 0 AND _lixel_protoc_out MATCHES "libprotoc ([0-9]+(\\.[0-9]+)+)")
    set(_lixel_system_protoc_version "${CMAKE_MATCH_1}")
  endif()
endif()

set(_lixel_fetch_protobuf OFF)
if(LIXEL_OPENAPI_PROTOBUF_PROVIDER STREQUAL "FETCH")
  set(_lixel_fetch_protobuf ON)
elseif(_lixel_system_protoc_version STREQUAL "")
  if(LIXEL_OPENAPI_PROTOBUF_PROVIDER STREQUAL "SYSTEM")
    message(FATAL_ERROR "LIXEL_OPENAPI_PROTOBUF_PROVIDER=SYSTEM but no system protoc was found")
  endif()
  message(STATUS "No system protobuf found; building ${LIXEL_PROTOBUF_PINNED_TAG}")
  set(_lixel_fetch_protobuf ON)
elseif(_lixel_system_protoc_version VERSION_LESS LIXEL_PROTOBUF_MIN_AUTO)
  if(LIXEL_OPENAPI_PROTOBUF_PROVIDER STREQUAL "SYSTEM")
    message(WARNING "System protobuf ${_lixel_system_protoc_version} is older than the tested "
                    "${LIXEL_PROTOBUF_PINNED_VERSION}; continuing because SYSTEM was requested")
  else()
    message(STATUS "System protobuf ${_lixel_system_protoc_version} is older than "
                   "${LIXEL_PROTOBUF_MIN_AUTO}; building ${LIXEL_PROTOBUF_PINNED_TAG}")
    set(_lixel_fetch_protobuf ON)
  endif()
endif()

if(NOT _lixel_fetch_protobuf)
  find_package(Protobuf REQUIRED)
endif()

if(_lixel_fetch_protobuf)
  set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
  set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
  set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(protobuf
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG ${LIXEL_PROTOBUF_PINNED_TAG}
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR cmake)
  FetchContent_MakeAvailable(protobuf)
  # Guard: both names must be aliases of the targets built from the pinned
  # sources. An imported target of the same name would mean a system protobuf
  # leaked in and shadowed them.
  foreach(_lixel_pb_target protobuf::libprotobuf protobuf::protoc)
    get_target_property(_lixel_alias ${_lixel_pb_target} ALIASED_TARGET)
    if(NOT _lixel_alias)
      message(FATAL_ERROR "${_lixel_pb_target} is not the fetched protobuf; "
                          "a system protobuf is shadowing it")
    endif()
  endforeach()
  set(LIXEL_PROTOBUF_VERSION "${LIXEL_PROTOBUF_PINNED_VERSION}")
  set(LIXEL_PROTOBUF_SOURCE "fetched ${LIXEL_PROTOBUF_PINNED_TAG}")
else()
  set(LIXEL_PROTOBUF_VERSION "${Protobuf_VERSION}")
  set(LIXEL_PROTOBUF_SOURCE "system")
endif()

if(NOT TARGET protobuf::libprotobuf OR NOT TARGET protobuf::protoc)
  message(FATAL_ERROR "protobuf::libprotobuf / protobuf::protoc targets are missing")
endif()

# proto3 'optional' is a stable feature from protobuf 3.15; 3.12 to 3.14 need an
# experimental switch to compile the definition.
if(LIXEL_PROTOBUF_VERSION VERSION_LESS "3.15")
  set(LIXEL_PROTOC_NEEDS_PROTO3_OPTIONAL_FLAG ON)
else()
  set(LIXEL_PROTOC_NEEDS_PROTO3_OPTIONAL_FLAG OFF)
endif()

# ---------------------------------------------------------------------------
# paho.mqtt.c (asynchronous client with TLS)
# ---------------------------------------------------------------------------
set(LIXEL_PAHO_PINNED_TAG "v1.3.13")
set(LIXEL_OPENAPI_PAHO_PROVIDER "FETCH" CACHE STRING
    "paho.mqtt.c source: FETCH (pinned ${LIXEL_PAHO_PINNED_TAG}) or SYSTEM (libpaho-mqtt-dev)")
set_property(CACHE LIXEL_OPENAPI_PAHO_PROVIDER PROPERTY STRINGS FETCH SYSTEM)

add_library(lixel_paho INTERFACE)
add_library(lixel::paho ALIAS lixel_paho)

if(LIXEL_OPENAPI_PAHO_PROVIDER STREQUAL "FETCH")
  set(PAHO_WITH_SSL ON CACHE BOOL "" FORCE)
  set(PAHO_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(PAHO_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(PAHO_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
  set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
  set(PAHO_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(PAHO_ENABLE_CPACK OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(paho_mqtt_c
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.c.git
    GIT_TAG ${LIXEL_PAHO_PINNED_TAG}
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(paho_mqtt_c)
  target_link_libraries(lixel_paho INTERFACE paho-mqtt3as-static)
  target_include_directories(lixel_paho INTERFACE "${paho_mqtt_c_SOURCE_DIR}/src")
  set(LIXEL_PAHO_SOURCE "fetched ${LIXEL_PAHO_PINNED_TAG}")
elseif(LIXEL_OPENAPI_PAHO_PROVIDER STREQUAL "SYSTEM")
  find_path(LIXEL_PAHO_INCLUDE_DIR MQTTAsync.h REQUIRED)
  find_library(LIXEL_PAHO_LIBRARY paho-mqtt3as REQUIRED)
  target_link_libraries(lixel_paho INTERFACE "${LIXEL_PAHO_LIBRARY}")
  target_include_directories(lixel_paho INTERFACE "${LIXEL_PAHO_INCLUDE_DIR}")
  set(LIXEL_PAHO_SOURCE "system")
else()
  message(FATAL_ERROR "LIXEL_OPENAPI_PAHO_PROVIDER must be FETCH or SYSTEM")
endif()
target_link_libraries(lixel_paho INTERFACE OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})
