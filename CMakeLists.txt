cmake_minimum_required(VERSION 3.25)
project(NekoPlayer VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

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

add_library(neko_playback src/playback/mpv/mpvcontroller.cpp src/playback/mpv/mpvtracks.cpp
    src/playback/mpv/audiodevice.cpp src/playback/mpv/mpvcontroller_audio.cpp
    src/playback/mpv/mpvcontroller_events.cpp src/playback/mpv/mpvcontroller_information.cpp
    src/playback/mpv/mpvcontroller_trackpreferences.cpp
    src/playback/mpv/mpvcontroller_languages.cpp
    src/playback/mpv/mpvcontroller_externaltracks.cpp
    src/playback/mpv/mpvcontroller_navigation.cpp
    src/playback/mpv/mpvcontroller_loop.cpp
    src/playback/mpv/mpvcontroller_video.cpp
    src/playback/mpv/mpvcontroller_framing.cpp
    src/playback/mpv/mpvcontroller_screenshot.cpp
    src/playback/mpv/mpvcontroller_subtitles.cpp src/playback/mpv/subtitlepreferences.cpp
    src/playback/mpv/mpvcontroller_video_color.cpp src/playback/media/videocoloradjustment.cpp
    src/playback/mpv/mpvcontroller_hdr.cpp
    src/playback/mpv/mpvcontroller_deinterlace.cpp
    src/playback/mpv/mpvchapters.cpp src/playback/media/playbacktime.cpp src/playback/media/mediasource.cpp
    src/playback/mpv/backendinfo.cpp src/playback/playlist/playlistcontroller.cpp
    src/playback/playlist/playlistmodel.cpp src/playback/playlist/shuffleorder.cpp
    src/playback/playlist/naturalcompare.cpp)
target_include_directories(neko_playback PUBLIC src)
target_link_libraries(neko_playback PUBLIC neko_options Qt6::Core PkgConfig::MPV)

add_library(neko_input src/input/localfiledrop.cpp src/input/mediafolderscan.cpp
    src/input/folderscanjob.cpp src/input/mediafiletypes.cpp)
target_link_libraries(neko_input PUBLIC neko_playback Qt6::Concurrent)

add_library(neko_session src/session/configfile.cpp src/session/sessionstore.cpp
    src/session/sessionmanager.cpp
    src/session/recentmediahistory.cpp
    src/session/playbackhistorystore.cpp src/session/trackpreferencestore.cpp
    src/library/medialibrary.cpp)
target_link_libraries(neko_session PUBLIC neko_playback neko_input Qt6::Concurrent)

add_library(neko_preview src/preview/ffmpegthumbnaildecoder.cpp src/preview/thumbnailservice.cpp)
target_include_directories(neko_preview PUBLIC src)
target_link_libraries(neko_preview PUBLIC neko_options Qt6::Core Qt6::Concurrent Qt6::Gui
    PkgConfig::AVFORMAT PkgConfig::AVCODEC PkgConfig::AVUTIL PkgConfig::SWSCALE)

add_library(neko_ui src/playback/render/videowidget.cpp src/ui/window/mainwindow.cpp src/ui/panels/playlistpane.cpp
    src/ui/theme/appearanceicons.cpp src/ui/theme/appicon.cpp
    src/ui/panels/playlistitemdelegate.cpp
    src/ui/panels/welcomepanel.cpp
    src/playlist/m3u8store.cpp
    src/ui/dialogs/mediainformationtext.cpp src/ui/dialogs/mediainformationdialog.cpp
    src/ui/panels/playbackoverlay.cpp src/ui/panels/playbackoverlaytext.cpp
    src/ui/window/windowactions.cpp src/ui/window/windowicons.cpp src/ui/window/shortcutoverride.cpp
    src/ui/menus/appearancemenu.cpp src/ui/window/windowsession.cpp
    src/ui/panels/transportbar.cpp src/ui/menus/foldermenu.cpp
    src/ui/window/windowopacitytransition.cpp
    src/ui/window/interactionpolicy.cpp src/ui/window/uimotionduration.cpp
    src/ui/panels/thumbnailpreview.cpp
    src/ui/panels/thumbnailpreviewpolicy.cpp
    src/ui/menus/recentmediamenu.cpp
    src/ui/menus/currentmediamenu.cpp src/ui/window/fullscreencontrols.cpp src/ui/theme/appearancepreferences.cpp
    src/ui/theme/appearancetokens.cpp src/ui/theme/appearancestylesheet.cpp
    src/ui/theme/languagemanager.cpp
    src/ui/menus/videoframingmenu.cpp
    src/ui/panels/actionfeedbacktext.cpp src/ui/panels/actionfeedbackoverlay.cpp src/ui/menus/trackmenus.cpp
    src/ui/menus/tuningmenus.cpp src/ui/menus/playlistmodemenu.cpp src/ui/menus/playlistmaintenancemenu.cpp
    src/ui/menus/navigationmenu.cpp src/ui/menus/videomenu.cpp
    src/ui/dialogs/timeseekdialog.cpp src/ui/dialogs/networkopendialog.cpp
    src/ui/menus/abloopmenu.cpp src/ui/menus/audiodevicemenu.cpp src/ui/menus/screenshotmenu.cpp
    src/ui/menus/subtitledisplaymenu.cpp src/ui/dialogs/subtitlestyledialog.cpp
    src/ui/dialogs/preferencesdialog.cpp src/ui/dialogs/preferencesformlayout.cpp src/ui/dialogs/preferencescoordinator.cpp src/ui/dialogs/preferencesappearancepage.cpp src/ui/dialogs/videocolordialog.cpp
    src/ui/dialogs/shortcutsdialog.cpp src/ui/dialogs/quickactionsdialog.cpp src/ui/window/windowtitle.cpp
    src/ui/dialogs/librarydialog.cpp)
target_link_libraries(neko_ui PUBLIC neko_playback neko_session neko_input neko_preview Qt6::Widgets
    Qt6::OpenGLWidgets)
qt_add_resources(neko_ui "appearance_resources" PREFIX "/appearance"
    FILES data/checkmark.png data/chevron.png data/chevron-up.png data/chevron-down.png)

add_executable(NekoPlayer src/main.cpp src/playback/mpv/backenddiagnostic.cpp)
target_link_libraries(NekoPlayer PRIVATE neko_ui)
target_compile_definitions(NekoPlayer PRIVATE NEKOPLAYER_VERSION="${PROJECT_VERSION}")
qt_add_translations(NekoPlayer TS_FILES translations/NekoPlayer_en.ts
    RESOURCE_PREFIX "/i18n")

install(TARGETS NekoPlayer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/nekoplayer.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/nekoplayer.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES data/nekoplayer.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/nekoplayer)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME code-size COMMAND ${CMAKE_COMMAND}
        -DNEKO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCodeSize.cmake)
    add_subdirectory(tests)
    add_test(NAME backend-startup COMMAND NekoPlayer --check-backend)
    set_tests_properties(backend-startup PROPERTIES TIMEOUT 15)
endif()
