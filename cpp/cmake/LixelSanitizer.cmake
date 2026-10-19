# Warning and sanitizer helpers. They apply to our own targets only, never to
# fetched third-party code or generated protobuf sources.

function(lixel_enable_warnings target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

function(lixel_apply_sanitizer target)
  if(LIXEL_OPENAPI_SANITIZER STREQUAL "")
    return()
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "LIXEL_OPENAPI_SANITIZER needs GCC or Clang")
  endif()
  if(NOT LIXEL_OPENAPI_SANITIZER MATCHES "^(address|thread|undefined)$")
    message(FATAL_ERROR "LIXEL_OPENAPI_SANITIZER must be address, thread or undefined")
  endif()
  target_compile_options(${target} PUBLIC -fsanitize=${LIXEL_OPENAPI_SANITIZER} -fno-omit-frame-pointer)
  target_link_options(${target} PUBLIC -fsanitize=${LIXEL_OPENAPI_SANITIZER})
endfunction()
