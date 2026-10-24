find_package(Qt6 6.5 REQUIRED COMPONENTS Test Network)
add_executable(seek_slider_geometry_test seek_slider_geometry_test.cpp)
target_include_directories(seek_slider_geometry_test PRIVATE ../src)
target_link_libraries(seek_slider_geometry_test PRIVATE Qt6::Widgets Qt6::Test)
add_test(NAME seek-slider-geometry COMMAND seek_slider_geometry_test)
set_tests_properties(seek-slider-geometry PROPERTIES TIMEOUT 20)

find_program(FFMPEG_EXECUTABLE ffmpeg REQUIRED)

add_executable(recent_media_history_test recent_media_history_test.cpp)
target_link_libraries(recent_media_history_test PRIVATE neko_session Qt6::Test)
add_test(NAME recent-media-history COMMAND recent_media_history_test)
set_tests_properties(recent-media-history PROPERTIES TIMEOUT 20)

include(folder_scanning_tests.cmake)
add_executable(media_source_test media_source_test.cpp)
target_link_libraries(media_source_test PRIVATE neko_playback Qt6::Test)
add_test(NAME media-source COMMAND media_source_test)
set_tests_properties(media-source PROPERTIES TIMEOUT 20)

add_executable(network_session_test network_session_test.cpp)
target_link_libraries(network_session_test PRIVATE neko_session Qt6::Test)
add_test(NAME network-session COMMAND network_session_test)
set_tests_properties(network-session PROPERTIES TIMEOUT 30)

add_executable(network_playback_test network_playback_test.cpp)
target_link_libraries(network_playback_test PRIVATE neko_session Qt6::Test Qt6::Network)
add_test(NAME network-playback COMMAND network_playback_test)
set_tests_properties(network-playback PROPERTIES TIMEOUT 60)

add_executable(preferences_dialog_test preferences_dialog_test.cpp)
target_link_libraries(preferences_dialog_test PRIVATE neko_ui Qt6::Test)
add_test(NAME preferences-dialog COMMAND preferences_dialog_test)
set_tests_properties(preferences-dialog PROPERTIES TIMEOUT 20)

