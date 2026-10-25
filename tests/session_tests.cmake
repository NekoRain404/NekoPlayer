add_executable(recent_media_history_test recent_media_history_test.cpp)
target_link_libraries(recent_media_history_test PRIVATE neko_session Qt6::Test)
add_test(NAME recent-media-history COMMAND recent_media_history_test)
set_tests_properties(recent-media-history PROPERTIES TIMEOUT 20)

add_executable(network_session_test network_session_test.cpp)
target_link_libraries(network_session_test PRIVATE neko_session Qt6::Test)
add_test(NAME network-session COMMAND network_session_test)
set_tests_properties(network-session PROPERTIES TIMEOUT 30)

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

add_executable(decoding_test decoding_test.cpp)
target_link_libraries(decoding_test PRIVATE neko_session Qt6::Test)
target_compile_definitions(decoding_test PRIVATE TEST_MEDIA_PATH="${TEST_MEDIA}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(decoding_test test_media)
add_test(NAME decoding COMMAND decoding_test)
set_tests_properties(decoding PROPERTIES TIMEOUT 60)

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

add_executable(interaction_preferences_test interaction_preferences_test.cpp)
target_link_libraries(interaction_preferences_test PRIVATE neko_session Qt6::Test)
add_test(NAME interaction-preferences COMMAND interaction_preferences_test)
set_tests_properties(interaction-preferences PROPERTIES TIMEOUT 20)

add_executable(language_preferences_test language_preferences_test.cpp)
target_link_libraries(language_preferences_test PRIVATE neko_session Qt6::Test)
add_test(NAME language-preferences COMMAND language_preferences_test)
set_tests_properties(language-preferences PROPERTIES TIMEOUT 20)

add_executable(external_audio_session_test external_audio_session_test.cpp)
target_link_libraries(external_audio_session_test PRIVATE neko_session Qt6::Test)
target_compile_definitions(external_audio_session_test PRIVATE
    TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_EXTERNAL_AUDIO_PATH="${TEST_EXTERNAL_AUDIO}")
add_dependencies(external_audio_session_test test_track_media test_external_audio)
add_test(NAME external-audio-session COMMAND external_audio_session_test)
set_tests_properties(external-audio-session PROPERTIES TIMEOUT 60)

add_executable(subtitles_test subtitles_test.cpp)
target_link_libraries(subtitles_test PRIVATE neko_session Qt6::Test)
target_compile_definitions(subtitles_test PRIVATE TEST_TRACK_MEDIA_PATH="${TEST_TRACK_MEDIA}"
    TEST_EXTERNAL_SUBTITLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt")
add_dependencies(subtitles_test test_track_media)
add_test(NAME subtitles COMMAND subtitles_test)
set_tests_properties(subtitles PROPERTIES TIMEOUT 60)

target_compile_definitions(subtitles_test PRIVATE TEST_MULTI_SUBTITLE_PATH="${TEST_MULTI_SUBTITLE}")
add_dependencies(subtitles_test test_multi_subtitle)

add_executable(video_color_test video_color_test.cpp)
target_link_libraries(video_color_test PRIVATE neko_session Qt6::Test)
add_test(NAME video-color COMMAND video_color_test)
set_tests_properties(video-color PROPERTIES TIMEOUT 60)
