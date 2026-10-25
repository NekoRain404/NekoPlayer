find_package(Qt6 6.5 REQUIRED COMPONENTS Test Network)
find_program(FFMPEG_EXECUTABLE ffmpeg REQUIRED)

include(test_media_fixtures.cmake)
include(folder_scanning_tests.cmake)
include(session_tests.cmake)
include(ui_tests.cmake)
include(playback_tests.cmake)
include(window_tests.cmake)
