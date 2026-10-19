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

# Python 3 (standard library only) is used by the catalog drift check and the broker script.
find_package(Python3 COMPONENTS Interpreter QUIET)

function(lixel_add_test name)
  cmake_parse_arguments(T "NO_DISCOVERY" "" "" ${ARGN})
  add_executable(${name} ${T_UNPARSED_ARGUMENTS})
  target_link_libraries(${name} PRIVATE lixel_openapi GTest::gtest GTest::gtest_main)
  lixel_enable_warnings(${name})
  lixel_apply_sanitizer(${name})
  if(T_NO_DISCOVERY)
    return()
  endif()
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

# Regression: a program that uses message names only must find the messages.
lixel_add_test(test_codec_standalone test_codec_standalone.cpp)

# T3/T4: request/response, subscriptions, reconnect, against a fake transport.
lixel_add_test(test_client test_client.cpp)
lixel_add_test(test_client_stress test_client_stress.cpp)

# T5: point cloud decoding (LZ4 block + LioPclReport).
lixel_add_test(test_pointcloud test_pointcloud.cpp)
target_link_libraries(test_pointcloud PRIVATE PkgConfig::LZ4)

# T6: the scan flow against a scripted device.
lixel_add_test(test_scan test_scan.cpp)

# The paho transport against a real Mosquitto broker, plain and TLS. The test program skips
# itself when it is started without a broker, so it can always be built and run by hand; ctest
# runs it through a script that starts a throw-away broker (one ctest entry, broker_integration,
# not one per case, because without a broker every case would show up as skipped). The ctest entry exists only when
# mosquitto and openssl are installed.
lixel_add_test(test_broker test_broker.cpp NO_DISCOVERY)
find_program(LIXEL_MOSQUITTO mosquitto)
find_program(LIXEL_MOSQUITTO_PASSWD mosquitto_passwd)
find_program(LIXEL_OPENSSL openssl)
if(LIXEL_MOSQUITTO AND LIXEL_MOSQUITTO_PASSWD AND LIXEL_OPENSSL AND Python3_Interpreter_FOUND)
  add_test(NAME broker_integration
           COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/broker/run_with_broker.sh $<TARGET_FILE:test_broker>)
  set_tests_properties(broker_integration PROPERTIES TIMEOUT 120)
else()
  message(STATUS "mosquitto, mosquitto_passwd, openssl or Python 3 not found: broker_integration is not registered")
endif()

# End to end: the 18 examples against a fake device played by the Python client side of this
# repository. Needs a Python that has paho-mqtt, protobuf and lz4 (pip install -r
# python/requirements.txt lz4); give it with -DLIXEL_E2E_PYTHON=/path/to/python. Without it the
# test is not registered.
set(LIXEL_E2E_PYTHON "" CACHE FILEPATH "Python with paho-mqtt, protobuf and lz4, for the examples_e2e test")
if(LIXEL_E2E_PYTHON AND LIXEL_OPENAPI_BUILD_EXAMPLES AND LIXEL_MOSQUITTO AND LIXEL_MOSQUITTO_PASSWD AND LIXEL_OPENSSL AND Python3_Interpreter_FOUND)
  add_test(NAME examples_e2e
           COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/broker/run_with_broker.sh ${LIXEL_E2E_PYTHON}
                   ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_examples.py
                   --examples-dir ${CMAKE_BINARY_DIR}/examples
                   --python-dir ${CMAKE_CURRENT_SOURCE_DIR}/../../python)
  set_tests_properties(examples_e2e PROPERTIES TIMEOUT 240 ENVIRONMENT "PYTHONPATH=")
else()
  message(STATUS "LIXEL_E2E_PYTHON is not set (or the broker tools are missing): examples_e2e is not registered")
endif()

# The C++ interface table must not drift from the Python catalog. Needs only Python 3 with the
# standard library.
if(Python3_Interpreter_FOUND)
  add_test(NAME catalog_drift
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/catalog_tool.py check)
else()
  message(STATUS "Python 3 not found: the catalog_drift test is not registered")
endif()
