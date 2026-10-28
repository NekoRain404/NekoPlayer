add_executable(seek_slider_geometry_test seek_slider_geometry_test.cpp)
target_include_directories(seek_slider_geometry_test PRIVATE ../src)
target_link_libraries(seek_slider_geometry_test PRIVATE Qt6::Widgets Qt6::Test)
add_test(NAME seek-slider-geometry COMMAND seek_slider_geometry_test)
set_tests_properties(seek-slider-geometry PROPERTIES TIMEOUT 20)

add_executable(preferences_dialog_test preferences_dialog_test.cpp)
target_link_libraries(preferences_dialog_test PRIVATE neko_ui Qt6::Test)
add_test(NAME preferences-dialog COMMAND preferences_dialog_test)
set_tests_properties(preferences-dialog PROPERTIES TIMEOUT 20)

add_executable(quick_actions_dialog_test quick_actions_dialog_test.cpp)
target_link_libraries(quick_actions_dialog_test PRIVATE neko_ui Qt6::Test)
add_test(NAME quick-actions-dialog COMMAND quick_actions_dialog_test)
set_tests_properties(quick-actions-dialog PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(preferences_appearance_test preferences_appearance_test.cpp)
target_link_libraries(preferences_appearance_test PRIVATE neko_ui Qt6::Test)
add_test(NAME preferences-appearance COMMAND preferences_appearance_test)
set_tests_properties(preferences-appearance PROPERTIES TIMEOUT 20)

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

add_executable(appearance_surface_contract_test appearance_surface_contract_test.cpp)
target_link_libraries(appearance_surface_contract_test PRIVATE neko_ui Qt6::Test)
add_test(NAME appearance-surface-contract COMMAND appearance_surface_contract_test)
set_tests_properties(appearance-surface-contract PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(window_title_test window_title_test.cpp ../src/ui/windowtitle.cpp)
target_include_directories(window_title_test PRIVATE ../src)
target_link_libraries(window_title_test PRIVATE neko_playback Qt6::Test)
add_test(NAME title-format COMMAND window_title_test)
set_tests_properties(title-format PROPERTIES TIMEOUT 20)

add_executable(interaction_policy_test interaction_policy_test.cpp
    ../src/ui/interactionpolicy.cpp ../src/ui/uimotionduration.cpp)
target_include_directories(interaction_policy_test PRIVATE ../src)
target_link_libraries(interaction_policy_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME interaction-policy COMMAND interaction_policy_test)
set_tests_properties(interaction-policy PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(window_opacity_transition_test window_opacity_transition_test.cpp
    ../src/ui/windowopacitytransition.cpp ../src/ui/uimotionduration.cpp
    ../src/ui/appearancetokens.cpp)
target_include_directories(window_opacity_transition_test PRIVATE ../src)
target_link_libraries(window_opacity_transition_test PRIVATE Qt6::Widgets Qt6::Test)
add_test(NAME window-opacity-transition COMMAND window_opacity_transition_test)
set_tests_properties(window-opacity-transition PROPERTIES TIMEOUT 20
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

add_executable(main_window_transport_material_test main_window_transport_material_test.cpp)
target_link_libraries(main_window_transport_material_test PRIVATE neko_ui Qt6::Test)
add_test(NAME main-window-transport-material COMMAND main_window_transport_material_test)
set_tests_properties(main-window-transport-material PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(fullscreen_transport_material_test fullscreen_transport_material_test.cpp)
target_link_libraries(fullscreen_transport_material_test PRIVATE neko_ui Qt6::Test)
add_test(NAME fullscreen-transport-material COMMAND fullscreen_transport_material_test)
set_tests_properties(fullscreen-transport-material PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(playback_overlay_style_test playback_overlay_style_test.cpp)
target_link_libraries(playback_overlay_style_test PRIVATE neko_ui Qt6::Test)
add_test(NAME playback-overlay-style COMMAND playback_overlay_style_test)
set_tests_properties(playback-overlay-style PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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

add_executable(action_feedback_overlay_test action_feedback_overlay_test.cpp)
target_link_libraries(action_feedback_overlay_test PRIVATE neko_ui Qt6::Test)
add_test(NAME action-feedback-overlay COMMAND action_feedback_overlay_test)
set_tests_properties(action-feedback-overlay PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(playback_overlay_text_test playback_overlay_text_test.cpp
    ../src/ui/playbackoverlaytext.cpp)
target_include_directories(playback_overlay_text_test PRIVATE ../src)
target_link_libraries(playback_overlay_text_test PRIVATE neko_playback Qt6::Test)
add_test(NAME playback-overlay-text COMMAND playback_overlay_text_test)
set_tests_properties(playback-overlay-text PROPERTIES TIMEOUT 20)

add_executable(thumbnail_preview_test thumbnail_preview_test.cpp)
target_link_libraries(thumbnail_preview_test PRIVATE neko_ui Qt6::Test)
add_test(NAME thumbnail-preview COMMAND thumbnail_preview_test)
set_tests_properties(thumbnail-preview PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(thumbnail_preview_policy_test thumbnail_preview_policy_test.cpp)
target_link_libraries(thumbnail_preview_policy_test PRIVATE neko_ui Qt6::Test)
add_test(NAME thumbnail-preview-policy COMMAND thumbnail_preview_policy_test)

add_executable(media_report_test media_report_test.cpp)
target_link_libraries(media_report_test PRIVATE neko_ui Qt6::Test)
add_test(NAME media-report COMMAND media_report_test)
set_tests_properties(media-report PROPERTIES TIMEOUT 15)

add_executable(playlist_pane_test playlist_pane_test.cpp)
target_link_libraries(playlist_pane_test PRIVATE neko_ui Qt6::Test)
add_test(NAME playlist-pane COMMAND playlist_pane_test)
set_tests_properties(playlist-pane PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(shortcuts_dialog_test shortcuts_dialog_test.cpp)
target_link_libraries(shortcuts_dialog_test PRIVATE neko_ui Qt6::Test)
add_test(NAME shortcuts-dialog COMMAND shortcuts_dialog_test)
set_tests_properties(shortcuts-dialog PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(playlist_mode_menu_test playlist_mode_menu_test.cpp)
target_link_libraries(playlist_mode_menu_test PRIVATE neko_ui Qt6::Test)
add_test(NAME playlist-mode-menu COMMAND playlist_mode_menu_test)
set_tests_properties(playlist-mode-menu PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(app_icon_test app_icon_test.cpp)
target_link_libraries(app_icon_test PRIVATE neko_ui Qt6::Test)
add_test(NAME app-icon COMMAND app_icon_test)
set_tests_properties(app-icon PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(video_color_dialog_test video_color_dialog_test.cpp)
target_link_libraries(video_color_dialog_test PRIVATE neko_ui Qt6::Test)
add_test(NAME video-color-dialog COMMAND video_color_dialog_test)
set_tests_properties(video-color-dialog PROPERTIES TIMEOUT 30)

add_executable(playlist_maintenance_menu_test playlist_maintenance_menu_test.cpp)
target_link_libraries(playlist_maintenance_menu_test PRIVATE neko_ui Qt6::Test)
add_test(NAME playlist-maintenance-menu COMMAND playlist_maintenance_menu_test)
set_tests_properties(playlist-maintenance-menu PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(shortcut_override_test shortcut_override_test.cpp)
target_link_libraries(shortcut_override_test PRIVATE neko_ui Qt6::Test)
add_test(NAME shortcut-override COMMAND shortcut_override_test)
set_tests_properties(shortcut-override PROPERTIES TIMEOUT 20
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(media_library_test media_library_test.cpp)
target_link_libraries(media_library_test PRIVATE neko_session Qt6::Test)
add_test(NAME media-library COMMAND media_library_test)
set_tests_properties(media-library PROPERTIES TIMEOUT 30)

add_executable(library_dialog_test library_dialog_test.cpp)
target_link_libraries(library_dialog_test PRIVATE neko_ui Qt6::Test)
add_test(NAME library-dialog COMMAND library_dialog_test)
set_tests_properties(library-dialog PROPERTIES TIMEOUT 30
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
