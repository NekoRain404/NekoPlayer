add_executable(media_source_test media_source_test.cpp)
target_link_libraries(media_source_test PRIVATE neko_playback Qt6::Test)
add_test(NAME media-source COMMAND media_source_test)
set_tests_properties(media-source PROPERTIES TIMEOUT 20)

add_executable(network_playback_test network_playback_test.cpp)
target_link_libraries(network_playback_test PRIVATE neko_session Qt6::Test Qt6::Network)
add_test(NAME network-playback COMMAND network_playback_test)
set_tests_properties(network-playback PROPERTIES TIMEOUT 60)

add_executable(m3u8_store_test m3u8_store_test.cpp ../src/playlist/m3u8store.cpp)
target_include_directories(m3u8_store_test PRIVATE ../src)
target_link_libraries(m3u8_store_test PRIVATE neko_playback Qt6::Test)
add_test(NAME m3u8-store COMMAND m3u8_store_test)
set_tests_properties(m3u8-store PROPERTIES TIMEOUT 20)

add_executable(audio_device_test audio_device_test.cpp ../src/playback/audiodevice.cpp)
target_include_directories(audio_device_test PRIVATE ../src)
target_link_libraries(audio_device_test PRIVATE Qt6::Core Qt6::Test neko_options)
add_test(NAME audio-device COMMAND audio_device_test)
set_tests_properties(audio-device PROPERTIES TIMEOUT 20)

add_executable(thumbnail_decoder_test thumbnail_decoder_test.cpp)
target_link_libraries(thumbnail_decoder_test PRIVATE neko_preview Qt6::Test)
target_compile_definitions(thumbnail_decoder_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}"
    FFMPEG_EXECUTABLE="${FFMPEG_EXECUTABLE}")
add_dependencies(thumbnail_decoder_test test_media)
add_test(NAME thumbnail-decoder COMMAND thumbnail_decoder_test)
set_tests_properties(thumbnail-decoder PROPERTIES TIMEOUT 30)

