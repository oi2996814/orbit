# Copyright (c) 2022 The Orbit Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

find_package(absl CONFIG)

if(absl_FOUND)
  return()
endif()

message("Abseil not found via CMake. Probably not a Conan build. Using the copy from third_party/")

set(ABSL_PROPAGATE_CXX_STD ON)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/abseil-cpp)

if(NOT MSVC)
  target_compile_options(absl_exponential_biased PRIVATE
    -Wno-error=float-conversion
  )

  target_compile_options(absl_log_internal_conditions PRIVATE
    -Wno-error=float-conversion
  )

  target_compile_options(absl_malloc_internal PRIVATE
    -Wno-error=old-style-cast
  )

  target_compile_options(absl_time_zone PRIVATE
    -Wno-error=format-nonliteral
  )

  target_compile_options(absl_stacktrace PRIVATE
    -Wno-error=old-style-cast
    -Wno-error=format-nonliteral
  )

  target_compile_options(absl_strings PRIVATE
    -Wno-error=float-conversion
  )

  target_compile_options(absl_str_format_internal PRIVATE
    -Wno-error=float-conversion
    -Wno-error=format-nonliteral
  )

  target_compile_options(absl_time PRIVATE
    -Wno-error=float-conversion
  )

  target_compile_options(absl_symbolize PRIVATE
    -Wno-error=old-style-cast
  )
endif()
