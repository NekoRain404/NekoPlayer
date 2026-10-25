set(TEST_MEDIA "${CMAKE_CURRENT_BINARY_DIR}/media/猫 sample [1].mkv")
add_custom_command(OUTPUT "${TEST_MEDIA}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/media"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -f lavfi -i testsrc2=size=320x180:rate=25
        -f lavfi -i sine=frequency=440:sample_rate=48000
        -t 4 -c:v mpeg4 -q:v 4 -c:a pcm_s16le "${TEST_MEDIA}"
    VERBATIM)
add_custom_target(test_media DEPENDS "${TEST_MEDIA}")

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

set(TEST_STILL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/media/still.png")
add_custom_command(OUTPUT "${TEST_STILL_IMAGE}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${TEST_MEDIA}" -frames:v 1 -update 1 "${TEST_STILL_IMAGE}"
    DEPENDS "${TEST_MEDIA}" VERBATIM)
add_custom_target(test_still_image DEPENDS "${TEST_STILL_IMAGE}")

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

set(TEST_VIDEO_ONLY "${CMAKE_CURRENT_BINARY_DIR}/media/video-only.mkv")
add_custom_command(OUTPUT "${TEST_VIDEO_ONLY}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${TEST_MEDIA}" -map 0:v -c copy "${TEST_VIDEO_ONLY}"
    DEPENDS "${TEST_MEDIA}" VERBATIM)
add_custom_target(test_video_only DEPENDS "${TEST_VIDEO_ONLY}")

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
set(TEST_EXTERNAL_AUDIO "${CMAKE_CURRENT_BINARY_DIR}/media/外挂 audio [2].mka")
add_custom_command(OUTPUT "${TEST_EXTERNAL_AUDIO}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${TEST_TRACK_MEDIA}" -map 0:a -c copy "${TEST_EXTERNAL_AUDIO}"
    DEPENDS "${TEST_TRACK_MEDIA}" VERBATIM)
add_custom_target(test_external_audio DEPENDS "${TEST_EXTERNAL_AUDIO}")

set(TEST_MULTI_SUBTITLE "${CMAKE_CURRENT_BINARY_DIR}/media/multiple-subtitles.mks")
add_custom_command(OUTPUT "${TEST_MULTI_SUBTITLE}"
    COMMAND ${FFMPEG_EXECUTABLE} -hide_banner -loglevel error -y
        -i "${CMAKE_CURRENT_SOURCE_DIR}/data/embedded.srt"
        -i "${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt"
        -map 0:s -map 1:s -c:s srt -f matroska "${TEST_MULTI_SUBTITLE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/embedded.srt"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/外部 subtitle [1].srt" test_track_media VERBATIM)
add_custom_target(test_multi_subtitle DEPENDS "${TEST_MULTI_SUBTITLE}")
