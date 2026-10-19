# Generates the C++ protobuf sources from the shared definition at build time.
#
# Generated code is never checked in: it is tied to the protobuf runtime it is
# built against, and a mismatch fails at compile or link time.

function(lixel_generate_proto target)
  set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
  set(out_cc "${gen_dir}/openapi/aiot_msgs.pb.cc")
  set(out_h "${gen_dir}/openapi/aiot_msgs.pb.h")

  set(extra_flags)
  if(LIXEL_PROTOC_NEEDS_PROTO3_OPTIONAL_FLAG)
    list(APPEND extra_flags --experimental_allow_proto3_optional)
  endif()

  add_custom_command(
    OUTPUT "${out_cc}" "${out_h}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${gen_dir}"
    COMMAND protobuf::protoc ${extra_flags}
            -I "${LIXEL_OPENAPI_PROTO_ROOT}"
            --cpp_out=${gen_dir}
            "${LIXEL_OPENAPI_PROTO_FILE}"
    DEPENDS "${LIXEL_OPENAPI_PROTO_FILE}" protobuf::protoc
    COMMENT "Generating C++ from aiot_msgs.proto with protobuf ${LIXEL_PROTOBUF_VERSION}"
    VERBATIM)

  add_library(${target} STATIC "${out_cc}")
  target_include_directories(${target} SYSTEM PUBLIC $<BUILD_INTERFACE:${gen_dir}>)
  target_link_libraries(${target} PUBLIC protobuf::libprotobuf)
  # Generated code is not ours to fix; keep its warnings out of the build log.
  target_compile_options(${target} PRIVATE -w)
endfunction()
