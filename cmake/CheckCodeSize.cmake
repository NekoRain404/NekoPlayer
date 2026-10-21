file(GLOB_RECURSE sources
    "${NEKO_SOURCE_DIR}/src/*.cpp" "${NEKO_SOURCE_DIR}/src/*.h"
    "${NEKO_SOURCE_DIR}/tests/*.cpp" "${NEKO_SOURCE_DIR}/tests/*.h"
    "${NEKO_SOURCE_DIR}/tests/*.sh" "${NEKO_SOURCE_DIR}/cmake/*.cmake")
list(APPEND sources "${NEKO_SOURCE_DIR}/CMakeLists.txt"
    "${NEKO_SOURCE_DIR}/tests/CMakeLists.txt")
set(default_line_limit 500)
set(special_line_limit 800)
# These integration/coordinator files may legitimately hold several tightly
# coupled desktop scenarios. New exceptions require an explicit entry and reason.
set(special_sources "${NEKO_SOURCE_DIR}/src/ui/mainwindow.cpp"
    "${NEKO_SOURCE_DIR}/tests/window_test.cpp")
foreach(source IN LISTS sources)
    file(READ "${source}" contents)
    string(REGEX REPLACE "[^\n]" "" newlines "${contents}")
    string(LENGTH "${newlines}" line_count)
    if(NOT contents STREQUAL "" AND NOT contents MATCHES "\n$")
        math(EXPR line_count "${line_count} + 1")
    endif()
    set(line_limit ${default_line_limit})
    list(FIND special_sources "${source}" special_index)
    if(NOT special_index EQUAL -1)
        set(line_limit ${special_line_limit})
    endif()
    if(line_count GREATER line_limit)
        message(FATAL_ERROR "${source}: ${line_count} lines exceeds the ${line_limit}-line limit")
    endif()
endforeach()
message(STATUS "All ordinary handwritten code files satisfy the 500-line limit; listed special files satisfy the 800-line limit")
