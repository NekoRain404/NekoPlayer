# Arch Linux package

The package installs the release binary, desktop entry, SVG icon, AppStream
metadata, and the GPL-3.0-only license.

The PKGBUILD downloads the matching source archive from the GitHub release
and verifies its SHA-256 checksum. It always tracks the **most recently
published** release (currently v0.1.0); the v0.2.0 pre-release will be published
by `.github/workflows/release.yml`, and `pkgver`/`sha256sums` are updated in a
follow-up commit after that asset exists. Never commit a placeholder checksum.

After the release is published:

```bash
cd packaging/arch
makepkg -si
```

`makepkg -si` installs the runtime dependencies (`qt6-base`, `qt6-translations`, `mpv`, `ffmpeg`,
`hicolor-icon-theme`) and the build tools (`cmake`, `ninja`, `qt6-tools`,
`pkgconf`) through pacman, then builds and installs the package.

The version and checksum in `PKGBUILD` correspond to the published source
archive. Update both when preparing a later release.

License: GPL-3.0-only. This was selected because the package links the system
FFmpeg build (GPL-3.0-only) and libmpv (GPL-2.0-or-later/LGPL-2.1-or-later).
