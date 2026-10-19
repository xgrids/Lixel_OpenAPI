# The command line examples. Each one is the C++ counterpart of a script in
# python/example/usage/ and has the same name and options.
add_library(lixel_examples_common STATIC common.cpp)
target_link_libraries(lixel_examples_common PUBLIC lixel_openapi)
lixel_enable_warnings(lixel_examples_common)
lixel_apply_sanitizer(lixel_examples_common)

set(LIXEL_EXAMPLE_NAMES
  control_point
  data_unpack
  device_config
  device_control
  device_info
  device_status
  get_ntrip_profile
  get_rtk_advance
  heartbeat
  lio_pcl
  lio_pose
  measure_point
  modeling
  modeling_status
  project_list
  set_ntrip_profile
  set_rtk_advance
  system_error
)
foreach(name IN LISTS LIXEL_EXAMPLE_NAMES)
  # The target carries a prefix so it cannot clash with another project's target (for example
  # "modeling"); the program is still called <name>.
  add_executable(lixel_example_${name} ${name}.cpp)
  target_link_libraries(lixel_example_${name} PRIVATE lixel_examples_common)
  set_target_properties(lixel_example_${name} PROPERTIES OUTPUT_NAME ${name})
  lixel_enable_warnings(lixel_example_${name})
  lixel_apply_sanitizer(lixel_example_${name})
endforeach()
set(LIXEL_EXAMPLE_NAMES "${LIXEL_EXAMPLE_NAMES}" PARENT_SCOPE)