add_executable(thumbnail_service_test thumbnail_service_test.cpp)
target_link_libraries(thumbnail_service_test PRIVATE neko_preview Qt6::Test)
target_compile_definitions(thumbnail_service_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(thumbnail_service_test test_media)
add_test(NAME thumbnail-service COMMAND thumbnail_service_test)
set_tests_properties(thumbnail-service PROPERTIES TIMEOUT 30)

target_compile_definitions(network_playback_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(network_playback_test test_media)

add_executable(screenshot_test screenshot_test.cpp)
target_link_libraries(screenshot_test PRIVATE neko_playback Qt6::Test Qt6::Gui)
target_compile_definitions(screenshot_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}"
    TEST_AUDIO_ONLY_PATH="${TEST_AUDIO_ONLY}")
add_dependencies(screenshot_test test_media test_audio_only)
add_test(NAME screenshot COMMAND screenshot_test)
set_tests_properties(screenshot PROPERTIES TIMEOUT 60)

add_executable(media_information_test media_information_test.cpp)
target_link_libraries(media_information_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(media_information_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}"
    TEST_AUDIO_ONLY_PATH="${TEST_AUDIO_ONLY}" TEST_HDR_MEDIA_PATH="${TEST_HDR_MEDIA}")
add_dependencies(media_information_test test_media test_audio_only test_hdr_media)
add_test(NAME media-information COMMAND media_information_test)
set_tests_properties(media-information PROPERTIES TIMEOUT 60)

add_executable(frame_step_test frame_step_test.cpp)
target_link_libraries(frame_step_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(frame_step_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}"
    TEST_AUDIO_ONLY_PATH="${TEST_AUDIO_ONLY}" TEST_STILL_IMAGE_PATH="${TEST_STILL_IMAGE}")
add_dependencies(frame_step_test test_media test_audio_only test_still_image)
add_test(NAME frame-step COMMAND frame_step_test)
set_tests_properties(frame-step PROPERTIES TIMEOUT 60)

add_executable(ab_loop_test ab_loop_test.cpp)
target_link_libraries(ab_loop_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(ab_loop_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(ab_loop_test test_media)
add_test(NAME ab-loop COMMAND ab_loop_test)
set_tests_properties(ab-loop PROPERTIES TIMEOUT 60)

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

add_executable(playback_test playback_test.cpp)
target_link_libraries(playback_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(playback_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(playback_test test_media)
add_test(NAME playback COMMAND playback_test)
set_tests_properties(playback PROPERTIES TIMEOUT 60)

add_executable(mpv_open_failure_test mpv_open_failure_test.cpp)
target_link_libraries(mpv_open_failure_test PRIVATE neko_playback Qt6::Test)
add_test(NAME mpv-open-failure COMMAND mpv_open_failure_test)
set_tests_properties(mpv-open-failure PROPERTIES TIMEOUT 20)

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

add_executable(playlist_edit_test playlist_edit_test.cpp)
target_link_libraries(playlist_edit_test PRIVATE neko_session Qt6::Test Qt6::Gui)
target_compile_definitions(playlist_edit_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(playlist_edit_test test_media)
add_test(NAME playlist-edit COMMAND playlist_edit_test)
set_tests_properties(playlist-edit PROPERTIES TIMEOUT 60)

add_executable(playlist_modes_test playlist_modes_test.cpp)
target_link_libraries(playlist_modes_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(playlist_modes_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(playlist_modes_test test_media)
add_test(NAME playlist-modes COMMAND playlist_modes_test)
set_tests_properties(playlist-modes PROPERTIES TIMEOUT 60)

add_executable(playlist_shuffle_test playlist_shuffle_test.cpp)
target_link_libraries(playlist_shuffle_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(playlist_shuffle_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(playlist_shuffle_test test_media)
add_test(NAME playlist-shuffle COMMAND playlist_shuffle_test)
set_tests_properties(playlist-shuffle PROPERTIES TIMEOUT 60)

add_executable(external_audio_test external_audio_test.cpp)
target_link_libraries(external_audio_test PRIVATE neko_playback Qt6::Test)
add_dependencies(external_audio_test test_track_media test_external_audio test_video_only)
add_test(NAME external-audio COMMAND external_audio_test
    "${TEST_TRACK_MEDIA}" "${TEST_EXTERNAL_AUDIO}" "${TEST_VIDEO_ONLY}")
set_tests_properties(external-audio PROPERTIES TIMEOUT 60)

target_compile_definitions(screenshot_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}")
add_dependencies(screenshot_test test_track_media)

target_compile_definitions(playlist_modes_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(playlist_modes_test test_track_media)

add_executable(tracks_test tracks_test.cpp)
target_link_libraries(tracks_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(tracks_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_MULTI_SUBTITLE_PATH="${TEST_MULTI_SUBTITLE}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(tracks_test test_track_media)
add_dependencies(tracks_test test_multi_subtitle)
add_test(NAME tracks COMMAND tracks_test)
set_tests_properties(tracks PROPERTIES TIMEOUT 60)

add_executable(file_drop_test file_drop_test.cpp)
target_link_libraries(file_drop_test PRIVATE neko_input Qt6::Test)
target_compile_definitions(file_drop_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(file_drop_test test_track_media)
add_test(NAME file-drop COMMAND file_drop_test)
set_tests_properties(file-drop PROPERTIES TIMEOUT 60)

add_executable(video_presentation_test video_presentation_test.cpp)
target_link_libraries(video_presentation_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(video_presentation_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(video_presentation_test test_track_media)
add_test(NAME video-presentation COMMAND video_presentation_test)
set_tests_properties(video-presentation PROPERTIES TIMEOUT 60)

add_executable(video_framing_test video_framing_test.cpp)
target_link_libraries(video_framing_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(video_framing_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(video_framing_test test_track_media)
add_test(NAME video-framing COMMAND video_framing_test)
set_tests_properties(video-framing PROPERTIES TIMEOUT 60)
