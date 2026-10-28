find_package(Qt6 6.5 REQUIRED COMPONENTS Test Network)
find_program(FFMPEG_EXECUTABLE ffmpeg REQUIRED)

include(test_media_fixtures.cmake)
include(folder_scanning_tests.cmake)
include(session_tests.cmake)
include(ui_tests.cmake)
include(playback_tests.cmake)
include(window_tests.cmake)

# libmpv requires the C numeric locale (the application does the same after Qt
# initialization). Apply it to every registered test so `ctest --preset debug`
# works on desktops whose LC_NUMERIC is not "C".
get_property(neko_registered_tests DIRECTORY PROPERTY TESTS)
foreach(neko_test IN LISTS neko_registered_tests)
    set_property(TEST "${neko_test}" APPEND PROPERTY ENVIRONMENT "LC_ALL=C" "LANG=C")
endforeach()