add_executable(preferences_coordinator_test preferences_coordinator_test.cpp)
target_link_libraries(preferences_coordinator_test PRIVATE neko_ui Qt6::Test)
add_test(NAME preferences-coordinator COMMAND preferences_coordinator_test)
set_tests_properties(preferences-coordinator PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(appearance_preferences_test appearance_preferences_test.cpp)
target_link_libraries(appearance_preferences_test PRIVATE neko_ui Qt6::Test)
qt_add_translations(appearance_preferences_test TS_FILES ../translations/NekoPlayer_en.ts
    RESOURCE_PREFIX "/i18n")
add_test(NAME appearance-preferences COMMAND appearance_preferences_test)
set_tests_properties(appearance-preferences PROPERTIES TIMEOUT 20)

add_executable(window_title_test window_title_test.cpp ../src/ui/windowtitle.cpp)
target_include_directories(window_title_test PRIVATE ../src)
target_link_libraries(window_title_test PRIVATE neko_playback Qt6::Test)
add_test(NAME title-format COMMAND window_title_test)
set_tests_properties(title-format PROPERTIES TIMEOUT 20)

add_executable(interaction_policy_test interaction_policy_test.cpp
    ../src/ui/interactionpolicy.cpp)
target_include_directories(interaction_policy_test PRIVATE ../src)
target_link_libraries(interaction_policy_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME interaction-policy COMMAND interaction_policy_test)
set_tests_properties(interaction-policy PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(transport_bar_test transport_bar_test.cpp)
target_link_libraries(transport_bar_test PRIVATE neko_ui Qt6::Test)
add_test(NAME transport-bar COMMAND transport_bar_test)
set_tests_properties(transport-bar PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(ui_surface_test ui_surface_test.cpp)
target_link_libraries(ui_surface_test PRIVATE neko_ui Qt6::Test)
qt_add_translations(ui_surface_test TS_FILES ../translations/NekoPlayer_en.ts
    RESOURCE_PREFIX "/i18n")
add_test(NAME ui-surface COMMAND ui_surface_test)
set_tests_properties(ui-surface PROPERTIES TIMEOUT 20 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(languagemanager_test languagemanager_test.cpp ../src/ui/languagemanager.cpp)
target_include_directories(languagemanager_test PRIVATE ../src)
target_link_libraries(languagemanager_test PRIVATE Qt6::Widgets Qt6::Test neko_options)
qt_add_translations(languagemanager_test TS_FILES ../translations/NekoPlayer_en.ts
    RESOURCE_PREFIX "/i18n")
add_test(NAME language-manager COMMAND languagemanager_test)
set_tests_properties(language-manager PROPERTIES TIMEOUT 20)
set_tests_properties(language-manager PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(commandlinearguments_test commandlinearguments_test.cpp)
target_include_directories(commandlinearguments_test PRIVATE ../src)
target_link_libraries(commandlinearguments_test PRIVATE neko_options Qt6::Core Qt6::Test)
add_test(NAME command-line-arguments COMMAND commandlinearguments_test)
set_tests_properties(command-line-arguments PROPERTIES TIMEOUT 20)

add_executable(action_feedback_test action_feedback_test.cpp ../src/ui/actionfeedbacktext.cpp)
target_include_directories(action_feedback_test PRIVATE ../src)
target_link_libraries(action_feedback_test PRIVATE neko_options Qt6::Core Qt6::Test)
add_test(NAME action-feedback COMMAND action_feedback_test)
set_tests_properties(action-feedback PROPERTIES TIMEOUT 20)

add_executable(playback_overlay_text_test playback_overlay_text_test.cpp
    ../src/ui/playbackoverlaytext.cpp)
target_include_directories(playback_overlay_text_test PRIVATE ../src)
target_link_libraries(playback_overlay_text_test PRIVATE neko_playback Qt6::Test)
add_test(NAME playback-overlay-text COMMAND playback_overlay_text_test)
set_tests_properties(playback-overlay-text PROPERTIES TIMEOUT 20)

add_executable(playback_history_store_test playback_history_store_test.cpp
    ../src/session/playbackhistorystore.cpp)
target_include_directories(playback_history_store_test PRIVATE ../src)
target_link_libraries(playback_history_store_test PRIVATE Qt6::Core Qt6::Test neko_options)
add_test(NAME playback-history-store COMMAND playback_history_store_test)
set_tests_properties(playback-history-store PROPERTIES TIMEOUT 20)

add_executable(track_preference_store_test track_preference_store_test.cpp
    ../src/session/trackpreferencestore.cpp)
target_include_directories(track_preference_store_test PRIVATE ../src)
target_link_libraries(track_preference_store_test PRIVATE Qt6::Core Qt6::Test neko_options)
add_test(NAME track-preference-store COMMAND track_preference_store_test)
set_tests_properties(track-preference-store PROPERTIES TIMEOUT 20)

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

set(TEST_MEDIA "${CMAKE_CURRENT_BINARY_DIR}/media/猫 sample [1].mkv")
add_custom_command(OUTPUT "${TEST_MEDIA}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/media"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -f lavfi -i testsrc2=size=320x180:rate=25
        -f lavfi -i sine=frequency=440:sample_rate=48000
        -t 4 -c:v mpeg4 -q:v 4 -c:a pcm_s16le "${TEST_MEDIA}"
    VERBATIM)
add_custom_target(test_media DEPENDS "${TEST_MEDIA}")
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
add_executable(thumbnail_preview_test thumbnail_preview_test.cpp)
target_link_libraries(thumbnail_preview_test PRIVATE neko_ui Qt6::Test)
add_test(NAME thumbnail-preview COMMAND thumbnail_preview_test)
set_tests_properties(thumbnail-preview PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_executable(thumbnail_preview_policy_test thumbnail_preview_policy_test.cpp)
target_link_libraries(thumbnail_preview_policy_test PRIVATE neko_ui Qt6::Test)
add_test(NAME thumbnail-preview-policy COMMAND thumbnail_preview_policy_test)
target_compile_definitions(network_playback_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(network_playback_test test_media)
set(TEST_AUDIO_ONLY "${CMAKE_CURRENT_BINARY_DIR}/media/audio-only.mka")
add_custom_command(OUTPUT "${TEST_AUDIO_ONLY}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${TEST_MEDIA}" -map 0:a -c copy "${TEST_AUDIO_ONLY}"
    DEPENDS "${TEST_MEDIA}" VERBATIM)
add_custom_target(test_audio_only DEPENDS "${TEST_AUDIO_ONLY}")
set(TEST_HDR_MEDIA "${CMAKE_CURRENT_BINARY_DIR}/media/hdr-pq.mp4")
add_custom_command(OUTPUT "${TEST_HDR_MEDIA}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -f lavfi -i testsrc2=size=64x64:rate=1 -t 1 -vf format=yuv420p10le
        -c:v libx265 -x265-params log-level=error:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc
        "${TEST_HDR_MEDIA}"
    VERBATIM)
add_custom_target(test_hdr_media DEPENDS "${TEST_HDR_MEDIA}")
add_executable(screenshot_test screenshot_test.cpp)
target_link_libraries(screenshot_test PRIVATE neko_playback Qt6::Test Qt6::Gui)
target_compile_definitions(screenshot_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}"
    TEST_AUDIO_ONLY_PATH="${TEST_AUDIO_ONLY}")
add_dependencies(screenshot_test test_media test_audio_only)
add_test(NAME screenshot COMMAND screenshot_test)
set_tests_properties(screenshot PROPERTIES TIMEOUT 60)
set(TEST_STILL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/media/still.png")
add_custom_command(OUTPUT "${TEST_STILL_IMAGE}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${TEST_MEDIA}" -frames:v 1 -update 1 "${TEST_STILL_IMAGE}"
    DEPENDS "${TEST_MEDIA}" VERBATIM)
add_custom_target(test_still_image DEPENDS "${TEST_STILL_IMAGE}")
add_executable(media_information_test media_information_test.cpp)
target_link_libraries(media_information_test PRIVATE neko_playback Qt6::Test)
target_compile_definitions(media_information_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}"
    TEST_AUDIO_ONLY_PATH="${TEST_AUDIO_ONLY}" TEST_HDR_MEDIA_PATH="${TEST_HDR_MEDIA}")
add_dependencies(media_information_test test_media test_audio_only test_hdr_media)
add_test(NAME media-information COMMAND media_information_test)
set_tests_properties(media-information PROPERTIES TIMEOUT 60)
add_executable(media_report_test media_report_test.cpp)
target_link_libraries(media_report_test PRIVATE neko_ui Qt6::Test)
add_test(NAME media-report COMMAND media_report_test)
set_tests_properties(media-report PROPERTIES TIMEOUT 15)
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

add_executable(resume_test resume_test.cpp)
target_link_libraries(resume_test PRIVATE neko_session Qt6::Test)
target_compile_definitions(resume_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(resume_test test_media)
add_test(NAME resume COMMAND resume_test)
set_tests_properties(resume PROPERTIES TIMEOUT 60)

add_executable(playback_history_session_test playback_history_session_test.cpp)
target_link_libraries(playback_history_session_test PRIVATE neko_session Qt6::Test)
target_compile_definitions(playback_history_session_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(playback_history_session_test test_media)
add_test(NAME playback-history-session COMMAND playback_history_session_test)
set_tests_properties(playback-history-session PROPERTIES TIMEOUT 60)

set(TEST_TRACK_MEDIA "${CMAKE_CURRENT_BINARY_DIR}/media/multiple-tracks.mkv")
add_executable(track_preference_session_test track_preference_session_test.cpp)
target_link_libraries(track_preference_session_test PRIVATE neko_session Qt6::Test)
target_compile_definitions(track_preference_session_test PRIVATE
    TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(track_preference_session_test test_track_media)
add_test(NAME track-preference-session COMMAND track_preference_session_test)
set_tests_properties(track-preference-session PROPERTIES TIMEOUT 60)

add_executable(session_test session_test.cpp)
target_link_libraries(session_test PRIVATE neko_session Qt6::Test)
target_compile_definitions(session_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(session_test test_media)
add_test(NAME session COMMAND session_test)
set_tests_properties(session PROPERTIES TIMEOUT 60)

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
set(TEST_EXTERNAL_AUDIO "${CMAKE_CURRENT_BINARY_DIR}/media/外挂 audio [2].mka")
add_custom_command(OUTPUT "${TEST_EXTERNAL_AUDIO}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${TEST_TRACK_MEDIA}" -map 0:a -c copy "${TEST_EXTERNAL_AUDIO}"
    DEPENDS "${TEST_TRACK_MEDIA}" VERBATIM)
add_custom_target(test_external_audio DEPENDS "${TEST_EXTERNAL_AUDIO}")
add_executable(external_audio_test external_audio_test.cpp)
target_link_libraries(external_audio_test PRIVATE neko_playback Qt6::Test)
add_dependencies(external_audio_test test_track_media test_external_audio test_video_only)
add_test(NAME external-audio COMMAND external_audio_test
    "${TEST_TRACK_MEDIA}" "${TEST_EXTERNAL_AUDIO}" "${TEST_VIDEO_ONLY}")
set_tests_properties(external-audio PROPERTIES TIMEOUT 60)
add_executable(external_audio_session_test external_audio_session_test.cpp)
target_link_libraries(external_audio_session_test PRIVATE neko_session Qt6::Test)
target_compile_definitions(external_audio_session_test PRIVATE
    TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_EXTERNAL_AUDIO_PATH="${TEST_EXTERNAL_AUDIO}")
add_dependencies(external_audio_session_test test_track_media test_external_audio)
add_test(NAME external-audio-session COMMAND external_audio_session_test)
set_tests_properties(external-audio-session PROPERTIES TIMEOUT 60)
target_compile_definitions(screenshot_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}")
add_dependencies(screenshot_test test_track_media)
add_executable(subtitles_test subtitles_test.cpp)
target_link_libraries(subtitles_test PRIVATE neko_session Qt6::Test)
target_compile_definitions(subtitles_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(subtitles_test test_track_media)
add_test(NAME subtitles COMMAND subtitles_test)
set_tests_properties(subtitles PROPERTIES TIMEOUT 60)
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
target_compile_definitions(subtitles_test PRIVATE TEST_MULTI_SUBTITLE_PATH="${TEST_MULTI_SUBTITLE}")
add_dependencies(subtitles_test test_multi_subtitle)
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

add_executable(video_color_test video_color_test.cpp)
target_link_libraries(video_color_test PRIVATE neko_session Qt6::Test)
add_test(NAME video-color COMMAND video_color_test)
set_tests_properties(video-color PROPERTIES TIMEOUT 60)

add_executable(video_color_dialog_test video_color_dialog_test.cpp)
target_link_libraries(video_color_dialog_test PRIVATE neko_ui Qt6::Test)
add_test(NAME video-color-dialog COMMAND video_color_dialog_test)
set_tests_properties(video-color-dialog PROPERTIES TIMEOUT 30)

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
add_executable(fullscreen_feedback_window_test fullscreen_feedback_window_test.cpp)
target_link_libraries(fullscreen_feedback_window_test PRIVATE neko_ui Qt6::Test)
target_compile_definitions(fullscreen_feedback_window_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(fullscreen_feedback_window_test test_media)
add_test(NAME fullscreen-feedback-window
    COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:fullscreen_feedback_window_test>)
set_tests_properties(fullscreen-feedback-window PROPERTIES TIMEOUT 30
    ENVIRONMENT "${WINDOW_TEST_ENVIRONMENT}")
add_executable(media_information_window_test media_information_window_test.cpp)
target_link_libraries(media_information_window_test PRIVATE neko_ui Qt6::Test)
target_compile_definitions(media_information_window_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(media_information_window_test test_media)
add_test(NAME media-information-window COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:media_information_window_test>)
set_tests_properties(media-information-window PROPERTIES TIMEOUT 30 ENVIRONMENT "${WINDOW_TEST_ENVIRONMENT}")
add_executable(frame_step_window_test frame_step_window_test.cpp)
target_link_libraries(frame_step_window_test PRIVATE neko_ui Qt6::Test)
target_compile_definitions(frame_step_window_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}"
    TEST_AUDIO_ONLY_PATH="${TEST_AUDIO_ONLY}")
add_dependencies(frame_step_window_test test_media test_audio_only)
add_test(NAME frame-step-window COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:frame_step_window_test>)
set_tests_properties(frame-step-window PROPERTIES TIMEOUT 30 ENVIRONMENT "${WINDOW_TEST_ENVIRONMENT}")
add_executable(ab_loop_window_test ab_loop_window_test.cpp)
target_link_libraries(ab_loop_window_test PRIVATE neko_ui Qt6::Test)
target_compile_definitions(ab_loop_window_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}")
add_dependencies(ab_loop_window_test test_media)
add_test(NAME ab-loop-window COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:ab_loop_window_test>)
set_tests_properties(ab-loop-window PROPERTIES TIMEOUT 30 ENVIRONMENT "${WINDOW_TEST_ENVIRONMENT}")
add_executable(video_window_test video_window_test.cpp)
target_link_libraries(video_window_test PRIVATE neko_ui Qt6::Test)
add_test(NAME video-window COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:video_window_test>)
set_tests_properties(video-window PROPERTIES TIMEOUT 30 ENVIRONMENT "${WINDOW_TEST_ENVIRONMENT}")
add_executable(folder_window_test folder_window_test.cpp)
target_link_libraries(folder_window_test PRIVATE neko_ui Qt6::Test)
add_test(NAME folder-window COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:folder_window_test>)
set_tests_properties(folder-window PROPERTIES TIMEOUT 30 ENVIRONMENT "${WINDOW_TEST_ENVIRONMENT}")
add_executable(file_drop_window_test file_drop_window_test.cpp)
target_link_libraries(file_drop_window_test PRIVATE neko_ui Qt6::Test)
target_compile_definitions(file_drop_window_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(file_drop_window_test test_track_media)
add_test(NAME file-drop-window COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:file_drop_window_test>)
set_tests_properties(file-drop-window PROPERTIES TIMEOUT 30 ENVIRONMENT "${WINDOW_TEST_ENVIRONMENT}")
add_executable(playlist_window_test playlist_window_test.cpp)
target_link_libraries(playlist_window_test PRIVATE neko_ui Qt6::Test)
add_test(NAME playlist-window COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:playlist_window_test>)
set_tests_properties(playlist-window PROPERTIES TIMEOUT 30 ENVIRONMENT "${WINDOW_TEST_ENVIRONMENT}")
add_executable(subtitle_window_test subtitle_window_test.cpp)
target_link_libraries(subtitle_window_test PRIVATE neko_ui Qt6::Test)
add_test(NAME subtitle-window COMMAND ${WINDOW_TEST_RUNNER} $<TARGET_FILE:subtitle_window_test>)
set_tests_properties(subtitle-window PROPERTIES TIMEOUT 30 ENVIRONMENT "${WINDOW_TEST_ENVIRONMENT}")
target_link_libraries(window_test PRIVATE neko_ui Qt6::Test)
qt_add_translations(window_test TS_FILES ../translations/NekoPlayer_en.ts
    RESOURCE_PREFIX "/i18n")
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
