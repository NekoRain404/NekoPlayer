cmake_minimum_required(VERSION 3.25)
project(NekoPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Concurrent Widgets OpenGLWidgets LinguistTools)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)

option(NEKOPLAYER_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_AUTOMOC ON)
add_library(neko_options INTERFACE)
target_compile_features(neko_options INTERFACE cxx_std_20)
if(MSVC)
    target_compile_options(neko_options INTERFACE /W4)
else()
    target_compile_options(neko_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

if(NEKOPLAYER_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
        target_compile_options(neko_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(neko_options INTERFACE -fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "This sanitizer preset requires GCC or Clang with ASan/UBSan")
    endif()
endif()

add_library(neko_playback src/playback/mpvcontroller.cpp src/playback/mpvtracks.cpp
    src/playback/audiodevice.cpp src/playback/mpvcontroller_audio.cpp
    src/playback/mpvcontroller_events.cpp src/playback/mpvcontroller_information.cpp
    src/playback/mpvcontroller_trackpreferences.cpp
    src/playback/mpvcontroller_externaltracks.cpp
    src/playback/mpvcontroller_navigation.cpp
    src/playback/mpvcontroller_loop.cpp
    src/playback/mpvcontroller_video.cpp
    src/playback/mpvcontroller_framing.cpp
    src/playback/mpvcontroller_screenshot.cpp
    src/playback/mpvcontroller_subtitles.cpp src/playback/subtitlepreferences.cpp
    src/playback/mpvcontroller_video_color.cpp src/playback/videocoloradjustment.cpp
    src/playback/mpvcontroller_hdr.cpp
    src/playback/mpvcontroller_deinterlace.cpp
    src/playback/mpvchapters.cpp src/playback/playbacktime.cpp src/playback/mediasource.cpp
    src/playback/playlistcontroller.cpp src/playback/playlistmodel.cpp src/playback/shuffleorder.cpp)
target_include_directories(neko_playback PUBLIC src)
target_link_libraries(neko_playback PUBLIC neko_options Qt6::Core PkgConfig::MPV)

add_library(neko_session src/session/sessionstore.cpp src/session/sessionmanager.cpp
    src/session/recentmediahistory.cpp
    src/session/playbackhistorystore.cpp src/session/trackpreferencestore.cpp)
target_link_libraries(neko_session PUBLIC neko_playback)

add_library(neko_input src/input/localfiledrop.cpp src/input/mediafolderscan.cpp
    src/input/folderscanjob.cpp src/input/mediafiletypes.cpp)
target_link_libraries(neko_input PUBLIC neko_playback Qt6::Concurrent)

add_library(neko_preview src/preview/ffmpegthumbnaildecoder.cpp src/preview/thumbnailservice.cpp)
target_include_directories(neko_preview PUBLIC src)
target_link_libraries(neko_preview PUBLIC neko_options Qt6::Core Qt6::Concurrent Qt6::Gui
    PkgConfig::AVFORMAT PkgConfig::AVCODEC PkgConfig::AVUTIL PkgConfig::SWSCALE)

add_library(neko_ui src/playback/videowidget.cpp src/ui/mainwindow.cpp src/ui/playlistpane.cpp
    src/ui/appearanceicons.cpp
    src/ui/playlistitemdelegate.cpp
    src/ui/welcomepanel.cpp
    src/playlist/m3u8store.cpp
    src/ui/mediainformationtext.cpp src/ui/mediainformationdialog.cpp
    src/ui/playbackoverlay.cpp src/ui/playbackoverlaytext.cpp
    src/ui/windowactions.cpp src/ui/appearancemenu.cpp src/ui/windowsession.cpp
    src/ui/transportbar.cpp src/ui/foldermenu.cpp
    src/ui/thumbnailpreview.cpp
    src/ui/thumbnailpreviewpolicy.cpp
    src/ui/recentmediamenu.cpp
    src/ui/currentmediamenu.cpp src/ui/fullscreencontrols.cpp src/ui/appearancepreferences.cpp
    src/ui/appearancetokens.cpp src/ui/appearancestylesheet.cpp
    src/ui/languagemanager.cpp
    src/ui/videoframingmenu.cpp
    src/ui/actionfeedbacktext.cpp src/ui/actionfeedbackoverlay.cpp src/ui/trackmenus.cpp
    src/ui/tuningmenus.cpp src/ui/playlistmodemenu.cpp src/ui/navigationmenu.cpp src/ui/videomenu.cpp
    src/ui/timeseekdialog.cpp src/ui/playbacksettingsdialog.cpp src/ui/networkopendialog.cpp
    src/ui/abloopmenu.cpp src/ui/audiodevicemenu.cpp src/ui/screenshotmenu.cpp
    src/ui/subtitledisplaymenu.cpp src/ui/subtitlesettingsdialog.cpp
    src/ui/preferencesdialog.cpp src/ui/preferencescoordinator.cpp src/ui/videocolordialog.cpp)
target_link_libraries(neko_ui PUBLIC neko_playback neko_session neko_input neko_preview Qt6::Widgets
    Qt6::OpenGLWidgets)

add_executable(NekoPlayer src/main.cpp src/playback/backenddiagnostic.cpp)
target_link_libraries(NekoPlayer PRIVATE neko_ui)
qt_add_translations(NekoPlayer TS_FILES translations/NekoPlayer_en.ts
    RESOURCE_PREFIX "/i18n")

include(CTest)
if(BUILD_TESTING)
    add_test(NAME code-size COMMAND ${CMAKE_COMMAND}
        -DNEKO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCodeSize.cmake)
    add_subdirectory(tests)
    add_test(NAME backend-startup COMMAND NekoPlayer --check-backend)
    set_tests_properties(backend-startup PROPERTIES TIMEOUT 15)
endif()
