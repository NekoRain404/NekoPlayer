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
add_executable(decoding_test decoding_test.cpp)
target_link_libraries(decoding_test PRIVATE neko_session Qt6::Test)
target_compile_definitions(decoding_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(decoding_test test_media)
add_test(NAME decoding COMMAND decoding_test)
set_tests_properties(decoding PROPERTIES TIMEOUT 60)
set(TEST_CHAPTER_MEDIA "${CMAKE_CURRENT_BINARY_DIR}/media/chapters.mkv")
add_custom_command(OUTPUT "${TEST_CHAPTER_MEDIA}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${TEST_MEDIA}" -i "${CMAKE_CURRENT_SOURCE_DIR}/data/chapters.ffmetadata"
        -map 0 -map_metadata 1 -map_chapters 1 -c copy "${TEST_CHAPTER_MEDIA}"
    DEPENDS "${TEST_MEDIA}" "${CMAKE_CURRENT_SOURCE_DIR}/data/chapters.ffmetadata" VERBATIM)
add_custom_target(test_chapter_media DEPENDS "${TEST_CHAPTER_MEDIA}")
set(TEST_OVERLAPPING_CHAPTER_MEDIA "${CMAKE_CURRENT_BINARY_DIR}/media/overlapping-chapters.mkv")
add_custom_command(OUTPUT "${TEST_OVERLAPPING_CHAPTER_MEDIA}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${TEST_MEDIA}" -i "${CMAKE_CURRENT_SOURCE_DIR}/data/overlapping-chapters.ffmetadata"
        -map 0 -map_metadata 1 -map_chapters 1 -c copy "${TEST_OVERLAPPING_CHAPTER_MEDIA}"
    DEPENDS "${TEST_MEDIA}" "${CMAKE_CURRENT_SOURCE_DIR}/data/overlapping-chapters.ffmetadata"
    VERBATIM)
add_custom_target(test_overlapping_chapter_media DEPENDS "${TEST_OVERLAPPING_CHAPTER_MEDIA}")
add_executable(navigation_test navigation_test.cpp)
target_link_libraries(navigation_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(navigation_test PRIVATE TEST_CHAPTER_MEDIA_PATH="${TEST_CHAPTER_MEDIA}"
    TEST_MEDIA_PATH="${TEST_MEDIA}"
    TEST_OVERLAPPING_CHAPTER_MEDIA_PATH="${TEST_OVERLAPPING_CHAPTER_MEDIA}")
add_dependencies(navigation_test test_chapter_media test_overlapping_chapter_media)
add_test(NAME navigation COMMAND navigation_test)
set_tests_properties(navigation PROPERTIES TIMEOUT 60)
add_executable(playback_time_test playback_time_test.cpp)
target_link_libraries(playback_time_test PRIVATE neko_playback Qt6::Test)
add_test(NAME playback-time COMMAND playback_time_test)
set_tests_properties(playback-time PROPERTIES TIMEOUT 20)
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

add_executable(tuning_test tuning_test.cpp)
target_link_libraries(tuning_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(tuning_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(tuning_test test_media)
add_test(NAME tuning COMMAND tuning_test)
set_tests_properties(tuning PROPERTIES TIMEOUT 60)

add_executable(playlist_test playlist_test.cpp)
target_link_libraries(playlist_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(playlist_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(playlist_test test_media)
add_test(NAME playlist COMMAND playlist_test)
set_tests_properties(playlist PROPERTIES TIMEOUT 60)

add_executable(playlist_modes_test playlist_modes_test.cpp)
target_link_libraries(playlist_modes_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(playlist_modes_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(playlist_modes_test test_media)
add_test(NAME playlist-modes COMMAND playlist_modes_test)
set_tests_properties(playlist-modes PROPERTIES TIMEOUT 60)

add_executable(session_test session_test.cpp)
target_link_libraries(session_test PRIVATE neko_session Qt6::Test)
target_compile_definitions(session_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(session_test test_media)
add_test(NAME session COMMAND session_test)
set_tests_properties(session PROPERTIES TIMEOUT 60)

set(TEST_TRACK_MEDIA "${CMAKE_CURRENT_BINARY_DIR}/media/multiple-tracks.mkv")
add_custom_command(OUTPUT "${TEST_TRACK_MEDIA}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${TEST_MEDIA}" -f lavfi -i sine=frequency=880:sample_rate=48000
        -i "${CMAKE_CURRENT_SOURCE_DIR}/data/embedded.srt"
        -map 0:v -map 0:a -map 1:a -map 2:s -t 4 -c:v copy -c:a pcm_s16le -c:s srt
        -metadata:s:a:0 language=jpn -metadata:s:a:0 title=Original
        -metadata:s:a:1 language=eng -metadata:s:a:1 title=English
        -metadata:s:s:0 language=zho -metadata:s:s:0 "title=简体中文"
        "${TEST_TRACK_MEDIA}"
    DEPENDS "${TEST_MEDIA}" "${CMAKE_CURRENT_SOURCE_DIR}/data/embedded.srt" VERBATIM)
add_custom_target(test_track_media DEPENDS "${TEST_TRACK_MEDIA}")
target_compile_definitions(playlist_modes_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(playlist_modes_test test_track_media)
set(TEST_MULTI_SUBTITLE "${CMAKE_CURRENT_BINARY_DIR}/media/multiple-subtitles.mks")
add_custom_command(OUTPUT "${TEST_MULTI_SUBTITLE}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${CMAKE_CURRENT_SOURCE_DIR}/data/embedded.srt"
        -i "${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt"
        -map 0:s -map 1:s -c:s srt -f matroska "${TEST_MULTI_SUBTITLE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/embedded.srt"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt" test_track_media VERBATIM)
add_custom_target(test_multi_subtitle DEPENDS "${TEST_MULTI_SUBTITLE}")
add_executable(tracks_test tracks_test.cpp)
target_link_libraries(tracks_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(tracks_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_MULTI_SUBTITLE_PATH="${TEST_MULTI_SUBTITLE}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(tracks_test test_track_media)
add_dependencies(tracks_test test_multi_subtitle)
add_test(NAME tracks COMMAND tracks_test)
set_tests_properties(tracks PROPERTIES TIMEOUT 60)

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
target_compile_definitions(window_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}")
target_compile_definitions(window_test PRIVATE TEST_CHAPTER_MEDIA_PATH="${TEST_CHAPTER_MEDIA}")
add_dependencies(window_test test_chapter_media)
add_dependencies(window_test test_track_media)
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
