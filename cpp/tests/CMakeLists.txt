find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(STATUS "System GoogleTest not found; fetching v1.14.0")
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(googletest)
endif()
include(GoogleTest)

function(lixel_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE lixel_openapi GTest::gtest GTest::gtest_main)
  lixel_enable_warnings(${name})
  lixel_apply_sanitizer(${name})
  # Discover the test cases when ctest runs, not while building. Running the test
  # program at build time fails whenever it needs a special runtime environment
  # (for example a sanitizer runtime that must be preloaded) or when cross-compiling.
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.18)
    gtest_discover_tests(${name} DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 30)
  else()
    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 30)
  endif()
endfunction()

# T1: the build pipeline works end to end (protobuf pin, generated code, wire
# compatibility with the Python client, paho and lz4 link).
lixel_add_test(test_smoke test_smoke.cpp)
target_compile_definitions(test_smoke PRIVATE
  LIXEL_EXPECTED_PROTOBUF_VERSION="${LIXEL_PROTOBUF_VERSION}"
  LIXEL_EXPECTED_VERSION="${PROJECT_VERSION}")
target_link_libraries(test_smoke PRIVATE lixel::paho PkgConfig::LZ4)

# T2: interface table, JSON codec, status codes.
lixel_add_test(test_catalog_codec test_catalog_codec.cpp)

# The C++ interface table must not drift from the Python catalog. Needs only Python 3 with the
# standard library.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME catalog_drift
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/catalog_tool.py check)
else()
  message(STATUS "Python 3 not found: the catalog_drift test is not registered")
endif()
