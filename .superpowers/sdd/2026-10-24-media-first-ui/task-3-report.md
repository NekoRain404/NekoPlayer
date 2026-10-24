# Task 3 report: shared surface icons

Implemented a shared 24 × 24 logical vector icon engine for all 20 requested symbols. The engine renders at requested device-pixel ratios, uses palette button-text colors including the disabled palette group, and is used by main-window actions, playlist editing controls, and recent-media cards. Dynamic play/pause, mute, fullscreen, and mini-player choices retain their existing state conditions. Main-window palette changes replace registered action icons; playlist and welcome surfaces refresh their local icons on palette changes.

The welcome panel continues to copy the existing QAction icons, and button bindings, labels, shortcuts, accessibility names, and signal paths were left intact. `transportbar.cpp` needed no modification because it already binds the shared actions.

TDD RED command: `cmake --build --preset debug --target ui_surface_test -j4` failed as expected because `ui/appearanceicons.h` was absent. GREEN command: `cmake --build --preset debug --target appearance_preferences_test ui_surface_test -j4` passed. Focused tests: `ctest --test-dir build/debug -R '^(appearance-preferences|ui-surface)$' --output-on-failure` passed both tests. `git diff --check` passed.

Formatting: the new icon source and header were formatted. A whole-file `clang-format --dry-run --Werror` check over touched legacy files reports existing formatting violations in those files (including untouched lines); broad formatting would create unrelated churn, so their established formatting was preserved.

Line counts: `appearanceicons.h` 31; `appearanceicons.cpp` 164; `windowactions.cpp` 431; `mainwindow.h` 128; `playlistpane.cpp` 170; `welcomepanel.cpp` 297; `ui_surface_test.cpp` 261; `appearance_preferences_test.cpp` 281. `mainwindow.cpp` is 581 lines, up from 580; it is an existing special file allowed up to 800 lines by `cmake/CheckCodeSize.cmake`.

Limitations: the explicit symbol list has no dedicated seek-direction or volume-direction variants. Seek-back/forward use the distinct `Previous`/`Next` arrows; volume-up/down use `Add`/`Remove`. Several source-era standard icons had no exact semantic counterpart and were mapped to the closest enumerated symbol.

Commit: `6894923` (`feat(ui): unify main window iconography`).
