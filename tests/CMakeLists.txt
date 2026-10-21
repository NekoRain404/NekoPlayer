find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
find_program(FFMPEG_EXECUTABLE ffmpeg REQUIRED)

set(TEST_MEDIA "${CMAKE_CURRENT_BINARY_DIR}/media/猫 sample [1].mkv")
add_custom_command(OUTPUT "${TEST_MEDIA}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/media"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -f lavfi -i testsrc2=size=320x180:rate=25
        -f lavfi -i sine=frequency=440:sample_rate=48000
        -t 4 -c:v mpeg4 -q:v 4 -c:a pcm_s16le "${TEST_MEDIA}"
    VERBATIM)
add_custom_target(test_media DEPENDS "${TEST_MEDIA}")
set(TEST_VIDEO_ONLY "${CMAKE_CURRENT_BINARY_DIR}/media/video-only.mkv")
add_custom_command(OUTPUT "${TEST_VIDEO_ONLY}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${TEST_MEDIA}" -map 0:v -c copy "${TEST_VIDEO_ONLY}"
    DEPENDS "${TEST_MEDIA}" VERBATIM)
add_custom_target(test_video_only DEPENDS "${TEST_VIDEO_ONLY}")

add_executable(playback_test playback_test.cpp)
target_link_libraries(playback_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(playback_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(playback_test test_media)
add_test(NAME playback COMMAND playback_test)
set_tests_properties(playback PROPERTIES TIMEOUT 60)

add_executable(playlist_test playlist_test.cpp)
target_link_libraries(playlist_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(playlist_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(playlist_test test_media)
add_test(NAME playlist COMMAND playlist_test)
set_tests_properties(playlist PROPERTIES TIMEOUT 60)

set(NEKOPLAYER_WINDOW_TEST_BACKEND "wayland" CACHE STRING
    "Desktop backend for GUI integration tests (wayland or x11)")
set_property(CACHE NEKOPLAYER_WINDOW_TEST_BACKEND PROPERTY STRINGS wayland x11)
set(WINDOW_TEST_ENVIRONMENT
    "QT_QPA_PLATFORMTHEME=generic;QT_IM_MODULE=compose;QT_IM_MODULES=compose;MALLOC_PERTURB_=128")
if(NEKOPLAYER_WINDOW_TEST_BACKEND STREQUAL "wayland")
    set(WINDOW_TEST_RUNNER)
    list(APPEND WINDOW_TEST_ENVIRONMENT "QT_QPA_PLATFORM=wayland")
    if(NEKOPLAYER_SANITIZERS)
        # Qt Wayland leaves client-side proxy allocations until process exit;
        # keep ASan/UBSan active while excluding that external leak report.
        list(APPEND WINDOW_TEST_ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
    endif()
elseif(NEKOPLAYER_WINDOW_TEST_BACKEND STREQUAL "x11")
    find_program(XVFB_RUN_EXECUTABLE xvfb-run REQUIRED)
    find_program(OPENBOX_EXECUTABLE openbox REQUIRED)
    set(WINDOW_TEST_RUNNER ${XVFB_RUN_EXECUTABLE} -a -s "-screen 0 1280x900x24"
        sh "${CMAKE_CURRENT_SOURCE_DIR}/run-desktop.sh" ${OPENBOX_EXECUTABLE})
    list(APPEND WINDOW_TEST_ENVIRONMENT "QT_QPA_PLATFORM=xcb" "LIBGL_ALWAYS_SOFTWARE=1")
else()
    message(FATAL_ERROR "NEKOPLAYER_WINDOW_TEST_BACKEND must be wayland or x11")
endif()
add_executable(window_test window_test.cpp)
target_link_libraries(window_test PRIVATE neko_ui Qt6::Test)
target_compile_definitions(window_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
target_compile_definitions(window_test PRIVATE PLAYER_EXECUTABLE="$<TARGET_FILE:NekoPlayer>")
target_compile_definitions(window_test PRIVATE TEST_VIDEO_ONLY_PATH="${TEST_VIDEO_ONLY}")
add_dependencies(window_test test_video_only)
add_dependencies(window_test NekoPlayer)
add_dependencies(window_test test_media)
add_test(NAME window COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:window_test>)
set_tests_properties(window PROPERTIES TIMEOUT 90 ENVIRONMENT "${WINDOW_TEST_ENVIRONMENT}")
add_test(NAME window-startup COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:NekoPlayer>
    --silent-audio --smoke-test)
add_test(NAME file-startup COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:NekoPlayer>
    --silent-audio --smoke-test "${TEST_MEDIA}")
set_tests_properties(window-startup file-startup PROPERTIES TIMEOUT 20 ENVIRONMENT
    "${WINDOW_TEST_ENVIRONMENT}")
