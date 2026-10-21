# NekoPlayer

- Product direction: a PotPlayer-like desktop experience, C++20 / Qt 6 UI, libmpv as the playback engine.
- Every ordinary handwritten code file must stay at or below 500 lines, including tests and headers. The explicitly listed coordination and desktop-integration files in `cmake/CheckCodeSize.cmake` may reach 800 lines; add an exception only with a written reason. Split by responsibility before approaching either limit; do not compress formatting to evade it.
- Keep playback state/commands, graphics resources, and desktop interaction in separate layers. Use explicit names and narrow interfaces; UI code must not issue raw mpv commands. Prefer shared QAction objects for PotPlayer-like menus, shortcuts and transport controls.
- The approved rendering approach is Qt Widgets + QOpenGLWidget + libmpv render API. Do not infer Qt Quick, a second playback engine, or a skin system as requirements.
- Use target-based CMake, explicit error handling, RAII, and Qt parent ownership where appropriate. Keep GUI work on the GUI thread.
- Review libmpv event delivery, callback lifetime, graphics-context requirements, and shutdown together when adding playback.
- `references/` contains ignored, independent shallow clones for study. Do not edit them as application source or add them as gitlinks. Verify licenses before reusing upstream code; the application license has not been selected.
- Build with `cmake --preset debug` and `cmake --build --preset debug`; run `ctest --preset debug`. Use the sanitizer preset for lifetime-sensitive changes. GUI offscreen startup is not proof of video rendering, hardware decoding, or HDR correctness.
- Relevant installed skills and source links are in `docs/skills.md`. Follow the user's scope; installing workflow skills does not itself authorize unrelated tasks.
